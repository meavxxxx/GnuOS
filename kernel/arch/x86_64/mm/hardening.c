#include <stddef.h>
#include <stdint.h>

#include <gnuos/msr.h>
#include <gnuos/serial.h>
#include <gnuos/x86_64_hardening.h>

#define X86_CPUID_LEAF_BASIC_INFO 0x0U
#define X86_CPUID_LEAF_EXT_INFO 0x80000000U
#define X86_CPUID_LEAF_EXT_FEATURES 0x80000001U
#define X86_CPUID_LEAF_STRUCT_FEATURES 0x7U

#define X86_CPUID_EXT_EDX_NX (1U << 20U)
#define X86_CPUID_STRUCT_EBX_SMEP (1U << 7U)
#define X86_CPUID_STRUCT_EBX_SMAP (1U << 20U)

#define X86_MSR_EFER 0xC0000080U
#define X86_EFER_NXE (1ULL << 11U)

#define X86_CR4_SMEP (1ULL << 20U)
#define X86_CR4_SMAP (1ULL << 21U)

static x86_64_hardening_state_t g_hardening_state;

static void cpuid_read(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *out_eax,
    uint32_t *out_ebx,
    uint32_t *out_ecx,
    uint32_t *out_edx)
{
    uint32_t eax = 0U;
    uint32_t ebx = 0U;
    uint32_t ecx = 0U;
    uint32_t edx = 0U;

    __asm__ volatile(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(subleaf));

    if (out_eax) {
        *out_eax = eax;
    }
    if (out_ebx) {
        *out_ebx = ebx;
    }
    if (out_ecx) {
        *out_ecx = ecx;
    }
    if (out_edx) {
        *out_edx = edx;
    }
}

void x86_64_hardening_init(void)
{
    uint32_t max_basic_leaf = 0U;
    uint32_t max_ext_leaf = 0U;
    uint32_t eax = 0U;
    uint32_t ebx = 0U;
    uint32_t ecx = 0U;
    uint32_t edx = 0U;
    uint64_t efer = 0U;
    uint64_t cr4 = 0U;

    g_hardening_state.nx_supported = 0U;
    g_hardening_state.nx_enabled = 0U;
    g_hardening_state.smep_supported = 0U;
    g_hardening_state.smep_enabled = 0U;
    g_hardening_state.smap_supported = 0U;
    g_hardening_state.smap_enabled = 0U;

    cpuid_read(X86_CPUID_LEAF_BASIC_INFO, 0U, &max_basic_leaf, NULL, NULL, NULL);
    cpuid_read(X86_CPUID_LEAF_EXT_INFO, 0U, &max_ext_leaf, NULL, NULL, NULL);

    if (max_ext_leaf >= X86_CPUID_LEAF_EXT_FEATURES) {
        cpuid_read(X86_CPUID_LEAF_EXT_FEATURES, 0U, &eax, &ebx, &ecx, &edx);
        (void)eax;
        (void)ebx;
        (void)ecx;
        g_hardening_state.nx_supported = ((edx & X86_CPUID_EXT_EDX_NX) != 0U) ? 1U : 0U;
    }

    if (max_basic_leaf >= X86_CPUID_LEAF_STRUCT_FEATURES) {
        cpuid_read(X86_CPUID_LEAF_STRUCT_FEATURES, 0U, &eax, &ebx, &ecx, &edx);
        (void)eax;
        (void)ecx;
        (void)edx;
        g_hardening_state.smep_supported =
            ((ebx & X86_CPUID_STRUCT_EBX_SMEP) != 0U) ? 1U : 0U;
        g_hardening_state.smap_supported =
            ((ebx & X86_CPUID_STRUCT_EBX_SMAP) != 0U) ? 1U : 0U;
    }

    if (g_hardening_state.nx_supported) {
        efer = msr_read(X86_MSR_EFER);
        efer |= X86_EFER_NXE;
        msr_write(X86_MSR_EFER, efer);
        efer = msr_read(X86_MSR_EFER);
        g_hardening_state.nx_enabled = ((efer & X86_EFER_NXE) != 0U) ? 1U : 0U;
    }

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    if (g_hardening_state.smep_supported) {
        cr4 |= X86_CR4_SMEP;
    }
    if (g_hardening_state.smap_supported) {
        cr4 |= X86_CR4_SMAP;
    }
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

    g_hardening_state.smep_enabled = ((cr4 & X86_CR4_SMEP) != 0U) ? 1U : 0U;
    g_hardening_state.smap_enabled = ((cr4 & X86_CR4_SMAP) != 0U) ? 1U : 0U;

    serial_write("GNU OS: x86_64 hardening nx=");
    serial_write_hex64((uint64_t)g_hardening_state.nx_enabled);
    serial_write("/");
    serial_write_hex64((uint64_t)g_hardening_state.nx_supported);
    serial_write(" smep=");
    serial_write_hex64((uint64_t)g_hardening_state.smep_enabled);
    serial_write("/");
    serial_write_hex64((uint64_t)g_hardening_state.smep_supported);
    serial_write(" smap=");
    serial_write_hex64((uint64_t)g_hardening_state.smap_enabled);
    serial_write("/");
    serial_write_hex64((uint64_t)g_hardening_state.smap_supported);
    serial_write("\n");
}

void x86_64_hardening_get_state(x86_64_hardening_state_t *out_state)
{
    if (!out_state) {
        return;
    }

    *out_state = g_hardening_state;
}

uint8_t x86_64_smap_enabled(void)
{
    return g_hardening_state.smap_enabled;
}

void x86_64_uaccess_begin(void)
{
    if (!g_hardening_state.smap_enabled) {
        return;
    }

    __asm__ volatile("stac" : : : "memory");
}

void x86_64_uaccess_end(void)
{
    if (!g_hardening_state.smap_enabled) {
        return;
    }

    __asm__ volatile("clac" : : : "memory");
}
