#include <stddef.h>
#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/pci.h>
#include <gnuos/printk.h>
#include <gnuos/serial.h>

#define PCI_CONFIG_ADDRESS_PORT 0xCF8U
#define PCI_CONFIG_DATA_PORT 0xCFCU

#define PCI_VENDOR_ID_NONE 0xFFFFU
#define PCI_HEADER_TYPE_MULTI_FUNCTION 0x80U

#define PCI_COMMAND_REG 0x04U
#define PCI_STATUS_REG 0x06U
#define PCI_CAP_PTR_REG 0x34U

#define PCI_STATUS_CAP_LIST 0x0010U
#define PCI_COMMAND_BUS_MASTER 0x0004U
#define PCI_COMMAND_INTX_DISABLE 0x0400U

#define PCI_CAP_ID_MSI 0x05U
#define PCI_CAP_ID_MSIX 0x11U

#define PCI_MSI_MSGCTL_ENABLE 0x0001U
#define PCI_MSI_MSGCTL_MULTIPLE_MESSAGE_ENABLE_MASK 0x0070U
#define PCI_MSI_MSGCTL_MULTIPLE_MESSAGE_CAPABLE_SHIFT 1U
#define PCI_MSI_MSGCTL_64BIT 0x0080U

#define PCI_MSIX_MSGCTL_ENABLE 0x8000U
#define PCI_MSIX_MSGCTL_TABLE_SIZE_MASK 0x07FFU

#define PCI_MSI_ADDRESS_BASE 0xFEE00000U

#define PCI_MAX_DEVICES 512U
#define PCI_LOG_LIMIT 32U
#define PCI_CAP_GUARD 48U

static pci_device_t g_pci_devices[PCI_MAX_DEVICES];
static uint16_t g_pci_device_count;
static uint32_t g_pci_total_device_count;
static uint32_t g_pci_truncated_count;
static uint16_t g_pci_msi_capable_count;
static uint16_t g_pci_msix_capable_count;

static const char *pci_class_name(uint8_t class_code)
{
    switch (class_code) {
        case 0x00:
            return "unclassified";
        case 0x01:
            return "mass-storage";
        case 0x02:
            return "network";
        case 0x03:
            return "display";
        case 0x04:
            return "multimedia";
        case 0x05:
            return "memory";
        case 0x06:
            return "bridge";
        case 0x07:
            return "simple-comm";
        case 0x08:
            return "base-system";
        case 0x09:
            return "input";
        case 0x0A:
            return "docking";
        case 0x0B:
            return "processor";
        case 0x0C:
            return "serial-bus";
        case 0x0D:
            return "wireless";
        case 0x0E:
            return "intelligent-io";
        case 0x0F:
            return "satellite";
        case 0x10:
            return "encryption";
        case 0x11:
            return "data-acquisition";
        default:
            return "vendor-specific";
    }
}

static uint32_t pci_cfg_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    return 0x80000000U |
        ((uint32_t)bus << 16U) |
        ((uint32_t)device << 11U) |
        ((uint32_t)function << 8U) |
        ((uint32_t)offset & 0xFCU);
}

static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    io_out32(PCI_CONFIG_ADDRESS_PORT, pci_cfg_address(bus, device, function, offset));
    return io_in32(PCI_CONFIG_DATA_PORT);
}

static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t value32;
    uint32_t shift;

    value32 = pci_read32(bus, device, function, offset);
    shift = (uint32_t)(offset & 2U) * 8U;
    return (uint16_t)((value32 >> shift) & 0xFFFFU);
}

static uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t value32;
    uint32_t shift;

    value32 = pci_read32(bus, device, function, offset);
    shift = (uint32_t)(offset & 3U) * 8U;
    return (uint8_t)((value32 >> shift) & 0xFFU);
}

static void pci_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    io_out32(PCI_CONFIG_ADDRESS_PORT, pci_cfg_address(bus, device, function, offset));
    io_out32(PCI_CONFIG_DATA_PORT, value);
}

static void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value)
{
    uint32_t value32;
    uint32_t shift;
    uint32_t clear_mask;

    value32 = pci_read32(bus, device, function, offset);
    shift = (uint32_t)(offset & 2U) * 8U;
    clear_mask = ~(0xFFFFU << shift);
    value32 = (value32 & clear_mask) | ((uint32_t)value << shift);
    pci_write32(bus, device, function, offset, value32);
}

static void pci_parse_capabilities(pci_device_t *entry)
{
    uint16_t status;
    uint8_t cap_ptr;
    uint8_t guard;

    status = pci_read16(entry->bus, entry->device, entry->function, PCI_STATUS_REG);
    if ((status & PCI_STATUS_CAP_LIST) == 0U) {
        return;
    }

    cap_ptr = (uint8_t)(pci_read8(entry->bus, entry->device, entry->function, PCI_CAP_PTR_REG) & 0xFCU);
    entry->capability_pointer = cap_ptr;

    for (guard = 0U; guard < PCI_CAP_GUARD && cap_ptr >= 0x40U; guard++) {
        uint8_t cap_id;
        uint8_t next_ptr;

        cap_id = pci_read8(entry->bus, entry->device, entry->function, cap_ptr);
        next_ptr = (uint8_t)(pci_read8(entry->bus, entry->device, entry->function, cap_ptr + 1U) & 0xFCU);

        if (cap_id == PCI_CAP_ID_MSI) {
            uint16_t message_control;

            entry->msi_cap_offset = cap_ptr;
            message_control =
                pci_read16(entry->bus, entry->device, entry->function, (uint8_t)(cap_ptr + 2U));
            entry->msi_enabled = (message_control & PCI_MSI_MSGCTL_ENABLE) ? 1U : 0U;
            entry->msi_multiple_message_capable = (uint8_t)(
                (message_control >> PCI_MSI_MSGCTL_MULTIPLE_MESSAGE_CAPABLE_SHIFT) & 0x07U);
        } else if (cap_id == PCI_CAP_ID_MSIX) {
            uint16_t message_control;

            entry->msix_cap_offset = cap_ptr;
            message_control =
                pci_read16(entry->bus, entry->device, entry->function, (uint8_t)(cap_ptr + 2U));
            entry->msix_enabled = (message_control & PCI_MSIX_MSGCTL_ENABLE) ? 1U : 0U;
            entry->msix_table_size =
                (uint16_t)((message_control & PCI_MSIX_MSGCTL_TABLE_SIZE_MASK) + 1U);
        }

        if (next_ptr == cap_ptr || next_ptr < 0x40U) {
            break;
        }
        cap_ptr = next_ptr;
    }
}

static int pci_record_device(uint8_t bus, uint8_t device, uint8_t function)
{
    uint16_t vendor_id;
    pci_device_t entry;

    vendor_id = pci_read16(bus, device, function, 0x00U);
    if (vendor_id == PCI_VENDOR_ID_NONE) {
        return 0;
    }

    entry.bus = bus;
    entry.device = device;
    entry.function = function;
    entry.vendor_id = vendor_id;
    entry.device_id = pci_read16(bus, device, function, 0x02U);
    entry.revision_id = pci_read8(bus, device, function, 0x08U);
    entry.prog_if = pci_read8(bus, device, function, 0x09U);
    entry.subclass = pci_read8(bus, device, function, 0x0AU);
    entry.class_code = pci_read8(bus, device, function, 0x0BU);
    entry.header_type = (uint8_t)(pci_read8(bus, device, function, 0x0EU) & 0x7FU);
    entry.capability_pointer = 0U;
    entry.interrupt_line = pci_read8(bus, device, function, 0x3CU);
    entry.interrupt_pin = pci_read8(bus, device, function, 0x3DU);
    entry.msi_cap_offset = 0U;
    entry.msix_cap_offset = 0U;
    entry.msi_enabled = 0U;
    entry.msix_enabled = 0U;
    entry.msi_multiple_message_capable = 0U;
    entry.msix_table_size = 0U;

    pci_parse_capabilities(&entry);

    if (entry.msi_cap_offset != 0U) {
        g_pci_msi_capable_count++;
    }
    if (entry.msix_cap_offset != 0U) {
        g_pci_msix_capable_count++;
    }

    if (g_pci_device_count < PCI_MAX_DEVICES) {
        g_pci_devices[g_pci_device_count] = entry;
        g_pci_device_count++;
    } else {
        g_pci_truncated_count++;
    }

    if (g_pci_total_device_count < PCI_LOG_LIMIT) {
        const char *irq_model;

        if (entry.msix_cap_offset != 0U) {
            irq_model = "MSI-X";
        } else if (entry.msi_cap_offset != 0U) {
            irq_model = "MSI";
        } else {
            irq_model = "INTx";
        }

        kprintf(
            "GNU OS: PCI %u:%u.%u vendor=0x%X device=0x%X class=%s "
            "(0x%X/0x%X/0x%X) irq=%s\n",
            (uint64_t)bus,
            (uint64_t)device,
            (uint64_t)function,
            (uint64_t)entry.vendor_id,
            (uint64_t)entry.device_id,
            pci_class_name(entry.class_code),
            (uint64_t)entry.class_code,
            (uint64_t)entry.subclass,
            (uint64_t)entry.prog_if,
            irq_model);
    }

    g_pci_total_device_count++;
    return 1;
}

static void pci_scan_device(uint8_t bus, uint8_t device)
{
    uint8_t header_type;

    if (!pci_record_device(bus, device, 0U)) {
        return;
    }

    header_type = pci_read8(bus, device, 0U, 0x0EU);
    if ((header_type & PCI_HEADER_TYPE_MULTI_FUNCTION) == 0U) {
        return;
    }

    for (uint8_t function = 1U; function < 8U; function++) {
        (void)pci_record_device(bus, device, function);
    }
}

static pci_device_t *pci_get_device_mut(uint16_t index)
{
    if (index >= g_pci_device_count) {
        return NULL;
    }

    return &g_pci_devices[index];
}

void pci_init(void)
{
    g_pci_device_count = 0U;
    g_pci_total_device_count = 0U;
    g_pci_truncated_count = 0U;
    g_pci_msi_capable_count = 0U;
    g_pci_msix_capable_count = 0U;

    for (uint16_t bus = 0U; bus < 256U; bus++) {
        for (uint8_t device = 0U; device < 32U; device++) {
            pci_scan_device((uint8_t)bus, device);
        }
    }

    kprintf(
        "GNU OS: PCI scan complete, devices=%u, stored=%u, truncated=%u, msi=%u, msix=%u\n",
        (uint64_t)g_pci_total_device_count,
        (uint64_t)g_pci_device_count,
        (uint64_t)g_pci_truncated_count,
        (uint64_t)g_pci_msi_capable_count,
        (uint64_t)g_pci_msix_capable_count);

    if (g_pci_total_device_count > PCI_LOG_LIMIT) {
        kprintf("GNU OS: PCI log limited to first %u entries.\n", (uint64_t)PCI_LOG_LIMIT);
    }

    if (g_pci_total_device_count == 0U) {
        serial_write("GNU OS: PCI scan found no devices.\n");
    }
}

uint16_t pci_device_count(void)
{
    return g_pci_device_count;
}

const pci_device_t *pci_get_device(uint16_t index)
{
    if (index >= g_pci_device_count) {
        return NULL;
    }

    return &g_pci_devices[index];
}

int pci_enable_msi(uint16_t index, uint8_t apic_id, uint8_t vector)
{
    pci_device_t *entry;
    uint16_t command;
    uint16_t msi_control;
    uint8_t msi_data_offset;
    uint8_t msi_cap;

    entry = pci_get_device_mut(index);
    if (!entry) {
        return -1;
    }

    msi_cap = entry->msi_cap_offset;
    if (msi_cap == 0U) {
        return -2;
    }

    if (entry->msix_cap_offset != 0U && entry->msix_enabled != 0U) {
        uint16_t msix_control;

        msix_control = pci_read16(
            entry->bus,
            entry->device,
            entry->function,
            (uint8_t)(entry->msix_cap_offset + 2U));
        msix_control = (uint16_t)(msix_control & ~PCI_MSIX_MSGCTL_ENABLE);
        pci_write16(
            entry->bus,
            entry->device,
            entry->function,
            (uint8_t)(entry->msix_cap_offset + 2U),
            msix_control);
        entry->msix_enabled = 0U;
    }

    msi_control = pci_read16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 2U));
    msi_control = (uint16_t)(msi_control & ~PCI_MSI_MSGCTL_ENABLE);
    msi_control = (uint16_t)(msi_control & ~PCI_MSI_MSGCTL_MULTIPLE_MESSAGE_ENABLE_MASK);
    pci_write16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 2U), msi_control);

    pci_write32(
        entry->bus,
        entry->device,
        entry->function,
        (uint8_t)(msi_cap + 4U),
        PCI_MSI_ADDRESS_BASE | ((uint32_t)apic_id << 12U));

    if ((msi_control & PCI_MSI_MSGCTL_64BIT) != 0U) {
        pci_write32(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 8U), 0U);
        msi_data_offset = 0x0CU;
    } else {
        msi_data_offset = 0x08U;
    }

    pci_write16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + msi_data_offset), vector);

    command = pci_read16(entry->bus, entry->device, entry->function, PCI_COMMAND_REG);
    command = (uint16_t)(command | PCI_COMMAND_BUS_MASTER | PCI_COMMAND_INTX_DISABLE);
    pci_write16(entry->bus, entry->device, entry->function, PCI_COMMAND_REG, command);

    msi_control = (uint16_t)(msi_control | PCI_MSI_MSGCTL_ENABLE);
    pci_write16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 2U), msi_control);
    entry->msi_enabled = 1U;

    kprintf(
        "GNU OS: PCI MSI enabled on %u:%u.%u vector=0x%X apic=0x%X\n",
        (uint64_t)entry->bus,
        (uint64_t)entry->device,
        (uint64_t)entry->function,
        (uint64_t)vector,
        (uint64_t)apic_id);

    return 0;
}

int pci_disable_msi(uint16_t index)
{
    pci_device_t *entry;
    uint16_t command;
    uint16_t msi_control;
    uint8_t msi_cap;

    entry = pci_get_device_mut(index);
    if (!entry) {
        return -1;
    }

    msi_cap = entry->msi_cap_offset;
    if (msi_cap == 0U) {
        return -2;
    }

    msi_control = pci_read16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 2U));
    msi_control = (uint16_t)(msi_control & ~PCI_MSI_MSGCTL_ENABLE);
    pci_write16(entry->bus, entry->device, entry->function, (uint8_t)(msi_cap + 2U), msi_control);

    command = pci_read16(entry->bus, entry->device, entry->function, PCI_COMMAND_REG);
    command = (uint16_t)(command & ~PCI_COMMAND_INTX_DISABLE);
    pci_write16(entry->bus, entry->device, entry->function, PCI_COMMAND_REG, command);
    entry->msi_enabled = 0U;

    kprintf(
        "GNU OS: PCI MSI disabled on %u:%u.%u\n",
        (uint64_t)entry->bus,
        (uint64_t)entry->device,
        (uint64_t)entry->function);

    return 0;
}
