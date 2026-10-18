#ifndef SRVROS_XHCI_H
#define SRVROS_XHCI_H

#include <stdbool.h>
#include <stdint.h>
#include <srvros/apple_trackpad.h>

#define XHCI_TRACKPAD_SAMPLES 5
struct xhci_trackpad_sample {
    uint32_t length;
    uint8_t data[APPLE_TRACKPAD_REPORT_MAX];
};
struct xhci_trackpad_capture {
    bool found, ready, pending, pointer_enabled;
    uint64_t pointer_queue_overflows, pointer_motion, pointer_events, pointer_buttons, pointer_cancels;
    uint8_t slot;
    uint32_t last_length, completion, max_contacts;
    uint64_t reports, contact_reports, button_reports, lifts, malformed, errors, keyboard_reports;
    /* First frame, first touch, first multiple-touch, first button, first malformed. */
    struct xhci_trackpad_sample samples[XHCI_TRACKPAD_SAMPLES];
};
void xhci_trackpad_snapshot(struct xhci_trackpad_capture *out);
void xhci_trackpad_reset_capture(void);

void xhci_set_storage_writable(bool writable);
void xhci_set_trackpad_capture(bool enabled);
void xhci_set_trackpad_pointer(bool enabled);
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
