#include <stddef.h>
#include <stdint.h>

#include "ldso_dlfcn.h"

static int g_fuzz_symbol;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static const ldso_dlfcn_builtin_symbol_t symbols[] = {
        {"fuzz_symbol", (uint64_t)(uintptr_t)&g_fuzz_symbol},
    };
    char name[64];
    size_t i = 0U;
    void *handle;
    void *resolved;

    if (!data) {
        return 0;
    }

    ldso_dlfcn_init(0, 0U);
    (void)ldso_dlfcn_register_builtin_object("libfuzz.so", symbols, 1U);

    while (i < size && i < (sizeof(name) - 1U)) {
        name[i] = (char)data[i];
        i++;
    }
    name[i] = '\0';

    handle = ldso_dlopen("libfuzz.so", LDSO_RTLD_NOW);
    if (size > 0U && (data[0] & 1U) != 0U) {
        handle = LDSO_RTLD_DEFAULT;
    }

    resolved = ldso_dlsym(handle, name);
    (void)resolved;
    (void)ldso_dlerror();

    if (handle != 0 && handle != LDSO_RTLD_DEFAULT && handle != LDSO_RTLD_NEXT) {
        (void)ldso_dlclose(handle);
    }

    return 0;
}
