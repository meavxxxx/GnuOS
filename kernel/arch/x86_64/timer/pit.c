#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/pit.h>
#include <gnuos/sched.h>
#include <gnuos/serial.h>

#define PIT_FREQUENCY_HZ 1193182U
#define PIT_COMMAND_PORT 0x43U
#define PIT_CHANNEL0_PORT 0x40U

static volatile uint64_t g_pit_ticks;

void pit_init(uint32_t frequency_hz)
{
    if (frequency_hz == 0U) {
        frequency_hz = 100U;
    }

    uint16_t divisor = (uint16_t)(PIT_FREQUENCY_HZ / frequency_hz);
    if (divisor == 0U) {
        divisor = 1U;
    }

    io_out8(PIT_COMMAND_PORT, 0x36U);
    io_out8(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFFU));
    io_out8(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8U) & 0xFFU));

    g_pit_ticks = 0;

    serial_write("GNU OS: PIT initialized, hz=");
    serial_write_hex64(frequency_hz);
    serial_write("\n");
}

void pit_on_irq(void)
{
    g_pit_ticks++;
    sched_tick();
}

uint64_t pit_ticks(void)
{
    return g_pit_ticks;
}

