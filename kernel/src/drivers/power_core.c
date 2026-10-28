#include <srvros/power.h>

bool power_cpu_supported(uint32_t b,uint32_t d,uint32_t c,uint32_t sig,
                         uint32_t fc,uint32_t fd,uint32_t tc) {
    /* Captured Broadwell-U 06_3D, Air Haswell-U 06_45 and Pro 06_46 only. */
    unsigned model=((sig>>4)&15)|((sig>>12)&0xf0);
    return b==0x756e6547 && d==0x49656e69 && c==0x6c65746e &&
        ((sig>>8)&15)==6 && !((sig>>20)&255) &&
        (model==0x3d || model==0x45 || model==0x46) &&
        !(fc&(1u<<31)) && (fd&(1u<<5)) && (tc&1);
}

bool power_configure(struct power_snapshot *s,uint64_t units,uint64_t platform) {
    *s=(struct power_snapshot){.state=POWER_UNAVAILABLE,.raw_units=units,.raw_platform=platform};
    /* Energy units 2^-14 J are captured on this host. Other units must be
     * deliberately validated, not inferred from a different Intel model. */
    unsigned shift=(units>>8)&31,ratio=(platform>>8)&255;
    if(shift!=14 || !ratio || ratio>64) return false;
    s->energy_shift=shift;s->nominal_mhz=ratio*100;
    return true;
}

void power_sample(struct power_sampler *p,struct power_snapshot *s,struct power_raw r) {
    struct power_raw old=p->previous;
    bool had=p->have_previous;p->previous=r;p->have_previous=true;
    s->raw=r;s->sampled_us=r.us;s->samples++;s->flags=0;
    s->interval_us=0;s->package_uj=0;s->package_mw=0;s->active_mhz=0;
    if(!had) return;
    /* Rebaseline discontinuities. Long gaps cannot safely infer counter wraps. */
    if(r.us<=old.us || r.us-old.us<100000 || r.us-old.us>5000000 ||
       s->energy_shift!=14 || !s->nominal_mhz || s->nominal_mhz>6400) {
        s->rejected++;return;
    }
    uint64_t dt=r.us-old.us;s->interval_us=dt;
    if(!(r.energy>>32) && !(old.energy>>32)) {
        uint32_t ticks=(uint32_t)r.energy-(uint32_t)old.energy;
        uint64_t uj=((uint64_t)ticks*1000000)>>s->energy_shift;
        uint64_t mw=uj*1000/dt; /* Bounded 32-bit delta keeps both products in u64. */
        /* A generous 100 W ceiling detects resets/backward jumps on this
         * mobile package. It is a sanity bound, never a power-limit write. */
        if(mw<=100000) { s->package_uj=uj;s->package_mw=mw;s->flags|=POWER_ENERGY_VALID; }
        else s->rejected++;
    } else s->rejected++;
    /* Both architectural counters clear when either overflows. Do not treat
     * backwards motion as an ordinary unsigned 64-bit wrap. */
    if(r.aperf<old.aperf || r.mperf<old.mperf) { s->rejected++;return; }
    uint64_t a=r.aperf-old.aperf,m=r.mperf-old.mperf;
    if(!m) return; /* No C0 reference counts: frequency is unavailable, not zero. */
    if(a>UINT64_MAX/s->nominal_mhz) { s->rejected++;return; }
    uint64_t mhz=a*s->nominal_mhz/m;
    if(mhz>6000 || !mhz) { s->rejected++;return; }
    s->active_mhz=mhz;s->flags|=POWER_FREQUENCY_VALID;
}

void power_expire(struct power_snapshot *s,uint64_t now) {
    if(s->state==POWER_ACTIVE && s->samples &&
       (now<s->sampled_us || now-s->sampled_us>3000000)) {
        s->idle.valid=0;
        for(unsigned i=0;i<POWER_IDLE_COUNT;i++) s->idle.basis_points[i]=0;
        s->flags=POWER_STALE;s->package_uj=s->package_mw=s->active_mhz=0;
    }
}


uint32_t power_idle_supported(uint32_t sig,uint32_t invariant_tsc_d) {
    /* Called only after the native vendor/feature/hypervisor power gate. */
    if(((sig>>8)&15)!=6 || ((sig>>20)&255) || !(invariant_tsc_d&(1u<<8))) return 0;
    unsigned model=((sig>>4)&15)|((sig>>12)&0xf0);
    /* Broadwell inherits the seven counters, not Haswell-ULT's C8-C10 table. */
    return model==0x45?POWER_IDLE_MASK:model==0x3d?POWER_IDLE_BROADWELL_MASK:0;
}

void power_idle_sample(struct power_idle_snapshot *s,struct power_idle_raw r) {
    struct power_idle_raw old=s->raw;
    s->raw=r;s->valid=0;s->interval_us=0;s->interval_tsc=0;
    for(unsigned i=0;i<POWER_IDLE_COUNT;i++) s->basis_points[i]=0;
    if((s->supported!=POWER_IDLE_MASK && s->supported!=POWER_IDLE_BROADWELL_MASK) || !old.tsc_end) return;
    if(r.us<=old.us || r.us-old.us<100000 || r.us-old.us>5000000 ||
       old.tsc_end<old.tsc_begin || r.tsc_begin<=old.tsc_end || r.tsc_end<r.tsc_begin)
        goto rejected;
    /* Outer TSC brackets include the bounded read skew, so reported ratios
     * are conservative. Reject resets/overflows; never clamp to 100 percent. */
    uint64_t dt=r.tsc_end-old.tsc_begin,core=0,package=0;
    for(unsigned i=0;i<POWER_IDLE_COUNT;i++) {
        if(!(s->supported&(1u<<i))) continue;
        if((i>=7 && ((r.ticks[i]|old.ticks[i])>>60)) || r.ticks[i]<old.ticks[i]) goto rejected;
        uint64_t delta=r.ticks[i]-old.ticks[i];
        uint64_t *sum=i<3?&core:&package;
        if(delta>dt || delta>UINT64_MAX/10000 || *sum>dt-delta) goto rejected;
        *sum+=delta;
    }
    for(unsigned i=0;i<POWER_IDLE_COUNT;i++)
        if(s->supported&(1u<<i)) s->basis_points[i]=(r.ticks[i]-old.ticks[i])*10000/dt;
    s->interval_us=r.us-old.us;s->interval_tsc=dt;s->valid=s->supported;return;
rejected:
    s->rejected++;
}
