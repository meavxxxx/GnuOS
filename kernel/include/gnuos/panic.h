#ifndef GNUOS_PANIC_H
#define GNUOS_PANIC_H

#include <stdint.h>

#include <gnuos/interrupts.h>

__attribute__((noreturn)) void kpanic(const char *message);
__attribute__((noreturn)) void kpanic_exception(
    uint8_t vector,
    uint64_t error_code,
    const struct interrupt_frame *frame);

#endif

