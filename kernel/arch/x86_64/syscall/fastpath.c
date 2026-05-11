#include <stdint.h>

#include <gnuos/msr.h>
#include <gnuos/printk.h>
#include <gnuos/syscall.h>

#define X86_MSR_EFER 0xC0000080U
#define X86_MSR_STAR 0xC0000081U
#define X86_MSR_LSTAR 0xC0000082U
#define X86_MSR_FMASK 0xC0000084U

#define X86_EFER_SCE (1ULL << 0U)

#define X86_GDT_KERNEL_CS 0x08ULL
/*
 * In 64-bit mode SYSRET computes selectors as:
 *   CS = STAR[63:48] + 16
 *   SS = STAR[63:48] + 8
 * We set STAR[63:48] = 0x10 so SYSRET targets CS=0x20, SS=0x18.
 */
#define X86_GDT_USER_STAR_BASE 0x10ULL

#define X86_RFLAGS_IF (1ULL << 9U)

#define X86_SYSCALL_STACK_SIZE 16384U

extern void x86_64_syscall_entry(void);

uint64_t g_x86_64_syscall_kernel_stack_top;
uint64_t g_x86_64_syscall_user_rsp_shadow;

static uint8_t g_x86_64_syscall_fastpath_ready;
static uint8_t g_x86_64_syscall_stack[X86_SYSCALL_STACK_SIZE] __attribute__((aligned(16)));

void x86_64_syscall_fastpath_init(void)
{
    uint64_t efer;
    uint64_t star;
    uint64_t lstar;

    g_x86_64_syscall_kernel_stack_top = (uint64_t)(uintptr_t)&g_x86_64_syscall_stack[0] +
        (uint64_t)X86_SYSCALL_STACK_SIZE;
    g_x86_64_syscall_user_rsp_shadow = 0U;

    star = (X86_GDT_USER_STAR_BASE << 48U) | (X86_GDT_KERNEL_CS << 32U);
    lstar = (uint64_t)(uintptr_t)x86_64_syscall_entry;

    msr_write(X86_MSR_STAR, star);
    msr_write(X86_MSR_LSTAR, lstar);
    msr_write(X86_MSR_FMASK, X86_RFLAGS_IF);

    efer = msr_read(X86_MSR_EFER);
    efer |= X86_EFER_SCE;
    msr_write(X86_MSR_EFER, efer);

    g_x86_64_syscall_fastpath_ready = 1U;
    kprintf("GNU OS: x86_64 syscall/sysret fast path armed.\n");
}

uint8_t x86_64_syscall_fastpath_ready(void)
{
    return g_x86_64_syscall_fastpath_ready;
}
