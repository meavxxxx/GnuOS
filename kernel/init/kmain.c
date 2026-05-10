#include <stdint.h>

#include <gnuos/interrupts.h>
#include <gnuos/mm.h>
#include <gnuos/multiboot2.h>
#include <gnuos/panic.h>
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

void kmain(uint64_t boot_magic, uint64_t boot_info_addr)
{
    const uint8_t color = 0x0A;
    uint64_t pmm_base = 0;
    uint64_t pmm_size = 0;
    int have_mmap = 0;

    vga_clear(0x07);
    serial_init();
    x86_64_idt_init();

    if ((uint32_t)boot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kpanic("invalid multiboot2 magic");
    }

    have_mmap = multiboot2_find_largest_available_region(
        boot_info_addr,
        &pmm_base,
        &pmm_size);
    if (!have_mmap) {
        serial_write("GNU OS: no valid multiboot memory map found, using fallback.\n");
        pmm_base = 0x100000ULL;
        pmm_size = 64ULL * 1024ULL * 1024ULL;
    }

    pmm_init(pmm_base, pmm_size);

    vga_write("GNU OS kernel bootstrap\n", color);
    vga_write("Phase 1.3 in progress: PMM from multiboot map.\n", 0x0F);

    serial_write("GNU OS: serial console initialized.\n");
    serial_write("GNU OS: kernel bootstrap reached kmain().\n");
    serial_write("GNU OS: multiboot info addr: ");
    serial_write_hex64(boot_info_addr);
    serial_write("\n");

    void *page = pmm_alloc_page();
    serial_write("GNU OS: PMM first allocated page: ");
    if (page) {
        serial_write_hex64((uint64_t)(uintptr_t)page);
    } else {
        serial_write("(null)");
    }
    serial_write("\n");

#if 0
    /* Optional bring-up test: should trigger #DE and halt in kpanic. */
    __asm__ volatile("xor %rdx, %rdx; div %rdx");
#endif

    for (;;) {
        __asm__ volatile("hlt");
    }
}
