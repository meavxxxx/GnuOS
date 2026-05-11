#include <gnuos/target.h>

#ifdef GNUOS_DYNAMIC_SMOKE
extern void gnuos_libc_stub_touch(void);
#endif

int main(int argc, char **argv, char **envp)
{
    (void)GNUOS_TARGET_TRIPLET;
    (void)argc;
    (void)argv;
    (void)envp;
#ifdef GNUOS_DYNAMIC_SMOKE
    gnuos_libc_stub_touch();
#endif
    return 0;
}
