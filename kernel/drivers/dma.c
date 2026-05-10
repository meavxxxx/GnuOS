#include <stdint.h>

#include <gnuos/dma.h>
#include <gnuos/io.h>
#include <gnuos/printk.h>

#define DMA_PRIMARY_SINGLE_MASK 0x0AU
#define DMA_PRIMARY_MODE 0x0BU
#define DMA_PRIMARY_CLEAR_FF 0x0CU
#define DMA_PRIMARY_MASTER_CLEAR 0x0DU

#define DMA_SECONDARY_SINGLE_MASK 0xD4U
#define DMA_SECONDARY_MODE 0xD6U
#define DMA_SECONDARY_CLEAR_FF 0xD8U
#define DMA_SECONDARY_MASTER_CLEAR 0xDAU

static const uint16_t g_dma_addr_port[8] = {
    0x00U, 0x02U, 0x04U, 0x06U, 0xC0U, 0xC4U, 0xC8U, 0xCCU};
static const uint16_t g_dma_count_port[8] = {
    0x01U, 0x03U, 0x05U, 0x07U, 0xC2U, 0xC6U, 0xCAU, 0xCEU};
static const uint16_t g_dma_page_port[8] = {
    0x87U, 0x83U, 0x81U, 0x82U, 0x8FU, 0x8BU, 0x89U, 0x8AU};

static uint8_t dma_is_valid_channel(uint8_t channel)
{
    return channel < 8U && channel != 4U;
}

static uint8_t dma_is_secondary(uint8_t channel)
{
    return channel >= 4U;
}

static uint8_t dma_local_channel(uint8_t channel)
{
    if (channel < 4U) {
        return channel;
    }

    return (uint8_t)(channel - 4U);
}

static void dma_clear_flip_flop(uint8_t channel)
{
    if (dma_is_secondary(channel)) {
        io_out8(DMA_SECONDARY_CLEAR_FF, 0U);
    } else {
        io_out8(DMA_PRIMARY_CLEAR_FF, 0U);
    }
}

void dma_mask_channel(uint8_t channel)
{
    uint8_t local_channel;
    uint8_t mask_value;

    if (!dma_is_valid_channel(channel)) {
        return;
    }

    local_channel = dma_local_channel(channel);
    mask_value = (uint8_t)(0x04U | local_channel);

    if (dma_is_secondary(channel)) {
        io_out8(DMA_SECONDARY_SINGLE_MASK, mask_value);
    } else {
        io_out8(DMA_PRIMARY_SINGLE_MASK, mask_value);
    }
}

void dma_unmask_channel(uint8_t channel)
{
    uint8_t local_channel;

    if (!dma_is_valid_channel(channel)) {
        return;
    }

    local_channel = dma_local_channel(channel);

    if (dma_is_secondary(channel)) {
        io_out8(DMA_SECONDARY_SINGLE_MASK, local_channel);
    } else {
        io_out8(DMA_PRIMARY_SINGLE_MASK, local_channel);
    }
}

void dma_init(void)
{
    io_out8(DMA_PRIMARY_MASTER_CLEAR, 0U);
    io_out8(DMA_SECONDARY_MASTER_CLEAR, 0U);

    for (uint8_t channel = 0U; channel < 8U; channel++) {
        dma_mask_channel(channel);
    }

    kprintf("GNU OS: ISA DMA engine initialized.\n");
}

int dma_program_channel(uint8_t channel, uint32_t address, uint16_t length, uint8_t mode)
{
    uint8_t local_channel;
    uint8_t page;
    uint16_t addr_word;
    uint16_t count_value;
    uint16_t mode_port;
    uint8_t mode_value;
    uint16_t address_port;
    uint16_t count_port;

    if (!dma_is_valid_channel(channel)) {
        return -1;
    }
    if (length == 0U) {
        return -2;
    }

    if (dma_is_secondary(channel)) {
        if ((address & 1U) != 0U || (length & 1U) != 0U) {
            return -3;
        }

        addr_word = (uint16_t)(address >> 1U);
        count_value = (uint16_t)((length >> 1U) - 1U);
    } else {
        addr_word = (uint16_t)address;
        count_value = (uint16_t)(length - 1U);
    }

    local_channel = dma_local_channel(channel);
    mode_port = dma_is_secondary(channel) ? DMA_SECONDARY_MODE : DMA_PRIMARY_MODE;
    mode_value = (uint8_t)((mode & 0xFCU) | local_channel);
    page = (uint8_t)(address >> 16U);
    address_port = g_dma_addr_port[channel];
    count_port = g_dma_count_port[channel];

    dma_mask_channel(channel);
    dma_clear_flip_flop(channel);
    io_out8(mode_port, mode_value);

    io_out8(address_port, (uint8_t)(addr_word & 0xFFU));
    io_out8(address_port, (uint8_t)((addr_word >> 8U) & 0xFFU));
    io_out8(g_dma_page_port[channel], page);

    dma_clear_flip_flop(channel);
    io_out8(count_port, (uint8_t)(count_value & 0xFFU));
    io_out8(count_port, (uint8_t)((count_value >> 8U) & 0xFFU));
    dma_unmask_channel(channel);

    return 0;
}
