#include <stdint.h>
#include <stddef.h>

#include <gnuos/multiboot2.h>

#define MULTIBOOT_TAG_TYPE_END 0U
#define MULTIBOOT_TAG_TYPE_MMAP 6U
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8U
#define MULTIBOOT_TAG_TYPE_ACPI_OLD 14U
#define MULTIBOOT_TAG_TYPE_ACPI_NEW 15U
#define MULTIBOOT_MEMORY_AVAILABLE 1U
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1U

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

typedef struct {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) multiboot_tag_framebuffer_t;

typedef struct {
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
} __attribute__((packed)) multiboot_tag_framebuffer_rgb_t;

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

int multiboot2_find_framebuffer(
    uint64_t boot_info_addr,
    multiboot2_framebuffer_info_t *out_info)
{
    if (!out_info || boot_info_addr == 0) {
        return 0;
    }

    const multiboot_info_header_t *header =
        (const multiboot_info_header_t *)(uintptr_t)boot_info_addr;
    if (header->total_size < sizeof(multiboot_info_header_t)) {
        return 0;
    }

    uintptr_t cursor = (uintptr_t)boot_info_addr + sizeof(multiboot_info_header_t);
    uintptr_t end = (uintptr_t)boot_info_addr + (uintptr_t)header->total_size;

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

        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER &&
            tag->size >= sizeof(multiboot_tag_framebuffer_t)) {
            const multiboot_tag_framebuffer_t *framebuffer_tag =
                (const multiboot_tag_framebuffer_t *)tag;

            out_info->address = framebuffer_tag->framebuffer_addr;
            out_info->pitch = framebuffer_tag->framebuffer_pitch;
            out_info->width = framebuffer_tag->framebuffer_width;
            out_info->height = framebuffer_tag->framebuffer_height;
            out_info->bpp = framebuffer_tag->framebuffer_bpp;
            out_info->type = framebuffer_tag->framebuffer_type;
            out_info->red_field_position = 0U;
            out_info->red_mask_size = 0U;
            out_info->green_field_position = 0U;
            out_info->green_mask_size = 0U;
            out_info->blue_field_position = 0U;
            out_info->blue_mask_size = 0U;

            if (framebuffer_tag->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
                uintptr_t rgb_offset = cursor + sizeof(multiboot_tag_framebuffer_t);
                uintptr_t rgb_end = rgb_offset + sizeof(multiboot_tag_framebuffer_rgb_t);
                if (rgb_end > (cursor + tag->size)) {
                    return 0;
                }

                const multiboot_tag_framebuffer_rgb_t *rgb =
                    (const multiboot_tag_framebuffer_rgb_t *)(const void *)rgb_offset;
                out_info->red_field_position = rgb->red_field_position;
                out_info->red_mask_size = rgb->red_mask_size;
                out_info->green_field_position = rgb->green_field_position;
                out_info->green_mask_size = rgb->green_mask_size;
                out_info->blue_field_position = rgb->blue_field_position;
                out_info->blue_mask_size = rgb->blue_mask_size;
            }
            return 1;
        }

        cursor = next;
    }

    return 0;
}

int multiboot2_find_acpi_rsdp(
    uint64_t boot_info_addr,
    uint64_t *out_rsdp_addr,
    uint8_t *out_revision)
{
    if (!out_rsdp_addr || boot_info_addr == 0U) {
        return 0;
    }

    const multiboot_info_header_t *header =
        (const multiboot_info_header_t *)(uintptr_t)boot_info_addr;
    if (header->total_size < sizeof(multiboot_info_header_t)) {
        return 0;
    }

    uintptr_t cursor = (uintptr_t)boot_info_addr + sizeof(multiboot_info_header_t);
    uintptr_t end = (uintptr_t)boot_info_addr + (uintptr_t)header->total_size;
    uint64_t old_rsdp = 0U;
    uint64_t new_rsdp = 0U;
    uint8_t new_revision = 0U;

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

        if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD) {
            if (tag->size >= (sizeof(multiboot_tag_t) + 20U)) {
                old_rsdp = (uint64_t)(cursor + sizeof(multiboot_tag_t));
            }
        } else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
            if (tag->size >= (sizeof(multiboot_tag_t) + 36U)) {
                const uint8_t *rsdp =
                    (const uint8_t *)(cursor + sizeof(multiboot_tag_t));
                new_rsdp = (uint64_t)(uintptr_t)rsdp;
                new_revision = rsdp[15];
            }
        }

        cursor = next;
    }

    if (new_rsdp != 0U) {
        *out_rsdp_addr = new_rsdp;
        if (out_revision) {
            *out_revision = new_revision;
        }
        return 1;
    }

    if (old_rsdp != 0U) {
        *out_rsdp_addr = old_rsdp;
        if (out_revision) {
            *out_revision = 0U;
        }
        return 1;
    }

    return 0;
}
