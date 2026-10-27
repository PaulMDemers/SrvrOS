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
/* Call only in the explicit capture after logging and timer initialization. */
bool xhci_wifi_capture_calibration(void);
void xhci_set_wifi_probe_enabled(bool enabled);
void xhci_set_wifi_firmware_enabled(bool enabled);
void xhci_set_wifi_mac_startup(bool enabled);
bool xhci_wifi_capture_firmware(const uint8_t *image, uint64_t size);
/* One-shot read-only MCU transport diagnostic after successful firmware capture.
 * Success validates the captured identity-read reply and disabled MAC only;
 * it does not validate other MCU operations or RF functionality. */
bool xhci_wifi_capture_mcu(void);
/* Opt-in MAC/BBP reset followed by strict MCU identity-read capture. */
bool xhci_wifi_capture_radio_reset(void);
bool xhci_wifi_capture_write_probe(void);
bool xhci_wifi_capture_pair_probe(void);
bool xhci_wifi_capture_timing_probe(void);
bool xhci_wifi_capture_mac_basics(void);
bool xhci_wifi_capture_mac_format(void);
bool xhci_wifi_capture_bbp_ready(void);
bool xhci_wifi_capture_bbp_core(void);
bool xhci_wifi_capture_bbp_front(void);
bool xhci_wifi_capture_bbp_agc(void);
bool xhci_wifi_capture_bbp_batch(void);
bool xhci_wifi_capture_compact(void);
bool xhci_wifi_capture_agc_more(void);
bool xhci_wifi_capture_agc_next(void);
bool xhci_wifi_capture_agc_tail(void);
bool xhci_wifi_capture_rxc_start(void);
bool xhci_wifi_capture_rxc_tail(void);
bool xhci_wifi_capture_txbe_start(void);
bool xhci_wifi_capture_txbe_next(void);
bool xhci_wifi_capture_txbe_observe(void);
bool xhci_wifi_capture_txbe_first(void);
bool xhci_wifi_capture_txbe_second(void);
bool xhci_wifi_capture_txbe_access(void);
bool xhci_wifi_capture_txbe_direct(void);
bool xhci_wifi_capture_clock(void);
bool xhci_wifi_capture_clock_direct(void);
bool xhci_wifi_capture_clock_pair(void);
bool xhci_wifi_capture_bbp_table(void);
bool xhci_wifi_capture_mac_snapshot(void);
bool xhci_wifi_capture_mac_raw(void);
bool xhci_wifi_capture_mac_route(void);
bool xhci_wifi_capture_mac_policy(void);
bool xhci_wifi_capture_mac_policy_observe(void);
bool xhci_wifi_capture_mac_policy_validate(void);
bool xhci_wifi_capture_mac_protect(void);
bool xhci_wifi_capture_mac_contention(void);
bool xhci_wifi_capture_mac_txsw(void);
bool xhci_wifi_capture_mac_beacon(void);
bool xhci_wifi_capture_mac_vht(void);
bool xhci_wifi_capture_mac_buffer(void);
bool xhci_wifi_capture_mac_fce(void);
bool xhci_wifi_capture_mac_fce_validate(void);
bool xhci_wifi_capture_mac_ampdu(void);
bool xhci_wifi_capture_bbp_after_mac(void);
bool xhci_wifi_capture_bbp_switch(void);
bool xhci_wifi_capture_dcoc(void);
bool xhci_wifi_capture_skey(void);
bool xhci_wifi_capture_wcid(void);
bool xhci_wifi_capture_mac_tail(void);
bool xhci_wifi_capture_mac_fixed(void);
bool xhci_wifi_capture_efuse_map(void);
bool xhci_wifi_capture_mac_address(void);
bool xhci_wifi_capture_mac_address_validate(void);
bool xhci_wifi_capture_mac_board(void);
bool xhci_wifi_capture_ant_snapshot(void);
bool xhci_wifi_capture_ant_init(void);
bool xhci_wifi_capture_rf_probe(void);
bool xhci_wifi_capture_rf_central(void);
bool xhci_wifi_capture_rf_six(void);
bool xhci_wifi_capture_rf_csr(void);
bool xhci_wifi_capture_rf_write(void);
bool xhci_wifi_capture_rf_state(void);
bool xhci_wifi_capture_rf_table(void);
bool xhci_wifi_capture_rf_bank5(void);
bool xhci_wifi_capture_rf_bank6(void);
bool xhci_wifi_capture_rf_vga(void);
bool xhci_wifi_capture_rf_g20(void);
bool xhci_wifi_capture_rf_trim(void);
bool xhci_wifi_capture_rf_cal(void);
bool xhci_wifi_capture_rf_wait(void);
bool xhci_wifi_capture_path(void);
bool xhci_wifi_capture_channel_state(void);
bool xhci_wifi_capture_channel_observe(void);
bool xhci_wifi_capture_vco_ready(void);
bool xhci_wifi_capture_usb_cycle(void);
bool xhci_wifi_capture_bw_observe(void);
bool xhci_wifi_capture_bbp20(void);
bool xhci_wifi_capture_channel_probe(void);
bool xhci_wifi_capture_band2g(void);
bool xhci_wifi_capture_synth1(void);
bool xhci_wifi_capture_post(void);
bool xhci_wifi_capture_gain(void);
bool xhci_wifi_capture_channel_bbp(void);
bool xhci_wifi_capture_post_vco(void);
bool xhci_wifi_capture_agc_state(void);
bool xhci_wifi_capture_rx_lifecycle(void);
bool xhci_wifi_capture_mac_cycle(void);
bool xhci_wifi_capture_cal_probe(void);
bool xhci_wifi_capture_poweron(void);
bool xhci_wifi_capture_passive(void);
bool xhci_wifi_capture_window(void);
bool xhci_wifi_capture_window_long(void);
bool xhci_wifi_capture_discovery(void);
bool xhci_wifi_capture_channel_preview(void);
bool xhci_wifi_capture_channel_tune(void);
bool xhci_wifi_capture_channel_receive(void);
bool xhci_wifi_capture_scan(void);
/* Diagnostic-only: IRQ-masked I/O owner, no outstanding/failed bulk DMA.
 * Select 0 (poll), 4000 or 20000 us; restore previous after the capture. */
bool xhci_storage_wait_policy(uint64_t poll_us,uint64_t *previous);
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
