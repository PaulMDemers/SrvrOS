/* Same CPU0 observer lock as sync_timing.c. No state is borrowed from a waiter. */
static struct commit_wait_record commit_wait_live[SYNC_TIMING_RECORDS];
static struct commit_wait_snapshot commit_wait_retained;
static struct commit_owner_timing commit_owner_active;
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
    if(commit_owner_active.thread) {
        commit_wait_live[free].live_owner=commit_owner_active;
        commit_wait_live[free].live_owner.overlap_start=start;
        if(commit_owner_active.start_us>start)commit_wait_retained.invalid=1;
    }
    commit_wait_retained.active++;unlock(f);return true;
}
static void commit_owner_select(struct commit_wait_record *w,uint64_t end,bool released) {
    struct commit_owner_timing *o=&w->live_owner;
    if(!o->thread || end<=w->start_us)return;
    if(end<o->overlap_start || o->start_us>o->overlap_start) {commit_wait_retained.invalid=1;return;}
    o->overlap_end=end;o->released=released;o->end_us=released?end:0;
    uint64_t us=end-o->overlap_start;
    commit_wait_add(&w->owner_calls,1);commit_wait_add(&w->owned_us,us);
    if(!w->owner.thread || us>w->owner.overlap_end-w->owner.overlap_start)w->owner=*o;
}
void sync_commit_owner_begin(uint64_t seq,uint64_t thread,uint64_t pid,uint64_t syscall,uint64_t start,const char *name) {
    if(!observed.enabled)return;
    uint64_t f=lock();
    if(commit_owner_active.thread || !seq || !thread || !start)commit_wait_retained.invalid=1;
    commit_owner_active=(struct commit_owner_timing){.seq=seq,.thread=thread,.pid=pid,.syscall=syscall,.start_us=start};
    if(!name || !name[0])name="unknown";
    for(unsigned j=0;name && j<sizeof(commit_owner_active.name)-1 && name[j];j++) {
        unsigned char c=name[j];commit_owner_active.name[j]=c>32 && c<127?c:'_';
    }
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++)if(commit_wait_live[i].pid) {
        if(commit_wait_live[i].live_owner.thread)commit_wait_retained.invalid=1;
        commit_wait_live[i].live_owner=commit_owner_active;
        commit_wait_live[i].live_owner.overlap_start=start;
        if(start<commit_wait_live[i].start_us)commit_wait_retained.invalid=1;
    }
    unlock(f);
}
void sync_commit_owner_end(uint64_t seq,uint64_t thread,uint64_t start,uint64_t end) {
    if(!observed.enabled)return;
    uint64_t f=lock();
    /* Enable may occur inside an existing unobserved scope. Do not invent it. */
    if(!commit_owner_active.thread) {unlock(f);return;}
    if(commit_owner_active.seq!=seq || commit_owner_active.thread!=thread ||
       commit_owner_active.start_us!=start || end<start)commit_wait_retained.invalid=1;
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++)if(commit_wait_live[i].pid) {
        struct commit_wait_record *w=&commit_wait_live[i];
        if(w->live_owner.seq!=seq || w->live_owner.thread!=thread || w->live_owner.start_us!=start)
            commit_wait_retained.invalid=1;
        commit_owner_select(w,end,true);w->live_owner=(struct commit_owner_timing){0};
    }
    commit_owner_active=(struct commit_owner_timing){0};unlock(f);
}
void sync_commit_wait_status(uint64_t tag,uint64_t opcode,uint64_t bytes,uint64_t start,uint64_t end,uint64_t requested,uint64_t actual,bool ok,const struct usb_bulk_timing *sample) {
    if(!commit_wait_retained.active || !commit_owner_active.thread)return;
    uint64_t f=lock();
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        struct commit_wait_record *w=&commit_wait_live[i];
        if(!w->pid || end<=w->start_us)continue;
        struct commit_owner_timing *o=&w->live_owner;
        if(!sample || o->seq!=commit_owner_active.seq || o->thread!=commit_owner_active.thread ||
           o->start_us>start || end<start) {commit_wait_retained.invalid=1;continue;}
        uint64_t clipped=end-(start>w->start_us?start:w->start_us);
        uint64_t previous=o->status.start_us>w->start_us?o->status.start_us:w->start_us;
        bool peak=!o->csws || clipped>o->status.end_us-previous;
        commit_wait_add(&o->csws,1);commit_wait_add(&o->csw_us,end-start);
        commit_wait_add(&o->csw_overlap_us,clipped);
        if(end-start>100000)commit_wait_add(&o->longs,1);
        if(opcode==0x2a && bytes==512) {commit_wait_add(&o->write512,1);commit_wait_add(&o->write512_us,end-start);}
        if(opcode==0x2a && bytes==16384) {commit_wait_add(&o->write16k,1);commit_wait_add(&o->write16k_us,end-start);}
        if(peak) {
            o->tag=tag;o->status=(struct usb_storage_csw_timing){.start_us=start,.end_us=end,
                .opcode=opcode,.command_bytes=bytes,.request_bytes=requested,.actual=actual,.ok=ok,.sample=*sample};
        }
    }
    unlock(f);
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
        commit_owner_select(w,end,false);
        w->end_us=end;w->retry=retry;w->open_at_end=content_active;
        if(end<start || w->overlap_us>end-start || w->overlap_end_us>end || w->owned_us>end-start)
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
