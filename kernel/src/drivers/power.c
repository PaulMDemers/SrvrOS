#include <srvros/power.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>

static struct power_snapshot snapshot;
static struct power_sampler sampler;
static bool initialized;
static uint64_t next_tick;
static struct scheduler_wait_queue wake;
#ifdef SRVROS_POWER_HOST_TEST
static uint64_t lock(void) { return 0; }
static void unlock(uint64_t flags) { (void)flags; }
void power_test_cpuid(unsigned,uint32_t *,uint32_t *,uint32_t *,uint32_t *);
uint64_t power_test_read_msr(unsigned);
#define cpuid power_test_cpuid
#define read_msr power_test_read_msr
#else
static uint64_t lock(void) { uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f; }
static void unlock(uint64_t f) { if(f&(1ull<<9)) __asm__ volatile("sti":::"memory"); }
static void cpuid(unsigned leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(0));
}
static uint64_t read_msr(unsigned n) {
    uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));return a|((uint64_t)d<<32);
}
#endif
static void sample(void) {
    /* A handful of CPU-local reads; no sleeping/I/O while IRQs are masked. */
    uint64_t f=lock();
    struct power_raw raw={.energy=read_msr(0x611),.aperf=read_msr(0xe8),.mperf=read_msr(0xe7)};
    raw.us=timer_monotonic_us();power_sample(&sampler,&snapshot,raw);
    next_tick=timer_ticks()+100;unlock(f);
}
static bool due(void *arg) { (void)arg;return timer_ticks()>=next_tick; }
static void worker(void *arg) {
    (void)arg;
    for(;;) { scheduler_wait_timeout(&wake,due,0,next_tick);if(due(0)) sample(); }
}
void power_init(void) {
    if(initialized) return;
    initialized=true;snapshot.state=POWER_UNAVAILABLE;
    uint32_t max,b,c,d,vb,vc,vd,sig,fc,fd,a;
    cpuid(0,&max,&vb,&vc,&vd);
    if(max<6 || !timer_reference_ready()) goto done;
    cpuid(1,&sig,&b,&fc,&fd);cpuid(6,&a,&b,&c,&d);
    if(!power_cpu_supported(vb,vd,vc,sig,fc,fd,c)) goto done;
    uint64_t units=read_msr(0x606),platform=read_msr(0xce);
    if(!power_configure(&snapshot,units,platform)) goto done;
    sample();
    if(scheduler_spawn_io_service("power",worker,0,due)) snapshot.state=POWER_ACTIVE;
done:
    power_print_status();
}
void power_status(struct power_snapshot *out) {
    if(!out) return;
    uint64_t f=lock();*out=snapshot;power_expire(out,timer_monotonic_us());unlock(f);
}
void power_print_status(void) {
    struct power_snapshot s;power_status(&s);
    console_printf("power: state=%u flags=%u sample_us=%u interval_us=%u samples=%u rejected=%u package_uj=%u package_mw=%u active_mhz=%u nominal_mhz=%u\n",
        (uint64_t)s.state,(uint64_t)s.flags,s.sampled_us,s.interval_us,s.samples,s.rejected,s.package_uj,s.package_mw,s.active_mhz,(uint64_t)s.nominal_mhz);
    console_printf("power: units=%x platform=%x energy=%x aperf=%x mperf=%x; read-only package energy / CPU0 active frequency; native acceptance pending\n",
        s.raw_units,s.raw_platform,s.raw.energy,s.raw.aperf,s.raw.mperf);
}
