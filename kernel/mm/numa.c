#include <stddef.h>
#include <stdint.h>

#include <gnuos/acpi.h>
#include <gnuos/mm.h>
#include <gnuos/numa.h>
#include <gnuos/printk.h>
#include <gnuos/vmm.h>

#define NUMA_MAX_NODES 8U

#define ACPI_SIG_SRAT "SRAT"
#define ACPI_SRAT_TYPE_MEMORY_AFFINITY 1U
#define ACPI_SRAT_MEM_ENABLED 0x1U

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
    uint32_t reserved1;
    uint64_t reserved2;
} __attribute__((packed)) acpi_srat_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_srat_entry_header_t;

typedef struct {
    acpi_srat_entry_header_t header;
    uint32_t proximity_domain;
    uint16_t reserved1;
    uint64_t base_address;
    uint64_t length;
    uint32_t reserved2;
    uint32_t flags;
    uint64_t reserved3;
} __attribute__((packed)) acpi_srat_memory_affinity_t;

static numa_node_info_t g_nodes[NUMA_MAX_NODES];
static uint32_t g_node_count;
static uint8_t g_numa_ready;
static uint8_t g_numa_from_srat;

static void numa_reset_state(void)
{
    for (uint32_t i = 0; i < NUMA_MAX_NODES; i++) {
        g_nodes[i].node_id = 0U;
        g_nodes[i].base = 0U;
        g_nodes[i].size = 0U;
    }

    g_node_count = 0U;
    g_numa_ready = 0U;
    g_numa_from_srat = 0U;
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

static int map_identity_range(uint64_t base, uint64_t length)
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

static void numa_add_or_extend_node(uint32_t node_id, uint64_t base, uint64_t size)
{
    uint64_t end = 0U;

    if (size == 0U || base > (UINT64_MAX - size)) {
        return;
    }
    end = base + size;

    for (uint32_t i = 0; i < g_node_count; i++) {
        if (g_nodes[i].node_id == node_id) {
            uint64_t node_base = g_nodes[i].base;
            uint64_t node_end = g_nodes[i].base + g_nodes[i].size;

            if (node_end < node_base) {
                node_end = UINT64_MAX;
            }

            if (base < node_base) {
                node_base = base;
            }
            if (end > node_end) {
                node_end = end;
            }

            g_nodes[i].base = node_base;
            g_nodes[i].size = node_end - node_base;
            return;
        }
    }

    if (g_node_count >= NUMA_MAX_NODES) {
        return;
    }

    g_nodes[g_node_count].node_id = node_id;
    g_nodes[g_node_count].base = base;
    g_nodes[g_node_count].size = size;
    g_node_count++;
}

static int numa_parse_srat(uint64_t srat_phys_addr)
{
    const acpi_srat_t *srat = NULL;
    const uint8_t *cursor = NULL;
    const uint8_t *end = NULL;

    if (srat_phys_addr == 0U) {
        return 0;
    }

    if (!map_identity_range(srat_phys_addr, sizeof(acpi_sdt_header_t))) {
        return 0;
    }

    srat = (const acpi_srat_t *)(uintptr_t)srat_phys_addr;
    if (!acpi_sig_match(srat->header.signature, ACPI_SIG_SRAT, 4U)) {
        return 0;
    }
    if (srat->header.length < sizeof(acpi_srat_t)) {
        return 0;
    }

    if (!map_identity_range(srat_phys_addr, (uint64_t)srat->header.length)) {
        return 0;
    }

    srat = (const acpi_srat_t *)(uintptr_t)srat_phys_addr;
    if (!acpi_checksum_ok(srat, srat->header.length)) {
        return 0;
    }

    cursor = (const uint8_t *)srat + sizeof(acpi_srat_t);
    end = (const uint8_t *)srat + srat->header.length;

    while ((cursor + sizeof(acpi_srat_entry_header_t)) <= end) {
        const acpi_srat_entry_header_t *entry =
            (const acpi_srat_entry_header_t *)(const void *)cursor;

        if (entry->length < sizeof(acpi_srat_entry_header_t)) {
            break;
        }
        if ((cursor + entry->length) > end) {
            break;
        }

        if (entry->type == ACPI_SRAT_TYPE_MEMORY_AFFINITY &&
            entry->length >= sizeof(acpi_srat_memory_affinity_t)) {
            const acpi_srat_memory_affinity_t *mem =
                (const acpi_srat_memory_affinity_t *)(const void *)entry;

            if ((mem->flags & ACPI_SRAT_MEM_ENABLED) != 0U && mem->length > 0U) {
                numa_add_or_extend_node(
                    mem->proximity_domain,
                    mem->base_address,
                    mem->length);
            }
        }

        cursor += entry->length;
    }

    return (g_node_count > 0U);
}

void numa_init(uint64_t fallback_base, uint64_t fallback_size)
{
    acpi_info_t acpi_info;

    numa_reset_state();

    if (acpi_get_info(&acpi_info) &&
        acpi_info.srat_address != 0U &&
        numa_parse_srat(acpi_info.srat_address)) {
        g_numa_from_srat = 1U;
    }

    if (g_node_count == 0U && fallback_size > 0U) {
        numa_add_or_extend_node(0U, fallback_base, fallback_size);
        g_numa_from_srat = 0U;
    }

    if (g_node_count == 0U) {
        kprintf("GNU OS: NUMA init failed: no memory nodes detected.\n");
        return;
    }

    g_numa_ready = 1U;
    kprintf(
        "GNU OS: NUMA initialized nodes=%u source=%s\n",
        (uint64_t)g_node_count,
        g_numa_from_srat ? "SRAT" : "fallback");

    for (uint32_t i = 0U; i < g_node_count; i++) {
        uint64_t end = g_nodes[i].base + g_nodes[i].size;
        uint64_t mib = g_nodes[i].size / (1024ULL * 1024ULL);

        kprintf(
            "GNU OS: NUMA node[%u] id=%u range=0x%X..0x%X size=%u MiB\n",
            (uint64_t)i,
            (uint64_t)g_nodes[i].node_id,
            g_nodes[i].base,
            end,
            mib);
    }
}

uint32_t numa_node_count(void)
{
    if (!g_numa_ready) {
        return 0U;
    }

    return g_node_count;
}

int numa_get_node(uint32_t index, numa_node_info_t *out_node)
{
    if (!g_numa_ready || !out_node || index >= g_node_count) {
        return 0;
    }

    *out_node = g_nodes[index];
    return 1;
}

int numa_phys_to_node(uint64_t phys_addr, uint32_t *out_node_id)
{
    if (!g_numa_ready || !out_node_id) {
        return 0;
    }

    for (uint32_t i = 0; i < g_node_count; i++) {
        uint64_t start = g_nodes[i].base;
        uint64_t end = start + g_nodes[i].size;
        if (end < start) {
            end = UINT64_MAX;
        }

        if (phys_addr >= start && phys_addr < end) {
            *out_node_id = g_nodes[i].node_id;
            return 1;
        }
    }

    return 0;
}
