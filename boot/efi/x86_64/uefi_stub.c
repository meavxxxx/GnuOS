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
    EFI_STATUS load_status;
    gnuos_kernel_main_t kmain_entry;

    (void)image_handle;
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

    load_status = gnuos_load_kernel_segments(system_table, kernel_blob, kernel_size, ehdr);
    if (load_status != EFI_SUCCESS) {
        efi_puts(system_table, failed);
        return load_status;
    }

    kmain_entry = gnuos_find_kmain_symbol(kernel_blob, kernel_size);
    if (!kmain_entry) {
        efi_puts(system_table, failed);
        return 5ULL;
    }

    efi_puts(system_table, handoff);
    kmain_entry(MULTIBOOT2_BOOTLOADER_MAGIC, 0ULL);

    static const CHAR16 returned[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'k', 'm', 'a', 'i', 'n', ' ', 'r', 'e', 't', 'u', 'r', 'n', 'e', 'd', '.',
        '\r', '\n', 0
    };
    efi_puts(system_table, returned);
    return EFI_SUCCESS;
}
