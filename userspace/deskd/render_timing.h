#ifndef DESKD_RENDER_TIMING_H
#define DESKD_RENDER_TIMING_H
#include <stdint.h>
#include <srvros/scheduler_abi.h>

/* Diagnostic-only bounded state. Copied with its enclosing worst loop, so a
 * later frame cannot replace attribution for an earlier retained interval. */
struct loop_render_record {
    uint64_t at[5],valid,next,damage,scale,capture_id,capture_busy,callback_end;
    uint64_t compose_us,upload_us,compose_calls,upload_calls,compose_pixels,upload_pixels;
    struct srv_sched_trace scheduler;
};
struct loop_capture_record {
    uint64_t begin,end,valid,id_before,id_after,busy_before,busy_after,pending_before,pending_after;
    struct srv_sched_trace scheduler;
};
static inline void render_record_begin(struct loop_render_record *r,uint64_t now) {
    *r=(struct loop_render_record){.valid=now>0 && now<INT64_MAX,.next=1};r->at[0]=now;
}
static inline void render_record_mark(struct loop_render_record *r,unsigned phase,uint64_t now) {
    if(!r->valid)return;
    if(phase!=r->next || phase>=5 || now<r->at[phase-1] || now>=INT64_MAX || (phase==2 && now<r->callback_end)) {r->valid=0;return;}
    r->at[phase]=now;r->next++;
}
static inline void render_record_rect(struct loop_render_record *r,int upload,
                                      uint64_t begin,uint64_t end,uint64_t pixels) {
    if(!r->valid)return;
    uint64_t *time=upload?&r->upload_us:&r->compose_us;
    uint64_t *calls=upload?&r->upload_calls:&r->compose_calls;
    uint64_t *area=upload?&r->upload_pixels:&r->compose_pixels;
    if(r->next!=2 || !begin || begin<r->at[1] || begin<r->callback_end || end<begin || end>=INT64_MAX ||
       *time>UINT64_MAX-(end-begin) || *calls==UINT64_MAX || *area>UINT64_MAX-pixels) {r->valid=0;return;}
    *time+=end-begin;(*calls)++;*area+=pixels;r->callback_end=end;
}
static inline int render_record_complete(const struct loop_render_record *r) {
    return r->valid && r->next==5 && r->compose_calls==r->damage && r->upload_calls==r->damage &&
        r->compose_us<=r->at[2]-r->at[1] && r->upload_us<=r->at[2]-r->at[1]-r->compose_us;
}
#endif
