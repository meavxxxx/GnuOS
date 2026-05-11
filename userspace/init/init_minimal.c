#include <gnuos/target.h>

#ifdef GNUOS_DYNAMIC_SMOKE
#include <execinfo.h>
#include <gnuos/tls.h>
#include <link.h>

extern void gnuos_libc_stub_touch(void);

__attribute__((constructor)) static void gnuos_dynamic_ctor(void)
{
    gnuos_libc_stub_touch();
}

static int gnuos_dl_phdr_cb(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)info;
    (void)size;
    (void)data;
    return 0;
}
#endif

int main(int argc, char **argv, char **envp)
{
    (void)GNUOS_TARGET_TRIPLET;
    (void)argc;
    (void)argv;
    (void)envp;
#ifdef GNUOS_DYNAMIC_SMOKE
    void *frames[8];
    gnuos_tls_index_t tls_index;
    void *tls_base;

    gnuos_libc_stub_touch();
    tls_base = __gnuos_get_tls_base();
    (void)__gnuos_set_tls_base(tls_base);
    tls_index.ti_module = 1UL;
    tls_index.ti_offset = 0UL;
    (void)__tls_get_addr(&tls_index);
    (void)backtrace(frames, 8);
    (void)dl_iterate_phdr(gnuos_dl_phdr_cb, 0);
#endif
    return 0;
}
