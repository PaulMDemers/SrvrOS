#ifndef SRVROS_CAPTURE_WRITE_TIMING_STATE_H
#define SRVROS_CAPTURE_WRITE_TIMING_STATE_H
#include <srvros/capture_write_timing.h>
static inline struct capture_write_record *capture_write_find(struct capture_write_snapshot *s,uint64_t pid) {
    if(!s->enabled || !pid)return 0;
    for(unsigned i=0;i<CAPTURE_WRITE_RECORDS;i++)if(s->records[i].pid==pid)return &s->records[i];
    return 0;
}
static inline struct capture_write_record *capture_write_touch(struct capture_write_snapshot *s,uint64_t pid,uint64_t now) {
    struct capture_write_record *r=capture_write_find(s,pid);if(r)return r;
    if(!s->enabled || !pid)return 0;
    for(unsigned i=0;i<CAPTURE_WRITE_RECORDS;i++)if(!s->records[i].pid) {
        r=&s->records[i];r->pid=pid;r->first_us=r->last_us=now;return r;
    }
    if(s->dropped!=UINT64_MAX)s->dropped++;else s->invalid=1;
    return 0;
}
static inline bool capture_write_add(uint64_t *out,uint64_t n) {
    if(*out>UINT64_MAX-n)return false;
    *out+=n;return true;
}
static inline void capture_write_sample(struct capture_write_record *r,unsigned site,uint64_t start,uint64_t end) {
    if(!r)return;
    if(r->retired || site>=CAP_WRITE_SITE_COUNT || !start || end<start || end<r->last_us) {r->invalid=1;return;}
    struct capture_write_sample *p=&r->site[site];uint64_t us=end-start;
    if(!capture_write_add(&p->count,1) || !capture_write_add(&p->total_us,us))r->invalid=1;
    if(us>p->max_us)p->max_us=us;
    r->last_us=end;
}
static inline void capture_write_result(struct capture_write_record *r,uint64_t start,uint64_t end,uint64_t requested,int64_t actual) {
    if(!r)return;
    capture_write_sample(r,CAP_WRITE_CALL,start,end);
    if(!capture_write_add(&r->calls,1) || !capture_write_add(&r->requested,requested))r->invalid=1;
    if(requested>65536 || actual>(int64_t)requested)r->invalid=1;
    if(actual<0) {if(!capture_write_add(&r->failures,1))r->invalid=1;}
    else if(!capture_write_add(&r->bytes,(uint64_t)actual))r->invalid=1;
}
static inline void capture_write_fast_result(struct capture_write_record *r,uint64_t requested,int64_t actual) {
    if(!r)return;
    if(r->retired || !requested || requested>65536 || actual>(int64_t)requested) {r->invalid=1;return;}
    if(!capture_write_add(&r->fast_calls,1) || !capture_write_add(&r->fast_requested,requested))r->invalid=1;
    if(requested>r->fast_max_request)r->fast_max_request=requested;
    if(actual<0) {if(!capture_write_add(&r->fast_failures,1))r->invalid=1;}
    else {
        if(!actual)r->invalid=1;
        if(!capture_write_add(&r->fast_bytes,(uint64_t)actual))r->invalid=1;
        if((uint64_t)actual>r->fast_max_bytes)r->fast_max_bytes=(uint64_t)actual;
    }
}
#endif
