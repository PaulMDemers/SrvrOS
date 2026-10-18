#include <srvros/thermal.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>
static struct srv_thermal_status snapshot={.version=SRV_THERMAL_VERSION};
static bool initialized,package_supported;
static uint64_t next_tick;
static struct scheduler_wait_queue wake;
static uint64_t lock(void) { uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f; }
static void unlock(uint64_t f) { if(f&(1ull<<9)) __asm__ volatile("sti":::"memory"); }
static void cpuid(unsigned leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) { __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0)); }
static uint64_t read_msr(unsigned n) { uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32); }
static void sample(void) {
    struct srv_thermal_status next=snapshot;
    thermal_decode(&next,read_msr(0x1a2),read_msr(0x19c),package_supported?read_msr(0x1b1):0,package_supported);
    next.sampled_us=timer_monotonic_us();next.samples++;
    uint64_t f=lock();snapshot=next;next_tick=timer_ticks()+100;unlock(f);
}
static bool due(void *arg) { (void)arg;return timer_ticks()>=next_tick; }
static void worker(void *arg) { (void)arg;for(;;) { scheduler_wait_timeout(&wake,due,0,next_tick);if(due(0)) sample(); } }
void thermal_init(void) {
    if(initialized) return;
    initialized=true;snapshot.state=SRV_THERMAL_UNAVAILABLE;
    uint32_t max,b,c,d,a,signature,fc,fd,vb,vc,vd;
    cpuid(0,&max,&vb,&vc,&vd);
    if(max<6 || !timer_reference_ready()) goto done;
    cpuid(1,&signature,&b,&fc,&fd);cpuid(6,&a,&b,&c,&d);
    snapshot.cpu_model=((signature>>4)&15)|((signature>>12)&0xf0);
    if(!thermal_cpu_supported(vb,vd,vc,signature,fc,fd,a)) goto done;
    package_supported=(a&64)!=0;sample();
    if(scheduler_spawn_io_service("thermal",worker,0,due)) snapshot.state=SRV_THERMAL_ACTIVE;
done:
    thermal_print_status();
}
void thermal_status(struct srv_thermal_status *out) { if(out) { uint64_t f=lock();*out=snapshot;unlock(f); } }
void thermal_print_status(void) {
    struct srv_thermal_status s;thermal_status(&s);
    console_printf("thermal: state=%u model=%x flags=%x core_mc=%d package_mc=%d target_mc=%d sample_us=%u samples=%u\n",
        (uint64_t)s.state,(uint64_t)s.cpu_model,(uint64_t)s.flags,(int64_t)s.core_mc,(int64_t)s.package_mc,(int64_t)s.target_mc,s.sampled_us,s.samples);
    console_printf("thermal: target=%x core=%x package=%x (read-only, boot CPU; native readings NOT_TESTED)\n",s.raw_target,s.raw_core,s.raw_package);
}
