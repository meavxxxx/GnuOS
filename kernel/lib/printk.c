#include <stdarg.h>
#include <stdint.h>

#include <gnuos/printk.h>
#include <gnuos/serial.h>

static int print_unsigned(uint64_t value, uint32_t base, int uppercase)
{
    static const char digits_lo[] = "0123456789abcdef";
    static const char digits_hi[] = "0123456789ABCDEF";
    const char *digits = uppercase ? digits_hi : digits_lo;
    char buffer[32];
    int count = 0;
    int index = 0;

    if (base < 2U || base > 16U) {
        return 0;
    }

    if (value == 0U) {
        serial_write_char('0');
        return 1;
    }

    while (value != 0U) {
        buffer[index++] = digits[value % base];
        value /= base;
    }

    while (index > 0) {
        serial_write_char(buffer[--index]);
        count++;
    }

    return count;
}

static int print_signed(int64_t value)
{
    if (value < 0) {
        uint64_t magnitude = (uint64_t)(-(value + 1LL)) + 1ULL;
        serial_write_char('-');
        return 1 + print_unsigned(magnitude, 10U, 0);
    }

    return print_unsigned((uint64_t)value, 10U, 0);
}

int kvprintf(const char *fmt, va_list args)
{
    int total = 0;

    if (!fmt) {
        return 0;
    }

    while (*fmt != '\0') {
        if (*fmt != '%') {
            serial_write_char(*fmt++);
            total++;
            continue;
        }

        fmt++;
        switch (*fmt) {
            case '\0':
                return total;
            case '%':
                serial_write_char('%');
                total++;
                break;
            case 'c': {
                char ch = (char)va_arg(args, int);
                serial_write_char(ch);
                total++;
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                if (!str) {
                    str = "(null)";
                }
                while (*str != '\0') {
                    serial_write_char(*str++);
                    total++;
                }
                break;
            }
            case 'u':
                total += print_unsigned(va_arg(args, uint64_t), 10U, 0);
                break;
            case 'd':
            case 'i':
                total += print_signed(va_arg(args, int64_t));
                break;
            case 'x':
                total += print_unsigned(va_arg(args, uint64_t), 16U, 0);
                break;
            case 'X':
                total += print_unsigned(va_arg(args, uint64_t), 16U, 1);
                break;
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(args, void *);
                serial_write("0x");
                total += 2;
                total += print_unsigned((uint64_t)ptr, 16U, 0);
                break;
            }
            default:
                serial_write_char('%');
                serial_write_char(*fmt);
                total += 2;
                break;
        }

        fmt++;
    }

    return total;
}

int kprintf(const char *fmt, ...)
{
    va_list args;
    int result = 0;

    va_start(args, fmt);
    result = kvprintf(fmt, args);
    va_end(args);

    return result;
}
