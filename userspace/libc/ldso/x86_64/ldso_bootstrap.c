#include <stdint.h>

#include <execinfo.h>
#include <gnuos/tls.h>
#include <link.h>
#include <pthread.h>
#include <semaphore.h>

#include "ldso_dlfcn.h"
#include "ldso_elf.h"

#define LDSO_AUXV_SCAN_LIMIT 256U
#define LDSO_STACK_SCAN_LIMIT 4096U

#define LDSO_AT_NULL 0U
#define LDSO_AT_PHDR 3U
#define LDSO_AT_PHENT 4U
#define LDSO_AT_PHNUM 5U
#define LDSO_AT_BASE 7U
#define LDSO_AT_ENTRY 9U
#define LDSO_LD_PRELOAD_KEY "LD_PRELOAD="
#define LDSO_LD_PRELOAD_KEY_LEN 11U
#define LDSO_PRELOAD_TOKEN_MAX 128U
#define LDSO_STAGE0_BUILTIN_SYMBOL_COUNT 27U

#define GNUOS_PTHREAD_ENOSYS 38
#define GNUOS_PTHREAD_EINVAL 22
#define GNUOS_PTHREAD_MAIN_THREAD ((pthread_t)1UL)
#define GNUOS_PTHREAD_STACK_MIN 16384UL
#define GNUOS_PTHREAD_STACK_DEFAULT (2UL * 1024UL * 1024UL)
#define GNUOS_SEM_EAGAIN 11
#define GNUOS_SEM_EOVERFLOW 75
#define GNUOS_SEM_VALUE_MAX 0x7FFFFFFFU

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
    uint64_t init_sequence_attempted;
    uint64_t init_sequence_completed;
    uint64_t dlfcn_ready;
    uint64_t builtin_object_registered;
    uint64_t ld_preload_seen;
    uint64_t ld_preload_attempted;
    uint64_t ld_preload_loaded;
    uint64_t ld_preload_failed;
} ldso_stage0_state_t;

volatile ldso_stage0_state_t g_ldso_stage0_state;
static ldso_dlfcn_builtin_symbol_t g_ldso_stage0_builtin_symbols[LDSO_STAGE0_BUILTIN_SYMBOL_COUNT];
static uintptr_t g_ldso_stage0_tls_storage[64];
static uintptr_t g_ldso_stage0_tls_base;

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

static int ldso_str_starts_with(const char *value, const char *prefix, uint64_t prefix_len)
{
    uint64_t index;

    if (!value || !prefix) {
        return 0;
    }

    for (index = 0U; index < prefix_len; index++) {
        if (value[index] != prefix[index]) {
            return 0;
        }
    }

    return 1;
}

static int ldso_is_preload_delimiter(char c)
{
    return c == ' ' || c == ':';
}

static const char *const *ldso_find_envp(const uint64_t *initial_stack)
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

    return (const char *const *)cursor;
}

static const char *ldso_find_env_value(const uint64_t *initial_stack, const char *key, uint64_t key_len)
{
    uint64_t index = 0U;
    const char *const *envp = ldso_find_envp(initial_stack);

    if (!envp || !key || key_len == 0U) {
        return 0;
    }

    while (envp[index]) {
        const char *entry = envp[index];
        if (ldso_str_starts_with(entry, key, key_len)) {
            return entry + key_len;
        }
        index++;
    }

    return 0;
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

int __gnuos_set_tls_base(void *base)
{
    g_ldso_stage0_tls_base = (uintptr_t)base;
    return 0;
}

void *__gnuos_get_tls_base(void)
{
    return (void *)g_ldso_stage0_tls_base;
}

void *__tls_get_addr(gnuos_tls_index_t *index)
{
    if (!index) {
        return 0;
    }

    return (void *)(g_ldso_stage0_tls_base + index->ti_offset);
}

int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg)
{
    (void)attr;
    (void)start_routine;
    (void)arg;

    if (thread) {
        *thread = 0UL;
    }

    return GNUOS_PTHREAD_ENOSYS;
}

pthread_t pthread_self(void)
{
    return GNUOS_PTHREAD_MAIN_THREAD;
}

int pthread_equal(pthread_t thread1, pthread_t thread2)
{
    return thread1 == thread2;
}

int pthread_join(pthread_t thread, void **retval)
{
    (void)thread;
    if (retval) {
        *retval = 0;
    }

    return GNUOS_PTHREAD_ENOSYS;
}

int pthread_detach(pthread_t thread)
{
    (void)thread;
    return GNUOS_PTHREAD_ENOSYS;
}

int pthread_attr_init(pthread_attr_t *attr)
{
    if (!attr) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__opaque = 0UL;
    attr->__stack_size = GNUOS_PTHREAD_STACK_DEFAULT;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (!attr) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__opaque = 0UL;
    attr->__stack_size = 0UL;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if (!attr || stacksize < GNUOS_PTHREAD_STACK_MIN) {
        return GNUOS_PTHREAD_EINVAL;
    }

    attr->__stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (!attr || !stacksize) {
        return GNUOS_PTHREAD_EINVAL;
    }

    *stacksize = attr->__stack_size;
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    if (!sem) {
        return GNUOS_PTHREAD_EINVAL;
    }
    if (pshared != 0) {
        return GNUOS_PTHREAD_ENOSYS;
    }

    sem->__value = value;
    return 0;
}

int sem_destroy(sem_t *sem)
{
    if (!sem) {
        return GNUOS_PTHREAD_EINVAL;
    }

    sem->__value = 0U;
    return 0;
}

int sem_wait(sem_t *sem)
{
    if (!sem) {
        return GNUOS_PTHREAD_EINVAL;
    }
    if (sem->__value == 0U) {
        return GNUOS_SEM_EAGAIN;
    }

    sem->__value--;
    return 0;
}

int sem_trywait(sem_t *sem)
{
    return sem_wait(sem);
}

int sem_post(sem_t *sem)
{
    if (!sem) {
        return GNUOS_PTHREAD_EINVAL;
    }
    if (sem->__value == GNUOS_SEM_VALUE_MAX) {
        return GNUOS_SEM_EOVERFLOW;
    }

    sem->__value++;
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval)
{
    if (!sem || !sval) {
        return GNUOS_PTHREAD_EINVAL;
    }

    *sval = (int)sem->__value;
    return 0;
}

void __gnuos_store_startup(unsigned long argc, char **argv, char **envp)
{
    (void)argc;
    (void)argv;
    (void)envp;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    struct dl_phdr_info info;

    if (!callback) {
        return -1;
    }
    if (g_ldso_stage0_state.at_phdr == 0U ||
        g_ldso_stage0_state.at_phnum == 0U ||
        g_ldso_stage0_state.at_phent != sizeof(ldso_elf_phdr_t)) {
        return -1;
    }

    info.dlpi_addr = g_ldso_stage0_state.load_bias;
    info.dlpi_name = "";
    info.dlpi_phdr = (const Elf64_Phdr *)(uintptr_t)g_ldso_stage0_state.at_phdr;
    info.dlpi_phnum = (Elf64_Half)g_ldso_stage0_state.at_phnum;
    return callback(&info, sizeof(info), data);
}

int backtrace(void **buffer, int size)
{
    void **frame;
    int count = 0;

    if (!buffer || size <= 0) {
        return 0;
    }

    frame = (void **)__builtin_frame_address(0);
    while (frame && count < size) {
        void *return_address;
        void **next_frame;
        uint64_t frame_addr;
        uint64_t next_addr;

        return_address = frame[1];
        if (!return_address) {
            break;
        }
        buffer[count++] = return_address;

        next_frame = (void **)frame[0];
        if (!next_frame) {
            break;
        }

        frame_addr = (uint64_t)(uintptr_t)frame;
        next_addr = (uint64_t)(uintptr_t)next_frame;
        if (next_addr <= frame_addr) {
            break;
        }
        if ((next_addr - frame_addr) > (1ULL << 20U)) {
            break;
        }

        frame = next_frame;
    }

    return count;
}

static int ldso_stage0_resolve_symbol(const char *name, uint64_t *address, void *context)
{
    void *resolved = 0;

    (void)context;

    if (!name || !address) {
        return 0;
    }

    resolved = ldso_dlfcn_resolve_global(name);
    if (!resolved) {
        return 0;
    }

    *address = (uint64_t)(uintptr_t)resolved;
    return 1;
}

static int ldso_stage0_register_builtin_symbols(void)
{
    int registered_primary;
    int registered_alias;

    g_ldso_stage0_builtin_symbols[0].name = "gnuos_libc_stub_touch";
    g_ldso_stage0_builtin_symbols[0].address = (uint64_t)(uintptr_t)ldso_builtin_touch;
    g_ldso_stage0_builtin_symbols[1].name = "_exit";
    g_ldso_stage0_builtin_symbols[1].address = (uint64_t)(uintptr_t)ldso_builtin_exit;
    g_ldso_stage0_builtin_symbols[2].name = "dlopen";
    g_ldso_stage0_builtin_symbols[2].address = (uint64_t)(uintptr_t)ldso_dlopen;
    g_ldso_stage0_builtin_symbols[3].name = "dlsym";
    g_ldso_stage0_builtin_symbols[3].address = (uint64_t)(uintptr_t)ldso_dlsym;
    g_ldso_stage0_builtin_symbols[4].name = "dlclose";
    g_ldso_stage0_builtin_symbols[4].address = (uint64_t)(uintptr_t)ldso_dlclose;
    g_ldso_stage0_builtin_symbols[5].name = "dlerror";
    g_ldso_stage0_builtin_symbols[5].address = (uint64_t)(uintptr_t)ldso_dlerror;
    g_ldso_stage0_builtin_symbols[6].name = "__gnuos_store_startup";
    g_ldso_stage0_builtin_symbols[6].address = (uint64_t)(uintptr_t)__gnuos_store_startup;
    g_ldso_stage0_builtin_symbols[7].name = "dl_iterate_phdr";
    g_ldso_stage0_builtin_symbols[7].address = (uint64_t)(uintptr_t)dl_iterate_phdr;
    g_ldso_stage0_builtin_symbols[8].name = "backtrace";
    g_ldso_stage0_builtin_symbols[8].address = (uint64_t)(uintptr_t)backtrace;
    g_ldso_stage0_builtin_symbols[9].name = "__gnuos_set_tls_base";
    g_ldso_stage0_builtin_symbols[9].address = (uint64_t)(uintptr_t)__gnuos_set_tls_base;
    g_ldso_stage0_builtin_symbols[10].name = "__gnuos_get_tls_base";
    g_ldso_stage0_builtin_symbols[10].address = (uint64_t)(uintptr_t)__gnuos_get_tls_base;
    g_ldso_stage0_builtin_symbols[11].name = "__tls_get_addr";
    g_ldso_stage0_builtin_symbols[11].address = (uint64_t)(uintptr_t)__tls_get_addr;
    g_ldso_stage0_builtin_symbols[12].name = "pthread_create";
    g_ldso_stage0_builtin_symbols[12].address = (uint64_t)(uintptr_t)pthread_create;
    g_ldso_stage0_builtin_symbols[13].name = "pthread_self";
    g_ldso_stage0_builtin_symbols[13].address = (uint64_t)(uintptr_t)pthread_self;
    g_ldso_stage0_builtin_symbols[14].name = "pthread_equal";
    g_ldso_stage0_builtin_symbols[14].address = (uint64_t)(uintptr_t)pthread_equal;
    g_ldso_stage0_builtin_symbols[15].name = "pthread_join";
    g_ldso_stage0_builtin_symbols[15].address = (uint64_t)(uintptr_t)pthread_join;
    g_ldso_stage0_builtin_symbols[16].name = "pthread_detach";
    g_ldso_stage0_builtin_symbols[16].address = (uint64_t)(uintptr_t)pthread_detach;
    g_ldso_stage0_builtin_symbols[17].name = "pthread_attr_init";
    g_ldso_stage0_builtin_symbols[17].address = (uint64_t)(uintptr_t)pthread_attr_init;
    g_ldso_stage0_builtin_symbols[18].name = "pthread_attr_destroy";
    g_ldso_stage0_builtin_symbols[18].address = (uint64_t)(uintptr_t)pthread_attr_destroy;
    g_ldso_stage0_builtin_symbols[19].name = "pthread_attr_setstacksize";
    g_ldso_stage0_builtin_symbols[19].address = (uint64_t)(uintptr_t)pthread_attr_setstacksize;
    g_ldso_stage0_builtin_symbols[20].name = "pthread_attr_getstacksize";
    g_ldso_stage0_builtin_symbols[20].address = (uint64_t)(uintptr_t)pthread_attr_getstacksize;
    g_ldso_stage0_builtin_symbols[21].name = "sem_init";
    g_ldso_stage0_builtin_symbols[21].address = (uint64_t)(uintptr_t)sem_init;
    g_ldso_stage0_builtin_symbols[22].name = "sem_destroy";
    g_ldso_stage0_builtin_symbols[22].address = (uint64_t)(uintptr_t)sem_destroy;
    g_ldso_stage0_builtin_symbols[23].name = "sem_wait";
    g_ldso_stage0_builtin_symbols[23].address = (uint64_t)(uintptr_t)sem_wait;
    g_ldso_stage0_builtin_symbols[24].name = "sem_trywait";
    g_ldso_stage0_builtin_symbols[24].address = (uint64_t)(uintptr_t)sem_trywait;
    g_ldso_stage0_builtin_symbols[25].name = "sem_post";
    g_ldso_stage0_builtin_symbols[25].address = (uint64_t)(uintptr_t)sem_post;
    g_ldso_stage0_builtin_symbols[26].name = "sem_getvalue";
    g_ldso_stage0_builtin_symbols[26].address = (uint64_t)(uintptr_t)sem_getvalue;

    registered_primary = ldso_dlfcn_register_builtin_object(
        "stage0-builtins",
        g_ldso_stage0_builtin_symbols,
        LDSO_STAGE0_BUILTIN_SYMBOL_COUNT);
    registered_alias = ldso_dlfcn_register_builtin_object(
        "libc.so.6",
        g_ldso_stage0_builtin_symbols,
        LDSO_STAGE0_BUILTIN_SYMBOL_COUNT);

    if (registered_primary != 0 || registered_alias != 0) {
        return -1;
    }

    return 0;
}

static void ldso_stage0_apply_ld_preload(const uint64_t *initial_stack)
{
    const char *value;
    const char *cursor;

    value = ldso_find_env_value(initial_stack, LDSO_LD_PRELOAD_KEY, LDSO_LD_PRELOAD_KEY_LEN);
    if (!value || value[0] == '\0') {
        return;
    }

    g_ldso_stage0_state.ld_preload_seen = 1U;
    cursor = value;
    while (*cursor) {
        char token[LDSO_PRELOAD_TOKEN_MAX];
        uint64_t token_len = 0U;

        while (*cursor && ldso_is_preload_delimiter(*cursor)) {
            cursor++;
        }
        if (!*cursor) {
            break;
        }

        while (*cursor && !ldso_is_preload_delimiter(*cursor)) {
            if (token_len < (LDSO_PRELOAD_TOKEN_MAX - 1U)) {
                token[token_len++] = *cursor;
            }
            cursor++;
        }
        token[token_len] = '\0';
        if (token_len == 0U) {
            continue;
        }

        g_ldso_stage0_state.ld_preload_attempted++;
        if (ldso_dlopen(token, LDSO_RTLD_NOW | LDSO_RTLD_GLOBAL)) {
            g_ldso_stage0_state.ld_preload_loaded++;
        } else {
            g_ldso_stage0_state.ld_preload_failed++;
        }
    }
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
    g_ldso_stage0_tls_base = (uintptr_t)&g_ldso_stage0_tls_storage[0];

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
    g_ldso_stage0_state.init_sequence_attempted = 0U;
    g_ldso_stage0_state.init_sequence_completed = 0U;
    g_ldso_stage0_state.dlfcn_ready = 0U;
    g_ldso_stage0_state.builtin_object_registered = 0U;
    g_ldso_stage0_state.ld_preload_seen = 0U;
    g_ldso_stage0_state.ld_preload_attempted = 0U;
    g_ldso_stage0_state.ld_preload_loaded = 0U;
    g_ldso_stage0_state.ld_preload_failed = 0U;
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
        ldso_dlfcn_init(&dynamic_info, load_bias);
        g_ldso_stage0_state.dlfcn_ready = 1U;
        if (ldso_stage0_register_builtin_symbols() == 0) {
            g_ldso_stage0_state.builtin_object_registered = 1U;
            ldso_stage0_apply_ld_preload(initial_stack);
        }

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

            g_ldso_stage0_state.init_sequence_attempted = 1U;
            if (ldso_elf_run_init_sequence(&dynamic_info) == 0) {
                g_ldso_stage0_state.init_sequence_completed = 1U;
            }
        }
    }

    g_ldso_stage0_state.ready = 1U;
}
