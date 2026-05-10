#include <stdint.h>

#include <gnuos/interrupts.h>
#include <gnuos/panic.h>
#include <gnuos/serial.h>

#define IDT_ENTRIES 256U
#define KERNEL_CODE_SELECTOR 0x08U
#define IDT_INT_GATE 0x8EU

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idt_entry_t g_idt[IDT_ENTRIES];
static idtr_t g_idtr;

static void idt_set_gate(uint8_t vector, uintptr_t handler)
{
    idt_entry_t *entry = &g_idt[vector];

    entry->offset_low = (uint16_t)(handler & 0xFFFFU);
    entry->selector = KERNEL_CODE_SELECTOR;
    entry->ist = 0;
    entry->type_attr = IDT_INT_GATE;
    entry->offset_mid = (uint16_t)((handler >> 16U) & 0xFFFFU);
    entry->offset_high = (uint32_t)((handler >> 32U) & 0xFFFFFFFFU);
    entry->zero = 0;
}

static void idt_load(const idtr_t *idtr)
{
    __asm__ volatile("lidt (%0)" : : "r"(idtr) : "memory");
}

#define DEFINE_ISR_NOERR(name, vec)                                      \
    __attribute__((interrupt)) static void name(struct interrupt_frame *f) \
    {                                                                     \
        kpanic_exception((vec), 0, f);                                    \
    }

#define DEFINE_ISR_ERR(name, vec)                                               \
    __attribute__((interrupt)) static void name(                                \
        struct interrupt_frame *f,                                              \
        uint64_t error_code)                                                    \
    {                                                                            \
        kpanic_exception((vec), error_code, f);                                 \
    }

DEFINE_ISR_NOERR(isr_divide_error, 0)
DEFINE_ISR_NOERR(isr_debug, 1)
DEFINE_ISR_NOERR(isr_nmi, 2)
DEFINE_ISR_NOERR(isr_breakpoint, 3)
DEFINE_ISR_NOERR(isr_overflow, 4)
DEFINE_ISR_NOERR(isr_bound_range, 5)
DEFINE_ISR_NOERR(isr_invalid_opcode, 6)
DEFINE_ISR_NOERR(isr_device_not_available, 7)
DEFINE_ISR_ERR(isr_double_fault, 8)
DEFINE_ISR_NOERR(isr_coprocessor_segment_overrun, 9)
DEFINE_ISR_ERR(isr_invalid_tss, 10)
DEFINE_ISR_ERR(isr_segment_not_present, 11)
DEFINE_ISR_ERR(isr_stack_segment_fault, 12)
DEFINE_ISR_ERR(isr_general_protection_fault, 13)
DEFINE_ISR_ERR(isr_page_fault, 14)
DEFINE_ISR_NOERR(isr_reserved_15, 15)
DEFINE_ISR_NOERR(isr_x87_floating_point, 16)
DEFINE_ISR_ERR(isr_alignment_check, 17)
DEFINE_ISR_NOERR(isr_machine_check, 18)
DEFINE_ISR_NOERR(isr_simd_floating_point, 19)
DEFINE_ISR_NOERR(isr_virtualization, 20)
DEFINE_ISR_ERR(isr_control_protection, 21)
DEFINE_ISR_NOERR(isr_reserved_22, 22)
DEFINE_ISR_NOERR(isr_reserved_23, 23)
DEFINE_ISR_NOERR(isr_reserved_24, 24)
DEFINE_ISR_NOERR(isr_reserved_25, 25)
DEFINE_ISR_NOERR(isr_reserved_26, 26)
DEFINE_ISR_NOERR(isr_reserved_27, 27)
DEFINE_ISR_NOERR(isr_hypervisor_injection, 28)
DEFINE_ISR_ERR(isr_vmm_communication, 29)
DEFINE_ISR_ERR(isr_security_exception, 30)
DEFINE_ISR_NOERR(isr_reserved_31, 31)

void x86_64_idt_init(void)
{
    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        g_idt[i].offset_low = 0;
        g_idt[i].selector = 0;
        g_idt[i].ist = 0;
        g_idt[i].type_attr = 0;
        g_idt[i].offset_mid = 0;
        g_idt[i].offset_high = 0;
        g_idt[i].zero = 0;
    }

    idt_set_gate(0, (uintptr_t)isr_divide_error);
    idt_set_gate(1, (uintptr_t)isr_debug);
    idt_set_gate(2, (uintptr_t)isr_nmi);
    idt_set_gate(3, (uintptr_t)isr_breakpoint);
    idt_set_gate(4, (uintptr_t)isr_overflow);
    idt_set_gate(5, (uintptr_t)isr_bound_range);
    idt_set_gate(6, (uintptr_t)isr_invalid_opcode);
    idt_set_gate(7, (uintptr_t)isr_device_not_available);
    idt_set_gate(8, (uintptr_t)isr_double_fault);
    idt_set_gate(9, (uintptr_t)isr_coprocessor_segment_overrun);
    idt_set_gate(10, (uintptr_t)isr_invalid_tss);
    idt_set_gate(11, (uintptr_t)isr_segment_not_present);
    idt_set_gate(12, (uintptr_t)isr_stack_segment_fault);
    idt_set_gate(13, (uintptr_t)isr_general_protection_fault);
    idt_set_gate(14, (uintptr_t)isr_page_fault);
    idt_set_gate(15, (uintptr_t)isr_reserved_15);
    idt_set_gate(16, (uintptr_t)isr_x87_floating_point);
    idt_set_gate(17, (uintptr_t)isr_alignment_check);
    idt_set_gate(18, (uintptr_t)isr_machine_check);
    idt_set_gate(19, (uintptr_t)isr_simd_floating_point);
    idt_set_gate(20, (uintptr_t)isr_virtualization);
    idt_set_gate(21, (uintptr_t)isr_control_protection);
    idt_set_gate(22, (uintptr_t)isr_reserved_22);
    idt_set_gate(23, (uintptr_t)isr_reserved_23);
    idt_set_gate(24, (uintptr_t)isr_reserved_24);
    idt_set_gate(25, (uintptr_t)isr_reserved_25);
    idt_set_gate(26, (uintptr_t)isr_reserved_26);
    idt_set_gate(27, (uintptr_t)isr_reserved_27);
    idt_set_gate(28, (uintptr_t)isr_hypervisor_injection);
    idt_set_gate(29, (uintptr_t)isr_vmm_communication);
    idt_set_gate(30, (uintptr_t)isr_security_exception);
    idt_set_gate(31, (uintptr_t)isr_reserved_31);

    g_idtr.limit = (uint16_t)(sizeof(g_idt) - 1U);
    g_idtr.base = (uint64_t)(uintptr_t)&g_idt[0];

    idt_load(&g_idtr);
    serial_write("GNU OS: IDT initialized for vectors 0..31.\n");
}

