#include <stddef.h>
#include <stdint.h>

#include <gnuos/ipc.h>
#include <gnuos/printk.h>
#include <gnuos/sched.h>
#include <gnuos/spinlock.h>

typedef struct {
    uint8_t in_use;
    char name[IPC_CHANNEL_NAME_MAX];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    ipc_message_t queue[IPC_CHANNEL_QUEUE_CAPACITY];
} ipc_channel_t;

static spinlock_t g_ipc_lock;
static ipc_channel_t g_ipc_channels[IPC_MAX_CHANNELS];
static uint16_t g_ipc_channel_count;

static uint64_t ipc_irq_save(void)
{
    uint64_t rflags = 0;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void ipc_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static uint16_t ipc_strnlen(const char *text, uint16_t limit)
{
    uint16_t length = 0;

    if (!text) {
        return 0U;
    }

    while (length < limit && text[length] != '\0') {
        length++;
    }

    return length;
}

static int ipc_name_equal(const char *left, const char *right)
{
    uint16_t index = 0;

    if (!left || !right) {
        return 0;
    }

    while (index < IPC_CHANNEL_NAME_MAX) {
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

static void ipc_name_copy(char *dst, const char *src)
{
    uint16_t index = 0;

    if (!dst) {
        return;
    }

    for (index = 0U; index < IPC_CHANNEL_NAME_MAX; index++) {
        dst[index] = '\0';
    }

    if (!src) {
        return;
    }

    for (index = 0U; index + 1U < IPC_CHANNEL_NAME_MAX && src[index] != '\0'; index++) {
        dst[index] = src[index];
    }
}

static int ipc_find_channel_locked(const char *name)
{
    for (uint16_t index = 0U; index < IPC_MAX_CHANNELS; index++) {
        if (!g_ipc_channels[index].in_use) {
            continue;
        }

        if (ipc_name_equal(g_ipc_channels[index].name, name)) {
            return (int)index;
        }
    }

    return -1;
}

static int ipc_is_valid_channel_id(int channel_id)
{
    if (channel_id < 0) {
        return 0;
    }
    if ((uint16_t)channel_id >= IPC_MAX_CHANNELS) {
        return 0;
    }
    if (!g_ipc_channels[(uint16_t)channel_id].in_use) {
        return 0;
    }

    return 1;
}

void ipc_init(void)
{
    uint64_t irq_flags;

    spinlock_init(&g_ipc_lock);
    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);

    for (uint16_t index = 0U; index < IPC_MAX_CHANNELS; index++) {
        g_ipc_channels[index].in_use = 0U;
        g_ipc_channels[index].head = 0U;
        g_ipc_channels[index].tail = 0U;
        g_ipc_channels[index].count = 0U;
        ipc_name_copy(g_ipc_channels[index].name, "");
    }
    g_ipc_channel_count = 0U;

    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);

    kprintf("GNU OS: IPC subsystem initialized.\n");
}

int ipc_channel_create(const char *name)
{
    int existing = -1;
    int free_slot = -1;
    uint16_t name_len;
    uint64_t irq_flags;

    name_len = ipc_strnlen(name, IPC_CHANNEL_NAME_MAX);
    if (name_len == 0U || name_len >= IPC_CHANNEL_NAME_MAX) {
        return -1;
    }

    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);

    existing = ipc_find_channel_locked(name);
    if (existing >= 0) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return existing;
    }

    for (uint16_t index = 0U; index < IPC_MAX_CHANNELS; index++) {
        if (!g_ipc_channels[index].in_use) {
            free_slot = (int)index;
            break;
        }
    }

    if (free_slot < 0) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -2;
    }

    g_ipc_channels[(uint16_t)free_slot].in_use = 1U;
    g_ipc_channels[(uint16_t)free_slot].head = 0U;
    g_ipc_channels[(uint16_t)free_slot].tail = 0U;
    g_ipc_channels[(uint16_t)free_slot].count = 0U;
    ipc_name_copy(g_ipc_channels[(uint16_t)free_slot].name, name);
    g_ipc_channel_count++;

    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);

    kprintf("GNU OS: IPC channel created name=%s id=%u\n", name, (uint64_t)(uint16_t)free_slot);
    return free_slot;
}

int ipc_channel_find(const char *name)
{
    int result;
    uint64_t irq_flags;

    if (!name) {
        return -1;
    }

    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);
    result = ipc_find_channel_locked(name);
    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);

    return result;
}

int ipc_channel_send(int channel_id, const void *payload, uint16_t size, uint64_t sender_tid)
{
    ipc_channel_t *channel;
    ipc_message_t *message;
    task_t *current_task = NULL;
    uint64_t irq_flags;

    if (!payload || size == 0U || size > IPC_MESSAGE_DATA_MAX) {
        return -1;
    }

    if (sender_tid == 0U) {
        current_task = sched_current_task();
        if (current_task) {
            sender_tid = current_task->tid;
        }
    }

    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);

    if (!ipc_is_valid_channel_id(channel_id)) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -2;
    }

    channel = &g_ipc_channels[(uint16_t)channel_id];
    if (channel->count >= IPC_CHANNEL_QUEUE_CAPACITY) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -3;
    }

    message = &channel->queue[channel->tail];
    message->sender_tid = sender_tid;
    message->size = size;
    for (uint16_t index = 0U; index < size; index++) {
        message->data[index] = ((const uint8_t *)payload)[index];
    }

    channel->tail = (uint16_t)((channel->tail + 1U) % IPC_CHANNEL_QUEUE_CAPACITY);
    channel->count++;

    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);
    return 0;
}

int ipc_channel_recv(
    int channel_id,
    void *payload,
    uint16_t payload_capacity,
    uint16_t *out_size,
    uint64_t *out_sender_tid)
{
    ipc_channel_t *channel;
    ipc_message_t *message;
    uint64_t irq_flags;

    if (!payload || !out_size || !out_sender_tid) {
        return -1;
    }

    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);

    if (!ipc_is_valid_channel_id(channel_id)) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -2;
    }

    channel = &g_ipc_channels[(uint16_t)channel_id];
    if (channel->count == 0U) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -3;
    }

    message = &channel->queue[channel->head];
    if (payload_capacity < message->size) {
        spinlock_unlock(&g_ipc_lock);
        ipc_irq_restore(irq_flags);
        return -4;
    }

    for (uint16_t index = 0U; index < message->size; index++) {
        ((uint8_t *)payload)[index] = message->data[index];
    }

    *out_size = message->size;
    *out_sender_tid = message->sender_tid;
    channel->head = (uint16_t)((channel->head + 1U) % IPC_CHANNEL_QUEUE_CAPACITY);
    channel->count--;

    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);
    return 0;
}

uint16_t ipc_channel_count(void)
{
    uint16_t count = 0U;
    uint64_t irq_flags;

    irq_flags = ipc_irq_save();
    spinlock_lock(&g_ipc_lock);
    count = g_ipc_channel_count;
    spinlock_unlock(&g_ipc_lock);
    ipc_irq_restore(irq_flags);

    return count;
}
