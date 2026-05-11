void gnuos_libc_stub_touch(void)
{
}

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    for (;;) {
        __asm__ volatile("pause");
    }
}
