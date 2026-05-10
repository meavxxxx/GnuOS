#ifndef GNUOS_PIC_H
#define GNUOS_PIC_H

#include <stdint.h>

#define PIC_IRQ_BASE 32U

void pic_init(uint8_t master_offset, uint8_t slave_offset);
void pic_set_mask(uint8_t irq_line);
void pic_clear_mask(uint8_t irq_line);
void pic_send_eoi(uint8_t irq_line);

#endif

