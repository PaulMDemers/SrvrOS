/* Explicit diagnostic only; never part of the ordinary threadstress suite. */
#include <srvros/sys.h>
#include <stdio.h>
#include <stdint.h>

static int reject(const char *reason) {
    printf("cpu-load: completed=NO reason=%s\n",reason);
    return 1;
}

static const char *check_sensors(long now, struct srv_thermal_status *t,
                                 struct srv_smc_status *f) {
    if(srv_thermal_status(t) || srv_smc_status(f)) return "sensor-read";
    unsigned required=SRV_THERMAL_CORE_VALID|SRV_THERMAL_PACKAGE_VALID|SRV_THERMAL_TARGET_VALID;
    if(t->version!=SRV_THERMAL_VERSION || t->state!=SRV_THERMAL_ACTIVE ||
       (t->flags&required)!=required || !t->samples || t->sampled_us>(uint64_t)now ||
       (uint64_t)now-t->sampled_us>3000000) return "thermal-unavailable-or-stale";
    if(t->flags&(SRV_THERMAL_THROTTLING|SRV_THERMAL_CRITICAL)) return "thermal-limit";
    if(t->target_mc<20000 || t->target_mc>125000 || t->core_mc<0 || t->package_mc<0)
        return "thermal-range";
    int limit=t->target_mc-10000;
    if(limit>90000) limit=90000;
    if(t->core_mc>=limit || t->package_mc>=limit) return "temperature-stop";
    if(f->version!=SRV_SMC_VERSION || f->state!=SRV_SMC_ACTIVE ||
       !(f->flags&SRV_SMC_FAN_VALID) || !f->samples || f->failures ||
       f->sampled_us>(uint64_t)now || (uint64_t)now-f->sampled_us>3000000 ||
       (f->fan_count!=1 && f->fan_count!=2)) return "fans-unavailable-or-stale";
    return NULL;
}

int cpu_load_capture(void) {
    const char *names[]={"idle","load","recovery"};
    volatile uint64_t work=1;
    long previous=srv_monotonic_us();
    if(previous<0) return reject("clock");
    printf("cpu-load: version=1 phases_seconds=30,30,30 workers=1 control_writes=none\n");
    for(unsigned phase=0;phase<3;phase++) {
        long start=previous,checked=-1,logged=-1;
        unsigned stalled=0;
        uint64_t chunks=0;
        for(;;) {
            long now=srv_monotonic_us();
            if(now<previous || now<0) return reject("clock-regressed");
            if(now==previous) { if(++stalled>=10000) return reject("clock-stalled"); }
            else stalled=0;
            previous=now;
            if(checked<0 || now-checked>=100000) {
                struct srv_thermal_status t={0};struct srv_smc_status f={0};
                const char *why=check_sensors(now,&t,&f);
                if(why) return reject(why);
                if(phase==1 && checked<0 && (t.core_mc>=80000 || t.package_mc>=80000))
                    return reject("load-start-too-warm");
                checked=now;
                if(logged<0 || now-logged>=1000000) {
                    printf("cpu-load: phase=%s now_us=%ld elapsed_us=%ld core_mc=%d package_mc=%d fan0_millirpm=%u fan1_millirpm=%u thermal_samples=%llu fan_samples=%llu chunks=%llu\n",
                        names[phase],now,now-start,t.core_mc,t.package_mc,f.fan_millirpm,f.fan1_millirpm,
                        (unsigned long long)t.samples,(unsigned long long)f.samples,(unsigned long long)chunks);
                    logged=now;
                }
            }
            if(now-start>=30000000) break;
            if(phase==1) {
                for(unsigned i=0;i<16384;i++) work=work*6364136223846793005ULL+1;
                chunks++;
            } else if(srv_sleep_ticks(1)<0) return reject("sleep");
        }
        if(phase==1 && !chunks) return reject("no-load-work");
        printf("cpu-load: phase=%s completed=yes elapsed_us=%ld chunks=%llu checksum=%llu\n",
            names[phase],previous-start,(unsigned long long)chunks,(unsigned long long)work);
    }
    printf("cpu-load: completed=yes\n");
    return 0;
}
