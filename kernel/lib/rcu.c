#include <stdint.h>

#include <gnuos/rcu.h>
#include <gnuos/sched.h>
#include <gnuos/spinlock.h>

static volatile uint64_t g_rcu_readers;
static spinlock_t g_rcu_lock;
static rcu_head_t *g_callbacks_head;
static rcu_head_t *g_callbacks_tail;
static uint64_t g_callbacks_queued_count;
static uint64_t g_callbacks_completed_count;

void rcu_init(void)
{
    g_rcu_readers = 0;
    spinlock_init(&g_rcu_lock);
    g_callbacks_head = NULL;
    g_callbacks_tail = NULL;
    g_callbacks_queued_count = 0;
    g_callbacks_completed_count = 0;
}

void rcu_read_lock(void)
{
    __atomic_add_fetch(&g_rcu_readers, 1U, __ATOMIC_ACQ_REL);
}

void rcu_read_unlock(void)
{
    uint64_t readers = __atomic_load_n(&g_rcu_readers, __ATOMIC_ACQUIRE);
    if (readers == 0U) {
        return;
    }

    (void)__atomic_sub_fetch(&g_rcu_readers, 1U, __ATOMIC_ACQ_REL);
}

void synchronize_rcu(void)
{
    while (__atomic_load_n(&g_rcu_readers, __ATOMIC_ACQUIRE) != 0U) {
        sched_yield();
    }
}

void call_rcu(rcu_head_t *head, rcu_callback_t func)
{
    if (!head || !func) {
        return;
    }

    head->func = func;
    head->next = NULL;

    spinlock_lock(&g_rcu_lock);
    if (!g_callbacks_head) {
        g_callbacks_head = head;
        g_callbacks_tail = head;
    } else {
        g_callbacks_tail->next = head;
        g_callbacks_tail = head;
    }
    g_callbacks_queued_count++;
    spinlock_unlock(&g_rcu_lock);
}

int rcu_process_callbacks(int block_if_empty)
{
    for (;;) {
        rcu_head_t *callback = NULL;

        spinlock_lock(&g_rcu_lock);
        if (g_callbacks_head) {
            callback = g_callbacks_head;
            g_callbacks_head = callback->next;
            if (!g_callbacks_head) {
                g_callbacks_tail = NULL;
            }
            callback->next = NULL;
        }
        spinlock_unlock(&g_rcu_lock);

        if (!callback) {
            if (!block_if_empty) {
                return 0;
            }

            sched_yield();
            continue;
        }

        synchronize_rcu();
        callback->func(callback);

        spinlock_lock(&g_rcu_lock);
        g_callbacks_completed_count++;
        spinlock_unlock(&g_rcu_lock);
        return 1;
    }
}

uint64_t rcu_reader_count(void)
{
    return __atomic_load_n(&g_rcu_readers, __ATOMIC_ACQUIRE);
}

uint64_t rcu_callbacks_queued(void)
{
    uint64_t queued = 0;

    spinlock_lock(&g_rcu_lock);
    queued = g_callbacks_queued_count;
    spinlock_unlock(&g_rcu_lock);

    return queued;
}

uint64_t rcu_callbacks_completed(void)
{
    uint64_t completed = 0;

    spinlock_lock(&g_rcu_lock);
    completed = g_callbacks_completed_count;
    spinlock_unlock(&g_rcu_lock);

    return completed;
}
