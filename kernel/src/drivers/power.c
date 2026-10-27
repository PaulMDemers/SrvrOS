#include <srvros/power.h>
#include <srvros/console.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>

static struct power_snapshot snapshot;
static struct power_sampler sampler;
static bool initialized;
static uint64_t next_tick;
#define POWER_CAPTURE_CAPACITY 24
static struct power_snapshot capture[POWER_CAPTURE_CAPACITY];
static unsigned capture_count;
static bool capturing,capture_overflow;
static uint64_t capture_last;
static struct scheduler_wait_queue wake;
static uint32_t idle_monitor, idle_leaf1_b, idle_leaf5[4];
static uint64_t idle_config_start;
static bool idle_auditing;
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
    if(snapshot.idle.supported) {
        static const unsigned registers[POWER_IDLE_COUNT]={0x3fc,0x3fd,0x3fe,0x60d,0x3f8,0x3f9,0x3fa,0x630,0x631,0x632};
        struct power_idle_raw idle={.us=raw.us,.tsc_begin=read_msr(0x10)};
        for(unsigned i=0;i<POWER_IDLE_COUNT;i++) idle.ticks[i]=read_msr(registers[i]);
        idle.tsc_end=read_msr(0x10);power_idle_sample(&snapshot.idle,idle);
    }
    if(capturing && (raw.us<capture_last || raw.us-capture_last>=5000000)) {
        if(idle_auditing) scheduler_idle_audit_snapshot(&snapshot.idle_entry);
        if(capture_count<POWER_CAPTURE_CAPACITY) capture[capture_count++]=snapshot;
        else capture_overflow=true;
        capture_last=raw.us;
    }
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
    cpuid(1,&sig,&b,&fc,&fd);idle_leaf1_b=b;cpuid(6,&a,&b,&c,&d);
    if(!power_cpu_supported(vb,vd,vc,sig,fc,fd,c)) goto done;
    uint64_t units=read_msr(0x606),platform=read_msr(0xce);
    if(!power_configure(&snapshot,units,platform)) goto done;
    uint32_t ext,ed;
    cpuid(0x80000000,&ext,&b,&c,&d);
    if(ext>=0x80000007) {
        cpuid(0x80000007,&a,&b,&c,&ed);
        if(power_idle_supported(sig,ed)) {
            snapshot.idle.supported=POWER_IDLE_MASK;
            idle_monitor=(fc>>3)&1;
            if(idle_monitor) cpuid(5,&idle_leaf5[0],&idle_leaf5[1],&idle_leaf5[2],&idle_leaf5[3]);
        }
    }
    sample();
    if(scheduler_spawn_io_service("power",worker,0,due)) snapshot.state=POWER_ACTIVE;
done:
    power_print_status();
}
void power_status(struct power_snapshot *out) {
    if(!out) return;
    uint64_t f=lock();*out=snapshot;power_expire(out,timer_monotonic_us());unlock(f);
}
static void print_idle(const char *prefix,const struct power_idle_snapshot *s) {
    console_printf("%s: supported=%u valid=%u rejected=%u sample_us=%u interval_us=%u interval_tsc=%u tsc_begin=%u tsc_end=%u c0_raw=%u c0_bp=%u c1_raw=%u c1_bp=%u c2_raw=%u c2_bp=%u c3_raw=%u c3_bp=%u c4_raw=%u c4_bp=%u c5_raw=%u c5_bp=%u c6_raw=%u c6_bp=%u c7_raw=%u c7_bp=%u c8_raw=%u c8_bp=%u c9_raw=%u c9_bp=%u\n",
        prefix,(uint64_t)s->supported,(uint64_t)s->valid,s->rejected,s->raw.us,s->interval_us,s->interval_tsc,s->raw.tsc_begin,s->raw.tsc_end,
        s->raw.ticks[0],s->basis_points[0],
        s->raw.ticks[1],s->basis_points[1],
        s->raw.ticks[2],s->basis_points[2],
        s->raw.ticks[3],s->basis_points[3],
        s->raw.ticks[4],s->basis_points[4],
        s->raw.ticks[5],s->basis_points[5],
        s->raw.ticks[6],s->basis_points[6],
        s->raw.ticks[7],s->basis_points[7],
        s->raw.ticks[8],s->basis_points[8],
        s->raw.ticks[9],s->basis_points[9]);
}
void power_print_status(void) {
    struct power_snapshot s;power_status(&s);
    console_printf("power: state=%u flags=%u sample_us=%u interval_us=%u samples=%u rejected=%u package_uj=%u package_mw=%u active_mhz=%u nominal_mhz=%u\n",
        (uint64_t)s.state,(uint64_t)s.flags,s.sampled_us,s.interval_us,s.samples,s.rejected,s.package_uj,s.package_mw,s.active_mhz,(uint64_t)s.nominal_mhz);
    console_printf("power: units=%x platform=%x energy=%x aperf=%x mperf=%x; read-only package energy / CPU0 active frequency; native acceptance pending\n",
        s.raw_units,s.raw_platform,s.raw.energy,s.raw.aperf,s.raw.mperf);
    if(s.idle.supported) print_idle("power-idle",&s.idle);
}

bool power_capture_start(void) {
    uint64_t f=lock();
    if(capturing || snapshot.state!=POWER_ACTIVE) { unlock(f);return false; }
    if(snapshot.idle.supported) {
        if(!scheduler_idle_audit_start()) { unlock(f);return false; }
        idle_config_start=read_msr(0xe2);idle_auditing=true;
    }
    capture_count=0;capture_overflow=false;capture_last=timer_monotonic_us();
    capturing=true;unlock(f);return true;
}
void power_capture_stop(void) {
    uint64_t f=lock();
    if(!capturing) { unlock(f);return; }
    capturing=false;unsigned count=capture_count;bool overflow=capture_overflow;
    bool audited=idle_auditing;struct scheduler_idle_audit final={0};uint64_t config_end=0;
    if(audited) { scheduler_idle_audit_stop(&final);config_end=read_msr(0xe2);idle_auditing=false; }
    unlock(f);
    /* The bootstrap caller owns start/stop; sampling cannot modify these
     * records after stop. Printing happens outside the IRQ-masked scope and
     * outside the measured phases, before workspace_log_stop persists them. */
    if(audited) {
        console_printf("cpu-idle-audit: version=1 entry=sti-hlt-cli policy_writes=0 managed_cpus=1 other_cpus_known=0 monitor=%u leaf1_b=%u leaf5_a=%u leaf5_b=%u leaf5_c=%u leaf5_d=%u e2_start=%u e2_end=%u epoch=%u enters=%u returns=%u ready=%u\n",
            (uint64_t)idle_monitor,(uint64_t)idle_leaf1_b,(uint64_t)idle_leaf5[0],(uint64_t)idle_leaf5[1],(uint64_t)idle_leaf5[2],(uint64_t)idle_leaf5[3],idle_config_start,config_end,final.epoch,final.enters,final.returns,final.ready);
    }
    console_printf("cpu-load-power: retained=%u overflow=%u\n",(uint64_t)count,(uint64_t)overflow);
    for(unsigned i=0;i<count;i++) {
        const struct power_snapshot *s=&capture[i];
        console_printf("cpu-load-power: power_state=%u flags=%u energy_shift=%u package_mw=%u active_mhz=%u nominal_mhz=%u package_uj=%u sample_us=%u interval_us=%u samples=%u rejected=%u energy=%u aperf=%u mperf=%u monotonic_us=%u\n",
            (uint64_t)s->state,(uint64_t)s->flags,(uint64_t)s->energy_shift,s->package_mw,s->active_mhz,(uint64_t)s->nominal_mhz,
            s->package_uj,s->sampled_us,s->interval_us,s->samples,s->rejected,s->raw.energy,s->raw.aperf,s->raw.mperf,s->sampled_us);
        if(s->idle.supported) print_idle("cpu-load-idle",&s->idle);
        if(audited) console_printf("cpu-idle-entry: sample_us=%u epoch=%u enters=%u returns=%u ready=%u\n",s->sampled_us,s->idle_entry.epoch,s->idle_entry.enters,s->idle_entry.returns,s->idle_entry.ready);
    }
}
