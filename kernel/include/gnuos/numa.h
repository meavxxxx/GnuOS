#ifndef GNUOS_NUMA_H
#define GNUOS_NUMA_H

#include <stdint.h>

typedef struct {
    uint32_t node_id;
    uint64_t base;
    uint64_t size;
} numa_node_info_t;

void numa_init(uint64_t fallback_base, uint64_t fallback_size);
uint32_t numa_node_count(void);
int numa_get_node(uint32_t index, numa_node_info_t *out_node);
int numa_phys_to_node(uint64_t phys_addr, uint32_t *out_node_id);

#endif
