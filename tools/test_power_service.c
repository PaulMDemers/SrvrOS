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
static unsigned idle_reads,idle_rows,config_reads,audit_rows,entry_rows,prereq_reads,prereq_rows,topology_reads;
static bool audit_ok=true,audit_active;
static uint64_t audit_epoch;
static bool mwait_ok=true,mwait_started;
static uint64_t mwait_fault;
static unsigned mwait_starts,mwait_stops,mwait_rows;
static uint64_t apic_config=4276095232ull,misc_config=8716425;
bool scheduler_mwait_test_start(void) { assert(audit_active);mwait_starts++;if(!mwait_ok)return false;mwait_started=true;return true; }
void scheduler_mwait_test_stop(struct scheduler_mwait_test *out) { assert(mwait_started && audit_active);mwait_stops++;mwait_started=false;*out=(struct scheduler_mwait_test){.epoch=audit_epoch,.attempts=32,.returns=32,.hlt=32,.max_us=10000,.tick_end=31,.fault=mwait_fault}; }
bool scheduler_idle_audit_start(void) { if(!audit_ok || audit_active)return false;audit_active=true;audit_epoch++;return true; }
void scheduler_idle_audit_snapshot(struct scheduler_idle_audit *out) { assert(audit_active);*out=(struct scheduler_idle_audit){.epoch=audit_epoch,.enters=now/10000,.returns=now/10000}; }
void scheduler_idle_audit_stop(struct scheduler_idle_audit *out) { scheduler_idle_audit_snapshot(out);audit_active=false; }
static uint32_t signature=0x306d4,feature_c,feature_d=32,thermal_c=1,max_leaf=6;
void power_test_cpuid(unsigned leaf,unsigned subleaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    assert(!subleaf || leaf==11);
    if(leaf==11) {
        assert(max_leaf>=11 && subleaf<3 && signature==0x40651 && invariant==256);topology_reads++;
        *a=subleaf==0?1:subleaf==1?4:0;*b=subleaf==0?2:subleaf==1?4:0;
        *c=subleaf==0?256:subleaf==1?513:2;*d=0;
    }
    else if(!leaf) { *a=max_leaf;*b=0x756e6547;*d=0x49656e69;*c=0x6c65746e; }
    else if(leaf==1) { *a=signature;*b=0;*c=feature_c;*d=feature_d; }
    else if(leaf==5) { assert(feature_c&8);*a=*b=64;*c=3;*d=0x11142120; }
    else if(leaf==0x80000000) { *a=max_ext;*b=*c=*d=0; }
    else if(leaf==0x80000007) { *a=*b=*c=0;*d=invariant; }
    else { assert(leaf==6);*a=*b=*d=0;*c=thermal_c; }
}
uint64_t power_test_read_msr(unsigned n) {
    reads++;
    switch(n) {
    case 0x1b:case 0x1a0:
        assert(signature==0x40651 && invariant==256);prereq_reads++;return n==0x1b?apic_config:misc_config;
    case 0xe2:assert(signature==0x40651 && invariant==256);config_reads++;return 1032;
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
    if(!strncmp(fmt,"cpu-mwait:",10)) { assert(!capturing && !mwait_started);mwait_rows++; }
    if(!strncmp(fmt,"cpu-idle-prereq:",16)) { assert(!capturing && !audit_active);prereq_rows++; }
    if(!strncmp(fmt,"cpu-idle-audit:",15)) { assert(!capturing && !audit_active);audit_rows++; }
    if(!strncmp(fmt,"cpu-idle-entry:",15)) { assert(!capturing && !audit_active);entry_rows++; }
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
    audit_ok=false;before=reads;assert(!power_capture_start() && reads==before);audit_ok=true;
    feature_c=8;max_leaf=20;init();assert(idle_monitor && idle_leaf5[3]==0x11142120);
    assert(!config_reads && !audit_rows && !entry_rows && !prereq_reads && !topology_reads);
    assert(power_capture_start() && config_reads==1 && audit_active && prereq_reads==2 && topology_reads==3);
    assert(idle_topology[0][1]==2 && idle_topology[1][1]==4 && idle_topology[2][1]==0);
    assert(!power_capture_start() && config_reads==1);
    for(unsigned i=0;i<18;i++) { now+=1000000;sample(); }
    assert(capture_count==3 && !capture_overflow);before=reads;power_capture_stop();assert(reads==before+3 && prereq_reads==4 && topology_reads==3 && prereq_rows==1 && idle_rows==3 && audit_rows==1 && entry_rows==3 && config_reads==2 && !audit_active);
    before=reads;power_capture_stop();assert(reads==before && config_reads==2);
    assert(power_capture_start());now+=6000000;sample();power_capture_stop();assert(audit_rows==2 && config_reads==4 && prereq_reads==8 && prereq_rows==2 && topology_reads==6);
    /* No unsupported-leaf aliasing; reset cached tuples on every capture. */
    max_leaf=6;init();assert(power_capture_start());
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<4;j++)assert(!idle_topology[i][j]);
    assert(topology_reads==6);power_capture_stop();assert(prereq_rows==3 && prereq_reads==12);
    unsigned prereq_before=prereq_reads;before=reads;power_status(&out);power_print_status();assert(reads==before && prereq_reads==prereq_before);
    signature=0x40661;init();assert(power_capture_start());power_capture_stop();assert(prereq_reads==prereq_before);
    signature=0x40651;
    feature_c=1u<<31;idle_reads=0;init();assert(!reads && !idle_reads && !spawns);
    /* New instructions are admitted only by an explicit, supported trial. */
    feature_c=8;max_leaf=20;init();assert(!mwait_starts);
    assert(power_capture_start());power_capture_stop();assert(!mwait_starts && !mwait_rows);
    assert(power_capture_mwait_start() && mwait_started && mwait_starts==1);
    assert(!power_capture_mwait_start() && mwait_starts==1);
    power_capture_stop();assert(mwait_stops==1 && mwait_rows==1 && power_capture_mwait_passed());
    power_capture_stop();assert(mwait_stops==1);
    assert(power_capture_mwait_start());mwait_fault=2;power_capture_stop();assert(!power_capture_mwait_passed());mwait_fault=0;
    assert(power_capture_mwait_start());misc_config^=1;power_capture_stop();assert(!power_capture_mwait_passed());misc_config^=1;
    idle_monitor=0;assert(!power_capture_mwait_start() && !audit_active);idle_monitor=1;
    idle_leaf5[0]=128;assert(!power_capture_mwait_start() && !audit_active);idle_leaf5[0]=64;
    idle_leaf5[2]=1;assert(!power_capture_mwait_start() && !audit_active);idle_leaf5[2]=3;
    idle_leaf5[3]=0;assert(!power_capture_mwait_start() && !audit_active);idle_leaf5[3]=0x11142120;
    misc_config=0;assert(!power_capture_mwait_start() && !audit_active);misc_config=8716425;
    apic_config|=1024;assert(!power_capture_mwait_start() && !audit_active);apic_config&=~1024ull;
    mwait_ok=false;assert(!power_capture_mwait_start() && !audit_active);mwait_ok=true;
    signature=0x40661;init();before=reads;assert(!power_capture_mwait_start() && reads==before);
    puts("power-service: PASS no reads on refused CPUs, exact read-only MSR allowlist, worker failure/idempotence, cadence and cached stale status");
}
