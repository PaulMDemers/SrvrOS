#ifndef SRVROS_MOUSE_H
#define SRVROS_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

struct mouse_event {
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t flags;
    uint16_t reserved;
};

bool mouse_init(void);
void mouse_handle_irq(void);
bool mouse_scan(struct mouse_event *event);

#endif
