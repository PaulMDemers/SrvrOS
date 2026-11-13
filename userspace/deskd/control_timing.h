#ifndef DESKD_CONTROL_TIMING_H
#define DESKD_CONTROL_TIMING_H
#include <stdint.h>
#include <srvros/scheduler_abi.h>

/* Copied only with the enclosing current/worst loop. Boundaries partition the
 * existing control wall interval; inner clocks are subsets, never extra time. */
enum control_region { CONTROL_REPORT, CONTROL_AUDIO, CONTROL_IO, CONTROL_REGIONS };
struct loop_control_record {
    uint64_t at[5],valid,next,tracing;
    struct srv_sched_trace scheduler[CONTROL_REGIONS];
    uint64_t due,state_checked,report_state,snapshot_called,snapshot_begin,snapshot_end;
    uint64_t submit_called,submit_begin,submit_end,accepted;
    uint64_t audio_enabled,finished_before,finished_after,child_before,child_after;
    uint64_t wait_called,wait_begin,wait_end,wait_pid,wait_flags;
    int64_t wait_result;
    uint64_t notice_called,notice_begin,notice_end,io_enabled;
};
static inline void control_record_begin(struct loop_control_record *r,uint64_t now) {
    *r=(struct loop_control_record){.valid=now>0 && now<INT64_MAX,.next=1};r->at[0]=now;
}
static inline void control_record_mark(struct loop_control_record *r,unsigned point,uint64_t now) {
    if(point!=r->next || point>=5 || now<r->at[point-1] || now>=INT64_MAX) {r->valid=0;return;}
    r->at[point]=now;r->next++;
}
static inline int control_record_complete(const struct loop_control_record *r) {
    return r->valid && r->next==5 && !r->tracing;
}
#endif
