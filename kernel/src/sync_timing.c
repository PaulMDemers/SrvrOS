#include <srvros/sync_timing.h>
#include <srvros/timer.h>
static struct sync_timing_snapshot observed;
static struct sync_timing_call current;
static unsigned slot,phase;
static bool tracing,content_active,commands_active;
static uint64_t content_identity;
static struct sync_content_call content_current;
static uint64_t lock(void) {
#ifdef SRVROS_SYNC_TIMING_HOST_TEST
    return sync_test_irq_save();
#else
    uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_SYNC_TIMING_HOST_TEST
    sync_test_irq_restore(f);
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static void add(struct sync_timing_record *r,uint64_t *v,uint64_t n) {
    if(*v>UINT64_MAX-n)r->invalid=1;else *v+=n;
}
bool sync_timing_enable(void) {
    uint64_t f=lock();bool ok=!observed.enabled && timer_reference_ready();
    if(ok)observed.enabled=1;
    unlock(f);return ok;
}
bool sync_timing_enabled(void) { return observed.enabled!=0; }
static void start_usb(void) {
    tracing=usb_storage_timing_aux_begin();
    if(!tracing) {struct sync_timing_record *r=&observed.records[slot];add(r,&r->conflicts,1);r->invalid=1;}
}
static void end_usb(void) {
    if(tracing)usb_storage_timing_aux_end(&current.usb[phase]);
    tracing=false;
    if(!current.usb[phase].valid)observed.records[slot].invalid=1;
}
bool sync_timing_begin(uint64_t pid) {
    uint64_t f=lock();
    if(!observed.enabled){unlock(f);return false;}
    if(observed.active || !pid || !timer_reference_ready()) {observed.invalid=1;unlock(f);return false;}
    unsigned free_slot=SYNC_TIMING_RECORDS,index=SYNC_TIMING_RECORDS;
    for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++) {
        if(observed.records[i].pid==pid){index=i;break;}
        if(!observed.records[i].pid && free_slot==SYNC_TIMING_RECORDS)free_slot=i;
    }
    if(index==SYNC_TIMING_RECORDS)index=free_slot;
    if(index==SYNC_TIMING_RECORDS) {
        if(observed.dropped==UINT64_MAX)observed.invalid=1;else observed.dropped++;
        unlock(f);return false;
    }
    slot=index;observed.records[slot].pid=pid;observed.active=1;phase=0;
    current=(struct sync_timing_call){.start_us=timer_monotonic_us()};
    if(!current.start_us)observed.records[slot].invalid=1;
    start_usb();unlock(f);return true;
}
void sync_timing_content_fault(void) {
    if(!content_active)return;
    uint64_t f=lock();if(content_active)observed.records[slot].invalid=1;unlock(f);
}
uint64_t sync_timing_content_identity(void) {
    uint64_t f=lock(),id=0;
    if(content_identity!=UINT64_MAX)id=++content_identity;
    else observed.invalid=1;
    unlock(f);return id;
}
bool sync_timing_content_begin(uint64_t index,uint64_t identity,uint64_t inode,uint64_t size) {
    if(!observed.enabled || !observed.active)return false;
    uint64_t f=lock();
    if(!observed.enabled || !observed.active) {unlock(f);return false;}
    if(phase || content_active || !identity) {observed.invalid=1;unlock(f);return false;}
    content_current=(struct sync_content_call){.slot=index,.identity=identity,.inode=inode,
        .size=size,.start_us=timer_monotonic_us()};
    if(!content_current.start_us || content_current.start_us<current.start_us)observed.invalid=1;
    commands_active=tracing && usb_storage_content_begin();
    if(!commands_active)observed.records[slot].invalid=1;
    content_active=true;unlock(f);return true;
}
unsigned sync_timing_content_role_begin(unsigned role) {
    if(!content_active || !commands_active)return ~0u;
    return usb_storage_content_role_begin(role);
}
void sync_timing_content_role_end(unsigned token) {
    if(token==~0u)return;
    if(!content_active || !commands_active) {observed.invalid=1;return;}
    usb_storage_content_role_end(token);
}
void sync_timing_content_io(unsigned role,unsigned kind,uint64_t bytes,uint64_t us,bool ok) {
    if(!content_active)return;
    uint64_t f=lock();
    if(!content_active) {unlock(f);return;}
    struct sync_timing_record *r=&observed.records[slot];
    if(role>=SYNC_CONTENT_ROLES || kind>2) {r->invalid=1;unlock(f);return;}
    struct sync_content_io *p=&content_current.io[role];
    if(kind==0) {add(r,&p->reads,1);add(r,&p->read_bytes,bytes);add(r,&p->read_us,us);}
    else if(kind==1) {add(r,&p->writes,1);add(r,&p->write_bytes,bytes);add(r,&p->write_us,us);}
    else {if(bytes)r->invalid=1;add(r,&p->flushes,1);add(r,&p->flush_us,us);}
    if(!ok)add(r,&p->failures,1);
    unlock(f);
}
static bool content_prefix_valid(const struct sync_content_prefix *p,bool ok,bool dirty_after) {
    if(!p || p->valid!=1 || p->end_generation<p->start_generation ||
       p->live_size<content_current.size || p->live_size>16u*1024u*1024u ||
       p->committed_size>16u*1024u*1024u ||
       p->append_calls!=p->end_generation-p->start_generation ||
       p->append_bytes!=p->live_size-content_current.size ||
       ((!p->append_calls)!=(!p->append_bytes)) ||
       (p->append_bytes && (p->append_bytes-1)/65536u>=p->append_calls))return false;
    if(ok)return p->committed_size==content_current.size && dirty_after==(p->append_calls!=0);
    return dirty_after;
}
static void content_end(bool ok,bool dirty_after,const struct sync_content_prefix *prefix,bool snapshot) {
    uint64_t f=lock();
    if(!content_active || !observed.active || phase) {observed.invalid=1;unlock(f);return;}
    struct sync_timing_record *r=&observed.records[slot];
    if(snapshot) {
        if(!content_prefix_valid(prefix,ok,dirty_after))r->invalid=1;
        else content_current.prefix=*prefix;
    }
    if(commands_active) {
        usb_storage_content_end(&content_current.commands);
        if(!content_current.commands.valid || content_current.commands.active || content_current.commands.invalid)r->invalid=1;
    }
    commands_active=false;
    content_current.end_us=timer_monotonic_us();content_current.ok=ok;content_current.dirty_after=dirty_after;
    struct sync_content_summary *c=&current.content;
    add(r,&c->calls,1);add(r,&c->bytes,content_current.size);if(!ok)add(r,&c->failures,1);
    if(content_current.end_us<content_current.start_us)r->invalid=1;
    else if(!c->worst.start_us || content_current.end_us-content_current.start_us>c->worst.end_us-c->worst.start_us)c->worst=content_current;
    content_active=false;content_current=(struct sync_content_call){0};unlock(f);
}
void sync_timing_content_end(bool ok,bool dirty_after) { content_end(ok,dirty_after,0,false); }
void sync_timing_content_end_snapshot(bool ok,bool dirty_after,const struct sync_content_prefix *prefix) {
    content_end(ok,dirty_after,prefix,true);
}
void sync_timing_files_done(bool ok) {
    uint64_t f=lock();
    if(!observed.active || phase!=0) {observed.invalid=1;unlock(f);return;}
    if(content_active)observed.invalid=1;
    end_usb();current.files_end_us=timer_monotonic_us();current.file_failed=!ok;
    phase=1;start_usb();unlock(f);
}
void sync_timing_end(bool ok) {
    uint64_t f=lock();
    if(!observed.active || phase!=1) {observed.invalid=1;unlock(f);return;}
    end_usb();current.end_us=timer_monotonic_us();current.device_failed=!ok;
    struct sync_timing_record *r=&observed.records[slot];add(r,&r->calls,1);
    if(current.file_failed || current.device_failed)add(r,&r->errors,1);
    if(!current.start_us || current.files_end_us<current.start_us || current.end_us<current.files_end_us)r->invalid=1;
    else {
        add(r,&r->total_us,current.end_us-current.start_us);
        add(r,&r->files_us,current.files_end_us-current.start_us);
        add(r,&r->devices_us,current.end_us-current.files_end_us);
        if(!r->worst.start_us || current.end_us-current.start_us>r->worst.end_us-r->worst.start_us)r->worst=current;
    }
    observed.active=0;current=(struct sync_timing_call){0};unlock(f);
}
void sync_timing_snapshot(struct sync_timing_snapshot *out) {
    if(!out)return;
    uint64_t f=lock();*out=observed;unlock(f);
}
