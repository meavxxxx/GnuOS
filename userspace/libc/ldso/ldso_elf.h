#ifndef GNUOS_LDSO_ELF_H
#define GNUOS_LDSO_ELF_H

#include <stdint.h>

#define LDSO_PT_LOAD 1U
#define LDSO_PT_DYNAMIC 2U
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
    const ldso_elf_phdr_t *dynamic_segment;
    const ldso_elf_phdr_t *relro_segment;
} ldso_elf_layout_t;

int ldso_elf_classify_phdrs(
    const ldso_elf_phdr_t *phdrs,
    uint64_t phnum,
    uint64_t phentsize,
    ldso_elf_layout_t *layout);

#endif
