#ifndef GNUOS_PCI_H
#define GNUOS_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
    uint8_t capability_pointer;

    uint8_t interrupt_line;
    uint8_t interrupt_pin;

    uint8_t msi_cap_offset;
    uint8_t msix_cap_offset;
    uint8_t msi_enabled;
    uint8_t msix_enabled;
    uint8_t msi_multiple_message_capable;
    uint16_t msix_table_size;
} pci_device_t;

void pci_init(void);
uint16_t pci_device_count(void);
const pci_device_t *pci_get_device(uint16_t index);
int pci_enable_msi(uint16_t index, uint8_t apic_id, uint8_t vector);
int pci_disable_msi(uint16_t index);

#endif
