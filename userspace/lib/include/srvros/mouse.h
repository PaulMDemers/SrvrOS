#ifndef SRVROS_USER_MOUSE_H
#define SRVROS_USER_MOUSE_H

#include <stdint.h>

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04

struct mouse_event {
    uint64_t abi_version;
    uint64_t struct_size;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t flags;
    uint16_t reserved;
};

int mouse_scan(struct mouse_event *event);

#endif
