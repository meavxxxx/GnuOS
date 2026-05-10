#include <stddef.h>
#include <stdint.h>

#include <gnuos/interrupts.h>
#include <gnuos/mm.h>
#include <gnuos/multiboot2.h>
#include <gnuos/panic.h>
#include <gnuos/pic.h>
#include <gnuos/pit.h>
#include <gnuos/sched.h>
#include <gnuos/serial.h>
#include <gnuos/vmm.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static uint8_t g_vga_row;
static uint8_t g_vga_col;

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

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
    uint64_t kernel_start = 0;
    uint64_t kernel_end = 0;
    uint64_t kernel_size = 0;
    uint64_t translated = 0;
    uint64_t test_virt = 0;
    uint64_t split_test_virt = 0x0000000000200000ULL;
    uint64_t ticks_before = 0;
    task_t *current_task = NULL;
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
    kernel_start = (uint64_t)(uintptr_t)&__kernel_start;
    kernel_end = (uint64_t)(uintptr_t)&__kernel_end;
    kernel_size = kernel_end - kernel_start;
    pmm_reserve_range(kernel_start, kernel_size);

    if (!vmm_init()) {
        kpanic("failed to initialize vmm");
    }

    sched_init();
    if (!sched_create_idle_task()) {
        kpanic("failed to create idle task");
    }
    sched_tick();

    pic_init(PIC_IRQ_BASE, (uint8_t)(PIC_IRQ_BASE + 8U));
    for (uint8_t irq = 0; irq < 16U; irq++) {
        pic_set_mask(irq);
    }
    pic_clear_mask(0U);
    pit_init(100U);
    x86_64_interrupts_enable();

    vga_write("GNU OS kernel bootstrap\n", color);
    vga_write("Phase 1.5 in progress: scheduler bootstrap online.\n", 0x0F);

    serial_write("GNU OS: serial console initialized.\n");
    serial_write("GNU OS: kernel bootstrap reached kmain().\n");
    serial_write("GNU OS: multiboot info addr: ");
    serial_write_hex64(boot_info_addr);
    serial_write("\n");
    serial_write("GNU OS: kernel image range: ");
    serial_write_hex64(kernel_start);
    serial_write("..");
    serial_write_hex64(kernel_end);
    serial_write("\n");
    serial_write("GNU OS: ready tasks: ");
    serial_write_hex64(sched_ready_count());
    serial_write("\n");
    current_task = sched_current_task();
    serial_write("GNU OS: current task tid: ");
    if (current_task) {
        serial_write_hex64(current_task->tid);
    } else {
        serial_write("(null)");
    }
    serial_write("\n");
    serial_write("GNU OS: IRQ0 timer unmasked and interrupts enabled.\n");

    void *page = pmm_alloc_page();
    serial_write("GNU OS: PMM first allocated page: ");
    if (page) {
        serial_write_hex64((uint64_t)(uintptr_t)page);
    } else {
        serial_write("(null)");
    }
    serial_write("\n");

    void *heap_page = vmm_alloc_kernel_pages(1, VMM_MAP_WRITABLE);
    test_virt = (uint64_t)(uintptr_t)heap_page;

    if (heap_page) {
        volatile uint64_t *probe = (volatile uint64_t *)(uintptr_t)test_virt;
        *probe = 0x474E554F53564D4DULL;

        if (vmm_translate(test_virt, &translated)) {
            serial_write("GNU OS: VMM allocated/mapped ");
            serial_write_hex64(test_virt);
            serial_write(" -> ");
            serial_write_hex64(translated);
            serial_write("\n");
        } else {
            serial_write("GNU OS: VMM translate failed for test mapping.\n");
        }
    } else {
        serial_write("GNU OS: VMM allocation test failed.\n");
    }

    void *split_page = pmm_alloc_page();
    if (split_page) {
        uint64_t split_phys = (uint64_t)(uintptr_t)split_page;
        int split_ok = vmm_unmap_page(split_test_virt) &&
            vmm_map_page(split_test_virt, split_phys, VMM_MAP_WRITABLE);

        if (split_ok) {
            volatile uint64_t *probe = (volatile uint64_t *)(uintptr_t)split_test_virt;
            *probe = 0x53504C4954564D4DULL;

            if (vmm_translate(split_test_virt, &translated)) {
                serial_write("GNU OS: VMM split/remap ");
                serial_write_hex64(split_test_virt);
                serial_write(" -> ");
                serial_write_hex64(translated);
                serial_write("\n");
            }
        } else {
            serial_write("GNU OS: VMM split/remap test failed.\n");
        }

        (void)vmm_unmap_page(split_test_virt);
        (void)vmm_map_page(split_test_virt, split_test_virt, VMM_MAP_WRITABLE);
        pmm_free_page(split_page);
    } else {
        serial_write("GNU OS: no free page for VMM split test.\n");
    }

    ticks_before = pit_ticks();
    while (pit_ticks() == ticks_before) {
        __asm__ volatile("hlt");
    }

    serial_write("GNU OS: timer interrupt path active, ticks=");
    serial_write_hex64(pit_ticks());
    serial_write("\n");

#if 0
    /* Optional bring-up test: should trigger #DE and halt in kpanic. */
    __asm__ volatile("xor %rdx, %rdx; div %rdx");
#endif

    for (;;) {
        __asm__ volatile("hlt");
    }
}
