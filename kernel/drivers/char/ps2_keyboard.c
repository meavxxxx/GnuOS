#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gnuos/io.h>
#include <gnuos/keyboard.h>
#include <gnuos/serial.h>

#define PS2_DATA_PORT 0x60U
#define PS2_STATUS_PORT 0x64U
#define PS2_COMMAND_PORT 0x64U

#define PS2_STATUS_OUTPUT_FULL 0x01U
#define PS2_STATUS_INPUT_FULL 0x02U
#define PS2_STATUS_AUX_DATA 0x20U

#define PS2_CMD_ENABLE_PORT1 0xAEU

#define PS2_KBD_SET_SCANCODE 0xF0U
#define PS2_KBD_ENABLE_SCANNING 0xF4U
#define PS2_KBD_DISABLE_SCANNING 0xF5U

#define PS2_KBD_ACK 0xFAU
#define PS2_KBD_RESEND 0xFEU

#define KBD_BUFFER_SIZE 128U
#define PS2_SPIN_LIMIT 100000U

static volatile uint64_t g_ps2_irq_count;
static volatile uint8_t g_ps2_last_scancode;
static volatile char g_kbd_buffer[KBD_BUFFER_SIZE];
static volatile size_t g_kbd_head;
static volatile size_t g_kbd_tail;

static bool g_kbd_shift;
static bool g_kbd_left_shift;
static bool g_kbd_right_shift;
static bool g_kbd_ctrl;
static bool g_kbd_alt;
static bool g_kbd_caps_lock;
static bool g_kbd_extended_prefix;

static const char g_scancode_set1_plain[128] = {
    [0x01] = 27,   [0x02] = '1',  [0x03] = '2',  [0x04] = '3',
    [0x05] = '4',  [0x06] = '5',  [0x07] = '6',  [0x08] = '7',
    [0x09] = '8',  [0x0A] = '9',  [0x0B] = '0',  [0x0C] = '-',
    [0x0D] = '=',  [0x0E] = '\b', [0x0F] = '\t', [0x10] = 'q',
    [0x11] = 'w',  [0x12] = 'e',  [0x13] = 'r',  [0x14] = 't',
    [0x15] = 'y',  [0x16] = 'u',  [0x17] = 'i',  [0x18] = 'o',
    [0x19] = 'p',  [0x1A] = '[',  [0x1B] = ']',  [0x1C] = '\n',
    [0x1E] = 'a',  [0x1F] = 's',  [0x20] = 'd',  [0x21] = 'f',
    [0x22] = 'g',  [0x23] = 'h',  [0x24] = 'j',  [0x25] = 'k',
    [0x26] = 'l',  [0x27] = ';',  [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z',  [0x2D] = 'x',  [0x2E] = 'c',
    [0x2F] = 'v',  [0x30] = 'b',  [0x31] = 'n',  [0x32] = 'm',
    [0x33] = ',',  [0x34] = '.',  [0x35] = '/',  [0x39] = ' '};

static const char g_scancode_set1_shifted[128] = {
    [0x01] = 27,   [0x02] = '!',  [0x03] = '@',  [0x04] = '#',
    [0x05] = '$',  [0x06] = '%',  [0x07] = '^',  [0x08] = '&',
    [0x09] = '*',  [0x0A] = '(',  [0x0B] = ')',  [0x0C] = '_',
    [0x0D] = '+',  [0x0E] = '\b', [0x0F] = '\t', [0x10] = 'Q',
    [0x11] = 'W',  [0x12] = 'E',  [0x13] = 'R',  [0x14] = 'T',
    [0x15] = 'Y',  [0x16] = 'U',  [0x17] = 'I',  [0x18] = 'O',
    [0x19] = 'P',  [0x1A] = '{',  [0x1B] = '}',  [0x1C] = '\n',
    [0x1E] = 'A',  [0x1F] = 'S',  [0x20] = 'D',  [0x21] = 'F',
    [0x22] = 'G',  [0x23] = 'H',  [0x24] = 'J',  [0x25] = 'K',
    [0x26] = 'L',  [0x27] = ':',  [0x28] = '"',  [0x29] = '~',
    [0x2B] = '|',  [0x2C] = 'Z',  [0x2D] = 'X',  [0x2E] = 'C',
    [0x2F] = 'V',  [0x30] = 'B',  [0x31] = 'N',  [0x32] = 'M',
    [0x33] = '<',  [0x34] = '>',  [0x35] = '?',  [0x39] = ' '};

static bool ps2_wait_for_write(void)
{
    uint32_t spins;

    for (spins = 0U; spins < PS2_SPIN_LIMIT; spins++) {
        if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0U) {
            return true;
        }
    }

    return false;
}

static bool ps2_wait_for_read(void)
{
    uint32_t spins;

    for (spins = 0U; spins < PS2_SPIN_LIMIT; spins++) {
        if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0U) {
            return true;
        }
    }

    return false;
}

static bool ps2_write_command(uint8_t value)
{
    if (!ps2_wait_for_write()) {
        return false;
    }

    io_out8(PS2_COMMAND_PORT, value);
    return true;
}

static bool ps2_write_data(uint8_t value)
{
    if (!ps2_wait_for_write()) {
        return false;
    }

    io_out8(PS2_DATA_PORT, value);
    return true;
}

static void ps2_flush_output(void)
{
    uint32_t guard;

    for (guard = 0U; guard < 64U; guard++) {
        if ((io_in8(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0U) {
            return;
        }

        (void)io_in8(PS2_DATA_PORT);
        io_wait();
    }
}

static bool ps2_expect_ack(void)
{
    uint8_t response;
    uint8_t attempts;

    for (attempts = 0U; attempts < 4U; attempts++) {
        if (!ps2_wait_for_read()) {
            return false;
        }

        response = io_in8(PS2_DATA_PORT);
        if (response == PS2_KBD_ACK) {
            return true;
        }
        if (response != PS2_KBD_RESEND) {
            return false;
        }
    }

    return false;
}

static bool ps2_send_keyboard_command(uint8_t cmd)
{
    uint8_t retries;

    for (retries = 0U; retries < 3U; retries++) {
        if (!ps2_write_data(cmd)) {
            return false;
        }

        if (ps2_expect_ack()) {
            return true;
        }
    }

    return false;
}

static bool ps2_send_keyboard_command_with_arg(uint8_t cmd, uint8_t arg)
{
    if (!ps2_send_keyboard_command(cmd)) {
        return false;
    }
    if (!ps2_write_data(arg)) {
        return false;
    }

    return ps2_expect_ack();
}

static void kbd_buffer_push(char ch)
{
    size_t next_head;

    if (ch == '\0') {
        return;
    }

    next_head = (g_kbd_head + 1U) % KBD_BUFFER_SIZE;
    if (next_head == g_kbd_tail) {
        return;
    }

    g_kbd_buffer[g_kbd_head] = ch;
    g_kbd_head = next_head;
}

static int kbd_buffer_pop(void)
{
    char value;

    if (g_kbd_head == g_kbd_tail) {
        return -1;
    }

    value = g_kbd_buffer[g_kbd_tail];
    g_kbd_tail = (g_kbd_tail + 1U) % KBD_BUFFER_SIZE;
    return (int)((unsigned char)value);
}

static bool kbd_is_alpha(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static void kbd_update_modifiers(uint8_t scancode, bool released, bool extended)
{
    if (extended) {
        switch (scancode) {
            case 0x1D:
                g_kbd_ctrl = !released;
                return;
            case 0x38:
                g_kbd_alt = !released;
                return;
            default:
                return;
        }
    }

    switch (scancode) {
        case 0x2A:
            g_kbd_left_shift = !released;
            g_kbd_shift = g_kbd_left_shift || g_kbd_right_shift;
            break;
        case 0x36:
            g_kbd_right_shift = !released;
            g_kbd_shift = g_kbd_left_shift || g_kbd_right_shift;
            break;
        case 0x1D:
            g_kbd_ctrl = !released;
            break;
        case 0x38:
            g_kbd_alt = !released;
            break;
        case 0x3A:
            if (!released) {
                g_kbd_caps_lock = !g_kbd_caps_lock;
            }
            break;
        default:
            break;
    }
}

static char kbd_translate_make_scancode(uint8_t scancode)
{
    char plain;
    char shifted;
    char result;

    plain = g_scancode_set1_plain[scancode];
    shifted = g_scancode_set1_shifted[scancode];

    if (kbd_is_alpha(plain)) {
        if (g_kbd_caps_lock ^ g_kbd_shift) {
            result = (char)(plain - ('a' - 'A'));
        } else {
            result = plain;
        }
    } else {
        result = g_kbd_shift ? shifted : plain;
    }

    if (g_kbd_ctrl && kbd_is_alpha(result)) {
        if (result >= 'a' && result <= 'z') {
            result = (char)(result - 'a' + 1);
        } else {
            result = (char)(result - 'A' + 1);
        }
    }

    return result;
}

static void kbd_handle_scancode(uint8_t raw_scancode)
{
    bool released;
    bool extended;
    uint8_t scancode;
    char translated;

    if (raw_scancode == 0xE0U || raw_scancode == 0xE1U) {
        g_kbd_extended_prefix = true;
        return;
    }

    released = (raw_scancode & 0x80U) != 0U;
    scancode = (uint8_t)(raw_scancode & 0x7FU);
    extended = g_kbd_extended_prefix;
    g_kbd_extended_prefix = false;

    kbd_update_modifiers(scancode, released, extended);
    if (released || extended) {
        return;
    }

    translated = kbd_translate_make_scancode(scancode);
    kbd_buffer_push(translated);
}

void ps2_keyboard_init(void)
{
    bool configured = true;

    g_ps2_irq_count = 0U;
    g_ps2_last_scancode = 0U;
    g_kbd_head = 0U;
    g_kbd_tail = 0U;
    g_kbd_shift = false;
    g_kbd_left_shift = false;
    g_kbd_right_shift = false;
    g_kbd_ctrl = false;
    g_kbd_alt = false;
    g_kbd_caps_lock = false;
    g_kbd_extended_prefix = false;

    ps2_flush_output();

    configured = configured && ps2_write_command(PS2_CMD_ENABLE_PORT1);
    configured = configured && ps2_send_keyboard_command(PS2_KBD_DISABLE_SCANNING);
    configured = configured && ps2_send_keyboard_command_with_arg(PS2_KBD_SET_SCANCODE, 0x01U);
    configured = configured && ps2_send_keyboard_command(PS2_KBD_ENABLE_SCANNING);

    if (configured) {
        serial_write("GNU OS: PS/2 keyboard initialized (set1 + buffered input).\n");
    } else {
        serial_write("GNU OS: PS/2 keyboard initialized with controller warnings.\n");
    }
}

void ps2_keyboard_on_irq(void)
{
    uint8_t status;
    uint8_t scancode;

    status = io_in8(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0U) {
        return;
    }

    if ((status & PS2_STATUS_AUX_DATA) != 0U) {
        (void)io_in8(PS2_DATA_PORT);
        return;
    }

    scancode = io_in8(PS2_DATA_PORT);
    g_ps2_irq_count++;
    g_ps2_last_scancode = scancode;
    kbd_handle_scancode(scancode);
}

uint64_t ps2_keyboard_irq_count(void)
{
    return g_ps2_irq_count;
}

uint8_t ps2_keyboard_last_scancode(void)
{
    return g_ps2_last_scancode;
}

int ps2_keyboard_getchar(void)
{
    return kbd_buffer_pop();
}

size_t ps2_keyboard_read(char *buffer, size_t max_len)
{
    size_t count;

    if (buffer == NULL || max_len == 0U) {
        return 0U;
    }

    for (count = 0U; count < max_len; count++) {
        int value;

        value = kbd_buffer_pop();
        if (value < 0) {
            break;
        }

        buffer[count] = (char)value;
    }

    return count;
}
