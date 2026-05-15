#ifndef GNUOS_VMM_H
#define GNUOS_VMM_H

#include <stdint.h>

#define VMM_MAP_WRITABLE (1ULL << 1)
#define VMM_MAP_USER (1ULL << 2)
#define VMM_MAP_NX (1ULL << 63)

typedef struct {
    uint64_t kaslr_window_base;
    uint64_t kaslr_window_size;
    uint64_t kaslr_slide;
    uint64_t kernel_image_base;
    uint64_t kernel_image_size;
    uint64_t kernel_heap_base;
    uint64_t kernel_heap_size;
} vmm_kernel_layout_t;

int vmm_init(void);
int vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
int vmm_unmap_page(uint64_t virt_addr);
int vmm_translate(uint64_t virt_addr, uint64_t *out_phys_addr);
int vmm_query_mapping(uint64_t virt_addr, uint64_t *out_phys_addr, uint64_t *out_flags);
void *vmm_alloc_kernel_pages(uint64_t page_count, uint64_t flags);
void vmm_get_kernel_layout(vmm_kernel_layout_t *out_layout);

#endif
