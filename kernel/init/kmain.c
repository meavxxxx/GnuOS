#include <stdint.h>

#include <gnuos/serial.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static uint8_t g_vga_row;
static uint8_t g_vga_col;

static uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8U);
}

static void vga_clear(uint8_t color)
{
    for (uint16_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint16_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }

    g_vga_row = 0;
    g_vga_col = 0;
}

static void vga_putc(char c, uint8_t color)
{
    if (c == '\n') {
        g_vga_col = 0;
        g_vga_row++;
        return;
    }

    if (g_vga_row >= VGA_HEIGHT) {
        g_vga_row = VGA_HEIGHT - 1;
    }

    VGA_MEMORY[g_vga_row * VGA_WIDTH + g_vga_col] = vga_entry(c, color);
    g_vga_col++;

    if (g_vga_col >= VGA_WIDTH) {
        g_vga_col = 0;
        g_vga_row++;
    }
}

static void vga_write(const char *message, uint8_t color)
{
    while (*message != '\0') {
        vga_putc(*message, color);
        message++;
    }
}

void kmain(void)
{
    const uint8_t color = 0x0A;

    vga_clear(0x07);
    serial_init();

    vga_write("GNU OS kernel bootstrap\n", color);
    vga_write("Phase 0 complete: build skeleton is alive.\n", 0x0F);

    serial_write("GNU OS: serial console initialized.\n");
    serial_write("GNU OS: kernel bootstrap reached kmain().\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}

