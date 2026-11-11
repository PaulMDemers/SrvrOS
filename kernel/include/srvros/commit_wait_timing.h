#ifndef SRVROS_COMMIT_WAIT_TIMING_H
#define SRVROS_COMMIT_WAIT_TIMING_H
#include <srvros/sync_timing.h>
/* Completed content calls intersecting one private append prelude. The chosen
 * call has the longest clipped overlap, first on ties. Whole-call counters may
 * include work before wait entry; they are not exclusive blocked/device time. */
struct commit_wait_record {
    uint64_t pid,start_us,end_us,target_identity,retry,open_at_end;
    uint64_t target_base,target_size,request_bytes,borrowed;
    uint64_t calls,overlap_us,overlap_start_us,overlap_end_us,parent_pid,parent_id;
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
#endif
