#include <stdint.h>

#include <gnuos/rwlock.h>

void rwlock_init(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    lock->readers = 0U;
    lock->writer = 0U;
}

void rwlock_read_lock(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    for (;;) {
        while (__atomic_load_n(&lock->writer, __ATOMIC_ACQUIRE) != 0U) {
            __asm__ volatile("pause");
        }

        __atomic_add_fetch(&lock->readers, 1U, __ATOMIC_ACQ_REL);
        if (__atomic_load_n(&lock->writer, __ATOMIC_ACQUIRE) == 0U) {
            return;
        }

        __atomic_sub_fetch(&lock->readers, 1U, __ATOMIC_ACQ_REL);
    }
}

void rwlock_read_unlock(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    __atomic_sub_fetch(&lock->readers, 1U, __ATOMIC_ACQ_REL);
}

void rwlock_write_lock(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    while (__atomic_exchange_n(&lock->writer, 1U, __ATOMIC_ACQUIRE) != 0U) {
        __asm__ volatile("pause");
    }

    while (__atomic_load_n(&lock->readers, __ATOMIC_ACQUIRE) != 0U) {
        __asm__ volatile("pause");
    }
}

void rwlock_write_unlock(rwlock_t *lock)
{
    if (!lock) {
        return;
    }

    __atomic_store_n(&lock->writer, 0U, __ATOMIC_RELEASE);
}
