#ifndef GNUOS_MULTIBOOT2_H
#define GNUOS_MULTIBOOT2_H

#include <stdint.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289U
#define MULTIBOOT2_FRAMEBUFFER_TYPE_INDEXED 0U
#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB 1U
#define MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT 2U

typedef struct {
    uint64_t address;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
    uint8_t red_field_position;
    uint8_t red_mask_size;
    uint8_t green_field_position;
    uint8_t green_mask_size;
    uint8_t blue_field_position;
    uint8_t blue_mask_size;
} multiboot2_framebuffer_info_t;

int multiboot2_find_largest_available_region(
    uint64_t boot_info_addr,
    uint64_t *out_base,
    uint64_t *out_size);

int multiboot2_find_framebuffer(
    uint64_t boot_info_addr,
    multiboot2_framebuffer_info_t *out_info);

#endif
