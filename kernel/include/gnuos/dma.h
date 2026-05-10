#ifndef GNUOS_DMA_H
#define GNUOS_DMA_H

#include <stdint.h>

#define DMA_MODE_SINGLE_WRITE 0x44U
#define DMA_MODE_SINGLE_READ 0x48U

void dma_init(void);
int dma_program_channel(uint8_t channel, uint32_t address, uint16_t length, uint8_t mode);
void dma_mask_channel(uint8_t channel);
void dma_unmask_channel(uint8_t channel);

#endif
