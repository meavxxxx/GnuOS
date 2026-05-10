#ifndef GNUOS_MM_H
#define GNUOS_MM_H

#include <stdint.h>

#define MM_PAGE_SIZE 4096ULL

void pmm_init(uint64_t base, uint64_t memory_size);
void *pmm_alloc_page(void);
void pmm_free_page(void *address);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);

#endif

