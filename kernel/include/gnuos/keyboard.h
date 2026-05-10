#ifndef GNUOS_KEYBOARD_H
#define GNUOS_KEYBOARD_H

#include <stddef.h>
#include <stdint.h>

void ps2_keyboard_init(void);
void ps2_keyboard_on_irq(void);
uint64_t ps2_keyboard_irq_count(void);
uint8_t ps2_keyboard_last_scancode(void);
int ps2_keyboard_getchar(void);
size_t ps2_keyboard_read(char *buffer, size_t max_len);

#endif
