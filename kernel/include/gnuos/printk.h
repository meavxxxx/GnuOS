#ifndef GNUOS_PRINTK_H
#define GNUOS_PRINTK_H

#include <stdarg.h>

int kvprintf(const char *fmt, va_list args);
int kprintf(const char *fmt, ...);

#endif
