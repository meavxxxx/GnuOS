#ifndef GNUOS_TLS_H
#define GNUOS_TLS_H

#include <sys/cdefs.h>

typedef struct {
    unsigned long ti_module;
    unsigned long ti_offset;
} gnuos_tls_index_t;

__BEGIN_DECLS

int __gnuos_set_tls_base(void *base);
void *__gnuos_get_tls_base(void);
void *__tls_get_addr(gnuos_tls_index_t *index);

__END_DECLS

#endif
