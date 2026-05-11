#ifndef _NETINET_IN_H
#define _NETINET_IN_H

#include <sys/cdefs.h>
#include <sys/socket.h>

typedef unsigned short in_port_t;
typedef unsigned int in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int sin6_scope_id;
};

#define INADDR_ANY 0x00000000U
#define INADDR_LOOPBACK 0x7F000001U
#define INADDR_BROADCAST 0xFFFFFFFFU

__BEGIN_DECLS

in_port_t htons(in_port_t hostshort);
in_port_t ntohs(in_port_t netshort);
in_addr_t htonl(in_addr_t hostlong);
in_addr_t ntohl(in_addr_t netlong);

__END_DECLS

#endif
