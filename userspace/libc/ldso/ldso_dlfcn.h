#ifndef GNUOS_LDSO_DLFCN_H
#define GNUOS_LDSO_DLFCN_H

#include <stdint.h>

#include "ldso_elf.h"

#define LDSO_RTLD_LAZY 0x00001
#define LDSO_RTLD_NOW 0x00002
#define LDSO_RTLD_LOCAL 0x00000
#define LDSO_RTLD_GLOBAL 0x00100

#define LDSO_RTLD_DEFAULT ((void *)0)
#define LDSO_RTLD_NEXT ((void *)(intptr_t)-1)

typedef struct {
    const char *name;
    uint64_t address;
} ldso_dlfcn_builtin_symbol_t;

void ldso_dlfcn_init(const ldso_elf_dynamic_info_t *main_dynamic_info, uint64_t main_load_bias);
int ldso_dlfcn_register_builtin_object(
    const char *name,
    const ldso_dlfcn_builtin_symbol_t *symbols,
    uint64_t symbol_count);
void *ldso_dlfcn_resolve_global(const char *symbol_name);

void *ldso_dlopen(const char *file, int mode);
void *ldso_dlsym(void *handle, const char *symbol_name);
int ldso_dlclose(void *handle);
char *ldso_dlerror(void);

#endif
