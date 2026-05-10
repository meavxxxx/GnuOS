#include <gnuos/panic.h>
#include <gnuos/serial.h>

static void panic_banner(void)
{
    serial_write("\n================ KERNEL PANIC ================\n");
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

    if (frame) {
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

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
