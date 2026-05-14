#ifndef GNUOS_MEMINFO_H
#define GNUOS_MEMINFO_H

#include <stdint.h>

typedef struct {
    uint64_t mem_total_kb;
    uint64_t mem_free_kb;
    uint64_t mem_available_kb;
    uint64_t mem_used_kb;
} meminfo_snapshot_t;

void meminfo_snapshot(meminfo_snapshot_t *out_snapshot);
void meminfo_log_snapshot(void);

#endif

