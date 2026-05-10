#include <stdint.h>
#include <stddef.h>

#include <gnuos/multiboot2.h>

#define MULTIBOOT_TAG_TYPE_END 0U
#define MULTIBOOT_TAG_TYPE_MMAP 6U
#define MULTIBOOT_MEMORY_AVAILABLE 1U

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) multiboot_info_header_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    multiboot_mmap_entry_t entries[];
} __attribute__((packed)) multiboot_tag_mmap_t;

static uint32_t align8(uint32_t value)
{
    return (value + 7U) & ~7U;
}

int multiboot2_find_largest_available_region(
    uint64_t boot_info_addr,
    uint64_t *out_base,
    uint64_t *out_size)
{
    if (!out_base || !out_size || boot_info_addr == 0) {
        return 0;
    }

    const multiboot_info_header_t *header =
        (const multiboot_info_header_t *)(uintptr_t)boot_info_addr;
    if (header->total_size < sizeof(multiboot_info_header_t)) {
        return 0;
    }

    uintptr_t cursor = (uintptr_t)boot_info_addr + sizeof(multiboot_info_header_t);
    uintptr_t end = (uintptr_t)boot_info_addr + (uintptr_t)header->total_size;

    uint64_t best_base = 0;
    uint64_t best_size = 0;

    while (cursor + sizeof(multiboot_tag_t) <= end) {
        const multiboot_tag_t *tag = (const multiboot_tag_t *)cursor;
        if (tag->size < sizeof(multiboot_tag_t)) {
            return 0;
        }

        uintptr_t next = cursor + align8(tag->size);
        if (next > end) {
            return 0;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP &&
            tag->size >= sizeof(multiboot_tag_mmap_t)) {
            const multiboot_tag_mmap_t *mmap_tag = (const multiboot_tag_mmap_t *)tag;
            if (mmap_tag->entry_size < sizeof(multiboot_mmap_entry_t)) {
                return 0;
            }

            uintptr_t entries_start = (uintptr_t)&mmap_tag->entries[0];
            uintptr_t entries_end = cursor + tag->size;

            for (uintptr_t p = entries_start;
                 p + mmap_tag->entry_size <= entries_end;
                 p += mmap_tag->entry_size) {
                const multiboot_mmap_entry_t *entry =
                    (const multiboot_mmap_entry_t *)p;

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->len > best_size) {
                    best_base = entry->addr;
                    best_size = entry->len;
                }
            }
        }

        cursor = next;
    }

    if (best_size == 0) {
        return 0;
    }

    *out_base = best_base;
    *out_size = best_size;
    return 1;
}

