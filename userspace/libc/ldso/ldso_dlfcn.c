#include "ldso_dlfcn.h"

#define LDSO_DLFCN_MAX_OBJECTS 8U
#define LDSO_DLFCN_MAX_ERROR_LEN 160U
#define LDSO_DLFCN_DYNAMIC_SCAN_FALLBACK 256U

#define LDSO_DLFCN_OBJECT_KIND_NONE 0U
#define LDSO_DLFCN_OBJECT_KIND_DYNAMIC 1U
#define LDSO_DLFCN_OBJECT_KIND_BUILTIN 2U

typedef struct {
    uint8_t in_use;
    uint8_t kind;
    const char *name;
    uint64_t refcount;
    const ldso_elf_dynamic_info_t *dynamic_info;
    uint64_t load_bias;
    const ldso_dlfcn_builtin_symbol_t *builtin_symbols;
    uint64_t builtin_symbol_count;
} ldso_dlfcn_object_t;

static ldso_dlfcn_object_t g_ldso_dlfcn_objects[LDSO_DLFCN_MAX_OBJECTS];
static uint8_t g_ldso_dlfcn_initialized;
static uint8_t g_ldso_dlfcn_error_ready;
static char g_ldso_dlfcn_error[LDSO_DLFCN_MAX_ERROR_LEN];

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

static void ldso_error_clear(void)
{
    g_ldso_dlfcn_error_ready = 0U;
    g_ldso_dlfcn_error[0] = '\0';
}

static void ldso_error_set(const char *message)
{
    uint64_t index;

    if (!message) {
        ldso_error_clear();
        return;
    }

    for (index = 0U; index < (LDSO_DLFCN_MAX_ERROR_LEN - 1U) && message[index] != '\0'; index++) {
        g_ldso_dlfcn_error[index] = message[index];
    }
    g_ldso_dlfcn_error[index] = '\0';
    g_ldso_dlfcn_error_ready = 1U;
}

static void ldso_objects_reset(void)
{
    uint64_t index;

    for (index = 0U; index < LDSO_DLFCN_MAX_OBJECTS; index++) {
        g_ldso_dlfcn_objects[index].in_use = 0U;
        g_ldso_dlfcn_objects[index].kind = LDSO_DLFCN_OBJECT_KIND_NONE;
        g_ldso_dlfcn_objects[index].name = 0;
        g_ldso_dlfcn_objects[index].refcount = 0U;
        g_ldso_dlfcn_objects[index].dynamic_info = 0;
        g_ldso_dlfcn_objects[index].load_bias = 0U;
        g_ldso_dlfcn_objects[index].builtin_symbols = 0;
        g_ldso_dlfcn_objects[index].builtin_symbol_count = 0U;
    }
}

static ldso_dlfcn_object_t *ldso_object_find_free_slot(void)
{
    uint64_t index;

    for (index = 0U; index < LDSO_DLFCN_MAX_OBJECTS; index++) {
        if (!g_ldso_dlfcn_objects[index].in_use) {
            return &g_ldso_dlfcn_objects[index];
        }
    }

    return 0;
}

static ldso_dlfcn_object_t *ldso_object_find_by_name(const char *name)
{
    uint64_t index;

    if (!name) {
        return 0;
    }

    for (index = 0U; index < LDSO_DLFCN_MAX_OBJECTS; index++) {
        if (!g_ldso_dlfcn_objects[index].in_use) {
            continue;
        }

        if (ldso_str_equal(g_ldso_dlfcn_objects[index].name, name)) {
            return &g_ldso_dlfcn_objects[index];
        }
    }

    return 0;
}

static int ldso_object_handle_valid(const ldso_dlfcn_object_t *object)
{
    if (!object) {
        return 0;
    }
    if (object < &g_ldso_dlfcn_objects[0] || object >= &g_ldso_dlfcn_objects[LDSO_DLFCN_MAX_OBJECTS]) {
        return 0;
    }

    return object->in_use != 0U;
}

static int ldso_dynamic_object_resolve_symbol(
    const ldso_dlfcn_object_t *object,
    const char *symbol_name,
    uint64_t *address_out)
{
    uint64_t index;
    uint64_t symbol_count = 0U;
    const ldso_elf_dynamic_info_t *dynamic_info;

    if (!object || !object->dynamic_info || !symbol_name || !address_out) {
        return 0;
    }

    dynamic_info = object->dynamic_info;
    if (!dynamic_info->symtab || !dynamic_info->strtab || dynamic_info->strsz == 0U) {
        return 0;
    }

    symbol_count = dynamic_info->hash_nchain;
    if (symbol_count == 0U) {
        symbol_count = LDSO_DLFCN_DYNAMIC_SCAN_FALLBACK;
    }

    for (index = 1U; index < symbol_count; index++) {
        const ldso_elf_sym_t *symbol = &dynamic_info->symtab[index];
        const char *candidate = 0;

        if (symbol->st_name >= dynamic_info->strsz) {
            continue;
        }
        candidate = dynamic_info->strtab + symbol->st_name;
        if (!ldso_str_equal(candidate, symbol_name)) {
            continue;
        }
        if (symbol->st_shndx == LDSO_SHN_UNDEF) {
            return 0;
        }

        *address_out = object->load_bias + symbol->st_value;
        return 1;
    }

    return 0;
}

static int ldso_builtin_object_resolve_symbol(
    const ldso_dlfcn_object_t *object,
    const char *symbol_name,
    uint64_t *address_out)
{
    uint64_t index;

    if (!object || !object->builtin_symbols || !symbol_name || !address_out) {
        return 0;
    }

    for (index = 0U; index < object->builtin_symbol_count; index++) {
        const ldso_dlfcn_builtin_symbol_t *symbol = &object->builtin_symbols[index];
        if (ldso_str_equal(symbol->name, symbol_name)) {
            *address_out = symbol->address;
            return 1;
        }
    }

    return 0;
}

static int ldso_object_resolve_symbol(
    const ldso_dlfcn_object_t *object,
    const char *symbol_name,
    uint64_t *address_out)
{
    if (!object || !symbol_name || !address_out) {
        return 0;
    }

    if (object->kind == LDSO_DLFCN_OBJECT_KIND_DYNAMIC) {
        return ldso_dynamic_object_resolve_symbol(object, symbol_name, address_out);
    }
    if (object->kind == LDSO_DLFCN_OBJECT_KIND_BUILTIN) {
        return ldso_builtin_object_resolve_symbol(object, symbol_name, address_out);
    }

    return 0;
}

void ldso_dlfcn_init(const ldso_elf_dynamic_info_t *main_dynamic_info, uint64_t main_load_bias)
{
    ldso_dlfcn_object_t *main_object;

    g_ldso_dlfcn_initialized = 1U;
    ldso_error_clear();
    ldso_objects_reset();

    if (!main_dynamic_info) {
        return;
    }

    main_object = ldso_object_find_free_slot();
    if (!main_object) {
        return;
    }

    main_object->in_use = 1U;
    main_object->kind = LDSO_DLFCN_OBJECT_KIND_DYNAMIC;
    main_object->name = "<main>";
    main_object->refcount = 1U;
    main_object->dynamic_info = main_dynamic_info;
    main_object->load_bias = main_load_bias;
}

int ldso_dlfcn_register_builtin_object(
    const char *name,
    const ldso_dlfcn_builtin_symbol_t *symbols,
    uint64_t symbol_count)
{
    ldso_dlfcn_object_t *object;

    if (!g_ldso_dlfcn_initialized) {
        return -1;
    }
    if (!name || !symbols || symbol_count == 0U) {
        return -1;
    }

    object = ldso_object_find_by_name(name);
    if (!object) {
        object = ldso_object_find_free_slot();
    }
    if (!object) {
        return -1;
    }

    object->in_use = 1U;
    object->kind = LDSO_DLFCN_OBJECT_KIND_BUILTIN;
    object->name = name;
    object->refcount = 1U;
    object->dynamic_info = 0;
    object->load_bias = 0U;
    object->builtin_symbols = symbols;
    object->builtin_symbol_count = symbol_count;
    return 0;
}

void *ldso_dlfcn_resolve_global(const char *symbol_name)
{
    uint64_t index;

    if (!g_ldso_dlfcn_initialized || !symbol_name) {
        return 0;
    }

    for (index = 0U; index < LDSO_DLFCN_MAX_OBJECTS; index++) {
        uint64_t address = 0U;
        if (!g_ldso_dlfcn_objects[index].in_use) {
            continue;
        }
        if (ldso_object_resolve_symbol(&g_ldso_dlfcn_objects[index], symbol_name, &address)) {
            return (void *)(uintptr_t)address;
        }
    }

    return 0;
}

void *ldso_dlopen(const char *file, int mode)
{
    ldso_dlfcn_object_t *object;

    (void)mode;

    if (!g_ldso_dlfcn_initialized) {
        ldso_error_set("ldso dlfcn state is not initialized");
        return 0;
    }

    if (!file) {
        object = ldso_object_find_by_name("<main>");
        if (!object) {
            ldso_error_set("main object is not registered");
            return 0;
        }

        object->refcount++;
        ldso_error_clear();
        return object;
    }

    object = ldso_object_find_by_name(file);
    if (!object) {
        ldso_error_set("dlopen in stage0 supports only pre-registered objects");
        return 0;
    }

    object->refcount++;
    ldso_error_clear();
    return object;
}

void *ldso_dlsym(void *handle, const char *symbol_name)
{
    uint64_t address = 0U;
    const ldso_dlfcn_object_t *object = 0;

    if (!g_ldso_dlfcn_initialized || !symbol_name) {
        ldso_error_set("dlsym invalid arguments");
        return 0;
    }

    if (handle == LDSO_RTLD_NEXT) {
        ldso_error_set("RTLD_NEXT is not implemented in stage0");
        return 0;
    }

    if (handle == LDSO_RTLD_DEFAULT || handle == 0) {
        void *resolved = ldso_dlfcn_resolve_global(symbol_name);
        if (!resolved) {
            ldso_error_set("symbol not found");
            return 0;
        }
        ldso_error_clear();
        return resolved;
    }

    object = (const ldso_dlfcn_object_t *)handle;
    if (!ldso_object_handle_valid(object)) {
        ldso_error_set("dlsym invalid handle");
        return 0;
    }

    if (ldso_object_resolve_symbol(object, symbol_name, &address)) {
        ldso_error_clear();
        return (void *)(uintptr_t)address;
    }

    ldso_error_set("symbol not found in object");
    return 0;
}

int ldso_dlclose(void *handle)
{
    ldso_dlfcn_object_t *object = (ldso_dlfcn_object_t *)handle;

    if (!g_ldso_dlfcn_initialized || !ldso_object_handle_valid(object)) {
        ldso_error_set("dlclose invalid handle");
        return -1;
    }
    if (object->refcount == 0U) {
        ldso_error_set("dlclose invalid object state");
        return -1;
    }

    object->refcount--;
    ldso_error_clear();
    return 0;
}

char *ldso_dlerror(void)
{
    static char empty[] = "";

    if (!g_ldso_dlfcn_error_ready) {
        return 0;
    }

    g_ldso_dlfcn_error_ready = 0U;
    if (g_ldso_dlfcn_error[0] == '\0') {
        return empty;
    }
    return g_ldso_dlfcn_error;
}

void *dlopen(const char *file, int mode)
{
    return ldso_dlopen(file, mode);
}

void *dlsym(void *handle, const char *symbol_name)
{
    return ldso_dlsym(handle, symbol_name);
}

int dlclose(void *handle)
{
    return ldso_dlclose(handle);
}

char *dlerror(void)
{
    return ldso_dlerror();
}
