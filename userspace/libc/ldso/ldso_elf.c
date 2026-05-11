#include "ldso_elf.h"

static void ldso_elf_layout_reset(ldso_elf_layout_t *layout)
{
    if (!layout) {
        return;
    }

    layout->phdr_table = 0;
    layout->phdr_count = 0;
    layout->load_count = 0;
    layout->phdr_segment = 0;
    layout->dynamic_segment = 0;
    layout->relro_segment = 0;
}

static void ldso_elf_dynamic_reset(ldso_elf_dynamic_info_t *dynamic_info)
{
    if (!dynamic_info) {
        return;
    }

    dynamic_info->dynamic_entries = 0;
    dynamic_info->dynamic_count = 0U;
    dynamic_info->strtab = 0;
    dynamic_info->strsz = 0U;
    dynamic_info->symtab = 0;
    dynamic_info->syment = 0U;
    dynamic_info->rela = 0;
    dynamic_info->rela_count = 0U;
    dynamic_info->rela_ent = 0U;
    dynamic_info->jmprel = 0;
    dynamic_info->jmprel_count = 0U;
    dynamic_info->pltrel_type = 0U;
    dynamic_info->init_func = 0U;
    dynamic_info->init_array = 0;
    dynamic_info->init_array_count = 0U;
}

static void ldso_elf_reloc_result_reset(ldso_elf_reloc_result_t *result)
{
    if (!result) {
        return;
    }

    result->applied_count = 0U;
    result->unresolved_count = 0U;
    result->unsupported_count = 0U;
}

static int ldso_elf_symbol_value(
    const ldso_elf_dynamic_info_t *dynamic_info,
    uint64_t load_bias,
    uint32_t sym_index,
    ldso_symbol_resolver_t resolver,
    void *resolver_context,
    uint64_t *value_out)
{
    const ldso_elf_sym_t *symbol;
    const char *symbol_name = 0;
    uint64_t symbol_value = 0U;

    if (!dynamic_info || !value_out || !dynamic_info->symtab) {
        return -1;
    }

    symbol = &dynamic_info->symtab[sym_index];
    if (symbol->st_name < dynamic_info->strsz && dynamic_info->strtab) {
        symbol_name = dynamic_info->strtab + symbol->st_name;
    }

    if (symbol->st_shndx != LDSO_SHN_UNDEF) {
        symbol_value = load_bias + symbol->st_value;
        *value_out = symbol_value;
        return 0;
    }

    if (!resolver || !symbol_name || symbol_name[0] == '\0') {
        return -1;
    }

    if (!resolver(symbol_name, &symbol_value, resolver_context)) {
        return -1;
    }

    *value_out = symbol_value;
    return 0;
}

static int ldso_elf_apply_rela_range(
    const ldso_elf_dynamic_info_t *dynamic_info,
    uint64_t load_bias,
    const ldso_elf_rela_t *rela_entries,
    uint64_t rela_count,
    ldso_symbol_resolver_t resolver,
    void *resolver_context,
    ldso_elf_reloc_result_t *result)
{
    uint64_t index;

    if (!dynamic_info || !rela_entries || !result) {
        return -1;
    }

    for (index = 0U; index < rela_count; index++) {
        const ldso_elf_rela_t *rela = &rela_entries[index];
        uint64_t target_addr = load_bias + rela->r_offset;
        uint64_t symbol_value = 0U;
        uint32_t rela_type = ldso_elf64_r_type(rela->r_info);
        uint32_t sym_index = ldso_elf64_r_sym(rela->r_info);
        uint64_t *target = (uint64_t *)(uintptr_t)target_addr;

        if (rela_type == LDSO_R_X86_64_NONE) {
            continue;
        }

        if (rela_type == LDSO_R_X86_64_RELATIVE) {
            *target = load_bias + (uint64_t)rela->r_addend;
            result->applied_count++;
            continue;
        }

        if (rela_type == LDSO_R_X86_64_64 ||
            rela_type == LDSO_R_X86_64_GLOB_DAT ||
            rela_type == LDSO_R_X86_64_JUMP_SLOT) {
            if (ldso_elf_symbol_value(
                    dynamic_info,
                    load_bias,
                    sym_index,
                    resolver,
                    resolver_context,
                    &symbol_value) != 0) {
                result->unresolved_count++;
                continue;
            }

            *target = symbol_value + (uint64_t)rela->r_addend;
            result->applied_count++;
            continue;
        }

        result->unsupported_count++;
    }

    return 0;
}

int ldso_elf_classify_phdrs(
    const ldso_elf_phdr_t *phdrs,
    uint64_t phnum,
    uint64_t phentsize,
    ldso_elf_layout_t *layout)
{
    uint64_t index;

    if (!phdrs || !layout || phnum == 0U || phentsize != sizeof(ldso_elf_phdr_t)) {
        return -1;
    }

    ldso_elf_layout_reset(layout);
    layout->phdr_table = phdrs;
    layout->phdr_count = phnum;

    for (index = 0U; index < phnum; index++) {
        const ldso_elf_phdr_t *phdr = &phdrs[index];
        if (phdr->p_type == LDSO_PT_LOAD) {
            layout->load_count++;
        } else if (phdr->p_type == LDSO_PT_PHDR) {
            if (!layout->phdr_segment) {
                layout->phdr_segment = phdr;
            }
        } else if (phdr->p_type == LDSO_PT_DYNAMIC) {
            if (!layout->dynamic_segment) {
                layout->dynamic_segment = phdr;
            }
        } else if (phdr->p_type == LDSO_PT_GNU_RELRO) {
            if (!layout->relro_segment) {
                layout->relro_segment = phdr;
            }
        }
    }

    return 0;
}

int ldso_elf_parse_dynamic(
    const ldso_elf_layout_t *layout,
    uint64_t load_bias,
    ldso_elf_dynamic_info_t *dynamic_info)
{
    uint64_t index;
    uint64_t dyn_count;
    const ldso_elf_dyn_t *dyn_entries;
    uint64_t rela_size = 0U;
    uint64_t pltrel_size = 0U;
    uint64_t init_array_size = 0U;

    if (!layout || !dynamic_info || !layout->dynamic_segment || layout->dynamic_segment->p_filesz == 0U) {
        return -1;
    }

    ldso_elf_dynamic_reset(dynamic_info);

    dyn_entries = (const ldso_elf_dyn_t *)(uintptr_t)(load_bias + layout->dynamic_segment->p_vaddr);
    dyn_count = layout->dynamic_segment->p_filesz / sizeof(ldso_elf_dyn_t);
    dynamic_info->dynamic_entries = dyn_entries;
    dynamic_info->dynamic_count = dyn_count;

    for (index = 0U; index < dyn_count; index++) {
        const ldso_elf_dyn_t *entry = &dyn_entries[index];
        if ((uint64_t)entry->d_tag == LDSO_DT_NULL) {
            break;
        }

        if ((uint64_t)entry->d_tag == LDSO_DT_STRTAB) {
            dynamic_info->strtab = (const char *)(uintptr_t)(load_bias + entry->d_un);
        } else if ((uint64_t)entry->d_tag == LDSO_DT_STRSZ) {
            dynamic_info->strsz = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_SYMTAB) {
            dynamic_info->symtab = (const ldso_elf_sym_t *)(uintptr_t)(load_bias + entry->d_un);
        } else if ((uint64_t)entry->d_tag == LDSO_DT_SYMENT) {
            dynamic_info->syment = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_RELA) {
            dynamic_info->rela = (const ldso_elf_rela_t *)(uintptr_t)(load_bias + entry->d_un);
        } else if ((uint64_t)entry->d_tag == LDSO_DT_RELASZ) {
            rela_size = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_RELAENT) {
            dynamic_info->rela_ent = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_JMPREL) {
            dynamic_info->jmprel = (const ldso_elf_rela_t *)(uintptr_t)(load_bias + entry->d_un);
        } else if ((uint64_t)entry->d_tag == LDSO_DT_PLTRELSZ) {
            pltrel_size = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_PLTREL) {
            dynamic_info->pltrel_type = entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_INIT) {
            dynamic_info->init_func = load_bias + entry->d_un;
        } else if ((uint64_t)entry->d_tag == LDSO_DT_INIT_ARRAY) {
            dynamic_info->init_array = (const uint64_t *)(uintptr_t)(load_bias + entry->d_un);
        } else if ((uint64_t)entry->d_tag == LDSO_DT_INIT_ARRAYSZ) {
            init_array_size = entry->d_un;
        }
    }

    if (!dynamic_info->symtab || !dynamic_info->strtab || dynamic_info->syment != sizeof(ldso_elf_sym_t)) {
        return -1;
    }

    if (dynamic_info->rela && dynamic_info->rela_ent == sizeof(ldso_elf_rela_t)) {
        dynamic_info->rela_count = rela_size / dynamic_info->rela_ent;
    }

    if (dynamic_info->jmprel && dynamic_info->pltrel_type == LDSO_DT_PLTREL_RELA) {
        dynamic_info->jmprel_count = pltrel_size / sizeof(ldso_elf_rela_t);
    }

    if (dynamic_info->init_array) {
        dynamic_info->init_array_count = init_array_size / sizeof(uint64_t);
    }

    return 0;
}

int ldso_elf_run_init_sequence(const ldso_elf_dynamic_info_t *dynamic_info)
{
    uint64_t index;

    if (!dynamic_info) {
        return -1;
    }

    if (dynamic_info->init_func != 0U) {
        void (*init_func)(void) = (void (*)(void))(uintptr_t)dynamic_info->init_func;
        init_func();
    }

    if (!dynamic_info->init_array || dynamic_info->init_array_count == 0U) {
        return 0;
    }

    for (index = 0U; index < dynamic_info->init_array_count; index++) {
        uint64_t init_ptr = dynamic_info->init_array[index];
        if (init_ptr == 0U || init_ptr == UINT64_MAX) {
            continue;
        }

        ((void (*)(void))(uintptr_t)init_ptr)();
    }

    return 0;
}

int ldso_elf_apply_relocations(
    const ldso_elf_dynamic_info_t *dynamic_info,
    uint64_t load_bias,
    ldso_symbol_resolver_t resolver,
    void *resolver_context,
    ldso_elf_reloc_result_t *result)
{
    if (!dynamic_info || !result) {
        return -1;
    }

    ldso_elf_reloc_result_reset(result);

    if (dynamic_info->rela && dynamic_info->rela_count > 0U) {
        if (ldso_elf_apply_rela_range(
                dynamic_info,
                load_bias,
                dynamic_info->rela,
                dynamic_info->rela_count,
                resolver,
                resolver_context,
                result) != 0) {
            return -1;
        }
    }

    if (dynamic_info->jmprel && dynamic_info->jmprel_count > 0U) {
        if (dynamic_info->pltrel_type != LDSO_DT_PLTREL_RELA) {
            result->unsupported_count += dynamic_info->jmprel_count;
            return 0;
        }

        if (ldso_elf_apply_rela_range(
                dynamic_info,
                load_bias,
                dynamic_info->jmprel,
                dynamic_info->jmprel_count,
                resolver,
                resolver_context,
                result) != 0) {
            return -1;
        }
    }

    return 0;
}
