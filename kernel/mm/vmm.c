#include <stddef.h>
#include <stdint.h>

#include <gnuos/mm.h>
#include <gnuos/serial.h>
#include <gnuos/vmm.h>

#define VMM_ENTRIES_PER_TABLE 512U
#define VMM_INDEX_MASK 0x1FFULL
#define VMM_PAGE_PRESENT 0x001ULL
#define VMM_PAGE_WRITABLE 0x002ULL
#define VMM_PAGE_USER 0x004ULL
#define VMM_PAGE_SIZE 0x080ULL
#define VMM_PHYS_MASK 0x000FFFFFFFFFF000ULL
#define VMM_PAGE_OFFSET_MASK 0xFFFULL
#define VMM_HUGE2M_SIZE (2ULL * 1024ULL * 1024ULL)
#define VMM_HUGE2M_OFFSET_MASK (VMM_HUGE2M_SIZE - 1ULL)
#define VMM_HUGE2M_BASE_MASK (VMM_PHYS_MASK & ~VMM_HUGE2M_OFFSET_MASK)
#define VMM_KASLR_WINDOW_BASE 0x0000000040000000ULL
#define VMM_KASLR_WINDOW_SIZE 0x0000000020000000ULL
#define VMM_KASLR_SLIDE_GRANULARITY VMM_HUGE2M_SIZE
#define VMM_KERNEL_IMAGE_MAX_SIZE 0x0000000004000000ULL
#define VMM_KERNEL_HEAP_GUARD_SIZE VMM_HUGE2M_SIZE
#define VMM_KERNEL_HEAP_SIZE 0x0000000010000000ULL

static uint64_t *g_pml4;
static vmm_kernel_layout_t g_kernel_layout;
static uint64_t g_kernel_heap_next;
static uint64_t g_kernel_heap_limit;

static uint16_t vmm_pml4_index(uint64_t virt_addr)
{
    return (uint16_t)((virt_addr >> 39U) & VMM_INDEX_MASK);
}

static uint16_t vmm_pdpt_index(uint64_t virt_addr)
{
    return (uint16_t)((virt_addr >> 30U) & VMM_INDEX_MASK);
}

static uint16_t vmm_pd_index(uint64_t virt_addr)
{
    return (uint16_t)((virt_addr >> 21U) & VMM_INDEX_MASK);
}

static uint16_t vmm_pt_index(uint64_t virt_addr)
{
    return (uint16_t)((virt_addr >> 12U) & VMM_INDEX_MASK);
}

static void vmm_invalidate_tlb(uint64_t virt_addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
}

static uint64_t *vmm_entry_to_table(uint64_t entry)
{
    return (uint64_t *)(uintptr_t)(entry & VMM_PHYS_MASK);
}

static int vmm_range_end(uint64_t base, uint64_t size, uint64_t *out_end)
{
    if (!out_end) {
        return 0;
    }
    if (base > UINT64_MAX - size) {
        return 0;
    }

    *out_end = base + size;
    return 1;
}

static void vmm_log_kernel_layout(void)
{
    serial_write("GNU OS: VMM layout kaslr_window_base=");
    serial_write_hex64(g_kernel_layout.kaslr_window_base);
    serial_write(" kaslr_window_size=");
    serial_write_hex64(g_kernel_layout.kaslr_window_size);
    serial_write(" kaslr_slide=");
    serial_write_hex64(g_kernel_layout.kaslr_slide);
    serial_write(" slide_step=");
    serial_write_hex64(VMM_KASLR_SLIDE_GRANULARITY);
    serial_write("\n");
    serial_write("GNU OS: VMM layout kernel_image_base=");
    serial_write_hex64(g_kernel_layout.kernel_image_base);
    serial_write(" kernel_image_size=");
    serial_write_hex64(g_kernel_layout.kernel_image_size);
    serial_write(" kernel_heap_base=");
    serial_write_hex64(g_kernel_layout.kernel_heap_base);
    serial_write(" kernel_heap_size=");
    serial_write_hex64(g_kernel_layout.kernel_heap_size);
    serial_write("\n");
}

static uint64_t vmm_entry_to_map_flags(uint64_t entry)
{
    uint64_t flags = 0U;

    if ((entry & VMM_PAGE_WRITABLE) != 0U) {
        flags |= VMM_MAP_WRITABLE;
    }
    if ((entry & VMM_PAGE_USER) != 0U) {
        flags |= VMM_MAP_USER;
    }
    if ((entry & VMM_MAP_NX) != 0U) {
        flags |= VMM_MAP_NX;
    }

    return flags;
}

static void vmm_zero_page(uint64_t *table)
{
    for (uint16_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
        table[i] = 0;
    }
}

static int vmm_split_large_pd_entry(uint64_t *pd, uint16_t index)
{
    uint64_t pde = pd[index];
    if ((pde & VMM_PAGE_PRESENT) == 0 || (pde & VMM_PAGE_SIZE) == 0) {
        return 0;
    }

    uint64_t *pt = (uint64_t *)pmm_alloc_page();
    if (!pt) {
        return 0;
    }

    vmm_zero_page(pt);

    uint64_t huge_base = pde & VMM_HUGE2M_BASE_MASK;
    uint64_t child_flags = VMM_PAGE_PRESENT;
    if ((pde & VMM_PAGE_WRITABLE) != 0) {
        child_flags |= VMM_PAGE_WRITABLE;
    }
    if ((pde & VMM_PAGE_USER) != 0) {
        child_flags |= VMM_PAGE_USER;
    }
    if ((pde & VMM_MAP_NX) != 0) {
        child_flags |= VMM_MAP_NX;
    }

    for (uint16_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
        uint64_t phys = huge_base + ((uint64_t)i * MM_PAGE_SIZE);
        pt[i] = (phys & VMM_PHYS_MASK) | child_flags;
    }

    uint64_t pd_flags = VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
    if ((pde & VMM_PAGE_USER) != 0) {
        pd_flags |= VMM_PAGE_USER;
    }
    pd[index] = ((uint64_t)(uintptr_t)pt & VMM_PHYS_MASK) | pd_flags;
    return 1;
}

static int vmm_get_or_create_next_table(
    uint64_t *table,
    uint16_t index,
    uint64_t **out_next)
{
    uint64_t entry = table[index];

    if ((entry & VMM_PAGE_PRESENT) != 0) {
        if ((entry & VMM_PAGE_SIZE) != 0) {
            return 0;
        }

        *out_next = vmm_entry_to_table(entry);
        return 1;
    }

    uint64_t *next = (uint64_t *)pmm_alloc_page();
    if (!next) {
        return 0;
    }

    vmm_zero_page(next);
    table[index] =
        ((uint64_t)(uintptr_t)next & VMM_PHYS_MASK) | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
    *out_next = next;
    return 1;
}

static int vmm_walk_to_pt(
    uint64_t virt_addr,
    int create,
    uint64_t **out_pt,
    uint16_t *out_pt_index)
{
    if (!g_pml4) {
        return 0;
    }

    uint64_t *pdpt = NULL;
    uint64_t *pd = NULL;
    uint64_t *pt = NULL;

    uint16_t l4 = vmm_pml4_index(virt_addr);
    uint16_t l3 = vmm_pdpt_index(virt_addr);
    uint16_t l2 = vmm_pd_index(virt_addr);
    uint16_t l1 = vmm_pt_index(virt_addr);

    if (create) {
        if (!vmm_get_or_create_next_table(g_pml4, l4, &pdpt)) {
            return 0;
        }
    } else {
        if ((g_pml4[l4] & VMM_PAGE_PRESENT) == 0 || (g_pml4[l4] & VMM_PAGE_SIZE) != 0) {
            return 0;
        }
        pdpt = vmm_entry_to_table(g_pml4[l4]);
    }

    if (create) {
        if (!vmm_get_or_create_next_table(pdpt, l3, &pd)) {
            return 0;
        }
    } else {
        if ((pdpt[l3] & VMM_PAGE_PRESENT) == 0 || (pdpt[l3] & VMM_PAGE_SIZE) != 0) {
            return 0;
        }
        pd = vmm_entry_to_table(pdpt[l3]);
    }

    if (create) {
        if (!vmm_get_or_create_next_table(pd, l2, &pt)) {
            if ((pd[l2] & VMM_PAGE_PRESENT) != 0 &&
                (pd[l2] & VMM_PAGE_SIZE) != 0 &&
                vmm_split_large_pd_entry(pd, l2)) {
                if (!vmm_get_or_create_next_table(pd, l2, &pt)) {
                    return 0;
                }
            } else {
                return 0;
            }
        }
    } else {
        if ((pd[l2] & VMM_PAGE_PRESENT) == 0) {
            return 0;
        }

        if ((pd[l2] & VMM_PAGE_SIZE) != 0) {
            if (!vmm_split_large_pd_entry(pd, l2)) {
                return 0;
            }
        }

        pt = vmm_entry_to_table(pd[l2]);
    }

    *out_pt = pt;
    *out_pt_index = l1;
    return 1;
}

int vmm_init(void)
{
    uint64_t heap_base = 0U;
    uint64_t cr3 = 0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    g_pml4 = (uint64_t *)(uintptr_t)(cr3 & VMM_PHYS_MASK);

    if (!g_pml4) {
        return 0;
    }

    g_kernel_layout.kaslr_window_base = VMM_KASLR_WINDOW_BASE;
    g_kernel_layout.kaslr_window_size = VMM_KASLR_WINDOW_SIZE;
    g_kernel_layout.kaslr_slide = 0U;
    g_kernel_layout.kernel_image_base =
        g_kernel_layout.kaslr_window_base + g_kernel_layout.kaslr_slide;
    g_kernel_layout.kernel_image_size = VMM_KERNEL_IMAGE_MAX_SIZE;
    if (!vmm_range_end(
            g_kernel_layout.kernel_image_base,
            g_kernel_layout.kernel_image_size,
            &heap_base)) {
        return 0;
    }
    if (!vmm_range_end(heap_base, VMM_KERNEL_HEAP_GUARD_SIZE, &heap_base)) {
        return 0;
    }
    g_kernel_layout.kernel_heap_base = heap_base;
    g_kernel_layout.kernel_heap_size = VMM_KERNEL_HEAP_SIZE;
    if (!vmm_range_end(
            g_kernel_layout.kernel_heap_base,
            g_kernel_layout.kernel_heap_size,
            &g_kernel_heap_limit)) {
        return 0;
    }
    g_kernel_heap_next = g_kernel_layout.kernel_heap_base;

    serial_write("GNU OS: VMM initialized, CR3=");
    serial_write_hex64(cr3 & VMM_PHYS_MASK);
    serial_write("\n");
    vmm_log_kernel_layout();
    return 1;
}

int vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    if ((virt_addr & VMM_PAGE_OFFSET_MASK) != 0 || (phys_addr & VMM_PAGE_OFFSET_MASK) != 0) {
        return 0;
    }

    uint64_t *pt = NULL;
    uint16_t pt_index = 0;

    if (!vmm_walk_to_pt(virt_addr, 1, &pt, &pt_index)) {
        return 0;
    }

    if ((pt[pt_index] & VMM_PAGE_PRESENT) != 0) {
        return 0;
    }

    uint64_t entry_flags = VMM_PAGE_PRESENT;
    if ((flags & VMM_MAP_WRITABLE) != 0) {
        entry_flags |= VMM_PAGE_WRITABLE;
    }
    if ((flags & VMM_MAP_USER) != 0) {
        entry_flags |= VMM_PAGE_USER;
    }
    if ((flags & VMM_MAP_NX) != 0) {
        entry_flags |= VMM_MAP_NX;
    }

    pt[pt_index] = (phys_addr & VMM_PHYS_MASK) | entry_flags;
    vmm_invalidate_tlb(virt_addr);
    return 1;
}

int vmm_unmap_page(uint64_t virt_addr)
{
    if ((virt_addr & VMM_PAGE_OFFSET_MASK) != 0) {
        return 0;
    }

    uint64_t *pt = NULL;
    uint16_t pt_index = 0;

    if (!vmm_walk_to_pt(virt_addr, 0, &pt, &pt_index)) {
        return 0;
    }

    if ((pt[pt_index] & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }

    pt[pt_index] = 0;
    vmm_invalidate_tlb(virt_addr);
    return 1;
}

int vmm_translate(uint64_t virt_addr, uint64_t *out_phys_addr)
{
    if (!g_pml4 || !out_phys_addr) {
        return 0;
    }

    uint16_t l4 = vmm_pml4_index(virt_addr);
    uint64_t pml4e = g_pml4[l4];
    if ((pml4e & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }

    uint64_t *pdpt = vmm_entry_to_table(pml4e);
    uint16_t l3 = vmm_pdpt_index(virt_addr);
    uint64_t pdpte = pdpt[l3];
    if ((pdpte & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }
    if ((pdpte & VMM_PAGE_SIZE) != 0) {
        *out_phys_addr =
            (pdpte & VMM_PHYS_MASK) | (virt_addr & ((1ULL << 30U) - 1ULL));
        return 1;
    }

    uint64_t *pd = vmm_entry_to_table(pdpte);
    uint16_t l2 = vmm_pd_index(virt_addr);
    uint64_t pde = pd[l2];
    if ((pde & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }
    if ((pde & VMM_PAGE_SIZE) != 0) {
        *out_phys_addr = (pde & VMM_PHYS_MASK) | (virt_addr & ((1ULL << 21U) - 1ULL));
        return 1;
    }

    uint64_t *pt = vmm_entry_to_table(pde);
    uint16_t l1 = vmm_pt_index(virt_addr);
    uint64_t pte = pt[l1];
    if ((pte & VMM_PAGE_PRESENT) == 0) {
        return 0;
    }

    *out_phys_addr = (pte & VMM_PHYS_MASK) | (virt_addr & VMM_PAGE_OFFSET_MASK);
    return 1;
}

int vmm_query_mapping(uint64_t virt_addr, uint64_t *out_phys_addr, uint64_t *out_flags)
{
    if (!g_pml4 || !out_phys_addr || !out_flags) {
        return 0;
    }

    uint16_t l4 = vmm_pml4_index(virt_addr);
    uint64_t pml4e = g_pml4[l4];
    if ((pml4e & VMM_PAGE_PRESENT) == 0U) {
        return 0;
    }

    uint64_t *pdpt = vmm_entry_to_table(pml4e);
    uint16_t l3 = vmm_pdpt_index(virt_addr);
    uint64_t pdpte = pdpt[l3];
    if ((pdpte & VMM_PAGE_PRESENT) == 0U) {
        return 0;
    }
    if ((pdpte & VMM_PAGE_SIZE) != 0U) {
        *out_phys_addr = (pdpte & VMM_PHYS_MASK) | (virt_addr & ((1ULL << 30U) - 1ULL));
        *out_flags = vmm_entry_to_map_flags(pdpte);
        return 1;
    }

    uint64_t *pd = vmm_entry_to_table(pdpte);
    uint16_t l2 = vmm_pd_index(virt_addr);
    uint64_t pde = pd[l2];
    if ((pde & VMM_PAGE_PRESENT) == 0U) {
        return 0;
    }
    if ((pde & VMM_PAGE_SIZE) != 0U) {
        *out_phys_addr = (pde & VMM_PHYS_MASK) | (virt_addr & ((1ULL << 21U) - 1ULL));
        *out_flags = vmm_entry_to_map_flags(pde);
        return 1;
    }

    uint64_t *pt = vmm_entry_to_table(pde);
    uint16_t l1 = vmm_pt_index(virt_addr);
    uint64_t pte = pt[l1];
    if ((pte & VMM_PAGE_PRESENT) == 0U) {
        return 0;
    }

    *out_phys_addr = (pte & VMM_PHYS_MASK) | (virt_addr & VMM_PAGE_OFFSET_MASK);
    *out_flags = vmm_entry_to_map_flags(pte);
    return 1;
}

void *vmm_alloc_kernel_pages(uint64_t page_count, uint64_t flags)
{
    if (page_count == 0) {
        return NULL;
    }
    if (!g_pml4) {
        return NULL;
    }

    uint64_t start = g_kernel_heap_next;
    uint64_t requested_bytes = page_count * MM_PAGE_SIZE;
    if (requested_bytes / MM_PAGE_SIZE != page_count) {
        return NULL;
    }

    if (start > UINT64_MAX - requested_bytes) {
        return NULL;
    }

    uint64_t expected_end = start + requested_bytes;
    if (start < g_kernel_layout.kernel_heap_base || expected_end > g_kernel_heap_limit) {
        serial_write("GNU OS: VMM kernel heap exhausted.\n");
        return NULL;
    }
    uint64_t current = start;

    for (uint64_t i = 0; i < page_count; i++) {
        void *phys_page = pmm_alloc_page();
        if (!phys_page) {
            break;
        }

        if (!vmm_map_page(current, (uint64_t)(uintptr_t)phys_page, flags | VMM_MAP_WRITABLE)) {
            pmm_free_page(phys_page);
            break;
        }

        current += MM_PAGE_SIZE;
    }

    if (current != expected_end) {
        for (uint64_t addr = start; addr < current; addr += MM_PAGE_SIZE) {
            uint64_t phys = 0;
            if (vmm_translate(addr, &phys)) {
                (void)vmm_unmap_page(addr);
                pmm_free_page((void *)(uintptr_t)(phys & VMM_PHYS_MASK));
            }
        }
        return NULL;
    }

    g_kernel_heap_next = current;
    return (void *)(uintptr_t)start;
}

void vmm_get_kernel_layout(vmm_kernel_layout_t *out_layout)
{
    if (!out_layout) {
        return;
    }

    *out_layout = g_kernel_layout;
}
