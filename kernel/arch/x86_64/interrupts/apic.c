#include <stdint.h>

#include <gnuos/apic.h>
#include <gnuos/msr.h>
#include <gnuos/serial.h>
#include <gnuos/vmm.h>

#define X86_MSR_IA32_APIC_BASE 0x1BU
#define X86_MSR_X2APIC_BASE 0x800U

#define IA32_APIC_BASE_BSP (1ULL << 8U)
#define IA32_APIC_BASE_X2APIC_ENABLE (1ULL << 10U)
#define IA32_APIC_BASE_APIC_GLOBAL_ENABLE (1ULL << 11U)
#define IA32_APIC_BASE_PHYS_MASK 0x000FFFFFFFFFF000ULL

#define CPUID_LEAF_FEATURES 0x1U
#define CPUID_FEAT_EDX_APIC (1U << 9U)
#define CPUID_FEAT_ECX_X2APIC (1U << 21U)

#define APIC_REG_ID 0x20U
#define APIC_REG_SPURIOUS 0xF0U
#define APIC_REG_EOI 0xB0U

#define APIC_SPURIOUS_VECTOR 0xFFU
#define APIC_SPURIOUS_SW_ENABLE (1U << 8U)

static apic_mode_t g_apic_mode;
static uint64_t g_apic_base_phys;
static uint32_t g_apic_id;

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

static uint32_t apic_x2apic_msr(uint32_t reg)
{
    return X86_MSR_X2APIC_BASE + (reg >> 4U);
}

static uint32_t apic_read_reg(uint32_t reg)
{
    if (g_apic_mode == APIC_MODE_X2APIC) {
        return (uint32_t)msr_read(apic_x2apic_msr(reg));
    }

    if (g_apic_mode == APIC_MODE_XAPIC) {
        volatile uint32_t *mmio = (volatile uint32_t *)(uintptr_t)(g_apic_base_phys + reg);
        return *mmio;
    }

    return 0U;
}

static void apic_write_reg(uint32_t reg, uint32_t value)
{
    if (g_apic_mode == APIC_MODE_X2APIC) {
        msr_write(apic_x2apic_msr(reg), value);
        return;
    }

    if (g_apic_mode == APIC_MODE_XAPIC) {
        volatile uint32_t *mmio = (volatile uint32_t *)(uintptr_t)(g_apic_base_phys + reg);
        *mmio = value;
    }
}

static int apic_ensure_mmio_mapping(uint64_t phys_addr)
{
    uint64_t mapped_phys = 0U;
    uint64_t mapped_flags = 0U;

    if (vmm_query_mapping(phys_addr, &mapped_phys, &mapped_flags)) {
        return 1;
    }

    return vmm_map_page(phys_addr, phys_addr, VMM_MAP_WRITABLE);
}

static void apic_program_spurious_vector(void)
{
    uint32_t svr = apic_read_reg(APIC_REG_SPURIOUS);
    svr &= ~0xFFU;
    svr |= APIC_SPURIOUS_VECTOR;
    svr |= APIC_SPURIOUS_SW_ENABLE;
    apic_write_reg(APIC_REG_SPURIOUS, svr);
}

int apic_init(void)
{
    uint32_t eax = 0U;
    uint32_t ebx = 0U;
    uint32_t ecx = 0U;
    uint32_t edx = 0U;
    uint64_t apic_base = 0U;
    uint8_t has_apic = 0U;
    uint8_t has_x2apic = 0U;
    uint8_t mode_locked_x2apic = 0U;

    g_apic_mode = APIC_MODE_DISABLED;
    g_apic_base_phys = 0U;
    g_apic_id = 0U;

    cpuid_read(CPUID_LEAF_FEATURES, 0U, &eax, &ebx, &ecx, &edx);
    has_apic = ((edx & CPUID_FEAT_EDX_APIC) != 0U) ? 1U : 0U;
    has_x2apic = ((ecx & CPUID_FEAT_ECX_X2APIC) != 0U) ? 1U : 0U;

    if (!has_apic) {
        serial_write("GNU OS: APIC not supported by CPU, keeping legacy PIC mode.\n");
        return 0;
    }

    apic_base = msr_read(X86_MSR_IA32_APIC_BASE);
    g_apic_base_phys = apic_base & IA32_APIC_BASE_PHYS_MASK;

    if ((apic_base & IA32_APIC_BASE_APIC_GLOBAL_ENABLE) == 0U) {
        apic_base |= IA32_APIC_BASE_APIC_GLOBAL_ENABLE;
        msr_write(X86_MSR_IA32_APIC_BASE, apic_base);
        apic_base = msr_read(X86_MSR_IA32_APIC_BASE);
    }

    mode_locked_x2apic = ((apic_base & IA32_APIC_BASE_X2APIC_ENABLE) != 0U) ? 1U : 0U;

    if (mode_locked_x2apic && has_x2apic) {
        g_apic_mode = APIC_MODE_X2APIC;
    } else {
        g_apic_mode = APIC_MODE_XAPIC;
    }

    if (g_apic_mode == APIC_MODE_XAPIC && !apic_ensure_mmio_mapping(g_apic_base_phys)) {
        serial_write("GNU OS: APIC MMIO map failed, APIC init aborted.\n");
        g_apic_mode = APIC_MODE_DISABLED;
        return 0;
    }

    apic_program_spurious_vector();
    g_apic_id = apic_local_id();

    serial_write("GNU OS: APIC initialized mode=");
    if (g_apic_mode == APIC_MODE_X2APIC) {
        serial_write("x2APIC");
    } else {
        serial_write("xAPIC");
    }
    serial_write(" id=0x");
    serial_write_hex64((uint64_t)g_apic_id);
    serial_write(" base=0x");
    serial_write_hex64(g_apic_base_phys);
    serial_write(" x2apic_cpuid=");
    serial_write_hex64((uint64_t)has_x2apic);
    serial_write(" bsp=");
    serial_write_hex64(
        (apic_base & IA32_APIC_BASE_BSP) != 0U ? 1ULL : 0ULL);
    serial_write("\n");

    return 1;
}

apic_mode_t apic_mode(void)
{
    return g_apic_mode;
}

uint32_t apic_local_id(void)
{
    uint32_t reg = apic_read_reg(APIC_REG_ID);

    if (g_apic_mode == APIC_MODE_X2APIC) {
        return reg;
    }

    return (reg >> 24U) & 0xFFU;
}

void apic_send_eoi(void)
{
    if (g_apic_mode == APIC_MODE_DISABLED) {
        return;
    }

    apic_write_reg(APIC_REG_EOI, 0U);
}

