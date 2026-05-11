#ifndef _EXECINFO_H
#define _EXECINFO_H

#include <sys/cdefs.h>

__BEGIN_DECLS

int backtrace(void **buffer, int size);

__END_DECLS

#endif
