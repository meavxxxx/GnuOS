#include <gnuos/meminfo.h>
#include <gnuos/mm.h>
#include <gnuos/printk.h>

void meminfo_snapshot(meminfo_snapshot_t *out_snapshot)
{
    uint64_t total_pages = 0U;
    uint64_t free_pages = 0U;
    uint64_t used_pages = 0U;
    uint64_t page_kb = MM_PAGE_SIZE / 1024ULL;

    if (!out_snapshot) {
        return;
    }

    total_pages = pmm_total_pages();
    free_pages = pmm_free_pages();
    if (free_pages > total_pages) {
        free_pages = total_pages;
    }
    used_pages = total_pages - free_pages;

    out_snapshot->mem_total_kb = total_pages * page_kb;
    out_snapshot->mem_free_kb = free_pages * page_kb;
    out_snapshot->mem_available_kb = out_snapshot->mem_free_kb;
    out_snapshot->mem_used_kb = used_pages * page_kb;
}

void meminfo_log_snapshot(void)
{
    meminfo_snapshot_t snapshot = {0};

    meminfo_snapshot(&snapshot);
    kprintf("MemTotal:       %u kB\n", snapshot.mem_total_kb);
    kprintf("MemFree:        %u kB\n", snapshot.mem_free_kb);
    kprintf("MemAvailable:   %u kB\n", snapshot.mem_available_kb);
    kprintf("MemUsed:        %u kB\n", snapshot.mem_used_kb);
    kprintf("SwapTotal:      0 kB\n");
    kprintf("SwapFree:       0 kB\n");
}

