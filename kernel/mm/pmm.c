#include <stddef.h>
#include <stdint.h>

#include <gnuos/mm.h>
#include <gnuos/serial.h>

#define PMM_MAX_PAGES 65536ULL
#define PMM_MAX_ORDER 16U
#define PMM_INVALID_INDEX 0xFFFFFFFFU

#define PMM_BLOCK_UNUSED 0U
#define PMM_BLOCK_FREE 1U
#define PMM_BLOCK_ALLOCATED 2U
#define PMM_BLOCK_RESERVED 3U

static uint32_t g_free_heads[PMM_MAX_ORDER + 1U];
static uint32_t g_free_next[PMM_MAX_PAGES];
static uint32_t g_free_prev[PMM_MAX_PAGES];
static uint32_t g_page_owner[PMM_MAX_PAGES];
static uint8_t g_block_state[PMM_MAX_PAGES];
static uint8_t g_block_order[PMM_MAX_PAGES];
static uint64_t g_base;
static uint64_t g_total_pages;
static uint64_t g_free_pages;

static uint32_t block_pages(uint8_t order)
{
    return (uint32_t)(1U << order);
}

static uint8_t max_order_for_pages(uint64_t pages)
{
    uint8_t order = 0U;

    while (order < PMM_MAX_ORDER) {
        if ((1ULL << (order + 1U)) > pages) {
            break;
        }
        order++;
    }

    return order;
}

static void reset_allocator_state(void)
{
    for (uint32_t order = 0; order <= PMM_MAX_ORDER; order++) {
        g_free_heads[order] = PMM_INVALID_INDEX;
    }

    for (uint32_t page = 0; page < PMM_MAX_PAGES; page++) {
        g_free_next[page] = PMM_INVALID_INDEX;
        g_free_prev[page] = PMM_INVALID_INDEX;
        g_page_owner[page] = PMM_INVALID_INDEX;
        g_block_state[page] = PMM_BLOCK_UNUSED;
        g_block_order[page] = 0U;
    }
}

static void mark_block_owner(uint32_t start_page, uint8_t order)
{
    uint32_t count = block_pages(order);
    for (uint32_t i = 0; i < count; i++) {
        g_page_owner[start_page + i] = start_page;
    }
}

static void set_block(uint32_t start_page, uint8_t order, uint8_t state)
{
    mark_block_owner(start_page, order);
    g_block_order[start_page] = order;
    g_block_state[start_page] = state;
}

static void clear_block_head(uint32_t start_page)
{
    g_block_order[start_page] = 0U;
    g_block_state[start_page] = PMM_BLOCK_UNUSED;
}

static void free_list_push(uint8_t order, uint32_t start_page)
{
    uint32_t head = g_free_heads[order];

    g_free_prev[start_page] = PMM_INVALID_INDEX;
    g_free_next[start_page] = head;
    if (head != PMM_INVALID_INDEX) {
        g_free_prev[head] = start_page;
    }

    g_free_heads[order] = start_page;
}

static void free_list_remove(uint8_t order, uint32_t start_page)
{
    uint32_t prev = g_free_prev[start_page];
    uint32_t next = g_free_next[start_page];

    if (prev != PMM_INVALID_INDEX) {
        g_free_next[prev] = next;
    } else {
        g_free_heads[order] = next;
    }

    if (next != PMM_INVALID_INDEX) {
        g_free_prev[next] = prev;
    }

    g_free_prev[start_page] = PMM_INVALID_INDEX;
    g_free_next[start_page] = PMM_INVALID_INDEX;
}

static void add_free_block(uint32_t start_page, uint8_t order)
{
    set_block(start_page, order, PMM_BLOCK_FREE);
    free_list_push(order, start_page);
    g_free_pages += (uint64_t)block_pages(order);
}

static void remove_free_block(uint32_t start_page)
{
    uint8_t order = g_block_order[start_page];

    free_list_remove(order, start_page);
    g_free_pages -= (uint64_t)block_pages(order);
    clear_block_head(start_page);
}

static uint32_t alloc_block(uint8_t target_order)
{
    for (uint8_t order = target_order; order <= PMM_MAX_ORDER; order++) {
        uint32_t block = g_free_heads[order];
        if (block == PMM_INVALID_INDEX) {
            continue;
        }

        remove_free_block(block);
        while (order > target_order) {
            uint32_t half_pages = block_pages((uint8_t)(order - 1U));
            uint32_t buddy = block + half_pages;

            order--;
            add_free_block(buddy, order);
        }

        set_block(block, target_order, PMM_BLOCK_ALLOCATED);
        return block;
    }

    return PMM_INVALID_INDEX;
}

static void free_block(uint32_t start_page, uint8_t order)
{
    uint32_t current = start_page;
    uint8_t current_order = order;

    set_block(current, current_order, PMM_BLOCK_FREE);

    while (current_order < PMM_MAX_ORDER) {
        uint32_t buddy = current ^ block_pages(current_order);
        if (buddy >= g_total_pages) {
            break;
        }

        if (g_block_state[buddy] != PMM_BLOCK_FREE ||
            g_block_order[buddy] != current_order) {
            break;
        }

        remove_free_block(buddy);
        clear_block_head(current);
        clear_block_head(buddy);

        if (buddy < current) {
            current = buddy;
        }

        current_order++;
        set_block(current, current_order, PMM_BLOCK_FREE);
    }

    add_free_block(current, current_order);
}

static int reserve_page(uint32_t page)
{
    if (page >= g_total_pages) {
        return 0;
    }

    uint32_t head = g_page_owner[page];
    if (head == PMM_INVALID_INDEX) {
        return 0;
    }

    if (g_block_state[head] == PMM_BLOCK_RESERVED) {
        return 1;
    }

    if (g_block_state[head] == PMM_BLOCK_ALLOCATED) {
        return 0;
    }

    if (g_block_state[head] != PMM_BLOCK_FREE) {
        return 0;
    }

    uint32_t current = head;
    uint8_t order = g_block_order[head];
    remove_free_block(head);
    while (order > 0U) {
        uint8_t child_order = (uint8_t)(order - 1U);
        uint32_t half = block_pages(child_order);
        uint32_t buddy = current + half;

        if (page < buddy) {
            add_free_block(buddy, child_order);
        } else {
            add_free_block(current, child_order);
            current = buddy;
        }

        order = child_order;
        set_block(current, order, PMM_BLOCK_ALLOCATED);
    }

    set_block(current, 0U, PMM_BLOCK_RESERVED);
    return 1;
}

static void add_free_range(uint32_t start_page, uint32_t page_count)
{
    uint32_t page = start_page;
    uint32_t remaining = page_count;

    while (remaining > 0U) {
        uint8_t order = max_order_for_pages(remaining);
        while (order > 0U && (page & (block_pages(order) - 1U)) != 0U) {
            order--;
        }

        add_free_block(page, order);
        page += block_pages(order);
        remaining -= block_pages(order);
    }
}

void pmm_init(uint64_t base, uint64_t memory_size)
{
    uint64_t end = base + memory_size;

    g_base = (base + (MM_PAGE_SIZE - 1ULL)) & ~(MM_PAGE_SIZE - 1ULL);
    if (end < base || g_base >= end) {
        g_total_pages = 0ULL;
    } else {
        g_total_pages = (end - g_base) / MM_PAGE_SIZE;
    }
    if (g_total_pages > PMM_MAX_PAGES) {
        g_total_pages = PMM_MAX_PAGES;
    }

    g_free_pages = 0ULL;
    reset_allocator_state();

    if (g_total_pages > 0ULL) {
        add_free_range(0U, (uint32_t)g_total_pages);
        (void)reserve_page(0U);
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
    uint32_t block = alloc_block(0U);
    if (block == PMM_INVALID_INDEX) {
        return NULL;
    }

    return (void *)(uintptr_t)(g_base + ((uint64_t)block * MM_PAGE_SIZE));
}

void pmm_free_page(void *address)
{
    uint64_t addr = (uint64_t)(uintptr_t)address;
    if (g_total_pages == 0ULL || addr < g_base) {
        return;
    }

    uint64_t diff = addr - g_base;
    if ((diff % MM_PAGE_SIZE) != 0ULL) {
        return;
    }

    uint64_t page = diff / MM_PAGE_SIZE;
    if (page >= g_total_pages) {
        return;
    }

    uint32_t page_index = (uint32_t)page;
    if (g_block_state[page_index] != PMM_BLOCK_ALLOCATED ||
        g_block_order[page_index] != 0U) {
        return;
    }

    clear_block_head(page_index);
    free_block(page_index, 0U);
}

void pmm_reserve_range(uint64_t base, uint64_t size)
{
    if (size == 0ULL || g_total_pages == 0ULL) {
        return;
    }

    uint64_t end = base + size;
    if (end < base) {
        end = UINT64_MAX;
    }

    uint64_t aligned_start = base & ~(MM_PAGE_SIZE - 1ULL);
    uint64_t aligned_end;
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

    uint32_t first_page = (uint32_t)((aligned_start - g_base) / MM_PAGE_SIZE);
    uint32_t last_page = (uint32_t)((aligned_end - g_base) / MM_PAGE_SIZE);

    for (uint32_t page = first_page; page < last_page; page++) {
        (void)reserve_page(page);
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
