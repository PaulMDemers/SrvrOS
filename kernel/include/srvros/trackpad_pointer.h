#ifndef SRVROS_TRACKPAD_POINTER_H
#define SRVROS_TRACKPAD_POINTER_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/apple_trackpad.h>
#include <srvros/pointer_config.h>

enum trackpad_tap_rejection {
    TAP_REJECT_POLICY, TAP_REJECT_PHYSICAL, TAP_REJECT_PALM, TAP_REJECT_TYPING,
    TAP_REJECT_CONTACTS, TAP_REJECT_CLOCK, TAP_REJECT_DURATION, TAP_REJECT_LIFT,
    TAP_REJECT_JOIN, TAP_REJECT_RECONTACT, TAP_REJECT_MOVEMENT, TAP_REJECT_SHORT,
    TAP_REJECT_SECONDARY_OFF, TAP_REJECT_CANCEL, TAP_REJECT_COUNT
};
struct trackpad_tap_rejections {
    /* First rejection of an armed candidate only, not all attempted gestures.
     * Maxima are independent aggregates, not a reconstructed contact trace. */
    uint64_t count[TAP_REJECT_COUNT], two_finger[TAP_REJECT_COUNT];
    uint64_t max_age_us[TAP_REJECT_COUNT];
    uint32_t max_travel[TAP_REJECT_COUNT];
};

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
    bool tap_armed,tap_inhibit,tap_releasing;
    uint8_t tap_contacts,tap_peak;
    unsigned tap_options;
    uint64_t tap_start_us,tap_last_us,tap_release_us;
    int32_t tap_x[2],tap_y[2];
    int32_t drag_x[2],drag_y[2],drag_anchor_x[2],drag_anchor_y[2];
    struct trackpad_tap_rejections tap_rejections;
};
struct trackpad_motion {
    int32_t dx,dy; uint8_t buttons;
    int32_t wheel_x,wheel_y;
    bool palm_suppressed,typing_suppressed;
    bool two_finger_drag,drag_rebased,drag_handoff;
    uint8_t tap_buttons; /* Completed click pulse: deliver press/release atomically. */
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
/* Configured desktop policy. Taps complete only on full lift; never hold a
 * synthetic button across reports. Physical clicking consumes any tap candidate. */
bool trackpad_pointer_configured(struct trackpad_pointer *state,
    const struct apple_trackpad_frame *frame,bool gestures,bool typing,
    uint64_t sample_us,unsigned speed,unsigned options,struct trackpad_motion *out);
bool trackpad_pointer_cancel(struct trackpad_pointer *state,struct trackpad_motion *out);
#endif
