#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <sys/cdefs.h>

typedef struct {
    unsigned int __value;
} sem_t;

__BEGIN_DECLS

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *sem, int *sval);

__END_DECLS

#endif
