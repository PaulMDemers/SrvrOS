#ifndef SRVROS_XHCI_H
#define SRVROS_XHCI_H

#include <stdbool.h>
#include <stdint.h>
#include <srvros/apple_trackpad.h>

struct srv_device_info;
int64_t xhci_device_snapshot(uint64_t index,struct srv_device_info *out);

#define XHCI_TRACKPAD_SAMPLES 5
struct xhci_trackpad_sample {
    uint32_t length;
    uint8_t data[APPLE_TRACKPAD_REPORT_MAX];
};
struct xhci_trackpad_capture {
    bool found, ready, pending, pointer_enabled, gestures_enabled;
    uint64_t scroll_events, typing_suppressed, palm_suppressed;
    uint64_t drag_frames, drag_rebases, drag_handoffs, tap_primary, tap_secondary;
    uint64_t pointer_queue_overflows, pointer_motion, pointer_events, pointer_buttons, pointer_cancels;
    uint8_t slot;
    uint32_t last_length, completion, max_contacts, max_touch_major, max_touch_minor;
    uint64_t reports, contact_reports, button_reports, lifts, malformed, errors, keyboard_reports;
    /* First frame, first touch, first multiple-touch, first button, first malformed. */
    struct xhci_trackpad_sample samples[XHCI_TRACKPAD_SAMPLES];
};
void xhci_trackpad_snapshot(struct xhci_trackpad_capture *out);
void xhci_trackpad_reset_capture(void);

bool xhci_wifi_identity_ready(void);
void xhci_set_wifi_probe_enabled(bool enabled);
void xhci_set_network_enabled(bool enabled);
void xhci_set_storage_writable(bool writable);
void xhci_set_trackpad_capture(bool enabled);
void xhci_set_trackpad_pointer(bool enabled);
void xhci_set_trackpad_gestures(bool enabled);
void xhci_init(void);
bool xhci_is_present(void);
uint64_t xhci_keyboard_count(void);
uint64_t xhci_mouse_count(void);
uint64_t xhci_device_count(void);
uint64_t xhci_hub_count(void);
void xhci_probe_input(uint64_t wait_ticks);
void xhci_print_input_summary(void);
void xhci_print_status(void);
void xhci_print_boot_diagnostic(void);

#endif
