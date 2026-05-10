#include <stddef.h>
#include <stdint.h>

#include <gnuos/mm.h>
#include <gnuos/serial.h>

#define PMM_MAX_PAGES 65536ULL
#define PMM_BITMAP_BYTES (PMM_MAX_PAGES / 8ULL)

static uint8_t g_page_bitmap[PMM_BITMAP_BYTES];
static uint64_t g_base;
static uint64_t g_total_pages;
static uint64_t g_free_pages;

static void bitmap_set(uint64_t bit)
{
    g_page_bitmap[bit / 8ULL] |= (uint8_t)(1U << (bit % 8ULL));
}

static void bitmap_clear(uint64_t bit)
{
    g_page_bitmap[bit / 8ULL] &= (uint8_t)~(1U << (bit % 8ULL));
}

static int bitmap_test(uint64_t bit)
{
    return (g_page_bitmap[bit / 8ULL] & (uint8_t)(1U << (bit % 8ULL))) != 0;
}

static void bitmap_fill_allocated(void)
{
    for (uint64_t i = 0; i < PMM_BITMAP_BYTES; i++) {
        g_page_bitmap[i] = 0xFFU;
    }
}

void pmm_init(uint64_t base, uint64_t memory_size)
{
    uint64_t end = base + memory_size;

    g_base = (base + (MM_PAGE_SIZE - 1ULL)) & ~(MM_PAGE_SIZE - 1ULL);
    if (end < base || g_base >= end) {
        g_total_pages = 0;
    } else {
        g_total_pages = (end - g_base) / MM_PAGE_SIZE;
    }
    if (g_total_pages > PMM_MAX_PAGES) {
        g_total_pages = PMM_MAX_PAGES;
    }

    g_free_pages = g_total_pages;
    bitmap_fill_allocated();

    for (uint64_t page = 0; page < g_total_pages; page++) {
        bitmap_clear(page);
    }

    /* Keep the first page unavailable to catch null-ish physical references. */
    if (g_total_pages > 0 && !bitmap_test(0)) {
        bitmap_set(0);
        g_free_pages--;
    }

    serial_write("GNU OS: PMM initialized\n");
    serial_write("  base: ");
    serial_write_hex64(g_base);
    serial_write("\n  pages(total/free): ");
    serial_write_hex64(g_total_pages);
    serial_write("/");
    serial_write_hex64(g_free_pages);
    serial_write("\n");
}

void *pmm_alloc_page(void)
{
    if (g_free_pages == 0) {
        return NULL;
    }

    for (uint64_t page = 0; page < g_total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            g_free_pages--;
            return (void *)(uintptr_t)(g_base + page * MM_PAGE_SIZE);
        }
    }

    return NULL;
}

void pmm_free_page(void *address)
{
    uint64_t addr = (uint64_t)(uintptr_t)address;
    if (addr < g_base || g_total_pages == 0) {
        return;
    }

    uint64_t diff = addr - g_base;
    if ((diff % MM_PAGE_SIZE) != 0) {
        return;
    }

    uint64_t page = diff / MM_PAGE_SIZE;
    if (page >= g_total_pages) {
        return;
    }

    if (bitmap_test(page)) {
        bitmap_clear(page);
        g_free_pages++;
    }
}

void pmm_reserve_range(uint64_t base, uint64_t size)
{
    if (size == 0 || g_total_pages == 0) {
        return;
    }

    uint64_t end = base + size;
    if (end < base) {
        end = UINT64_MAX;
    }

    uint64_t aligned_start = base & ~(MM_PAGE_SIZE - 1ULL);
    uint64_t aligned_end = 0;
    if (end > UINT64_MAX - (MM_PAGE_SIZE - 1ULL)) {
        aligned_end = UINT64_MAX & ~(MM_PAGE_SIZE - 1ULL);
    } else {
        aligned_end = (end + (MM_PAGE_SIZE - 1ULL)) & ~(MM_PAGE_SIZE - 1ULL);
    }

    if (aligned_end <= g_base) {
        return;
    }

    if (aligned_start < g_base) {
        aligned_start = g_base;
    }

    uint64_t pmm_end = g_base + g_total_pages * MM_PAGE_SIZE;
    if (aligned_start >= pmm_end) {
        return;
    }

    if (aligned_end > pmm_end) {
        aligned_end = pmm_end;
    }

    uint64_t first_page = (aligned_start - g_base) / MM_PAGE_SIZE;
    uint64_t last_page = (aligned_end - g_base) / MM_PAGE_SIZE;

    for (uint64_t page = first_page; page < last_page; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            g_free_pages--;
        }
    }
}

uint64_t pmm_total_pages(void)
{
    return g_total_pages;
}

uint64_t pmm_free_pages(void)
{
    return g_free_pages;
}
