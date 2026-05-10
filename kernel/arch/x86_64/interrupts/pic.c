#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/pic.h>
#include <gnuos/serial.h>

#define PIC1_COMMAND 0x20U
#define PIC1_DATA 0x21U
#define PIC2_COMMAND 0xA0U
#define PIC2_DATA 0xA1U

#define PIC_ICW1_ICW4 0x01U
#define PIC_ICW1_INIT 0x10U
#define PIC_ICW4_8086 0x01U

void pic_init(uint8_t master_offset, uint8_t slave_offset)
{
    uint8_t master_mask = io_in8(PIC1_DATA);
    uint8_t slave_mask = io_in8(PIC2_DATA);

    io_out8(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    io_out8(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    io_out8(PIC1_DATA, master_offset);
    io_wait();
    io_out8(PIC2_DATA, slave_offset);
    io_wait();

    io_out8(PIC1_DATA, 0x04U);
    io_wait();
    io_out8(PIC2_DATA, 0x02U);
    io_wait();

    io_out8(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    io_out8(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    io_out8(PIC1_DATA, master_mask);
    io_out8(PIC2_DATA, slave_mask);

    serial_write("GNU OS: PIC remapped.\n");
}

void pic_set_mask(uint8_t irq_line)
{
    uint16_t port = (irq_line < 8U) ? PIC1_DATA : PIC2_DATA;
    uint8_t line = (irq_line < 8U) ? irq_line : (uint8_t)(irq_line - 8U);
    uint8_t mask = io_in8(port);
    mask = (uint8_t)(mask | (uint8_t)(1U << line));
    io_out8(port, mask);
}

void pic_clear_mask(uint8_t irq_line)
{
    uint16_t port = (irq_line < 8U) ? PIC1_DATA : PIC2_DATA;
    uint8_t line = (irq_line < 8U) ? irq_line : (uint8_t)(irq_line - 8U);
    uint8_t mask = io_in8(port);
    mask = (uint8_t)(mask & (uint8_t)~(1U << line));
    io_out8(port, mask);
}

void pic_send_eoi(uint8_t irq_line)
{
    if (irq_line >= 8U) {
        io_out8(PIC2_COMMAND, 0x20U);
    }

    io_out8(PIC1_COMMAND, 0x20U);
}

