#ifndef GNUOS_SERIAL_H
#define GNUOS_SERIAL_H

#include <stdint.h>

typedef void (*serial_mirror_callback_t)(char c);

void serial_init(void);
void serial_set_mirror_callback(serial_mirror_callback_t callback);
void serial_set_mirror_enabled(uint8_t enabled);
void serial_write_char(char c);
void serial_write(const char *message);
void serial_write_hex64(uint64_t value);

#endif
