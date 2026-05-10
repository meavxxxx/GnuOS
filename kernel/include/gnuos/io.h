#ifndef GNUOS_IO_H
#define GNUOS_IO_H

#include <stdint.h>

static inline void io_out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port)
{
    uint8_t value = 0;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_out32(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t io_in32(uint16_t port)
{
    uint32_t value = 0;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void)
{
    io_out8(0x80U, 0U);
}

#endif

