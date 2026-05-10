#include <stdint.h>

#include <gnuos/mutex.h>

void mutex_init(mutex_t *lock)
{
    if (!lock) {
        return;
    }

    lock->state = 0U;
}

int mutex_try_lock(mutex_t *lock)
{
    if (!lock) {
        return 0;
    }

    return __atomic_exchange_n(&lock->state, 1U, __ATOMIC_ACQUIRE) == 0U;
}

void mutex_lock(mutex_t *lock)
{
    if (!lock) {
        return;
    }

    while (!mutex_try_lock(lock)) {
        __asm__ volatile("pause");
    }
}

void mutex_unlock(mutex_t *lock)
{
    if (!lock) {
        return;
    }

    __atomic_store_n(&lock->state, 0U, __ATOMIC_RELEASE);
}
