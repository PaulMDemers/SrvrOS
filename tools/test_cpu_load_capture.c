#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/sys.h>
int cpu_load_capture(void);
static long now;
static int scenario,reads;
long srv_monotonic_us(void) {
    if(scenario==8) return 0;
    if(scenario==9 && now>1000000) return -1;
    now+=100000;return now;
}
long srv_sleep_ticks(uint64_t ticks) { assert(ticks==1);return scenario==10?-1:0; }
long srv_thermal_status(struct srv_thermal_status *s) {
    reads++;
    *s=(struct srv_thermal_status){.version=SRV_THERMAL_VERSION,.state=SRV_THERMAL_ACTIVE,
        .flags=19,.samples=(uint64_t)now/1000000+1,.sampled_us=now,.core_mc=65000,.package_mc=68000,.target_mc=100000};
    if(scenario==1) s->state=SRV_THERMAL_UNAVAILABLE;
    if(scenario==2 && now>32000000) s->package_mc=90000;
    if(scenario==3 && now>4000000) s->sampled_us=1;
    if(scenario==4) s->flags|=SRV_THERMAL_THROTTLING;
    if(scenario==5 && now>30000000) s->core_mc=81000;
    if(scenario==11) s->version=99;
    if(scenario==12) s->target_mc=5000;
    return 0;
}
long srv_smc_status(struct srv_smc_status *s) {
    *s=(struct srv_smc_status){.version=SRV_SMC_VERSION,.state=SRV_SMC_ACTIVE,
        .flags=SRV_SMC_FAN_VALID,.fan_count=2,.samples=(uint64_t)now/1000000+1,
        .sampled_us=now,.fan_millirpm=2162000,.fan1_millirpm=1991000};
    if(scenario==6) s->failures=1;
    if(scenario==7) s->flags=0;
    if(scenario==13) s->fan_count=1;
    return 0;
}
int main(void) {
    for(scenario=0;scenario<=13;scenario++) {
        now=0;reads=0;
        int rc=cpu_load_capture();
        assert(rc==((scenario==0 || scenario==13)?0:1));
        if(!rc) assert(now>=90000000 && reads>=900);
        else assert(now<90000000);
    }
    puts("cpu-load fixtures: PASS normal Air/Pro completion and 12 refusal/stop paths");
}
