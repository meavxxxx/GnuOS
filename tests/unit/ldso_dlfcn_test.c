#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ldso_dlfcn.h"

static int g_counter = 0;

static void assert_true(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "assertion failed: %s\n", message);
        exit(1);
    }
}

static void reset_state(void)
{
    ldso_dlfcn_init(0, 0U);
}

int main(void)
{
    static const ldso_dlfcn_builtin_symbol_t symbols[] = {
        {"counter", (uint64_t)(uintptr_t)&g_counter},
    };
    void *handle;
    void *resolved;
    int rc;

    reset_state();

    rc = ldso_dlfcn_register_builtin_object("libtest.so", symbols, 1U);
    assert_true(rc == 0, "register builtin object");

    handle = ldso_dlopen("libtest.so", LDSO_RTLD_NOW);
    assert_true(handle != 0, "dlopen returns handle");

    resolved = ldso_dlsym(handle, "counter");
    assert_true(resolved == (void *)(uintptr_t)&g_counter, "dlsym resolves builtin symbol");

    resolved = ldso_dlsym(LDSO_RTLD_DEFAULT, "counter");
    assert_true(resolved == (void *)(uintptr_t)&g_counter, "global resolver works");

    rc = ldso_dlclose(handle);
    assert_true(rc == 0, "dlclose succeeds");

    resolved = ldso_dlsym(handle, "missing_symbol");
    assert_true(resolved == 0, "unknown symbol returns null");
    assert_true(ldso_dlerror() != 0, "unknown symbol sets dlerror");

    return 0;
}
