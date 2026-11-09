#ifndef SRVROS_CAPTURE_GATE_TIMING_STATE_H
#define SRVROS_CAPTURE_GATE_TIMING_STATE_H
#include <srvros/capture_gate_timing.h>
static inline void capture_gate_add(struct capture_gate_counts *c,uint64_t *v,uint64_t n) {
    if(*v>UINT64_MAX-n)c->invalid=1;else *v+=n;
}
static inline void capture_gate_phase(struct capture_gate_counts *c,unsigned phase,uint64_t start,uint64_t end) {
    if(end<start || phase>2){c->invalid=1;return;}
    uint64_t *v=phase==0?&c->running_us:phase==1?&c->blocked_us:&c->ready_us;
    capture_gate_add(c,v,end-start);
}
static inline void capture_gate_complete(struct capture_gate_summary *s,struct capture_gate_wait *w) {
    struct capture_gate_counts *c=&w->counts;
    if(!w->start_us || w->end_us<w->start_us)c->invalid=1;
    else c->wall_us=w->end_us-w->start_us;
    c->calls=1;
#define ADD(field) capture_gate_add(&s->total,&s->total.field,c->field)
    ADD(calls);ADD(wall_us);ADD(owned_us);ADD(scopes);ADD(blocked_us);ADD(ready_us);
    ADD(running_us);ADD(blocks);ADD(dispatches);ADD(exclusions);ADD(invalid);
#undef ADD
    if(!s->worst.counts.calls || c->wall_us>s->worst.counts.wall_us)s->worst=*w;
}
#endif
