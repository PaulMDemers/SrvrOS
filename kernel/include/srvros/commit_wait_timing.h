#ifndef SRVROS_COMMIT_WAIT_TIMING_H
#define SRVROS_COMMIT_WAIT_TIMING_H
#include <srvros/sync_timing.h>
/* Full original command validation, distinct from bulk callback success.
 * Phase clocks are optional: outside content tracing only exact CSW anchors
 * and CDB context are available. No clock or device operation in this sink. */
struct commit_command_anchor { uint64_t start_us,end_us; };
struct commit_command_timing {
    uint64_t available,phases,tag,opcode,lba,blocks,sector_size,flags;
    struct usb_storage_command_timing timing;
};
/* Actual scheduler owner intersection. end_us=0/released=0 means still live at
 * wait return, never a fabricated release. Status counters cover only already-
 * timed completed callbacks; whole callback wall may begin before wait entry. */
struct commit_owner_timing {
    uint64_t seq,thread,pid,syscall,start_us,end_us,released,overlap_start,overlap_end;
    uint64_t csws,csw_us,csw_overlap_us,longs,write512,write512_us,write16k,write16k_us,tag;
    struct usb_storage_csw_timing status;
    struct commit_command_timing command;
    char name[32];
};
/* Completed content calls intersecting one private append prelude. The chosen
 * call has the longest clipped overlap, first on ties. Whole-call counters may
 * include work before wait entry; they are not exclusive blocked/device time. */
struct commit_wait_record {
    uint64_t pid,start_us,end_us,target_identity,retry,open_at_end;
    uint64_t target_base,target_size,request_bytes,borrowed;
    uint64_t calls,overlap_us,overlap_start_us,overlap_end_us,parent_pid,parent_id;
    uint64_t owner_calls,owned_us;
    struct commit_owner_timing owner,live_owner;
    struct sync_content_call content;
};
struct commit_wait_snapshot {
    uint64_t enabled,invalid,active,waits;
    struct commit_wait_record worst;
};
#if defined(SRVROS_PROCESS_HOST_TEST) && !defined(SRVROS_COMMIT_WAIT_HOST_TEST) && !defined(SRVROS_SYNC_CONTENT_HOST_TEST)
static inline bool sync_commit_wait_begin(uint64_t pid,uint64_t start,uint64_t target,uint64_t base,uint64_t size,uint64_t bytes,bool borrowed) {
    (void)pid;(void)start;(void)target;(void)base;(void)size;(void)bytes;(void)borrowed;return false;
}
static inline void sync_commit_wait_end(uint64_t pid,uint64_t start,uint64_t end,bool retry) {
    (void)pid;(void)start;(void)end;(void)retry;
}
#else
/* Caller already holds CPU0 IRQ exclusion. No scheduling, I/O, pointer retention
 * or eligibility grant. Start/end reuse the original aggregate wait clocks. */
bool sync_commit_wait_begin(uint64_t pid,uint64_t start,uint64_t target,uint64_t base,uint64_t size,uint64_t bytes,bool borrowed);
void sync_commit_wait_end(uint64_t pid,uint64_t start,uint64_t end,bool retry);
#endif
void sync_commit_wait_snapshot(struct commit_wait_snapshot *out);
#if (defined(SRVROS_SCHEDULER_HOST_TEST) || defined(SRVROS_USB_STORAGE_HOST_TEST)) && !defined(SRVROS_COMMIT_WAIT_HOST_TEST)
static inline bool sync_commit_wait_status_enabled(void) {return false;}
static inline void sync_commit_owner_begin(uint64_t seq,uint64_t thread,uint64_t pid,uint64_t syscall,uint64_t start,const char *name) {
    (void)seq;(void)thread;(void)pid;(void)syscall;(void)start;(void)name;
}
static inline void sync_commit_owner_end(uint64_t seq,uint64_t thread,uint64_t start,uint64_t end) {
    (void)seq;(void)thread;(void)start;(void)end;
}
static inline void sync_commit_wait_status(uint64_t tag,uint64_t opcode,uint64_t bytes,uint64_t start,uint64_t end,uint64_t requested,uint64_t actual,bool ok,const struct usb_bulk_timing *sample) {
    (void)tag;(void)opcode;(void)bytes;(void)start;(void)end;(void)requested;(void)actual;(void)ok;(void)sample;
}
static inline void sync_commit_wait_command(const struct commit_command_timing *command) {(void)command;}
#else
bool sync_commit_wait_status_enabled(void);
void sync_commit_owner_begin(uint64_t seq,uint64_t thread,uint64_t pid,uint64_t syscall,uint64_t start,const char *name);
void sync_commit_owner_end(uint64_t seq,uint64_t thread,uint64_t start,uint64_t end);
void sync_commit_wait_status(uint64_t tag,uint64_t opcode,uint64_t bytes,uint64_t start,uint64_t end,uint64_t requested,uint64_t actual,bool ok,const struct usb_bulk_timing *sample);
void sync_commit_wait_command(const struct commit_command_timing *command);
#endif
#endif
