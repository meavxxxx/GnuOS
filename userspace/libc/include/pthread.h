#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <sys/cdefs.h>
#include <sys/types.h>

typedef unsigned long pthread_t;

typedef struct {
    unsigned long __opaque;
    size_t __stack_size;
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
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);

__END_DECLS

#endif
