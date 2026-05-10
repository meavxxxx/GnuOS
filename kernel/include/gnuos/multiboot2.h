#ifndef GNUOS_MULTIBOOT2_H
#define GNUOS_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289U

int multiboot2_find_largest_available_region(
    uint64_t boot_info_addr,
    uint64_t *out_base,
    uint64_t *out_size);

#endif

