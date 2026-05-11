#ifndef _UNISTD_H
#define _UNISTD_H 1

#include <features.h>
#include <sys/types.h>

__BEGIN_DECLS

void _exit(int status) __THROW __attribute__((noreturn));

__END_DECLS

#endif
