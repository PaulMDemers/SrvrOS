#ifndef SRVROS_SYNC_TIMING_H
#define SRVROS_SYNC_TIMING_H
#include <stdint.h>
#include <stdbool.h>
#include <srvros/usb_storage.h>
#define SYNC_TIMING_RECORDS 8u
struct sync_timing_call {
    uint64_t start_us,files_end_us,end_us,file_failed,device_failed;
    struct usb_storage_timing usb[2];
};
struct sync_timing_record {
    uint64_t pid,calls,total_us,files_us,devices_us,errors,conflicts,invalid;
    struct sync_timing_call worst;
};
struct sync_timing_snapshot {
    uint64_t enabled,active,invalid,dropped;
    struct sync_timing_record records[SYNC_TIMING_RECORDS];
};
/* CPU0 opt-in software wall observations only; no transfers, allocation,
 * logging, scheduler/flush policy or userspace ABI. Records last until reboot. */
bool sync_timing_enable(void);
bool sync_timing_enabled(void);
bool sync_timing_begin(uint64_t pid);
void sync_timing_files_done(bool ok);
void sync_timing_end(bool ok);
void sync_timing_snapshot(struct sync_timing_snapshot *out);
#endif
