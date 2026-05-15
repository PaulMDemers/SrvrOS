#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/serial.h>

#include "io.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define KEYBOARD_BUFFER_SIZE 128

static char buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint64_t read_index;
static volatile uint64_t write_index;
static bool shift_down;
static bool ctrl_down;
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
        return;
    }

    buffer[write_index] = c;
    write_index = next;
    scheduler_wake_all(&keyboard_wait_queue);
}

void keyboard_init(void) {
    while ((inb(PS2_STATUS) & 0x01) != 0) {
        (void)inb(PS2_DATA);
    }

    console_write("keyboard: ps/2 controller drained\n");
}

void keyboard_handle_irq(void) {
    uint8_t scancode = inb(PS2_DATA);
    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7f;

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
    return process_should_exit_current() || read_index != write_index || serial_has_char();
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
