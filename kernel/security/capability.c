#include <stdint.h>

#include <gnuos/capability.h>
#include <gnuos/printk.h>
#include <gnuos/spinlock.h>

typedef struct {
    uint8_t in_use;
    uint64_t owner_tid;
    uint64_t object_id;
    uint16_t rights;
} capability_slot_t;

static spinlock_t g_capability_lock;
static capability_slot_t g_capability_slots[CAPABILITY_MAX_ENTRIES];
static uint16_t g_capability_count;

static uint64_t capability_irq_save(void)
{
    uint64_t rflags = 0;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void capability_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static int capability_is_valid_id(uint16_t capability_id)
{
    return capability_id > 0U && capability_id <= CAPABILITY_MAX_ENTRIES;
}

static uint16_t capability_index_from_id(uint16_t capability_id)
{
    return (uint16_t)(capability_id - 1U);
}

static int capability_rights_allow(uint16_t current, uint16_t requested)
{
    return (current & requested) == requested;
}

void capability_init(void)
{
    uint64_t irq_flags;

    spinlock_init(&g_capability_lock);
    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);

    for (uint16_t index = 0U; index < CAPABILITY_MAX_ENTRIES; index++) {
        g_capability_slots[index].in_use = 0U;
        g_capability_slots[index].owner_tid = 0U;
        g_capability_slots[index].object_id = 0U;
        g_capability_slots[index].rights = 0U;
    }
    g_capability_count = 0U;

    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);

    kprintf("GNU OS: capability subsystem initialized.\n");
}

int capability_create(
    uint64_t owner_tid,
    uint64_t object_id,
    uint16_t rights,
    uint16_t *out_capability_id)
{
    int status = -2;
    uint64_t irq_flags;

    if (!out_capability_id || rights == 0U) {
        return -1;
    }

    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);

    for (uint16_t index = 0U; index < CAPABILITY_MAX_ENTRIES; index++) {
        if (g_capability_slots[index].in_use != 0U) {
            continue;
        }

        g_capability_slots[index].in_use = 1U;
        g_capability_slots[index].owner_tid = owner_tid;
        g_capability_slots[index].object_id = object_id;
        g_capability_slots[index].rights = rights;
        g_capability_count++;
        *out_capability_id = (uint16_t)(index + 1U);
        status = 0;
        break;
    }

    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);
    return status;
}

int capability_validate_transfer(uint16_t capability_id, uint64_t owner_tid, uint16_t rights_mask)
{
    int status = 0;
    uint16_t index;
    capability_slot_t *slot;
    uint64_t irq_flags;

    if (!capability_is_valid_id(capability_id) || rights_mask == 0U) {
        return -1;
    }

    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);

    index = capability_index_from_id(capability_id);
    slot = &g_capability_slots[index];
    if (slot->in_use == 0U) {
        status = -2;
    } else if (slot->owner_tid != owner_tid) {
        status = -3;
    } else if ((slot->rights & CAP_RIGHT_TRANSFER) == 0U) {
        status = -4;
    } else if (!capability_rights_allow(slot->rights, rights_mask)) {
        status = -5;
    }

    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);
    return status;
}

int capability_copy_for_owner(
    uint16_t capability_id,
    uint64_t owner_tid,
    uint16_t rights_mask,
    uint64_t target_tid,
    uint16_t *out_capability_id)
{
    int status = -2;
    uint16_t source_index;
    capability_slot_t source_slot;
    uint64_t irq_flags;

    if (!out_capability_id ||
        !capability_is_valid_id(capability_id) ||
        rights_mask == 0U) {
        return -1;
    }

    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);

    source_index = capability_index_from_id(capability_id);
    source_slot = g_capability_slots[source_index];
    if (source_slot.in_use == 0U) {
        status = -2;
        goto done;
    }
    if (source_slot.owner_tid != owner_tid) {
        status = -3;
        goto done;
    }
    if ((source_slot.rights & CAP_RIGHT_TRANSFER) == 0U) {
        status = -4;
        goto done;
    }
    if (!capability_rights_allow(source_slot.rights, rights_mask)) {
        status = -5;
        goto done;
    }

    for (uint16_t index = 0U; index < CAPABILITY_MAX_ENTRIES; index++) {
        if (g_capability_slots[index].in_use != 0U) {
            continue;
        }

        g_capability_slots[index].in_use = 1U;
        g_capability_slots[index].owner_tid = target_tid;
        g_capability_slots[index].object_id = source_slot.object_id;
        g_capability_slots[index].rights = rights_mask;
        g_capability_count++;
        *out_capability_id = (uint16_t)(index + 1U);
        status = 0;
        goto done;
    }

done:
    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);
    return status;
}

int capability_describe(uint16_t capability_id, capability_info_t *out_info)
{
    capability_slot_t slot;
    uint16_t index;
    uint64_t irq_flags;

    if (!out_info || !capability_is_valid_id(capability_id)) {
        return -1;
    }

    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);

    index = capability_index_from_id(capability_id);
    slot = g_capability_slots[index];
    if (slot.in_use == 0U) {
        spinlock_unlock(&g_capability_lock);
        capability_irq_restore(irq_flags);
        return -2;
    }

    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);

    out_info->id = capability_id;
    out_info->owner_tid = slot.owner_tid;
    out_info->object_id = slot.object_id;
    out_info->rights = slot.rights;
    return 0;
}

uint16_t capability_count(void)
{
    uint16_t count;
    uint64_t irq_flags;

    irq_flags = capability_irq_save();
    spinlock_lock(&g_capability_lock);
    count = g_capability_count;
    spinlock_unlock(&g_capability_lock);
    capability_irq_restore(irq_flags);

    return count;
}
