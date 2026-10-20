#include <assert.h>
#include <stdio.h>
#include <srvros/power.h>
static struct power_snapshot s;
static struct power_sampler p;
static void reset(void) {
    p=(struct power_sampler){0};assert(power_configure(&s,0xa0e03,0x5053bf3011600ull));
    s.state=POWER_ACTIVE;
}
static void pair(struct power_raw a,struct power_raw b) { reset();power_sample(&p,&s,a);assert(!s.flags);power_sample(&p,&s,b); }
int main(void) {
    uint32_t vb=0x756e6547,vd=0x49656e69,vc=0x6c65746e;
    assert(power_cpu_supported(vb,vd,vc,0x306d4,0,32,1));
    assert(!power_cpu_supported(vb,vd,vc,0x306d4,1u<<31,32,1));
    assert(!power_cpu_supported(vb,vd,vc,0x306d4,0,0,1));
    assert(!power_cpu_supported(vb,vd,vc,0x306d4,0,32,0));
    assert(!power_cpu_supported(vb,vd,vc,0x306c3,0,32,1));
    assert(!power_cpu_supported(vb,vd,vc,0x1306d4,0,32,1));
    assert(!power_cpu_supported(0,vd,vc,0x306d4,0,32,1));
    assert(power_cpu_supported(vb,vd,vc,0x40661,0x7ffafbff,0xbfebfbff,9));
    assert(!power_cpu_supported(vb,vd,vc,0x40661,1u<<31,32,1));
    assert(!power_cpu_supported(vb,vd,vc,0x40661,0,0,1));
    assert(!power_cpu_supported(vb,vd,vc,0x40661,0,32,0));
    assert(!power_cpu_supported(vb,vd,vc,0x140661,0,32,1));
    assert(!power_cpu_supported(vb,vd,vc,0x40651,0,32,1));
    assert(!power_configure(&s,0xa1003,22u<<8));
    assert(!power_configure(&s,0xa0e03,0));assert(!power_configure(&s,0xa0e03,65u<<8));
    struct power_raw a={.us=1000000,.energy=1000000,.aperf=100,.mperf=100};
    struct power_raw b={.us=2000000,.energy=1163840,.aperf=1500000100,.mperf=1000000100};
    pair(a,b);assert(s.flags==3 && s.package_uj==10000000 && s.package_mw==10000 && s.active_mhz==3300);
    assert(s.nominal_mhz==2200 && s.interval_us==1000000 && s.samples==2 && !s.rejected);
    power_expire(&s,5000000);assert(s.flags==3);
    power_expire(&s,5000001);assert(s.flags==POWER_STALE && !s.package_mw && !s.active_mhz);
    pair(a,b);power_expire(&s,1999999);assert(s.flags==POWER_STALE);
    /* 32-bit energy rollover, with unchanged 64-bit performance counters. */
    a.energy=UINT32_MAX-8191;b.energy=8192;b.aperf=a.aperf;b.mperf=a.mperf;
    pair(a,b);assert(s.flags==POWER_ENERGY_VALID && s.package_mw==1000 && !s.active_mhz);
    b.energy=a.energy;pair(a,b);assert(s.flags==1 && !s.package_uj && !s.package_mw);
    a.energy=1000000;b.energy=0;pair(a,b);assert(!s.flags && s.rejected==1);
    a.energy=0;b.energy=1ull<<32;pair(a,b);assert(!s.flags && s.rejected==1);
    b.energy=16384;b.mperf=a.mperf+1;b.aperf=UINT64_MAX;
    pair(a,b);assert(s.flags==1 && s.rejected==1); /* Multiplication overflow refused. */
    b.aperf=100;b.mperf=99;pair(a,b);assert(s.flags==1 && s.rejected==1); /* Counter reset. */
    b.aperf=100000;b.mperf=101;pair(a,b);assert(s.flags==1 && s.rejected==1); /* Implausible ratio. */
    b.aperf=100;b.mperf=200;pair(a,b);assert(s.flags==1 && s.rejected==1); /* No active counts. */
    for(unsigned i=0;i<4;i++) {
        b.us=(uint64_t[]){1000000,999999,1099999,6000001}[i];
        pair(a,b);assert(!s.flags && !s.interval_us && s.rejected==1);
    }
    /* Recovery starts at the rejected sample, never spans the discontinuity. */
    b.us=7000001;b.energy+=16384;b.aperf+=1000;b.mperf+=1000;
    power_sample(&p,&s,b);assert(s.flags==3 && s.package_mw==1000 && s.active_mhz==2200);
    /* Actual Ubuntu read-only capture, development load; not SrvrOS acceptance. */
    a=(struct power_raw){5193353430ull,0x27e53c8d,0x32cd90b7940,0x3ac834eee9b};
    b=(struct power_raw){5194354317ull,0x27e6bd98,0x32cf0d76bd1,0x3aca80d53ed};
    pair(a,b);assert(s.flags==3 && s.package_mw==6010 && s.active_mhz==1424);
    /* Pro11,2 Linux CPU0, development load: captured registers, no idle claim. */
    assert(power_configure(&s,658947ull,2260840501811456ull));
    p=(struct power_sampler){0};
    power_sample(&p,&s,(struct power_raw){196189642ull,111460355ull,482409017462ull,386016201885ull});
    power_sample(&p,&s,(struct power_raw){197189818ull,112003969ull,484541671259ull,387725480695ull});
    assert(s.nominal_mhz==2500 && s.flags==3 && s.package_mw==33173 && s.active_mhz==3119);
    puts("power: PASS exact CPU/feature gate, units, energy/frequency scopes, wrap/reset/gap/overflow/stale handling and actual Linux fixture");
}
