#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_POWER_HOST_TEST
#include "../kernel/src/drivers/power.c"
static uint64_t now=1000000,units=0xa0e03,energy=1000000,aperf=100,mperf=100;
static unsigned reads,spawns;
static bool reference=true,spawn_ok=true;
static uint32_t signature=0x306d4,feature_c,feature_d=32,thermal_c=1,max_leaf=6;
void power_test_cpuid(unsigned leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    if(!leaf) { *a=max_leaf;*b=0x756e6547;*d=0x49656e69;*c=0x6c65746e; }
    else if(leaf==1) { *a=signature;*b=0;*c=feature_c;*d=feature_d; }
    else { assert(leaf==6);*a=*b=*d=0;*c=thermal_c; }
}
uint64_t power_test_read_msr(unsigned n) {
    reads++;
    switch(n) { case 0x606:return units;case 0xce:return 22u<<8;case 0x611:return energy;case 0xe8:return aperf;case 0xe7:return mperf;default:assert(0);return 0; }
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
void console_printf(const char *fmt,...) { (void)fmt; }
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
    signature=0x40651;init();assert(!reads && !spawns);
    puts("power-service: PASS no reads on refused CPUs, exact read-only MSR allowlist, worker failure/idempotence, cadence and cached stale status");
}
