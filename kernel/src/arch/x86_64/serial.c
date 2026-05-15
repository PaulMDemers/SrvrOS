#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/process.h>
#include <srvros/serial.h>

#include "io.h"

#define COM1 0x3f8
#define SERIAL_BUFFER_SIZE 128
#define SERIAL_IER_RX_AVAILABLE 0x01
#define SERIAL_LSR_DATA_READY 0x01
#define SERIAL_LSR_TX_EMPTY 0x20

static bool serial_ready;
static char rx_buffer[SERIAL_BUFFER_SIZE];
static volatile uint64_t rx_read_index;
static volatile uint64_t rx_write_index;

static bool rx_data_ready(void) {
    return (inb(COM1 + 5) & SERIAL_LSR_DATA_READY) != 0;
}

static void push_rx_char(char c) {
    if (c == '\r') {
        c = '\n';
    }
    if (c == 3 && process_interrupt_foreground(SRV_SIGNAL_INT)) {
        keyboard_wake_waiters();
        return;
    }

    uint64_t next = (rx_write_index + 1) % SERIAL_BUFFER_SIZE;
    if (next == rx_read_index) {
        return;
    }

    rx_buffer[rx_write_index] = c;
    rx_write_index = next;
    keyboard_wake_waiters();
}

static void drain_rx_fifo(void) {
    while (serial_ready && rx_data_ready()) {
        push_rx_char((char)inb(COM1));
    }
}

bool serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xc7);
    outb(COM1 + 4, 0x0b);

    serial_ready = true;
    return true;
}

void serial_enable_interrupts(void) {
    if (!serial_ready) {
        return;
    }

    drain_rx_fifo();
    outb(COM1 + 1, SERIAL_IER_RX_AVAILABLE);
    console_write("serial: com1 rx irq enabled\n");
}

void serial_handle_irq(void) {
    drain_rx_fifo();
}

bool serial_has_char(void) {
    return rx_read_index != rx_write_index || (serial_ready && rx_data_ready());
}

bool serial_try_read_char(char *out) {
    if (out == 0) {
        return false;
    }

    drain_rx_fifo();
    if (rx_read_index == rx_write_index) {
        return false;
    }

    *out = rx_buffer[rx_read_index];
    rx_read_index = (rx_read_index + 1) % SERIAL_BUFFER_SIZE;
    return true;
}

static bool tx_empty(void) {
    return (inb(COM1 + 5) & SERIAL_LSR_TX_EMPTY) != 0;
}

void serial_putc(char c) {
    if (!serial_ready) {
        return;
    }

    if (c == '\n') {
        serial_putc('\r');
    }

    while (!tx_empty()) {
    }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    while (*s != '\0') {
        serial_putc(*s++);
    }
}
