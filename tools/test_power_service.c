#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define SRVROS_POWER_HOST_TEST
#include "../kernel/src/drivers/power.c"
static uint64_t now=1000000,units=0xa0e03,energy=1000000,aperf=100,mperf=100;
static unsigned reads,spawns,capture_rows,capture_receipts;
static uint64_t platform=22u<<8;
static bool reference=true,spawn_ok=true;
static uint32_t max_ext=0x80000007,invariant;
static unsigned idle_reads,idle_rows;
static uint32_t signature=0x306d4,feature_c,feature_d=32,thermal_c=1,max_leaf=6;
void power_test_cpuid(unsigned leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    if(!leaf) { *a=max_leaf;*b=0x756e6547;*d=0x49656e69;*c=0x6c65746e; }
    else if(leaf==1) { *a=signature;*b=0;*c=feature_c;*d=feature_d; }
    else if(leaf==0x80000000) { *a=max_ext;*b=*c=*d=0; }
    else if(leaf==0x80000007) { *a=*b=*c=0;*d=invariant; }
    else { assert(leaf==6);*a=*b=*d=0;*c=thermal_c; }
}
uint64_t power_test_read_msr(unsigned n) {
    reads++;
    switch(n) {
    case 0x10: idle_reads++;return now*2000;
    case 0x3fc:case 0x3fd:case 0x3fe:case 0x60d:case 0x3f8:case 0x3f9:case 0x3fa:case 0x630:case 0x631:case 0x632:
        assert(signature==0x40651 && invariant==256);idle_reads++;return 0;
    case 0x606:return units;case 0xce:return platform;case 0x611:return energy;case 0xe8:return aperf;case 0xe7:return mperf;default:assert(0);return 0; }
}
bool timer_reference_ready(void) { return reference; }
uint64_t timer_monotonic_us(void) { return now; }
uint64_t timer_ticks(void) { return now/10000; }
bool scheduler_spawn_io_service(const char *n,scheduler_thread_fn fn,void *arg,scheduler_wait_condition_fn eligible) {
    assert(!strcmp(n,"power") && fn==worker && !arg && eligible==due);spawns++;return spawn_ok;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t deadline) {
    (void)q;(void)fn;(void)arg;(void)deadline;assert(0);return false;
}
void console_printf(const char *fmt,...) {
    if(!strncmp(fmt,"%s: supported=",14)) {
        va_list ap;va_start(ap,fmt);const char *prefix=va_arg(ap,const char *);
        if(!strcmp(prefix,"cpu-load-idle")) { assert(!capturing);idle_rows++; }
        va_end(ap);
    }
    if(!strncmp(fmt,"cpu-load-power:",15)) assert(!capturing);
    if(!strncmp(fmt,"cpu-load-power: power_state=",27)) capture_rows++;
    if(!strncmp(fmt,"cpu-load-power: retained=",24)) capture_receipts++;
}
static void init(void) { initialized=false;snapshot=(struct power_snapshot){0};sampler=(struct power_sampler){0};reads=spawns=0;power_init(); }
int main(void) {
    reference=false;init();assert(!reads&&!spawns);reference=true;
    max_leaf=5;init();assert(!reads&&!spawns);max_leaf=6;
    feature_c=1u<<31;init();assert(!reads&&!spawns);feature_c=0;
    feature_d=0;init();assert(!reads&&!spawns);feature_d=32;
    thermal_c=0;init();assert(!reads&&!spawns);thermal_c=1;
    signature=0x306c3;init();assert(!reads&&!spawns);signature=0x306d4;
    units=0xa1003;init();assert(reads==2&&!spawns && snapshot.state==POWER_UNAVAILABLE);units=0xa0e03;
    spawn_ok=false;init();assert(reads==5&&spawns==1&&snapshot.state==POWER_UNAVAILABLE&&!snapshot.flags);spawn_ok=true;
    init();assert(reads==5&&spawns==1&&snapshot.state==POWER_ACTIVE&&!snapshot.flags);
    power_init();assert(reads==5&&spawns==1&&!due(0));
    now+=1000000;energy+=163840;aperf+=1500000000;mperf+=1000000000;
    assert(due(0));sample();assert(!due(0));
    struct power_snapshot out;power_status(&out);
    assert(reads==8&&out.flags==3&&out.package_mw==10000&&out.active_mhz==3300);
    now+=3000001;power_status(&out);assert(reads==8&&out.flags==POWER_STALE&&!out.package_mw);
    assert(snapshot.flags==3); /* Expiry is per-reader; sampling baseline intact. */
    signature=0x40661;init();assert(reads==5 && spawns==1 && snapshot.state==POWER_ACTIVE);
    feature_c=1u<<31;init();assert(!reads && !spawns);feature_c=0;
    signature=0x40651;platform=0x8083df3011400ull;
    init();assert(reads==5 && spawns==1 && snapshot.state==POWER_ACTIVE && snapshot.nominal_mhz==2000);
    now+=1000000;energy+=163840;aperf+=1500000000;mperf+=1000000000;
    sample();power_status(&out);assert(reads==8 && out.flags==3 && out.package_mw==10000 && out.active_mhz==3000);
    feature_c=1u<<31;init();assert(!reads && !spawns);feature_c=0;
    feature_d=0;init();assert(!reads && !spawns);feature_d=32;
    thermal_c=0;init();assert(!reads && !spawns);thermal_c=1;
    signature=0x40671;init();assert(!reads && !spawns);
    assert(!power_capture_start() && !reads && !spawns);
    power_capture_stop();assert(!capture_rows && !capture_receipts);
    signature=0x40651;init();unsigned before=reads;
    assert(power_capture_start() && !power_capture_start() && reads==before);
    uint64_t start=now;
    /* Native sample cadence drifts just below one second. USB checkpoints at
     * ten-second cadence then cover only 9.999841s; retain independent samples. */
    for(unsigned i=0;i<90;i++) {
        now+=999984;energy+=16384;aperf+=1000;mperf+=1000;sample();
    }
    assert(reads==before+270 && capture_count==15 && !capture_overflow);
    for(unsigned phase=0;phase<3;phase++) {
        uint64_t first=0,last=0;
        for(unsigned i=0;i<capture_count;i++) {
            struct power_snapshot *c=&capture[i];
            assert(c->flags==3 && !c->rejected && c->state==POWER_ACTIVE);
            if(c->sampled_us>=start+phase*30000000ull && c->sampled_us<=start+(phase+1)*30000000ull) {
                if(!first)first=c->sampled_us;
                last=c->sampled_us;
            }
        }
        assert(first && last-first>=10000000);
    }
    before=reads;power_capture_stop();
    assert(reads==before && capture_rows==15 && capture_receipts==1 && !capturing);
    power_capture_stop();assert(capture_receipts==1);
    now+=6000000;sample();assert(capture_count==15 && capture_rows==15);
    assert(power_capture_start() && capture_count==0 && !capture_overflow);
    for(unsigned i=0;i<25;i++) { now+=5000000;sample(); }
    assert(capture_count==24 && capture_overflow);
    before=reads;power_capture_stop();assert(reads==before && capture_rows==39 && capture_receipts==2);
    assert(power_capture_start() && !capture_count && !capture_overflow);
    power_capture_stop();assert(capture_receipts==3 && capture_rows==39);
    /* Air residency is extra cached read-only data, not a new worker/cadence.
     * Sibling power profiles and missing invariant-TSC capability get no reads. */
    invariant=256;
    signature=0x306d4;init();assert(reads==5 && !snapshot.idle.supported);
    signature=0x40661;init();assert(reads==5 && !snapshot.idle.supported);
    signature=0x40651;max_ext=0x80000006;init();assert(reads==5 && !snapshot.idle.supported);
    max_ext=0x80000007;init();assert(reads==17 && idle_reads==12 && snapshot.idle.supported==1023 && !snapshot.idle.valid);
    now+=1000000;sample();power_status(&out);assert(reads==32 && idle_reads==24 && out.idle.valid==1023);
    now+=3000001;power_status(&out);assert(!out.idle.valid && snapshot.idle.valid==1023 && reads==32);
    assert(power_capture_start());
    for(unsigned i=0;i<18;i++) { now+=1000000;sample(); }
    assert(capture_count==3 && !capture_overflow);before=reads;power_capture_stop();assert(reads==before && idle_rows==3);
    feature_c=1u<<31;idle_reads=0;init();assert(!reads && !idle_reads && !spawns);
    puts("power-service: PASS no reads on refused CPUs, exact read-only MSR allowlist, worker failure/idempotence, cadence and cached stale status");
}
