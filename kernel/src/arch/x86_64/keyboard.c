#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>

#include "io.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define KEYBOARD_BUFFER_SIZE 128
#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_ABSENT 0xff
#define PS2_DRAIN_LIMIT 256
#define PS2_WAIT_LIMIT 100000

#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_ENABLE_FIRST 0xae

#define KEYBOARD_CMD_ENABLE_SCANNING 0xf4
#define PS2_ACK 0xfa

static char buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint64_t read_index;
static volatile uint64_t write_index;
static volatile uint64_t pushed_count;
static volatile uint64_t dropped_count;
static volatile uint64_t irq_count;
static volatile uint64_t scancode_count;
static volatile uint8_t last_scancode;
static bool data_enabled;
static bool shift_down;
static bool ctrl_down;
static bool extended_scancode;
static struct scheduler_wait_queue keyboard_wait_queue;

static const char scancode_set1[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0,
};

static const char scancode_set1_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0,
};

static void push_char(char c) {
    uint64_t next = (write_index + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == read_index) {
        dropped_count++;
        return;
    }

    buffer[write_index] = c;
    write_index = next;
    pushed_count++;
    scheduler_wake_all(&keyboard_wait_queue);
}

static void push_escape_sequence(const char *sequence) {
    while (*sequence != '\0') {
        push_char(*sequence++);
    }
}

static bool wait_input_empty(void) {
    for (uint64_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool wait_output_full(void) {
    for (uint64_t i = 0; i < PS2_WAIT_LIMIT; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) != 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool controller_write(uint8_t command) {
    if (!wait_input_empty()) {
        return false;
    }
    outb(PS2_COMMAND, command);
    return true;
}

static bool data_write(uint8_t value) {
    if (!wait_input_empty()) {
        return false;
    }
    outb(PS2_DATA, value);
    return true;
}

static bool data_read(uint8_t *value) {
    if (value == 0 || !wait_output_full()) {
        return false;
    }
    *value = inb(PS2_DATA);
    return true;
}

bool keyboard_init(void) {
    uint8_t status = inb(PS2_STATUS);
    if (status == PS2_STATUS_ABSENT) {
        console_write("keyboard: ps/2 controller unavailable\n");
        return false;
    }

    uint64_t drained = 0;
    while ((status & PS2_STATUS_OUTPUT_FULL) != 0 && drained < PS2_DRAIN_LIMIT) {
        (void)inb(PS2_DATA);
        drained++;
        status = inb(PS2_STATUS);
        if (status == PS2_STATUS_ABSENT) {
            console_write("keyboard: ps/2 controller unavailable during drain\n");
            return false;
        }
    }

    if ((status & PS2_STATUS_OUTPUT_FULL) != 0) {
        console_write("keyboard: ps/2 drain limit reached, skipping controller\n");
        return false;
    }

    uint8_t config = 0;
    if (!controller_write(PS2_CMD_READ_CONFIG) || !data_read(&config)) {
        console_write("keyboard: ps/2 config read failed\n");
        return true;
    }

    config |= 0x41;
    config &= (uint8_t)~0x10;
    if (!controller_write(PS2_CMD_WRITE_CONFIG) || !data_write(config)) {
        console_write("keyboard: ps/2 config write failed\n");
        return true;
    }

    if (!controller_write(PS2_CMD_ENABLE_FIRST)) {
        console_write("keyboard: ps/2 first port enable failed\n");
        return true;
    }

    uint8_t response = 0;
    if (data_write(KEYBOARD_CMD_ENABLE_SCANNING) && data_read(&response) && response == PS2_ACK) {
        data_enabled = true;
        console_write("keyboard: ps/2 controller drained, irq enabled, scanning enabled\n");
    } else {
        console_printf("keyboard: ps/2 scanning enable no ack response=%x\n", (uint64_t)response);
    }
    return true;
}

void keyboard_handle_irq(void) {
    irq_count++;
    uint8_t scancode = inb(PS2_DATA);
    scancode_count++;
    last_scancode = scancode;
    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7f;

    if (scancode == 0xe0) {
        extended_scancode = true;
        return;
    }

    if (extended_scancode) {
        extended_scancode = false;
        if (released) {
            return;
        }
        switch (key) {
            case 0x48:
                push_escape_sequence("\x1b[A");
                return;
            case 0x50:
                push_escape_sequence("\x1b[B");
                return;
            case 0x4b:
                push_escape_sequence("\x1b[D");
                return;
            case 0x4d:
                push_escape_sequence("\x1b[C");
                return;
            case 0x47:
                push_escape_sequence("\x1b[H");
                return;
            case 0x4f:
                push_escape_sequence("\x1b[F");
                return;
            case 0x53:
                push_escape_sequence("\x1b[3~");
                return;
        }
        return;
    }

    if (key == 42 || key == 54) {
        shift_down = !released;
        return;
    }
    if (key == 29) {
        ctrl_down = !released;
        return;
    }

    if (released) {
        return;
    }

    char c = key < sizeof(scancode_set1) ?
        (shift_down ? scancode_set1_shift[key] : scancode_set1[key]) :
        0;
    if (ctrl_down && (key == 46 || key == 32 || key == 44)) {
        c = key == 46 ? 3 : (key == 32 ? 4 : 26);
    }
    if (c == 3 && process_interrupt_foreground(SRV_SIGNAL_INT)) {
        scheduler_wake_all(&keyboard_wait_queue);
        return;
    }
    if (c != 0) {
        push_char(c);
    }
}

void keyboard_wake_waiters(void) {
    scheduler_wake_all(&keyboard_wait_queue);
}

void keyboard_inject_char(char c) {
    if (c != 0) {
        push_char(c);
    }
}

bool keyboard_try_read_char(char *out) {
    if (read_index == write_index) {
        return false;
    }

    *out = buffer[read_index];
    read_index = (read_index + 1) % KEYBOARD_BUFFER_SIZE;
    return true;
}

bool keyboard_scan_char(char *out) {
    return keyboard_try_read_char(out) || serial_try_read_char(out);
}

static bool keyboard_has_char(void *arg) {
    (void)arg;
    return process_should_exit_current() ||
        process_signal_pending_current() ||
        read_index != write_index ||
        serial_has_char();
}

char keyboard_read_char(void) {
    char c;

    while (!keyboard_try_read_char(&c) && !serial_try_read_char(&c)) {
        if (process_should_exit_current()) {
            return 0;
        }
        if (!scheduler_wait(&keyboard_wait_queue, keyboard_has_char, 0)) {
            __asm__ volatile ("pause");
        }
    }

    return c;
}

uint64_t keyboard_buffered_count(void) {
    if (write_index >= read_index) {
        return write_index - read_index;
    }
    return KEYBOARD_BUFFER_SIZE - read_index + write_index;
}

uint64_t keyboard_pushed_count(void) {
    return pushed_count;
}

uint64_t keyboard_dropped_count(void) {
    return dropped_count;
}

uint64_t keyboard_irq_count(void) {
    return irq_count;
}

uint64_t keyboard_scancode_count(void) {
    return scancode_count;
}

uint64_t keyboard_last_scancode(void) {
    return last_scancode;
}

bool keyboard_data_enabled(void) {
    return data_enabled;
}

void keyboard_print_status(void) {
    console_printf("input-ps2: enabled=%u irq=%u scancodes=%u last=%x keybuf=%u pushed=%u dropped=%u\n",
        data_enabled ? 1ull : 0ull,
        irq_count,
        scancode_count,
        (uint64_t)last_scancode,
        keyboard_buffered_count(),
        pushed_count,
        dropped_count);
}
