#include <stddef.h>
#include <stdint.h>

#include <gnuos/printk.h>
#include <gnuos/sched.h>
#include <gnuos/spinlock.h>
#include <gnuos/syscall.h>
#include <gnuos/uaccess.h>

#define SYSCALL_ENOSYS (-38LL)
#define SYSCALL_EFAULT (-14LL)

static spinlock_t g_syscall_lock;
static syscall_handler_t g_syscall_table[SYSCALL_MAX_ENTRIES];
static uint16_t g_syscall_registered_count;

static uint64_t syscall_irq_save(void)
{
    uint64_t rflags = 0U;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void syscall_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static int64_t sys_gettid(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5)
{
    task_t *current;

    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    current = sched_current_task();
    if (!current) {
        return 0LL;
    }

    return (int64_t)current->tid;
}

static int64_t sys_sched_yield(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5)
{
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    sched_yield();
    return 0LL;
}

static int64_t sys_gettid_user(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5)
{
    task_t *current;
    uint64_t tid = 0U;

    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    current = sched_current_task();
    if (current) {
        tid = current->tid;
    }

    if (uaccess_copy_to_user(arg0, &tid, sizeof(tid)) != 0) {
        return SYSCALL_EFAULT;
    }

    return 0LL;
}

void syscall_init(void)
{
    uint64_t irq_flags;

    spinlock_init(&g_syscall_lock);
    irq_flags = syscall_irq_save();
    spinlock_lock(&g_syscall_lock);

    for (uint16_t index = 0U; index < SYSCALL_MAX_ENTRIES; index++) {
        g_syscall_table[index] = NULL;
    }
    g_syscall_registered_count = 0U;

    spinlock_unlock(&g_syscall_lock);
    syscall_irq_restore(irq_flags);

    (void)syscall_register(SYS_GETTID, sys_gettid);
    (void)syscall_register(SYS_SCHED_YIELD, sys_sched_yield);
    (void)syscall_register(SYS_GETTID_USER, sys_gettid_user);
    kprintf(
        "GNU OS: syscall table initialized (registered=%u, max=%u).\n",
        (uint64_t)g_syscall_registered_count,
        (uint64_t)SYSCALL_MAX_ENTRIES);
}

int syscall_register(uint16_t number, syscall_handler_t handler)
{
    uint64_t irq_flags;

    if (number >= SYSCALL_MAX_ENTRIES || !handler) {
        return -1;
    }

    irq_flags = syscall_irq_save();
    spinlock_lock(&g_syscall_lock);

    if (!g_syscall_table[number]) {
        g_syscall_registered_count++;
    }
    g_syscall_table[number] = handler;

    spinlock_unlock(&g_syscall_lock);
    syscall_irq_restore(irq_flags);
    return 0;
}

int64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5)
{
    syscall_handler_t handler = NULL;
    uint64_t irq_flags;

    if (number >= SYSCALL_MAX_ENTRIES) {
        return SYSCALL_ENOSYS;
    }

    irq_flags = syscall_irq_save();
    spinlock_lock(&g_syscall_lock);
    handler = g_syscall_table[number];
    spinlock_unlock(&g_syscall_lock);
    syscall_irq_restore(irq_flags);

    if (!handler) {
        return SYSCALL_ENOSYS;
    }

    return handler(arg0, arg1, arg2, arg3, arg4, arg5);
}

int64_t syscall_dispatch_fastpath(const syscall_fastpath_frame_t *frame)
{
    if (!frame) {
        return SYSCALL_ENOSYS;
    }

    return syscall_dispatch(
        frame->number,
        frame->arg0,
        frame->arg1,
        frame->arg2,
        frame->arg3,
        frame->arg4,
        frame->arg5);
}

uint16_t syscall_registered_count(void)
{
    uint16_t count;
    uint64_t irq_flags;

    irq_flags = syscall_irq_save();
    spinlock_lock(&g_syscall_lock);
    count = g_syscall_registered_count;
    spinlock_unlock(&g_syscall_lock);
    syscall_irq_restore(irq_flags);

    return count;
}
