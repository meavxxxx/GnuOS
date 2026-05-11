#include "ldso_elf.h"

static void ldso_elf_layout_reset(ldso_elf_layout_t *layout)
{
    if (!layout) {
        return;
    }

    layout->phdr_table = 0;
    layout->phdr_count = 0;
    layout->load_count = 0;
    layout->dynamic_segment = 0;
    layout->relro_segment = 0;
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
