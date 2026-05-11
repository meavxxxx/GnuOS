#ifndef GNUOS_MSR_H
#define GNUOS_MSR_H

#include <stdint.h>

static inline uint64_t msr_read(uint32_t msr)
{
    uint32_t lo = 0U;
    uint32_t hi = 0U;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32U) | lo;
}

static inline void msr_write(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFFULL);
    uint32_t hi = (uint32_t)(value >> 32U);

    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#endif
