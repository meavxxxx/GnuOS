#include <stdint.h>

#include "ldso_elf.h"

#define LDSO_AUXV_SCAN_LIMIT 256U
#define LDSO_STACK_SCAN_LIMIT 4096U

#define LDSO_AT_NULL 0U
#define LDSO_AT_PHDR 3U
#define LDSO_AT_PHENT 4U
#define LDSO_AT_PHNUM 5U
#define LDSO_AT_BASE 7U
#define LDSO_AT_ENTRY 9U

typedef struct {
    uint64_t a_type;
    uint64_t a_val;
} ldso_auxv_entry_t;

typedef struct {
    uint64_t ready;
    uint64_t at_base;
    uint64_t at_entry;
    uint64_t at_phdr;
    uint64_t at_phent;
    uint64_t at_phnum;
    uint64_t load_bias;
    uint64_t load_count;
    uint64_t has_dynamic_segment;
    uint64_t has_gnu_relro;
    uint64_t dynamic_ready;
    uint64_t relocations_attempted;
    uint64_t relocations_applied;
    uint64_t relocations_unresolved;
    uint64_t relocations_unsupported;
} ldso_stage0_state_t;

volatile ldso_stage0_state_t g_ldso_stage0_state;

static const uint64_t *ldso_stack_skip_argv(const uint64_t *cursor, uint64_t argc)
{
    uint64_t index;

    if (!cursor) {
        return 0;
    }

    for (index = 0U; index < argc; index++) {
        if (*cursor == 0U) {
            return 0;
        }
        cursor++;
    }

    if (*cursor != 0U) {
        return 0;
    }

    return cursor + 1U;
}

static const uint64_t *ldso_stack_skip_envp(const uint64_t *cursor)
{
    uint64_t scanned = 0U;

    if (!cursor) {
        return 0;
    }

    while (scanned < LDSO_STACK_SCAN_LIMIT) {
        if (*cursor == 0U) {
            return cursor + 1U;
        }

        cursor++;
        scanned++;
    }

    return 0;
}

static const ldso_auxv_entry_t *ldso_find_auxv(const uint64_t *initial_stack)
{
    uint64_t argc;
    const uint64_t *cursor;

    if (!initial_stack) {
        return 0;
    }

    argc = initial_stack[0];
    cursor = ldso_stack_skip_argv(initial_stack + 1U, argc);
    if (!cursor) {
        return 0;
    }

    cursor = ldso_stack_skip_envp(cursor);
    if (!cursor) {
        return 0;
    }

    return (const ldso_auxv_entry_t *)cursor;
}

static uint64_t ldso_auxv_lookup(const ldso_auxv_entry_t *auxv, uint64_t key)
{
    uint64_t index;

    if (!auxv) {
        return 0U;
    }

    for (index = 0U; index < LDSO_AUXV_SCAN_LIMIT; index++) {
        const ldso_auxv_entry_t *entry = &auxv[index];
        if (entry->a_type == LDSO_AT_NULL) {
            break;
        }
        if (entry->a_type == key) {
            return entry->a_val;
        }
    }

    return 0U;
}

static int ldso_str_equal(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs) {
        return 0;
    }

    while (*lhs && *rhs) {
        if (*lhs != *rhs) {
            return 0;
        }
        lhs++;
        rhs++;
    }

    return *lhs == '\0' && *rhs == '\0';
}

static void ldso_builtin_touch(void)
{
}

static __attribute__((noreturn)) void ldso_builtin_exit(void)
{
    for (;;) {
        __asm__ volatile("pause");
    }
}

static int ldso_stage0_resolve_symbol(const char *name, uint64_t *address, void *context)
{
    (void)context;

    if (!name || !address) {
        return 0;
    }

    if (ldso_str_equal(name, "gnuos_libc_stub_touch")) {
        *address = (uint64_t)(uintptr_t)ldso_builtin_touch;
        return 1;
    }

    if (ldso_str_equal(name, "_exit")) {
        *address = (uint64_t)(uintptr_t)ldso_builtin_exit;
        return 1;
    }

    return 0;
}

static uint64_t ldso_compute_load_bias(const ldso_elf_layout_t *layout, uint64_t at_phdr)
{
    if (!layout || !layout->phdr_segment) {
        return 0U;
    }

    if (at_phdr < layout->phdr_segment->p_vaddr) {
        return 0U;
    }

    return at_phdr - layout->phdr_segment->p_vaddr;
}

static void ldso_stage0_state_reset(void)
{
    g_ldso_stage0_state.ready = 0U;
    g_ldso_stage0_state.at_base = 0U;
    g_ldso_stage0_state.at_entry = 0U;
    g_ldso_stage0_state.at_phdr = 0U;
    g_ldso_stage0_state.at_phent = 0U;
    g_ldso_stage0_state.at_phnum = 0U;
    g_ldso_stage0_state.load_bias = 0U;
    g_ldso_stage0_state.load_count = 0U;
    g_ldso_stage0_state.has_dynamic_segment = 0U;
    g_ldso_stage0_state.has_gnu_relro = 0U;
    g_ldso_stage0_state.dynamic_ready = 0U;
    g_ldso_stage0_state.relocations_attempted = 0U;
    g_ldso_stage0_state.relocations_applied = 0U;
    g_ldso_stage0_state.relocations_unresolved = 0U;
    g_ldso_stage0_state.relocations_unsupported = 0U;
}

void ldso_stage0_bootstrap(const uint64_t *initial_stack)
{
    const ldso_auxv_entry_t *auxv;
    ldso_elf_layout_t layout;
    ldso_elf_dynamic_info_t dynamic_info;
    ldso_elf_reloc_result_t reloc_result;
    uint64_t load_bias;
    int parsed;

    ldso_stage0_state_reset();

    auxv = ldso_find_auxv(initial_stack);
    if (!auxv) {
        return;
    }

    g_ldso_stage0_state.at_base = ldso_auxv_lookup(auxv, LDSO_AT_BASE);
    g_ldso_stage0_state.at_entry = ldso_auxv_lookup(auxv, LDSO_AT_ENTRY);
    g_ldso_stage0_state.at_phdr = ldso_auxv_lookup(auxv, LDSO_AT_PHDR);
    g_ldso_stage0_state.at_phent = ldso_auxv_lookup(auxv, LDSO_AT_PHENT);
    g_ldso_stage0_state.at_phnum = ldso_auxv_lookup(auxv, LDSO_AT_PHNUM);

    if (g_ldso_stage0_state.at_phdr == 0U || g_ldso_stage0_state.at_phnum == 0U) {
        return;
    }

    parsed = ldso_elf_classify_phdrs(
        (const ldso_elf_phdr_t *)(uintptr_t)g_ldso_stage0_state.at_phdr,
        g_ldso_stage0_state.at_phnum,
        g_ldso_stage0_state.at_phent,
        &layout);
    if (parsed != 0) {
        return;
    }

    load_bias = ldso_compute_load_bias(&layout, g_ldso_stage0_state.at_phdr);
    g_ldso_stage0_state.load_bias = load_bias;
    g_ldso_stage0_state.load_count = layout.load_count;
    g_ldso_stage0_state.has_dynamic_segment = layout.dynamic_segment != 0;
    g_ldso_stage0_state.has_gnu_relro = layout.relro_segment != 0;

    if (ldso_elf_parse_dynamic(&layout, load_bias, &dynamic_info) == 0) {
        g_ldso_stage0_state.dynamic_ready = 1U;
        g_ldso_stage0_state.relocations_attempted = 1U;

        if (ldso_elf_apply_relocations(
                &dynamic_info,
                load_bias,
                ldso_stage0_resolve_symbol,
                0,
                &reloc_result) == 0) {
            g_ldso_stage0_state.relocations_applied = reloc_result.applied_count;
            g_ldso_stage0_state.relocations_unresolved = reloc_result.unresolved_count;
            g_ldso_stage0_state.relocations_unsupported = reloc_result.unsupported_count;
        }
    }

    g_ldso_stage0_state.ready = 1U;
}
