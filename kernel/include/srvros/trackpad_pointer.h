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
    uint64_t sample_us;
    unsigned speed; bool accelerated;
    /* Two-contact physical drag: logical ordering survives report-slot swaps.
     * origin is deliberately not treated as a hardware finger identifier. */
    uint8_t drag_count,drag_active;
    int32_t drag_x[2],drag_y[2],drag_anchor_x[2],drag_anchor_y[2];
};
struct trackpad_motion {
    int32_t dx,dy; uint8_t buttons;
    int32_t wheel_x,wheel_y;
    bool palm_suppressed,typing_suppressed;
    bool two_finger_drag,drag_rebased,drag_handoff;
};
/* Legacy fixed 16 native units/pixel. No acceleration,
 * tap-to-click or inferred finger IDs. Screen Y is opposite native device Y. */
bool trackpad_pointer_frame(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,struct trackpad_motion *out);
/* Natural scrolling, held-click steering, broad-contact and typing suppression. */
bool trackpad_gesture_frame(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,bool typing,struct trackpad_motion *out);
/* speed: 1..5 (3 is original base scale), acceleration changes pointer
 * movement, including the selected drag contact. disable_while_typing is opt-in; turning it off clears a latched
 * suppression without replaying motion. Timestamps are monotonic device-sample
 * times, before queueing. */
bool trackpad_pointer_timed(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,bool gestures,bool typing,
    uint64_t sample_us,unsigned speed,bool acceleration,bool disable_while_typing,struct trackpad_motion *out);
bool trackpad_pointer_cancel(struct trackpad_pointer *state,struct trackpad_motion *out);
#endif
