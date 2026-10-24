#ifndef SRVROS_USB_STORAGE_H
#define SRVROS_USB_STORAGE_H
#include <srvros/block.h>
#define USB_STORAGE_BULK_BYTES 4096u
#define USB_STORAGE_COMMAND_BYTES 16384u
/* Synchronous BOT transport. The controller owns/pins its DMA bounce buffer.
 * Calls are at most USB_STORAGE_BULK_BYTES, including each data-phase chunk.
 * A false return makes this device unusable until the next boot. */
typedef bool (*usb_bulk_fn)(void *, bool, void *, uint32_t, uint32_t *);
/* Software timestamps for exactly one synchronous request; observation is event
 * dispatch time, not the controller's hardware completion time. */
struct usb_bulk_timing {
    uint64_t submitted, observed, resumed;
    uint64_t service_checks,service_samples,service_gap_max_us,completion_gap_us;
    bool seen,service_valid;
};
struct usb_storage {
    void *context;
    usb_bulk_fn bulk;
    struct block_device *block;
    uint32_t tag, sector_size;
    uint64_t sectors, commands, errors;
    /* Retain the most recent failure even after later successful commands. */
    uint64_t error_command;
    uint8_t error_opcode, error_phase, error_status;
    const char *error_reason;
    uint32_t error_expected_tag, error_actual, error_signature, error_tag, error_residue;
    uint8_t error_csw_status;
    /* Read-only controller diagnostics, called only when displaying a failure. */
    void (*diagnose)(void *);
    void (*transfer_timing)(void *, struct usb_bulk_timing *);
    bool failed, busy, services;
    char name[12];
    uint8_t data[USB_STORAGE_COMMAND_BYTES];
};
/* CPU0 diagnostic scope; system-wide synchronous BOT callback wall time,
 * including copies, event dispatch and yielded service. Not CPU/device time.
 * No extra transfers. A nested begin refuses without disturbing its owner. */
struct usb_storage_phase_timing {
    uint64_t max_us, over20ms, over100ms, over1s;
    uint64_t observed_calls, observe_us, resume_us, observe_max_us, resume_max_us;
    uint64_t service_checks,service_samples,service_gap_max_us,completion_gap_max_us;
};
struct usb_storage_timing {
    uint64_t calls[3], us[3], failures;
    struct usb_storage_phase_timing phase[3];
    bool valid;
};
bool usb_storage_timing_enabled(void);
bool usb_storage_timing_begin(void);
void usb_storage_timing_end(struct usb_storage_timing *out);
bool usb_storage_is_device(struct block_device *device);
void usb_storage_print_status(struct block_device *device);
bool usb_storage_enable_volume_writes(struct block_device *device);
bool usb_storage_disable_volume_writes(struct block_device *device);
bool usb_storage_attach(struct usb_storage *, void *, usb_bulk_fn, bool writable);
#endif
