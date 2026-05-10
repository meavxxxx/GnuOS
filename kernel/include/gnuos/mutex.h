#ifndef GNUOS_MUTEX_H
#define GNUOS_MUTEX_H

#include <stdint.h>

typedef struct {
    volatile uint32_t state;
} mutex_t;

void mutex_init(mutex_t *lock);
int mutex_try_lock(mutex_t *lock);
void mutex_lock(mutex_t *lock);
void mutex_unlock(mutex_t *lock);

#endif
