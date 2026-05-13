#include <srvros/console.h>
#include <srvros/mouse.h>

#include "io.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_MOUSE_OUTPUT 0x20

#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_ENABLE_AUX 0xa8
#define PS2_CMD_WRITE_AUX 0xd4

#define PS2_ACK 0xfa

#define MOUSE_CMD_SET_DEFAULTS 0xf6
#define MOUSE_CMD_ENABLE_DATA 0xf4

#define MOUSE_FLAG_LEFT 0x01
#define MOUSE_FLAG_RIGHT 0x02
#define MOUSE_FLAG_MIDDLE 0x04

static uint8_t packet[3];
static uint8_t packet_index;
static bool initialized;
static volatile int32_t pending_dx;
static volatile int32_t pending_dy;
static volatile uint8_t current_buttons;
static volatile bool changed;

static bool wait_input_empty(void) {
    for (uint64_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static bool wait_output_full(void) {
    for (uint64_t i = 0; i < 100000; i++) {
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

static bool mouse_write(uint8_t command) {
    uint8_t response = 0;
    if (!controller_write(PS2_CMD_WRITE_AUX) || !data_write(command) || !data_read(&response)) {
        return false;
    }
    return response == PS2_ACK;
}

bool mouse_init(void) {
    uint8_t config = 0;

    if (!controller_write(PS2_CMD_ENABLE_AUX) ||
        !controller_write(PS2_CMD_READ_CONFIG) ||
        !data_read(&config)) {
        console_write("mouse: controller init failed\n");
        return false;
    }

    config |= 0x02;
    config &= (uint8_t)~0x20;
    if (!controller_write(PS2_CMD_WRITE_CONFIG) ||
        !data_write(config)) {
        console_write("mouse: irq enable failed\n");
        return false;
    }

    if (!mouse_write(MOUSE_CMD_SET_DEFAULTS) ||
        !mouse_write(MOUSE_CMD_ENABLE_DATA)) {
        console_write("mouse: device enable failed\n");
        return false;
    }

    initialized = true;
    packet_index = 0;
    pending_dx = 0;
    pending_dy = 0;
    current_buttons = 0;
    changed = false;
    console_write("mouse: ps/2 auxiliary device enabled\n");
    return true;
}

void mouse_handle_irq(void) {
    if (!initialized) {
        (void)inb(PS2_DATA);
        return;
    }

    uint8_t status = inb(PS2_STATUS);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
        return;
    }

    uint8_t value = inb(PS2_DATA);
    if ((status & PS2_STATUS_MOUSE_OUTPUT) == 0) {
        return;
    }

    if (packet_index == 0 && (value & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = value;
    if (packet_index < sizeof(packet)) {
        return;
    }
    packet_index = 0;

    if ((packet[0] & 0xc0) != 0) {
        return;
    }

    int32_t dx = (int8_t)packet[1];
    int32_t dy = -(int32_t)(int8_t)packet[2];
    pending_dx += dx;
    pending_dy += dy;
    current_buttons = packet[0] & (MOUSE_FLAG_LEFT | MOUSE_FLAG_RIGHT | MOUSE_FLAG_MIDDLE);
    changed = true;
}

bool mouse_scan(struct mouse_event *event) {
    if (event == 0 || !changed) {
        return false;
    }

    event->dx = pending_dx;
    event->dy = pending_dy;
    event->buttons = current_buttons;
    event->flags = current_buttons;
    event->reserved = 0;

    pending_dx = 0;
    pending_dy = 0;
    changed = false;
    return true;
}
