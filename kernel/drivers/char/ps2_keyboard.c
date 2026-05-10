#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/keyboard.h>
#include <gnuos/printk.h>
#include <gnuos/serial.h>

#define PS2_DATA_PORT 0x60U
#define PS2_STATUS_PORT 0x64U
#define PS2_STATUS_OUTPUT_FULL 0x01U
#define PS2_STATUS_AUX_DATA 0x20U

static volatile uint64_t g_ps2_irq_count;
static volatile uint8_t g_ps2_last_scancode;
static volatile uint8_t g_ps2_extended;

static int ps2_has_data(void)
{
    return (io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0U;
}

void ps2_keyboard_init(void)
{
    g_ps2_irq_count = 0U;
    g_ps2_last_scancode = 0U;
    g_ps2_extended = 0U;

    /* Drain stale controller output before enabling keyboard IRQ line. */
    for (uint16_t i = 0; i < 32U && ps2_has_data(); i++) {
        (void)io_in8(PS2_DATA_PORT);
    }

    serial_write("GNU OS: PS/2 keyboard initialized.\n");
}

void ps2_keyboard_on_irq(void)
{
    uint8_t status = io_in8(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0U) {
        return;
    }

    /* Ignore auxiliary device bytes (e.g., PS/2 mouse) for now. */
    if ((status & PS2_STATUS_AUX_DATA) != 0U) {
        (void)io_in8(PS2_DATA_PORT);
        return;
    }

    uint8_t scancode = io_in8(PS2_DATA_PORT);
    g_ps2_irq_count++;
    g_ps2_last_scancode = scancode;

    if (scancode == 0xE0U || scancode == 0xE1U) {
        g_ps2_extended = 1U;
        return;
    }

    uint8_t released = (uint8_t)((scancode & 0x80U) != 0U);
    uint8_t keycode = (uint8_t)(scancode & 0x7FU);

    if (g_ps2_extended != 0U) {
        kprintf(
            "GNU OS: PS/2 key %s ext scancode=0x%X (irq=%u)\n",
            released ? "release" : "press",
            keycode,
            g_ps2_irq_count);
        g_ps2_extended = 0U;
    } else {
        kprintf(
            "GNU OS: PS/2 key %s scancode=0x%X (irq=%u)\n",
            released ? "release" : "press",
            keycode,
            g_ps2_irq_count);
    }
}

uint64_t ps2_keyboard_irq_count(void)
{
    return g_ps2_irq_count;
}

uint8_t ps2_keyboard_last_scancode(void)
{
    return g_ps2_last_scancode;
}
