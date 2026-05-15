#ifndef GNUOS_X86_64_HARDENING_H
#define GNUOS_X86_64_HARDENING_H

#include <stdint.h>

typedef struct {
    uint8_t nx_supported;
    uint8_t nx_enabled;
    uint8_t smep_supported;
    uint8_t smep_enabled;
    uint8_t smap_supported;
    uint8_t smap_enabled;
} x86_64_hardening_state_t;

void x86_64_hardening_init(void);
void x86_64_hardening_get_state(x86_64_hardening_state_t *out_state);
uint8_t x86_64_smap_enabled(void);
void x86_64_uaccess_begin(void);
void x86_64_uaccess_end(void);

#endif

