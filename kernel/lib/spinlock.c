#include <stdint.h>

#include <gnuos/spinlock.h>

void spinlock_init(spinlock_t *lock)
{
    if (!lock) {
        return;
    }

    lock->state = 0;
}

int spinlock_try_lock(spinlock_t *lock)
{
    if (!lock) {
        return 0;
    }

    return __atomic_exchange_n(&lock->state, 1U, __ATOMIC_ACQUIRE) == 0U;
}

void spinlock_lock(spinlock_t *lock)
{
    if (!lock) {
        return;
    }

    while (!spinlock_try_lock(lock)) {
        __asm__ volatile("pause");
    }
}

void spinlock_unlock(spinlock_t *lock)
{
    if (!lock) {
        return;
    }

    __atomic_store_n(&lock->state, 0U, __ATOMIC_RELEASE);
}

