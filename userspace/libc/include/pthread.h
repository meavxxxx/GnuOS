#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <sys/cdefs.h>

typedef unsigned long pthread_t;

typedef struct {
    unsigned long __opaque;
} pthread_attr_t;

__BEGIN_DECLS

int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg);
pthread_t pthread_self(void);
int pthread_equal(pthread_t thread1, pthread_t thread2);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);

__END_DECLS

#endif
