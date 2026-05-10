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

static uint64_t *g_pml4;

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

static void vmm_zero_page(uint64_t *table)
{
    for (uint16_t i = 0; i < VMM_ENTRIES_PER_TABLE; i++) {
        table[i] = 0;
    }
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
            return 0;
        }
    } else {
        if ((pd[l2] & VMM_PAGE_PRESENT) == 0 || (pd[l2] & VMM_PAGE_SIZE) != 0) {
            return 0;
        }
        pt = vmm_entry_to_table(pd[l2]);
    }

    *out_pt = pt;
    *out_pt_index = l1;
    return 1;
}

int vmm_init(void)
{
    uint64_t cr3 = 0;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    g_pml4 = (uint64_t *)(uintptr_t)(cr3 & VMM_PHYS_MASK);

    if (!g_pml4) {
        return 0;
    }

    serial_write("GNU OS: VMM initialized, CR3=");
    serial_write_hex64(cr3 & VMM_PHYS_MASK);
    serial_write("\n");
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

