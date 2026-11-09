#include <srvros/sync_timing.h>
#include <srvros/timer.h>
static struct sync_timing_snapshot observed;
static struct sync_timing_call current;
static unsigned slot,phase;
static bool tracing;
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
void sync_timing_files_done(bool ok) {
    uint64_t f=lock();
    if(!observed.active || phase!=0) {observed.invalid=1;unlock(f);return;}
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
