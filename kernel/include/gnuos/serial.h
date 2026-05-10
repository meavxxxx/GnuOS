#ifndef GNUOS_SERIAL_H
#define GNUOS_SERIAL_H

void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *message);

#endif

