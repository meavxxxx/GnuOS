#ifndef GNUOS_RWLOCK_H
#define GNUOS_RWLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t readers;
    volatile uint32_t writer;
} rwlock_t;

void rwlock_init(rwlock_t *lock);
void rwlock_read_lock(rwlock_t *lock);
void rwlock_read_unlock(rwlock_t *lock);
void rwlock_write_lock(rwlock_t *lock);
void rwlock_write_unlock(rwlock_t *lock);

#endif
