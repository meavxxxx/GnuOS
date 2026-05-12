#ifndef GNUOS_APIC_H
#define GNUOS_APIC_H

#include <stdint.h>

typedef enum {
    APIC_MODE_DISABLED = 0,
    APIC_MODE_XAPIC = 1,
    APIC_MODE_X2APIC = 2,
} apic_mode_t;

#define APIC_TIMER_VECTOR 0xF0U

int apic_init(void);
apic_mode_t apic_mode(void);
uint32_t apic_local_id(void);
void apic_send_eoi(void);
int apic_timer_init(uint8_t vector, uint32_t target_hz);
void apic_timer_on_irq(void);
uint64_t apic_timer_ticks(void);
uint32_t apic_timer_current_count(void);

#endif
