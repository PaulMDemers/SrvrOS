#ifndef SRVROS_MOUSE_H
#define SRVROS_MOUSE_H

#include <stdbool.h>
#include <stdint.h>
#include <srvros/input_timing.h>

#define MOUSE_CANCEL 0x80

struct mouse_event {
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t flags;
    uint16_t reserved;
    int32_t wheel_x, wheel_y; /* Viewport pixels: positive is right/down. */
};

void mouse_trackpad_settings(unsigned speed,unsigned options);
void mouse_trackpad_get_settings(unsigned *speed,unsigned *options);
bool mouse_init(void);
void mouse_handle_irq(void);
void mouse_inject_event(int32_t dx, int32_t dy, uint8_t buttons);
void mouse_inject_trackpad(int32_t dx, int32_t dy, uint8_t buttons);
void mouse_inject_trackpad_scroll(int32_t dx,int32_t dy,int32_t wheel_x,int32_t wheel_y,uint8_t buttons);
void mouse_inject_trackpad_tap(uint8_t button);
/* Discard queued pointer input and cancel an active trackpad press without a
 * click-producing release. Preserve an external mouse's physical button state. */
void mouse_cancel_trackpad(void);
uint64_t mouse_overflow_count(void);
void mouse_queue_timing(struct input_timing *oldest,struct input_timing *latest);
bool mouse_scan(struct mouse_event *event);
struct scheduler_wait_queue;
struct scheduler_wait_queue *mouse_wait_queue(void);
bool mouse_pending(void);

#endif
