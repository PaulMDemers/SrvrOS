#ifndef SRVROS_TRACKPAD_POINTER_H
#define SRVROS_TRACKPAD_POINTER_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/apple_trackpad.h>

struct trackpad_pointer {
    bool baseline, blocked, palm_blocked, typing_blocked;
    uint8_t mode;
    uint8_t buttons;
    int32_t x,y,remainder_x,remainder_y;
};
struct trackpad_motion {
    int32_t dx,dy; uint8_t buttons;
    int32_t wheel_x,wheel_y;
    bool palm_suppressed,typing_suppressed;
};
/* Fixed 16 native units/pixel for the initial hardware trial. No acceleration,
 * tap-to-click or inferred finger IDs. Screen Y is opposite native device Y. */
bool trackpad_pointer_frame(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,struct trackpad_motion *out);
/* Experimental natural scrolling, broad-contact and typing suppression. */
bool trackpad_gesture_frame(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,bool typing,struct trackpad_motion *out);
bool trackpad_pointer_cancel(struct trackpad_pointer *state,struct trackpad_motion *out);
#endif
