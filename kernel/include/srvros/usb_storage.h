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
    uint64_t empty_checks,event_checks,last_empty_sample_us;
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
    bool failed, busy, services, retiring, retired, initializing;
    unsigned diagnostic_users;
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
struct usb_storage_csw_timing {
    uint64_t start_us,end_us,opcode,command_bytes,request_bytes,actual,ok;
    struct usb_bulk_timing sample;
};
#define USB_STORAGE_CONTENT_ROLES 7u
#define USB_STORAGE_CONTENT_NO_ROLE (~0u)
/* Content-owned diagnostic window, nested inside the auxiliary sync observer.
 * Roles0..5 mirror the exclusive filesystem callbacks;6 is unclassified work
 * (including pre-mutation scans). No paths, pointers or command addresses. */
struct usb_storage_content_role {
    uint64_t read_commands,read_bytes,write_commands,write_bytes;
    uint64_t flush_commands,other_commands,failed_commands;
    uint64_t csw_calls,csw_us,over100ms;
    uint64_t write512_commands,write_csw_us,write512_csw_us;
    uint64_t write_over100ms,write512_over100ms;
    struct usb_storage_csw_timing worst;
};
/* One complete command companion for the first strict CSW maximum in this
 * content. Callback/checkpoint clocks are reused, never sampled again here.
 * The command result follows full original validation, unlike bulk .ok. */
struct usb_storage_command_phase {
    uint64_t calls,requested,actual,us,first_us,last_us;
};
struct usb_storage_command_timing {
    uint64_t valid,complete,ok,role,start_us,end_us,csw_start_us,csw_end_us;
    uint64_t checkpoint_calls,checkpoint_us,checkpoint_first_us,checkpoint_last_us;
    struct usb_storage_command_phase phase[3];
};
struct usb_storage_content_timing {
    uint64_t valid,active,invalid;
    struct usb_storage_content_role roles[USB_STORAGE_CONTENT_ROLES];
    struct usb_storage_command_timing command;
};
bool usb_storage_content_begin(void);
void usb_storage_content_end(struct usb_storage_content_timing *out);
/* ExFAT labels the already-owned window independently of SYS_SYNC. No hook
 * enables observation, transfers media, samples a clock or grants I/O authority.
 * Return an opaque previous-role/depth token. No-role is a harmless no-op.
 * Invalid nesting only invalidates observations; transport work is unchanged. */
unsigned usb_storage_content_role_begin(unsigned role);
void usb_storage_content_role_end(unsigned previous);
struct usb_storage_timing {
    uint64_t calls[3], us[3], failures;
    struct usb_storage_phase_timing phase[3];
    /* Auxiliary sync observer only; software wall, no command addresses. */
    uint64_t checkpoint_calls,checkpoint_us;
    struct usb_storage_csw_timing csw;
    bool valid;
};
bool usb_storage_timing_enabled(void);
bool usb_storage_timing_begin(void);
void usb_storage_timing_end(struct usb_storage_timing *out);
/* Second fixed observer for SYS_SYNC; independent of legacy logger windows.
 * A second auxiliary begin refuses; either end leaves the other scope intact.
 * Both consume the same single callback/controller observation, no extra I/O. */
bool usb_storage_timing_aux_begin(void);
void usb_storage_timing_aux_end(struct usb_storage_timing *out);
/* CPU0: permanently fail/invalidate first, then detach published transport and
 * diagnostic callbacks only after initializer/request/diagnostic owners drain.
 * Retry false.
 * Geometry/name/claims remain as an offline tombstone. The storage record/name
 * stays boot-lifetime; true permits transport DMA release, not record free. */
bool usb_storage_retire(struct usb_storage *storage);
bool usb_storage_is_device(struct block_device *device);
void usb_storage_print_status(struct block_device *device);
bool usb_storage_enable_volume_writes(struct block_device *device);
bool usb_storage_disable_volume_writes(struct block_device *device);
bool usb_storage_attach(struct usb_storage *, void *, usb_bulk_fn, bool writable);
#endif
