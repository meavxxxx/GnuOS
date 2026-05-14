#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/serial.h>

#define COM1_PORT 0x3F8

static serial_mirror_callback_t g_serial_mirror_callback;
static uint8_t g_serial_mirror_enabled;

void serial_init(void)
{
    io_out8(COM1_PORT + 1, 0x00);
    io_out8(COM1_PORT + 3, 0x80);
    io_out8(COM1_PORT + 0, 0x03);
    io_out8(COM1_PORT + 1, 0x00);
    io_out8(COM1_PORT + 3, 0x03);
    io_out8(COM1_PORT + 2, 0xC7);
    io_out8(COM1_PORT + 4, 0x0B);
}

void serial_set_mirror_callback(serial_mirror_callback_t callback)
{
    g_serial_mirror_callback = callback;
}

void serial_set_mirror_enabled(uint8_t enabled)
{
    g_serial_mirror_enabled = enabled ? 1U : 0U;
}

static int serial_tx_ready(void)
{
    return (io_in8(COM1_PORT + 5) & 0x20) != 0;
}

void serial_write_char(char c)
{
    while (!serial_tx_ready()) {
    }

    io_out8(COM1_PORT, (uint8_t)c);
    if (g_serial_mirror_enabled && g_serial_mirror_callback) {
        g_serial_mirror_callback(c);
    }
}

void serial_write(const char *message)
{
    if (!message) {
        return;
    }

    while (*message != '\0') {
        if (*message == '\n') {
            serial_write_char('\r');
        }

        serial_write_char(*message);
        message++;
    }
}

void serial_write_hex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    serial_write("0x");
    for (int nibble = 15; nibble >= 0; nibble--) {
        uint8_t index = (uint8_t)((value >> (nibble * 4)) & 0xFU);
        serial_write_char(digits[index]);
    }
}
