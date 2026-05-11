#include <execinfo.h>
#include <link.h>

#define GNUOS_AT_NULL 0UL
#define GNUOS_AT_PHDR 3UL
#define GNUOS_AT_PHENT 4UL
#define GNUOS_AT_PHNUM 5UL

#define GNUOS_PT_PHDR 6U

static char **g_startup_envp;

void gnuos_libc_stub_touch(void)
{
}

void __gnuos_store_startup(unsigned long argc, char **argv, char **envp)
{
    (void)argc;
    (void)argv;
    g_startup_envp = envp;
}

static const unsigned long *gnuos_startup_auxv(void)
{
    char **envp = g_startup_envp;

    if (!envp) {
        return 0;
    }

    while (*envp) {
        envp++;
    }

    return (const unsigned long *)(envp + 1);
}

static unsigned long gnuos_auxv_lookup(unsigned long key)
{
    const unsigned long *auxv = gnuos_startup_auxv();

    if (!auxv) {
        return 0UL;
    }

    while (auxv[0] != GNUOS_AT_NULL) {
        if (auxv[0] == key) {
            return auxv[1];
        }
        auxv += 2;
    }

    return 0UL;
}

static unsigned long gnuos_compute_dlpi_addr(const Elf64_Phdr *phdr, unsigned long phnum, unsigned long at_phdr)
{
    unsigned long index;

    if (!phdr || at_phdr == 0UL) {
        return 0UL;
    }

    for (index = 0UL; index < phnum; index++) {
        if (phdr[index].p_type == GNUOS_PT_PHDR && at_phdr >= phdr[index].p_vaddr) {
            return at_phdr - phdr[index].p_vaddr;
        }
    }

    return 0UL;
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    struct dl_phdr_info info;
    unsigned long at_phdr;
    unsigned long at_phnum;
    unsigned long at_phent;
    const Elf64_Phdr *phdr;

    if (!callback) {
        return -1;
    }

    at_phdr = gnuos_auxv_lookup(GNUOS_AT_PHDR);
    at_phnum = gnuos_auxv_lookup(GNUOS_AT_PHNUM);
    at_phent = gnuos_auxv_lookup(GNUOS_AT_PHENT);
    if (at_phdr == 0UL || at_phnum == 0UL || at_phent != sizeof(Elf64_Phdr)) {
        return -1;
    }

    phdr = (const Elf64_Phdr *)(unsigned long)at_phdr;
    info.dlpi_addr = gnuos_compute_dlpi_addr(phdr, at_phnum, at_phdr);
    info.dlpi_name = "";
    info.dlpi_phdr = phdr;
    info.dlpi_phnum = (Elf64_Half)at_phnum;

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
        unsigned long frame_addr;
        unsigned long next_addr;

        return_address = frame[1];
        if (!return_address) {
            break;
        }
        buffer[count++] = return_address;

        next_frame = (void **)frame[0];
        if (!next_frame) {
            break;
        }

        frame_addr = (unsigned long)frame;
        next_addr = (unsigned long)next_frame;
        if (next_addr <= frame_addr) {
            break;
        }
        if ((next_addr - frame_addr) > (1UL << 20)) {
            break;
        }

        frame = next_frame;
    }

    return count;
}

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    for (;;) {
        __asm__ volatile("pause");
    }
}
