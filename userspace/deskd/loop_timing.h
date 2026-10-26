#ifndef DESKD_LOOP_TIMING_H
#define DESKD_LOOP_TIMING_H
#include <stdint.h>
#include <string.h>
#include <srvros/scheduler_abi.h>

/* One current interval and one retained maximum: no allocation or unbounded
 * event history. Stages measure wall time, including scheduling and interrupts. */
enum loop_stage { LOOP_CONTROL, LOOP_MESSAGES, LOOP_LIFECYCLE, LOOP_SERVICE,
    LOOP_TRIAL, LOOP_INPUT, LOOP_RENDER, LOOP_POST, LOOP_WAIT, LOOP_STAGE_COUNT };
struct loop_wait_record {
    uint64_t valid,begin,gui_begin,gui_end,sleep_begin,sleep_end,end;
    int64_t gui_result,sleep_result;
    struct srv_sched_trace scheduler;
};
struct loop_record {
    uint64_t start,end,gap,sequence,cycle,pid,phase,end_cycle,end_pid,end_phase;
    uint64_t stage_us[LOOP_STAGE_COUNT];
    struct loop_wait_record wait;
};
struct loop_trace {
    struct loop_record current,worst;
    uint64_t cursor,samples,over_limit,invalid;
    unsigned stage,active;
};
static inline void loop_trace_begin(struct loop_trace *t,uint64_t now,
                                    uint64_t cycle,uint64_t pid,unsigned phase) {
    if(t->active) { t->invalid++;return; }
    t->current=(struct loop_record){.start=now,.cycle=cycle,.pid=pid,.phase=phase};
    t->cursor=now;t->stage=LOOP_CONTROL;t->active=1;
}
static inline void loop_trace_mark(struct loop_trace *t,unsigned stage,uint64_t now) {
    if(!t->active)return;
    if(stage<=t->stage || stage>=LOOP_STAGE_COUNT || now<t->cursor) {
        t->invalid++;t->active=0;return;
    }
    t->current.stage_us[t->stage]+=now-t->cursor;
    t->cursor=now;t->stage=stage;
}
static inline void loop_trace_end(struct loop_trace *t,uint64_t now,
                                  uint64_t cycle,uint64_t pid,unsigned phase) {
    if(!t->active)return;
    t->active=0;
    if(now<t->cursor) {t->invalid++;return;}
    t->current.stage_us[t->stage]+=now-t->cursor;
    t->current.end=now;t->current.gap=now-t->current.start;
    t->current.end_cycle=cycle;t->current.end_pid=pid;t->current.end_phase=phase;
    t->current.sequence=++t->samples;
    if(t->current.gap>500000)t->over_limit++;
    if(t->current.gap>t->worst.gap)t->worst=t->current;
}
#endif
