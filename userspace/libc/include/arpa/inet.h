#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#include <netinet/in.h>

__BEGIN_DECLS

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);

__END_DECLS

#endif
