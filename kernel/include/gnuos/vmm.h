#ifndef GNUOS_VMM_H
#define GNUOS_VMM_H

#include <stdint.h>

#define VMM_MAP_WRITABLE (1ULL << 1)
#define VMM_MAP_USER (1ULL << 2)
#define VMM_MAP_NX (1ULL << 63)

int vmm_init(void);
int vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
int vmm_unmap_page(uint64_t virt_addr);
int vmm_translate(uint64_t virt_addr, uint64_t *out_phys_addr);

#endif

