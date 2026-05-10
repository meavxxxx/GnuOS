#ifndef GNUOS_KEYBOARD_H
#define GNUOS_KEYBOARD_H

#include <stdint.h>

void ps2_keyboard_init(void);
void ps2_keyboard_on_irq(void);
uint64_t ps2_keyboard_irq_count(void);
uint8_t ps2_keyboard_last_scancode(void);

#endif
