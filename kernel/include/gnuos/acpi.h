#ifndef GNUOS_ACPI_H
#define GNUOS_ACPI_H

#include <stdint.h>

typedef struct {
    uint8_t revision;
    uint64_t rsdp_address;
    uint32_t rsdp_length;
    uint64_t root_sdt_address;
    uint32_t root_sdt_length;
    uint64_t rsdt_address;
    uint64_t xsdt_address;
    uint64_t madt_address;
    uint32_t madt_length;
    uint64_t fadt_address;
    uint32_t fadt_length;
    uint64_t srat_address;
    uint32_t srat_length;
    uint32_t local_apic_address;
} acpi_info_t;

int acpi_init(uint64_t boot_info_addr);
int acpi_get_info(acpi_info_t *out_info);

#endif
