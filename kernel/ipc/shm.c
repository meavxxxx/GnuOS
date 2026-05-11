#include <stddef.h>
#include <stdint.h>

#include <gnuos/mm.h>
#include <gnuos/printk.h>
#include <gnuos/sched.h>
#include <gnuos/shm.h>
#include <gnuos/spinlock.h>
#include <gnuos/vmm.h>

typedef struct {
    uint8_t in_use;
    char name[SHM_SEGMENT_NAME_MAX];
    void *base;
    uint64_t size;
    uint64_t owner_tid;
    uint16_t attachments;
    uint64_t attached_tids[SHM_MAX_ATTACHMENTS];
} shm_segment_t;

static spinlock_t g_shm_lock;
static shm_segment_t g_shm_segments[SHM_MAX_SEGMENTS];
static uint16_t g_shm_segment_count;

static uint64_t shm_irq_save(void)
{
    uint64_t rflags = 0;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void shm_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static uint64_t shm_current_tid(void)
{
    task_t *current = sched_current_task();
    if (current) {
        return current->tid;
    }

    return 0U;
}

static int shm_resolve_tid(uint64_t requested_tid, uint64_t *out_tid)
{
    uint64_t current_tid = shm_current_tid();

    if (!out_tid) {
        return 0;
    }

    if (requested_tid == 0U) {
        *out_tid = current_tid;
        return 1;
    }

    if (current_tid != 0U && requested_tid != current_tid) {
        return 0;
    }

    *out_tid = requested_tid;
    return 1;
}

static uint16_t shm_strnlen(const char *text, uint16_t limit)
{
    uint16_t length = 0U;

    if (!text) {
        return 0U;
    }

    while (length < limit && text[length] != '\0') {
        length++;
    }

    return length;
}

static void shm_name_copy(char *dst, const char *src)
{
    uint16_t index;

    if (!dst) {
        return;
    }

    for (index = 0U; index < SHM_SEGMENT_NAME_MAX; index++) {
        dst[index] = '\0';
    }

    if (!src) {
        return;
    }

    for (index = 0U; index + 1U < SHM_SEGMENT_NAME_MAX && src[index] != '\0'; index++) {
        dst[index] = src[index];
    }
}

static int shm_name_equal(const char *left, const char *right)
{
    uint16_t index = 0U;

    if (!left || !right) {
        return 0;
    }

    while (index < SHM_SEGMENT_NAME_MAX) {
        if (left[index] != right[index]) {
            return 0;
        }
        if (left[index] == '\0') {
            return 1;
        }
        index++;
    }

    return 0;
}

static int shm_find_locked(const char *name)
{
    for (uint16_t index = 0U; index < SHM_MAX_SEGMENTS; index++) {
        if (g_shm_segments[index].in_use == 0U) {
            continue;
        }

        if (shm_name_equal(g_shm_segments[index].name, name)) {
            return (int)index;
        }
    }

    return -1;
}

static int shm_find_attachment_locked(const shm_segment_t *segment, uint64_t tid)
{
    if (!segment) {
        return -1;
    }

    for (uint16_t index = 0U; index < segment->attachments; index++) {
        if (segment->attached_tids[index] == tid) {
            return (int)index;
        }
    }

    return -1;
}

void shm_init(void)
{
    uint64_t irq_flags;

    spinlock_init(&g_shm_lock);
    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);

    for (uint16_t index = 0U; index < SHM_MAX_SEGMENTS; index++) {
        g_shm_segments[index].in_use = 0U;
        g_shm_segments[index].base = NULL;
        g_shm_segments[index].size = 0U;
        g_shm_segments[index].owner_tid = 0U;
        g_shm_segments[index].attachments = 0U;
        for (uint16_t slot = 0U; slot < SHM_MAX_ATTACHMENTS; slot++) {
            g_shm_segments[index].attached_tids[slot] = 0U;
        }
        shm_name_copy(g_shm_segments[index].name, "");
    }
    g_shm_segment_count = 0U;

    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);

    kprintf("GNU OS: SHM subsystem initialized.\n");
}

int shm_create(const char *name, uint64_t size, uint64_t owner_tid)
{
    int existing = -1;
    int free_slot = -1;
    uint64_t resolved_owner_tid = 0U;
    uint64_t aligned_size = 0U;
    uint64_t page_count = 0U;
    void *mapping = NULL;
    uint64_t irq_flags;

    if (!name || size == 0U) {
        return -1;
    }
    if (!shm_resolve_tid(owner_tid, &resolved_owner_tid)) {
        return -1;
    }
    if (shm_strnlen(name, SHM_SEGMENT_NAME_MAX) >= SHM_SEGMENT_NAME_MAX) {
        return -1;
    }

    aligned_size = (size + (MM_PAGE_SIZE - 1U)) & ~(MM_PAGE_SIZE - 1U);
    page_count = aligned_size / MM_PAGE_SIZE;
    if (page_count == 0U) {
        return -1;
    }

    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);

    existing = shm_find_locked(name);
    if (existing >= 0) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return existing;
    }

    for (uint16_t index = 0U; index < SHM_MAX_SEGMENTS; index++) {
        if (g_shm_segments[index].in_use == 0U) {
            free_slot = (int)index;
            break;
        }
    }

    if (free_slot < 0) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return -2;
    }

    mapping = vmm_alloc_kernel_pages(page_count, VMM_MAP_WRITABLE);
    if (!mapping) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return -3;
    }

    g_shm_segments[(uint16_t)free_slot].in_use = 1U;
    g_shm_segments[(uint16_t)free_slot].base = mapping;
    g_shm_segments[(uint16_t)free_slot].size = aligned_size;
    g_shm_segments[(uint16_t)free_slot].owner_tid = resolved_owner_tid;
    g_shm_segments[(uint16_t)free_slot].attachments = 0U;
    for (uint16_t slot = 0U; slot < SHM_MAX_ATTACHMENTS; slot++) {
        g_shm_segments[(uint16_t)free_slot].attached_tids[slot] = 0U;
    }
    shm_name_copy(g_shm_segments[(uint16_t)free_slot].name, name);
    g_shm_segment_count++;

    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);

    kprintf(
        "GNU OS: SHM segment created name=%s id=%u size=0x%X owner=%u\n",
        name,
        (uint64_t)(uint16_t)free_slot,
        aligned_size,
        resolved_owner_tid);
    return free_slot;
}

int shm_find(const char *name)
{
    int segment_id = -1;
    uint64_t irq_flags;

    if (!name) {
        return -1;
    }

    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);
    segment_id = shm_find_locked(name);
    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);

    return segment_id;
}

int shm_attach(int segment_id, uint64_t requester_tid, void **out_address, uint64_t *out_size)
{
    shm_segment_t *segment = NULL;
    uint64_t resolved_tid = 0U;
    uint64_t irq_flags;

    if (!out_address || !out_size || segment_id < 0 || (uint16_t)segment_id >= SHM_MAX_SEGMENTS) {
        return -1;
    }
    if (!shm_resolve_tid(requester_tid, &resolved_tid)) {
        return -1;
    }

    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);

    segment = &g_shm_segments[(uint16_t)segment_id];
    if (segment->in_use == 0U) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return -2;
    }

    if (shm_find_attachment_locked(segment, resolved_tid) < 0) {
        if (segment->attachments >= SHM_MAX_ATTACHMENTS) {
            spinlock_unlock(&g_shm_lock);
            shm_irq_restore(irq_flags);
            return -3;
        }

        segment->attached_tids[segment->attachments] = resolved_tid;
        segment->attachments++;
    }

    *out_address = segment->base;
    *out_size = segment->size;

    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);
    return 0;
}

int shm_detach(int segment_id, uint64_t requester_tid)
{
    shm_segment_t *segment = NULL;
    uint64_t resolved_tid = 0U;
    int attachment_index = -1;
    uint64_t irq_flags;

    if (segment_id < 0 || (uint16_t)segment_id >= SHM_MAX_SEGMENTS) {
        return -1;
    }
    if (!shm_resolve_tid(requester_tid, &resolved_tid)) {
        return -1;
    }

    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);

    segment = &g_shm_segments[(uint16_t)segment_id];
    if (segment->in_use == 0U) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return -2;
    }

    attachment_index = shm_find_attachment_locked(segment, resolved_tid);
    if (attachment_index < 0) {
        spinlock_unlock(&g_shm_lock);
        shm_irq_restore(irq_flags);
        return -3;
    }

    for (uint16_t index = (uint16_t)attachment_index; index + 1U < segment->attachments; index++) {
        segment->attached_tids[index] = segment->attached_tids[index + 1U];
    }
    if (segment->attachments > 0U) {
        segment->attachments--;
        segment->attached_tids[segment->attachments] = 0U;
    }

    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);
    return 0;
}

uint16_t shm_segment_count(void)
{
    uint16_t count = 0U;
    uint64_t irq_flags;

    irq_flags = shm_irq_save();
    spinlock_lock(&g_shm_lock);
    count = g_shm_segment_count;
    spinlock_unlock(&g_shm_lock);
    shm_irq_restore(irq_flags);

    return count;
}
