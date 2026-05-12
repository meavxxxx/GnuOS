#ifndef GNUOS_APIC_H
#define GNUOS_APIC_H

#include <stdint.h>

typedef enum {
    APIC_MODE_DISABLED = 0,
    APIC_MODE_XAPIC = 1,
    APIC_MODE_X2APIC = 2,
} apic_mode_t;

int apic_init(void);
apic_mode_t apic_mode(void);
uint32_t apic_local_id(void);
void apic_send_eoi(void);

#endif

