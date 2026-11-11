/* Same CPU0 observer lock as sync_timing.c. No state is borrowed from a waiter. */
static struct commit_wait_record commit_wait_live[SYNC_TIMING_RECORDS];
static struct commit_wait_snapshot commit_wait_retained;
static void commit_wait_add(uint64_t *v,uint64_t n) {
    if(*v>UINT64_MAX-n)commit_wait_retained.invalid=1;else *v+=n;
}
bool sync_commit_wait_begin(uint64_t pid,uint64_t start,uint64_t target,uint64_t base,uint64_t size,uint64_t bytes,bool borrowed) {
    if(!observed.enabled)return false;
    uint64_t f=lock();unsigned free=SYNC_TIMING_RECORDS;
    if(!pid || !start || !target || base>size || size>16u*1024u*1024u ||
       !bytes || bytes>65536 || bytes>16u*1024u*1024u-size || size+bytes-base<=262144) {commit_wait_retained.invalid=1;unlock(f);return false;}
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        if(commit_wait_live[i].pid==pid) {commit_wait_retained.invalid=1;unlock(f);return false;}
        if(!commit_wait_live[i].pid && free==SYNC_TIMING_RECORDS)free=i;
    }
    if(free==SYNC_TIMING_RECORDS) {commit_wait_retained.invalid=1;unlock(f);return false;}
    commit_wait_live[free]=(struct commit_wait_record){.pid=pid,.start_us=start,.target_identity=target,
        .target_base=base,.target_size=size,.request_bytes=bytes,.borrowed=borrowed};
    commit_wait_retained.active++;unlock(f);return true;
}
/* Called after the exact content's command observer closes, before it is reset
 * or considered for the unrelated global maximum. A PID/nearby timestamp is
 * never substituted. Unfinished calls at wait return remain explicitly absent. */
static void commit_wait_content_end(void) {
    if(!commit_wait_retained.active)return;
    const struct sync_content_call *c=&content_current;
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        struct commit_wait_record *w=&commit_wait_live[i];
        if(!w->pid || c->end_us<=w->start_us)continue;
        if(observed.invalid || observed.records[slot].invalid || c->end_us<c->start_us ||
           !c->start_us || !current.start_us || current.start_us>c->start_us) {
            commit_wait_retained.invalid=1;continue;
        }
        uint64_t start=c->start_us>w->start_us?c->start_us:w->start_us;
        commit_wait_add(&w->calls,1);commit_wait_add(&w->overlap_us,c->end_us-start);
        if(!w->content.start_us || c->end_us-start>w->overlap_end_us-w->overlap_start_us) {
            w->parent_pid=observed.records[slot].pid;w->parent_id=current.start_us;
            w->overlap_start_us=start;w->overlap_end_us=c->end_us;w->content=*c;
        }
    }
}
void sync_commit_wait_end(uint64_t pid,uint64_t start,uint64_t end,bool retry) {
    uint64_t f=lock();
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        struct commit_wait_record *w=&commit_wait_live[i];
        if(w->pid!=pid || w->start_us!=start)continue;
        w->end_us=end;w->retry=retry;w->open_at_end=content_active;
        if(end<start || w->overlap_us>end-start || w->overlap_end_us>end)
            commit_wait_retained.invalid=1;
        else if(!commit_wait_retained.worst.pid || end-start>
                commit_wait_retained.worst.end_us-commit_wait_retained.worst.start_us)
            commit_wait_retained.worst=*w;
        commit_wait_add(&commit_wait_retained.waits,1);
        *w=(struct commit_wait_record){0};commit_wait_retained.active--;unlock(f);return;
    }
    commit_wait_retained.invalid=1;unlock(f);
}
void sync_commit_wait_snapshot(struct commit_wait_snapshot *out) {
    if(!out)return;
    uint64_t f=lock();*out=commit_wait_retained;out->enabled=observed.enabled;unlock(f);
}
