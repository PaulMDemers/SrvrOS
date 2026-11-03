#ifndef SRVROS_KEYBOARD_H
#define SRVROS_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>
#include <srvros/key_event.h>

bool keyboard_init(void);
void keyboard_handle_irq(void);
void keyboard_wake_waiters(void);
void keyboard_inject_char(char c);
struct keyboard_hid_state {
    uint8_t previous_report[8];
    bool caps_lock;
    uint8_t repeat_usage;
    uint64_t repeat_due_us;
    uint64_t repeat_generation;
};
void keyboard_set_event_mode(bool enabled);
bool keyboard_read_event(struct srv_key_event *event);
bool keyboard_events_pending(void);
void keyboard_inject_hid_report(const uint8_t report[8], struct keyboard_hid_state *state);
void keyboard_repeat_hid(struct keyboard_hid_state *state, uint64_t now_us);
/* Normalized event devices keep every HID usage independently, without the USB
 * boot report's six-key limit. State belongs to one physical input source. */
struct keyboard_usage_state {
    uint8_t pressed[32], modifiers, repeat_usage;
    bool caps_lock;
    uint64_t repeat_due_us, repeat_generation;
};
void keyboard_inject_usage(uint8_t usage, bool down, struct keyboard_usage_state *state);
void keyboard_repeat_usage(struct keyboard_usage_state *state, uint64_t now_us);
void keyboard_release_usage(struct keyboard_usage_state *state);
bool keyboard_try_read_char(char *out);
bool keyboard_scan_char(char *out);
char keyboard_read_char(void);
uint64_t keyboard_buffered_count(void);
uint64_t keyboard_pushed_count(void);
uint64_t keyboard_dropped_count(void);
uint64_t keyboard_irq_count(void);
uint64_t keyboard_scancode_count(void);
uint64_t keyboard_last_scancode(void);
bool keyboard_data_enabled(void);
void keyboard_print_status(void);

#endif
