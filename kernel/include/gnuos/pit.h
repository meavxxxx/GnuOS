#ifndef GNUOS_PIT_H
#define GNUOS_PIT_H

#include <stdint.h>

void pit_init(uint32_t frequency_hz);
void pit_on_irq(void);
uint64_t pit_ticks(void);

#endif

