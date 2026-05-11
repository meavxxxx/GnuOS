#ifndef GNUOS_LDSO_ELF_H
#define GNUOS_LDSO_ELF_H

#include <stdint.h>

#define LDSO_PT_LOAD 1U
#define LDSO_PT_DYNAMIC 2U
#define LDSO_PT_PHDR 6U
#define LDSO_PT_GNU_RELRO 0x6474E552U

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} ldso_elf_phdr_t;

typedef struct {
    const ldso_elf_phdr_t *phdr_table;
    uint64_t phdr_count;
    uint64_t load_count;
    const ldso_elf_phdr_t *phdr_segment;
    const ldso_elf_phdr_t *dynamic_segment;
    const ldso_elf_phdr_t *relro_segment;
} ldso_elf_layout_t;

#define LDSO_DT_NULL 0ULL
#define LDSO_DT_NEEDED 1ULL
#define LDSO_DT_PLTRELSZ 2ULL
#define LDSO_DT_PLTGOT 3ULL
#define LDSO_DT_HASH 4ULL
#define LDSO_DT_STRTAB 5ULL
#define LDSO_DT_SYMTAB 6ULL
#define LDSO_DT_RELA 7ULL
#define LDSO_DT_RELASZ 8ULL
#define LDSO_DT_RELAENT 9ULL
#define LDSO_DT_STRSZ 10ULL
#define LDSO_DT_SYMENT 11ULL
#define LDSO_DT_INIT 12ULL
#define LDSO_DT_FINI 13ULL
#define LDSO_DT_PLTREL 20ULL
#define LDSO_DT_JMPREL 23ULL
#define LDSO_DT_INIT_ARRAY 25ULL
#define LDSO_DT_FINI_ARRAY 26ULL
#define LDSO_DT_INIT_ARRAYSZ 27ULL
#define LDSO_DT_FINI_ARRAYSZ 28ULL

#define LDSO_DT_PLTREL_RELA LDSO_DT_RELA

#define LDSO_R_X86_64_NONE 0U
#define LDSO_R_X86_64_64 1U
#define LDSO_R_X86_64_GLOB_DAT 6U
#define LDSO_R_X86_64_JUMP_SLOT 7U
#define LDSO_R_X86_64_RELATIVE 8U

#define LDSO_SHN_UNDEF 0U

typedef struct {
    int64_t d_tag;
    uint64_t d_un;
} ldso_elf_dyn_t;

typedef struct {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} ldso_elf_sym_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t r_addend;
} ldso_elf_rela_t;

typedef struct {
    const ldso_elf_dyn_t *dynamic_entries;
    uint64_t dynamic_count;
    const char *strtab;
    uint64_t strsz;
    const ldso_elf_sym_t *symtab;
    uint64_t syment;
    const ldso_elf_rela_t *rela;
    uint64_t rela_count;
    uint64_t rela_ent;
    const ldso_elf_rela_t *jmprel;
    uint64_t jmprel_count;
    uint64_t pltrel_type;
    uint64_t init_func;
    const uint64_t *init_array;
    uint64_t init_array_count;
} ldso_elf_dynamic_info_t;

typedef struct {
    uint64_t applied_count;
    uint64_t unresolved_count;
    uint64_t unsupported_count;
} ldso_elf_reloc_result_t;

typedef int (*ldso_symbol_resolver_t)(const char *name, uint64_t *address, void *context);

static inline uint32_t ldso_elf64_r_sym(uint64_t info)
{
    return (uint32_t)(info >> 32U);
}

static inline uint32_t ldso_elf64_r_type(uint64_t info)
{
    return (uint32_t)(info & 0xFFFFFFFFU);
}

int ldso_elf_classify_phdrs(
    const ldso_elf_phdr_t *phdrs,
    uint64_t phnum,
    uint64_t phentsize,
    ldso_elf_layout_t *layout);
int ldso_elf_parse_dynamic(
    const ldso_elf_layout_t *layout,
    uint64_t load_bias,
    ldso_elf_dynamic_info_t *dynamic_info);
int ldso_elf_apply_relocations(
    const ldso_elf_dynamic_info_t *dynamic_info,
    uint64_t load_bias,
    ldso_symbol_resolver_t resolver,
    void *resolver_context,
    ldso_elf_reloc_result_t *result);
int ldso_elf_run_init_sequence(const ldso_elf_dynamic_info_t *dynamic_info);

#endif
