#include <stddef.h>
#include <stdint.h>

#include <gnuos/seccomp.h>
#include <gnuos/spinlock.h>
#include <gnuos/syscall.h>

#define SECCOMP_ACTION_UNSET 0xFFU

static spinlock_t g_seccomp_lock;
static uint8_t g_seccomp_default_action = SECCOMP_ACTION_ALLOW;
static uint8_t g_seccomp_overrides[SYSCALL_MAX_ENTRIES];
static seccomp_audit_event_t g_seccomp_audit_ring[SECCOMP_AUDIT_CAPACITY];
static uint16_t g_seccomp_audit_head;
static uint16_t g_seccomp_audit_size;
static uint64_t g_seccomp_audit_sequence;
static uint64_t g_seccomp_audit_total;

static uint64_t seccomp_irq_save(void)
{
    uint64_t rflags = 0U;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void seccomp_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static uint8_t seccomp_action_valid(uint8_t action)
{
    return action == SECCOMP_ACTION_ALLOW ||
        action == SECCOMP_ACTION_LOG ||
        action == SECCOMP_ACTION_ERRNO;
}

void seccomp_init(void)
{
    uint64_t irq_flags;

    spinlock_init(&g_seccomp_lock);
    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);

    g_seccomp_default_action = SECCOMP_ACTION_ALLOW;
    for (uint16_t index = 0U; index < SYSCALL_MAX_ENTRIES; index++) {
        g_seccomp_overrides[index] = SECCOMP_ACTION_UNSET;
    }

    g_seccomp_audit_head = 0U;
    g_seccomp_audit_size = 0U;
    g_seccomp_audit_sequence = 0U;
    g_seccomp_audit_total = 0U;

    for (uint16_t index = 0U; index < SECCOMP_AUDIT_CAPACITY; index++) {
        g_seccomp_audit_ring[index].sequence = 0U;
        g_seccomp_audit_ring[index].tid = 0U;
        g_seccomp_audit_ring[index].syscall_number = 0U;
        g_seccomp_audit_ring[index].action = SECCOMP_ACTION_ALLOW;
        g_seccomp_audit_ring[index].result = 0LL;
    }

    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
}

void seccomp_set_default_action(uint8_t action)
{
    uint64_t irq_flags;

    if (!seccomp_action_valid(action)) {
        return;
    }

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);
    g_seccomp_default_action = action;
    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
}

int seccomp_set_syscall_action(uint16_t syscall_number, uint8_t action)
{
    uint64_t irq_flags;

    if (syscall_number >= SYSCALL_MAX_ENTRIES || !seccomp_action_valid(action)) {
        return -1;
    }

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);
    g_seccomp_overrides[syscall_number] = action;
    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
    return 0;
}

uint8_t seccomp_get_syscall_action(uint64_t syscall_number)
{
    uint8_t action = SECCOMP_ACTION_ALLOW;
    uint64_t irq_flags;

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);

    action = g_seccomp_default_action;
    if (syscall_number < SYSCALL_MAX_ENTRIES &&
        g_seccomp_overrides[syscall_number] != SECCOMP_ACTION_UNSET) {
        action = g_seccomp_overrides[syscall_number];
    }

    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
    return action;
}

void seccomp_audit_record(uint64_t tid, uint64_t syscall_number, uint8_t action, int64_t result)
{
    uint64_t irq_flags;
    uint16_t slot;

    if (!seccomp_action_valid(action)) {
        return;
    }

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);

    slot = (uint16_t)((g_seccomp_audit_head + g_seccomp_audit_size) % SECCOMP_AUDIT_CAPACITY);
    if (g_seccomp_audit_size == SECCOMP_AUDIT_CAPACITY) {
        slot = g_seccomp_audit_head;
        g_seccomp_audit_head = (uint16_t)((g_seccomp_audit_head + 1U) % SECCOMP_AUDIT_CAPACITY);
    } else {
        g_seccomp_audit_size++;
    }

    g_seccomp_audit_sequence++;
    g_seccomp_audit_ring[slot].sequence = g_seccomp_audit_sequence;
    g_seccomp_audit_ring[slot].tid = tid;
    g_seccomp_audit_ring[slot].syscall_number = syscall_number;
    g_seccomp_audit_ring[slot].action = action;
    g_seccomp_audit_ring[slot].result = result;
    g_seccomp_audit_total++;

    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
}

uint64_t seccomp_audit_count(void)
{
    uint64_t count;
    uint64_t irq_flags;

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);
    count = g_seccomp_audit_total;
    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);

    return count;
}

int seccomp_audit_latest(seccomp_audit_event_t *out_event)
{
    uint16_t latest_slot;
    uint64_t irq_flags;

    if (!out_event) {
        return -1;
    }

    irq_flags = seccomp_irq_save();
    spinlock_lock(&g_seccomp_lock);

    if (g_seccomp_audit_size == 0U) {
        spinlock_unlock(&g_seccomp_lock);
        seccomp_irq_restore(irq_flags);
        return -2;
    }

    latest_slot =
        (uint16_t)((g_seccomp_audit_head + g_seccomp_audit_size - 1U) % SECCOMP_AUDIT_CAPACITY);
    *out_event = g_seccomp_audit_ring[latest_slot];

    spinlock_unlock(&g_seccomp_lock);
    seccomp_irq_restore(irq_flags);
    return 0;
}
