#include <uefi.h>

typedef void (*gnuos_kernel_main_t)(uint64_t boot_magic, uint64_t boot_info_addr);

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr_t;

typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} elf64_sym_t;

#define ELF64_PT_LOAD 1U
#define ELF64_SHT_SYMTAB 2U
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289ULL
#define MULTIBOOT2_TAG_TYPE_END 0U
#define MULTIBOOT2_TAG_TYPE_MMAP 6U
#define MULTIBOOT2_MEMORY_AVAILABLE 1U
#define MULTIBOOT2_MEMORY_RESERVED 2U
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE 3U
#define MULTIBOOT2_MEMORY_ACPI_NVS 4U
#define MULTIBOOT2_MEMORY_BADRAM 5U

#define EFI_INVALID_PARAMETER 0x8000000000000002ULL
#define EFI_BUFFER_TOO_SMALL 0x8000000000000005ULL
#define EFI_PAGE_SIZE 4096ULL

#define GNUOS_EFI_MMAP_SLOP_DESCRIPTORS 8U
#define GNUOS_EFI_EXIT_BOOT_SERVICES_RETRIES 4U
#define GNUOS_EFI_MULTIBOOT_INFO_PAGES 16U

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) multiboot2_info_header_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) multiboot2_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) multiboot2_mmap_entry_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed)) multiboot2_mmap_tag_t;

typedef struct {
    EFI_PHYSICAL_ADDRESS base;
    UINTN size_bytes;
} gnuos_efi_buffer_t;

extern const unsigned char gnuos_kernel_blob_start[];
extern const unsigned char gnuos_kernel_blob_end[];

static void efi_puts(EFI_SYSTEM_TABLE *system_table, const CHAR16 *text)
{
    if (!system_table || !system_table->ConOut || !system_table->ConOut->OutputString || !text) {
        return;
    }

    (void)system_table->ConOut->OutputString(system_table->ConOut, text);
}

static void *gnuos_memcpy(void *dst, const void *src, UINTN size)
{
    UINTN i;
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;

    for (i = 0U; i < size; i++) {
        out[i] = in[i];
    }

    return dst;
}

static void *gnuos_memset(void *dst, int value, UINTN size)
{
    UINTN i;
    unsigned char *out = (unsigned char *)dst;

    for (i = 0U; i < size; i++) {
        out[i] = (unsigned char)value;
    }

    return dst;
}

static int gnuos_str_equal(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs) {
        return 0;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs != *rhs) {
            return 0;
        }
        lhs++;
        rhs++;
    }

    return *lhs == '\0' && *rhs == '\0';
}

static uint64_t gnuos_align_up(uint64_t value, uint64_t alignment)
{
    return (value + (alignment - 1ULL)) & ~(alignment - 1ULL);
}

static uint32_t gnuos_efi_memory_type_to_multiboot(uint32_t efi_memory_type)
{
    switch (efi_memory_type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiConventionalMemory:
            return MULTIBOOT2_MEMORY_AVAILABLE;
        case EfiACPIReclaimMemory:
            return MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE;
        case EfiACPIMemoryNVS:
            return MULTIBOOT2_MEMORY_ACPI_NVS;
        case EfiUnusableMemory:
            return MULTIBOOT2_MEMORY_BADRAM;
        default:
            return MULTIBOOT2_MEMORY_RESERVED;
    }
}

static EFI_STATUS gnuos_allocate_multiboot2_buffer(
    EFI_SYSTEM_TABLE *system_table,
    gnuos_efi_buffer_t *buffer_out)
{
    EFI_PHYSICAL_ADDRESS buffer_base = 0U;
    UINTN buffer_size = 0U;
    EFI_STATUS status = EFI_SUCCESS;

    if (!system_table || !system_table->BootServices || !system_table->BootServices->AllocatePages ||
        !buffer_out) {
        return 1ULL;
    }

    status = system_table->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        GNUOS_EFI_MULTIBOOT_INFO_PAGES,
        &buffer_base);
    if (status != EFI_SUCCESS) {
        return status;
    }

    buffer_size = (UINTN)(GNUOS_EFI_MULTIBOOT_INFO_PAGES * EFI_PAGE_SIZE);
    buffer_out->base = buffer_base;
    buffer_out->size_bytes = buffer_size;
    return EFI_SUCCESS;
}

static EFI_STATUS gnuos_fetch_memory_map(
    EFI_SYSTEM_TABLE *system_table,
    EFI_MEMORY_DESCRIPTOR **memory_map_out,
    UINTN *memory_map_size_out,
    UINTN *map_key_out,
    UINTN *descriptor_size_out)
{
    EFI_BOOT_SERVICES *boot_services = 0;
    EFI_STATUS status = EFI_SUCCESS;
    EFI_MEMORY_DESCRIPTOR *memory_map = 0;
    UINTN memory_map_size = 0U;
    UINTN map_key = 0U;
    UINTN descriptor_size = 0U;
    uint32_t descriptor_version = 0U;
    uint8_t attempt;

    if (!system_table || !memory_map_out || !memory_map_size_out || !map_key_out ||
        !descriptor_size_out) {
        return 2ULL;
    }

    boot_services = system_table->BootServices;
    if (!boot_services || !boot_services->GetMemoryMap || !boot_services->AllocatePool ||
        !boot_services->FreePool) {
        return 3ULL;
    }

    status = boot_services->GetMemoryMap(
        &memory_map_size,
        0,
        &map_key,
        &descriptor_size,
        &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL || descriptor_size == 0U) {
        return 4ULL;
    }

    memory_map_size += descriptor_size * GNUOS_EFI_MMAP_SLOP_DESCRIPTORS;
    for (attempt = 0U; attempt < GNUOS_EFI_EXIT_BOOT_SERVICES_RETRIES; attempt++) {
        status = boot_services->AllocatePool(
            EfiLoaderData,
            memory_map_size,
            (void **)&memory_map);
        if (status != EFI_SUCCESS || !memory_map) {
            return (status != EFI_SUCCESS) ? status : 5ULL;
        }

        status = boot_services->GetMemoryMap(
            &memory_map_size,
            memory_map,
            &map_key,
            &descriptor_size,
            &descriptor_version);
        if (status == EFI_SUCCESS) {
            *memory_map_out = memory_map;
            *memory_map_size_out = memory_map_size;
            *map_key_out = map_key;
            *descriptor_size_out = descriptor_size;
            return EFI_SUCCESS;
        }

        (void)boot_services->FreePool(memory_map);
        memory_map = 0;
        if (status != EFI_BUFFER_TOO_SMALL || descriptor_size == 0U) {
            return status;
        }
        memory_map_size += descriptor_size * GNUOS_EFI_MMAP_SLOP_DESCRIPTORS;
    }

    return status;
}

static EFI_STATUS gnuos_build_multiboot2_info(
    const EFI_MEMORY_DESCRIPTOR *memory_map,
    UINTN memory_map_size,
    UINTN descriptor_size,
    const gnuos_efi_buffer_t *boot_info_buffer,
    uint64_t *boot_info_addr_out)
{
    uint64_t descriptor_count;
    uint64_t mmap_tag_size;
    uint64_t total_size;
    multiboot2_info_header_t *header;
    multiboot2_mmap_tag_t *mmap_tag;
    multiboot2_mmap_entry_t *entries;
    multiboot2_tag_t *end_tag;
    uint64_t index;
    unsigned char *boot_info_base;

    if (!memory_map || descriptor_size == 0U || !boot_info_buffer || !boot_info_addr_out) {
        return 6ULL;
    }

    descriptor_count = (uint64_t)(memory_map_size / descriptor_size);
    if (descriptor_count == 0ULL) {
        return 7ULL;
    }

    mmap_tag_size = (uint64_t)sizeof(multiboot2_mmap_tag_t) +
        (descriptor_count * (uint64_t)sizeof(multiboot2_mmap_entry_t));
    if (mmap_tag_size > 0xFFFFFFFFULL) {
        return 8ULL;
    }

    total_size = (uint64_t)sizeof(multiboot2_info_header_t);
    total_size += gnuos_align_up(mmap_tag_size, 8ULL);
    total_size += gnuos_align_up((uint64_t)sizeof(multiboot2_tag_t), 8ULL);
    if (total_size > 0xFFFFFFFFULL ||
        total_size > (uint64_t)boot_info_buffer->size_bytes) {
        return 8ULL;
    }

    boot_info_base = (unsigned char *)(UINTN)boot_info_buffer->base;
    (void)gnuos_memset(boot_info_base, 0, boot_info_buffer->size_bytes);

    header = (multiboot2_info_header_t *)(void *)boot_info_base;
    mmap_tag = (multiboot2_mmap_tag_t *)(void *)(boot_info_base + sizeof(multiboot2_info_header_t));
    entries = (multiboot2_mmap_entry_t *)(void *)((unsigned char *)mmap_tag + sizeof(multiboot2_mmap_tag_t));

    for (index = 0ULL; index < descriptor_count; index++) {
        const EFI_MEMORY_DESCRIPTOR *descriptor =
            (const EFI_MEMORY_DESCRIPTOR *)(const void
                *)((const unsigned char *)memory_map + (index * descriptor_size));
        entries[index].addr = descriptor->PhysicalStart;
        entries[index].len = descriptor->NumberOfPages * EFI_PAGE_SIZE;
        entries[index].type = gnuos_efi_memory_type_to_multiboot(descriptor->Type);
        entries[index].reserved = 0U;
    }

    mmap_tag->type = MULTIBOOT2_TAG_TYPE_MMAP;
    mmap_tag->size = (uint32_t)mmap_tag_size;
    mmap_tag->entry_size = (uint32_t)sizeof(multiboot2_mmap_entry_t);
    mmap_tag->entry_version = 0U;

    end_tag = (multiboot2_tag_t *)(void
            *)(boot_info_base + sizeof(multiboot2_info_header_t) +
               (UINTN)gnuos_align_up((uint64_t)mmap_tag->size, 8ULL));
    end_tag->type = MULTIBOOT2_TAG_TYPE_END;
    end_tag->size = (uint32_t)sizeof(multiboot2_tag_t);

    header->total_size = (uint32_t)total_size;
    header->reserved = 0U;
    *boot_info_addr_out = (uint64_t)(UINTN)boot_info_base;
    return EFI_SUCCESS;
}

static EFI_STATUS gnuos_exit_boot_services(
    EFI_HANDLE image_handle,
    EFI_SYSTEM_TABLE *system_table,
    UINTN map_key)
{
    if (!system_table || !system_table->BootServices || !system_table->BootServices->ExitBootServices) {
        return 9ULL;
    }

    return system_table->BootServices->ExitBootServices(image_handle, map_key);
}

static EFI_STATUS gnuos_load_kernel_segments(
    EFI_SYSTEM_TABLE *system_table,
    const unsigned char *blob,
    UINTN blob_size,
    const elf64_ehdr_t *ehdr)
{
    uint16_t index;
    const unsigned char *phdr_base;

    if (ehdr->e_phoff + ((uint64_t)ehdr->e_phnum * ehdr->e_phentsize) > blob_size) {
        return 1ULL;
    }

    phdr_base = blob + ehdr->e_phoff;
    for (index = 0U; index < ehdr->e_phnum; index++) {
        const elf64_phdr_t *phdr =
            (const elf64_phdr_t *)(const void *)(phdr_base + ((uint64_t)index * ehdr->e_phentsize));
        uint64_t segment_start;
        uint64_t segment_end;
        uint64_t alloc_start;
        UINTN pages;
        EFI_PHYSICAL_ADDRESS alloc_addr;

        if (phdr->p_type != ELF64_PT_LOAD) {
            continue;
        }
        if (phdr->p_filesz > phdr->p_memsz) {
            return 2ULL;
        }
        if ((phdr->p_offset + phdr->p_filesz) > blob_size) {
            return 3ULL;
        }

        segment_start = phdr->p_paddr;
        segment_end = phdr->p_paddr + phdr->p_memsz;
        alloc_start = segment_start & ~0xFFFULL;
        segment_end = gnuos_align_up(segment_end, 0x1000ULL);
        pages = (UINTN)((segment_end - alloc_start) / 0x1000ULL);

        if (system_table && system_table->BootServices && system_table->BootServices->AllocatePages) {
            alloc_addr = alloc_start;
            if (system_table->BootServices->AllocatePages(
                    AllocateAddress,
                    EfiLoaderData,
                    pages,
                    &alloc_addr) != EFI_SUCCESS) {
                return 4ULL;
            }
        }

        (void)gnuos_memset((void *)(UINTN)segment_start, 0, (UINTN)phdr->p_memsz);
        (void)gnuos_memcpy(
            (void *)(UINTN)segment_start,
            (const void *)(blob + phdr->p_offset),
            (UINTN)phdr->p_filesz);
    }

    return EFI_SUCCESS;
}

static gnuos_kernel_main_t gnuos_find_kmain_symbol(const unsigned char *blob, UINTN blob_size)
{
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)(const void *)blob;
    const unsigned char *shdr_base;
    uint16_t section_index;

    if (ehdr->e_shoff == 0ULL || ehdr->e_shentsize == 0U || ehdr->e_shnum == 0U) {
        return 0;
    }
    if (ehdr->e_shoff + ((uint64_t)ehdr->e_shnum * ehdr->e_shentsize) > blob_size) {
        return 0;
    }

    shdr_base = blob + ehdr->e_shoff;
    for (section_index = 0U; section_index < ehdr->e_shnum; section_index++) {
        const elf64_shdr_t *symtab_shdr = (const elf64_shdr_t *)(const void
                *)(shdr_base + ((uint64_t)section_index * ehdr->e_shentsize));
        const elf64_shdr_t *strtab_shdr;
        const char *strtab;
        const unsigned char *symtab_base;
        uint64_t sym_count;
        uint64_t sym_index;

        if (symtab_shdr->sh_type != ELF64_SHT_SYMTAB || symtab_shdr->sh_entsize < sizeof(elf64_sym_t)) {
            continue;
        }
        if (symtab_shdr->sh_link >= ehdr->e_shnum) {
            continue;
        }
        if (symtab_shdr->sh_offset + symtab_shdr->sh_size > blob_size) {
            continue;
        }

        strtab_shdr = (const elf64_shdr_t *)(const void
                *)(shdr_base + ((uint64_t)symtab_shdr->sh_link * ehdr->e_shentsize));
        if (strtab_shdr->sh_offset + strtab_shdr->sh_size > blob_size) {
            continue;
        }

        strtab = (const char *)(const void *)(blob + strtab_shdr->sh_offset);
        symtab_base = blob + symtab_shdr->sh_offset;
        sym_count = symtab_shdr->sh_size / symtab_shdr->sh_entsize;

        for (sym_index = 0ULL; sym_index < sym_count; sym_index++) {
            const elf64_sym_t *sym =
                (const elf64_sym_t *)(const void *)(symtab_base + (sym_index * symtab_shdr->sh_entsize));
            const char *name;

            if (sym->st_name >= strtab_shdr->sh_size) {
                continue;
            }
            name = strtab + sym->st_name;
            if (gnuos_str_equal(name, "kmain")) {
                return (gnuos_kernel_main_t)(UINTN)sym->st_value;
            }
        }
    }

    return 0;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    static const CHAR16 banner[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'U', 'E', 'F', 'I', ' ', 's', 't', 'u', 'b', ' ',
        'l', 'o', 'a', 'd', 'e', 'r', ' ', 's', 't', 'a', 'r', 't', '.', '\r', '\n', 0
    };
    static const CHAR16 loading[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'l', 'o', 'a', 'd', 'i', 'n', 'g', ' ', 'k', 'e', 'r', 'n', 'e', 'l', ' ', 'b', 'l', 'o', 'b', '.', '\r', '\n', 0
    };
    static const CHAR16 failed[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'U', 'E', 'F', 'I', ' ', 'k', 'e', 'r', 'n', 'e', 'l', ' ', 'h', 'a', 'n', 'd', 'o', 'f', 'f', ' ',
        'f', 'a', 'i', 'l', 'e', 'd', '.', '\r', '\n', 0
    };
    static const CHAR16 handoff[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'h', 'a', 'n', 'd', 'o', 'f', 'f', ' ', 't', 'o', ' ', 'k', 'm', 'a', 'i', 'n', '.', '\r', '\n', 0
    };
    const unsigned char *kernel_blob = gnuos_kernel_blob_start;
    UINTN kernel_size = (UINTN)(gnuos_kernel_blob_end - gnuos_kernel_blob_start);
    const elf64_ehdr_t *ehdr;
    EFI_STATUS status = EFI_SUCCESS;
    EFI_MEMORY_DESCRIPTOR *memory_map = 0;
    UINTN memory_map_size = 0U;
    UINTN map_key = 0U;
    UINTN descriptor_size = 0U;
    gnuos_efi_buffer_t multiboot_info_buffer = {0};
    gnuos_kernel_main_t kmain_entry;
    uint64_t multiboot_info_addr = 0U;
    uint8_t attempt = 0U;

    efi_puts(system_table, banner);
    efi_puts(system_table, loading);

    if (!kernel_blob || kernel_size < sizeof(elf64_ehdr_t)) {
        efi_puts(system_table, failed);
        return 1ULL;
    }

    ehdr = (const elf64_ehdr_t *)(const void *)kernel_blob;
    if (ehdr->e_ident[0] != 0x7FU || ehdr->e_ident[1] != 'E' || ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        efi_puts(system_table, failed);
        return 2ULL;
    }
    if (ehdr->e_phentsize < sizeof(elf64_phdr_t) || ehdr->e_phnum == 0U) {
        efi_puts(system_table, failed);
        return 3ULL;
    }

    status = gnuos_load_kernel_segments(system_table, kernel_blob, kernel_size, ehdr);
    if (status != EFI_SUCCESS) {
        efi_puts(system_table, failed);
        return status;
    }

    kmain_entry = gnuos_find_kmain_symbol(kernel_blob, kernel_size);
    if (!kmain_entry) {
        efi_puts(system_table, failed);
        return 5ULL;
    }

    status = gnuos_allocate_multiboot2_buffer(system_table, &multiboot_info_buffer);
    if (status != EFI_SUCCESS) {
        efi_puts(system_table, failed);
        return status;
    }

    efi_puts(system_table, handoff);
    for (attempt = 0U; attempt < GNUOS_EFI_EXIT_BOOT_SERVICES_RETRIES; attempt++) {
        if (memory_map && system_table && system_table->BootServices &&
            system_table->BootServices->FreePool) {
            (void)system_table->BootServices->FreePool(memory_map);
            memory_map = 0;
        }

        status = gnuos_fetch_memory_map(
            system_table,
            &memory_map,
            &memory_map_size,
            &map_key,
            &descriptor_size);
        if (status != EFI_SUCCESS) {
            efi_puts(system_table, failed);
            return status;
        }

        status = gnuos_build_multiboot2_info(
            memory_map,
            memory_map_size,
            descriptor_size,
            &multiboot_info_buffer,
            &multiboot_info_addr);
        if (status != EFI_SUCCESS) {
            efi_puts(system_table, failed);
            return status;
        }

        status = gnuos_exit_boot_services(image_handle, system_table, map_key);
        if (status == EFI_SUCCESS) {
            break;
        }
        if (status != EFI_INVALID_PARAMETER) {
            efi_puts(system_table, failed);
            return status;
        }
    }
    if (status != EFI_SUCCESS) {
        efi_puts(system_table, failed);
        return status;
    }

    kmain_entry(MULTIBOOT2_BOOTLOADER_MAGIC, multiboot_info_addr);
    for (;;) {
        __asm__ volatile("hlt");
    }
    return EFI_SUCCESS;
}
