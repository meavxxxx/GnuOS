#include <gnuos/panic.h>
#include <gnuos/serial.h>

#define PF_ERR_PRESENT 0x01ULL
#define PF_ERR_WRITE 0x02ULL
#define PF_ERR_USER 0x04ULL
#define PF_ERR_RSVD 0x08ULL
#define PF_ERR_INSTR 0x10ULL
#define PF_ERR_PK 0x20ULL
#define PF_ERR_SS 0x40ULL
#define PF_ERR_SGX 0x80ULL

static void panic_banner(void)
{
    serial_write("\n================ KERNEL PANIC ================\n");
}

static void panic_dump_frame(const struct interrupt_frame *frame)
{
    if (!frame) {
        return;
    }

    serial_write("rip: ");
    serial_write_hex64(frame->rip);
    serial_write("\ncs: ");
    serial_write_hex64(frame->cs);
    serial_write("\nrflags: ");
    serial_write_hex64(frame->rflags);
    serial_write("\nrsp: ");
    serial_write_hex64(frame->rsp);
    serial_write("\nss: ");
    serial_write_hex64(frame->ss);
    serial_write("\n");
}

__attribute__((noreturn)) void kpanic(const char *message)
{
    panic_banner();
    serial_write("reason: ");
    serial_write(message ? message : "(null)");
    serial_write("\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

__attribute__((noreturn)) void kpanic_exception(
    uint8_t vector,
    uint64_t error_code,
    const struct interrupt_frame *frame)
{
    panic_banner();
    serial_write("reason: unhandled cpu exception\n");
    serial_write("vector: ");
    serial_write_hex64(vector);
    serial_write("\nerror: ");
    serial_write_hex64(error_code);
    serial_write("\n");
    panic_dump_frame(frame);

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

__attribute__((noreturn)) void kpanic_page_fault(
    uint64_t fault_address,
    uint64_t error_code,
    const struct interrupt_frame *frame)
{
    panic_banner();
    serial_write("reason: page fault\n");
    serial_write("cr2: ");
    serial_write_hex64(fault_address);
    serial_write("\nerror: ");
    serial_write_hex64(error_code);
    serial_write("\nflags:");

    if ((error_code & PF_ERR_PRESENT) != 0) {
        serial_write(" PRESENT");
    } else {
        serial_write(" NONPRESENT");
    }
    if ((error_code & PF_ERR_WRITE) != 0) {
        serial_write(" WRITE");
    } else {
        serial_write(" READ");
    }
    if ((error_code & PF_ERR_USER) != 0) {
        serial_write(" USER");
    } else {
        serial_write(" KERNEL");
    }
    if ((error_code & PF_ERR_RSVD) != 0) {
        serial_write(" RSVD");
    }
    if ((error_code & PF_ERR_INSTR) != 0) {
        serial_write(" INSTR");
    }
    if ((error_code & PF_ERR_PK) != 0) {
        serial_write(" PK");
    }
    if ((error_code & PF_ERR_SS) != 0) {
        serial_write(" SS");
    }
    if ((error_code & PF_ERR_SGX) != 0) {
        serial_write(" SGX");
    }
    serial_write("\n");
    panic_dump_frame(frame);

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
