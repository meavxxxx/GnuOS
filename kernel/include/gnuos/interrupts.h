#ifndef GNUOS_INTERRUPTS_H
#define GNUOS_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void x86_64_idt_init(void);

#endif

