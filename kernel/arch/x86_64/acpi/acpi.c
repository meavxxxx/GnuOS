#include <stddef.h>
#include <stdint.h>

#include <gnuos/acpi.h>
#include <gnuos/mm.h>
#include <gnuos/multiboot2.h>
#include <gnuos/printk.h>
#include <gnuos/serial.h>
#include <gnuos/vmm.h>

#define ACPI_SIG_RSDP "RSD PTR "
#define ACPI_SIG_RSDT "RSDT"
#define ACPI_SIG_XSDT "XSDT"
#define ACPI_SIG_MADT "APIC"
#define ACPI_SIG_FADT "FACP"

#define ACPI_MADT_TYPE_LOCAL_APIC_ADDR_OVERRIDE 5U

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_v1_t;

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_v2_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_header_t;

typedef struct {
    acpi_madt_entry_header_t header;
    uint16_t reserved;
    uint64_t local_apic_address;
} __attribute__((packed)) acpi_madt_local_apic_override_t;

static acpi_info_t g_acpi_info;
static uint8_t g_acpi_ready;

static void acpi_zero_info(acpi_info_t *info)
{
    uint8_t *bytes = (uint8_t *)(void *)info;

    for (uint64_t i = 0; i < sizeof(*info); i++) {
        bytes[i] = 0U;
    }
}

static int acpi_sig_match(const char *a, const char *b, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }

    return 1;
}

static int acpi_checksum_ok(const void *base, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)base;
    uint8_t sum = 0U;

    if (!base || length == 0U) {
        return 0;
    }

    for (uint32_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }

    return (sum == 0U);
}

static int acpi_map_identity_range(uint64_t base, uint64_t length)
{
    uint64_t page_mask = MM_PAGE_SIZE - 1ULL;
    uint64_t start = 0U;
    uint64_t end = 0U;

    if (base == 0U || length == 0U) {
        return 0;
    }

    if (base > (UINT64_MAX - length)) {
        return 0;
    }
    if ((base + length) > (UINT64_MAX - page_mask)) {
        return 0;
    }

    start = base & ~page_mask;
    end = (base + length + page_mask) & ~page_mask;

    for (uint64_t page = start; page < end; page += MM_PAGE_SIZE) {
        uint64_t mapped_phys = 0U;
        uint64_t mapped_flags = 0U;

        if (vmm_query_mapping(page, &mapped_phys, &mapped_flags)) {
            continue;
        }

        if (!vmm_map_page(page, page, VMM_MAP_WRITABLE)) {
            return 0;
        }
    }

    return 1;
}

static int acpi_map_table(uint64_t table_phys_addr, const acpi_sdt_header_t **out_table)
{
    const acpi_sdt_header_t *table = NULL;

    if (!out_table || table_phys_addr == 0U) {
        return 0;
    }

    if (!acpi_map_identity_range(table_phys_addr, sizeof(acpi_sdt_header_t))) {
        return 0;
    }

    table = (const acpi_sdt_header_t *)(uintptr_t)table_phys_addr;
    if (table->length < sizeof(acpi_sdt_header_t)) {
        return 0;
    }

    if (!acpi_map_identity_range(table_phys_addr, (uint64_t)table->length)) {
        return 0;
    }

    if (!acpi_checksum_ok(table, table->length)) {
        return 0;
    }

    *out_table = table;
    return 1;
}

static int acpi_find_sdt(
    const acpi_sdt_header_t *root_sdt,
    uint8_t is_xsdt,
    const char signature[4],
    uint64_t *out_table_phys)
{
    uint32_t entry_size = is_xsdt ? 8U : 4U;
    uint32_t payload_len = 0U;
    uint32_t entry_count = 0U;
    const uint8_t *entries = NULL;

    if (!root_sdt || !out_table_phys || root_sdt->length < sizeof(acpi_sdt_header_t)) {
        return 0;
    }

    payload_len = root_sdt->length - (uint32_t)sizeof(acpi_sdt_header_t);
    entry_count = payload_len / entry_size;
    entries = (const uint8_t *)root_sdt + sizeof(acpi_sdt_header_t);

    for (uint32_t i = 0; i < entry_count; i++) {
        uint64_t table_phys = 0U;
        const acpi_sdt_header_t *table = NULL;

        if (is_xsdt) {
            table_phys = ((const uint64_t *)(const void *)entries)[i];
        } else {
            table_phys = (uint64_t)((const uint32_t *)(const void *)entries)[i];
        }

        if (table_phys == 0U) {
            continue;
        }

        if (!acpi_map_table(table_phys, &table)) {
            continue;
        }

        if (acpi_sig_match(table->signature, signature, 4U)) {
            *out_table_phys = table_phys;
            return 1;
        }
    }

    return 0;
}

static uint64_t acpi_madt_lapic_addr(const acpi_madt_t *madt)
{
    uint64_t lapic_addr = 0U;
    const uint8_t *cursor = NULL;
    const uint8_t *end = NULL;

    if (!madt || madt->header.length < sizeof(acpi_madt_t)) {
        return lapic_addr;
    }

    lapic_addr = madt->local_apic_address;
    cursor = (const uint8_t *)madt + sizeof(acpi_madt_t);
    end = (const uint8_t *)madt + madt->header.length;

    while ((cursor + sizeof(acpi_madt_entry_header_t)) <= end) {
        const acpi_madt_entry_header_t *entry =
            (const acpi_madt_entry_header_t *)(const void *)cursor;

        if (entry->length < sizeof(acpi_madt_entry_header_t)) {
            break;
        }
        if ((cursor + entry->length) > end) {
            break;
        }

        if (entry->type == ACPI_MADT_TYPE_LOCAL_APIC_ADDR_OVERRIDE &&
            entry->length >= sizeof(acpi_madt_local_apic_override_t)) {
            const acpi_madt_local_apic_override_t *override =
                (const acpi_madt_local_apic_override_t *)(const void *)entry;
            lapic_addr = override->local_apic_address;
            break;
        }

        cursor += entry->length;
    }

    return lapic_addr;
}

int acpi_init(uint64_t boot_info_addr)
{
    uint64_t rsdp_addr = 0U;
    uint8_t rsdp_revision_from_tag = 0U;
    const acpi_rsdp_v1_t *rsdp_v1 = NULL;
    const acpi_rsdp_v2_t *rsdp_v2 = NULL;
    uint64_t root_sdt_addr = 0U;
    const acpi_sdt_header_t *root_sdt = NULL;
    uint8_t use_xsdt = 0U;

    acpi_zero_info(&g_acpi_info);
    g_acpi_ready = 0U;

    if (!multiboot2_find_acpi_rsdp(boot_info_addr, &rsdp_addr, &rsdp_revision_from_tag)) {
        serial_write("GNU OS: ACPI RSDP tag not found in multiboot2 info.\n");
        return 0;
    }

    rsdp_v1 = (const acpi_rsdp_v1_t *)(uintptr_t)rsdp_addr;
    if (!acpi_sig_match(rsdp_v1->signature, ACPI_SIG_RSDP, 8U)) {
        serial_write("GNU OS: ACPI RSDP signature mismatch.\n");
        return 0;
    }
    if (!acpi_checksum_ok(rsdp_v1, 20U)) {
        serial_write("GNU OS: ACPI RSDP checksum invalid.\n");
        return 0;
    }

    g_acpi_info.revision = rsdp_v1->revision;
    g_acpi_info.rsdp_address = rsdp_addr;
    g_acpi_info.rsdt_address = rsdp_v1->rsdt_address;

    if (rsdp_v1->revision >= 2U) {
        rsdp_v2 = (const acpi_rsdp_v2_t *)(uintptr_t)rsdp_addr;
        if (rsdp_v2->length < sizeof(acpi_rsdp_v2_t)) {
            serial_write("GNU OS: ACPI RSDP v2 length invalid.\n");
            return 0;
        }
        if (!acpi_checksum_ok(rsdp_v2, rsdp_v2->length)) {
            serial_write("GNU OS: ACPI RSDP v2 extended checksum invalid.\n");
            return 0;
        }
        g_acpi_info.xsdt_address = rsdp_v2->xsdt_address;
    }

    if (g_acpi_info.xsdt_address != 0U) {
        root_sdt_addr = g_acpi_info.xsdt_address;
        use_xsdt = 1U;
    } else {
        root_sdt_addr = g_acpi_info.rsdt_address;
        use_xsdt = 0U;
    }

    if (!acpi_map_table(root_sdt_addr, &root_sdt)) {
        serial_write("GNU OS: ACPI root SDT mapping/validation failed.\n");
        return 0;
    }

    if (use_xsdt) {
        if (!acpi_sig_match(root_sdt->signature, ACPI_SIG_XSDT, 4U)) {
            serial_write("GNU OS: ACPI root table is not XSDT.\n");
            return 0;
        }
    } else if (!acpi_sig_match(root_sdt->signature, ACPI_SIG_RSDT, 4U)) {
        serial_write("GNU OS: ACPI root table is not RSDT.\n");
        return 0;
    }

    (void)acpi_find_sdt(root_sdt, use_xsdt, ACPI_SIG_MADT, &g_acpi_info.madt_address);
    (void)acpi_find_sdt(root_sdt, use_xsdt, ACPI_SIG_FADT, &g_acpi_info.fadt_address);

    if (g_acpi_info.madt_address != 0U) {
        const acpi_sdt_header_t *madt_hdr = NULL;

        if (acpi_map_table(g_acpi_info.madt_address, &madt_hdr)) {
            const acpi_madt_t *madt = (const acpi_madt_t *)(const void *)madt_hdr;
            g_acpi_info.local_apic_address = (uint32_t)acpi_madt_lapic_addr(madt);
        }
    }

    kprintf(
        "GNU OS: ACPI initialized rev=%u tag_rev=%u rsdp=0x%X root=%s@0x%X madt=0x%X fadt=0x%X lapic=0x%X\n",
        (uint64_t)g_acpi_info.revision,
        (uint64_t)rsdp_revision_from_tag,
        g_acpi_info.rsdp_address,
        use_xsdt ? "XSDT" : "RSDT",
        root_sdt_addr,
        g_acpi_info.madt_address,
        g_acpi_info.fadt_address,
        (uint64_t)g_acpi_info.local_apic_address);

    if (g_acpi_info.madt_address == 0U || g_acpi_info.fadt_address == 0U) {
        serial_write("GNU OS: ACPI warning: MADT or FADT missing.\n");
        return 0;
    }

    g_acpi_ready = 1U;
    return 1;
}

int acpi_get_info(acpi_info_t *out_info)
{
    if (!out_info || !g_acpi_ready) {
        return 0;
    }

    *out_info = g_acpi_info;
    return 1;
}
