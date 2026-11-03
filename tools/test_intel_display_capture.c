#include <srvros/intel_display_capture.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fake {
    struct intel_capture_identity id;
    unsigned identities,maps,unmaps,reads,clocks;
    int fail_map,fail_unmap,change_identity;
    uint32_t bad_offset;
    bool well_off,well_changes,register_changes,frozen,backwards;
    uint64_t time;
    bool owned[INTEL_CAPTURE_PAGES];
};
static bool identify(void *p,struct intel_capture_identity *id) {
    struct fake *f=p;*id=f->id;
    if((int)++f->identities==f->change_identity) id->bar[0]+=0x1000000;
    return true;
}
static bool map(void *p,unsigned n,uint64_t physical) {
    struct fake *f=p;assert(n<INTEL_CAPTURE_PAGES && !f->owned[n]);
    assert(physical==0xc0000000ull+intel_capture_pages[n]);
    if((int)n==f->fail_map)return false;
    f->owned[n]=true;f->maps++;return true;
}
static bool unmap(void *p,unsigned n) {
    struct fake *f=p;assert(n<INTEL_CAPTURE_PAGES && f->owned[n]);f->unmaps++;
    if((int)n==f->fail_unmap)return false;
    f->owned[n]=false;return true;
}
static uint64_t now(void *p) {
    struct fake *f=p;f->clocks++;
    if(f->backwards && f->clocks>3)return 0;
    if(!f->frozen)f->time+=100;
    return f->time;
}
static bool read(void *p,uint32_t off,uint32_t *value) {
    struct fake *f=p;assert(intel_capture_offset_allowed(off));
    bool mapped=false;
    for(unsigned i=0;i<INTEL_CAPTURE_PAGES;i++)if((off&~4095u)==intel_capture_pages[i])mapped=f->owned[i];
    assert(mapped);f->reads++;*value=0;
    if(off>=0x45400 && off<=0x4540c) {
        *value=f->well_off?0:0xc0000000;
        if(f->well_changes && f->reads>50)*value^=0x80000000;
    }
    if(off==0x6001c)*value=(1439u<<16)|899;
    if(off==0x70180)*value=0x98000000;
    if(off==0x70188)*value=6144;
    if(off==0x70000 || off==0x70040)*value=(uint32_t)f->time;
    if(off==f->bad_offset)*value=UINT32_MAX;
    if(f->register_changes && off==0x70188 && f->time>20000)*value+=64;
    return true;
}
static struct fake fresh(void) {
    return (struct fake){.id={.id=0x16268086,.class_rev=0x03000009,.subsystem=0x011b106b,
        .pch=0x9cc38086,.command=2,.power_valid=1,.firmware_match=1,.mmio_safe=1,
        .bar={0xc0000004,0,0xb000000c,0}},.fail_map=-1,.fail_unmap=-1,.change_identity=-1,.bad_offset=UINT32_MAX};
}
static const struct intel_capture_fb fb={0xb0000000,1440,900,6144,32,1,8,16,8,8,8,0};
static bool run(struct fake *f,const struct intel_capture_fb *frame,struct intel_capture_result *r) {
    return intel_display_capture(r,frame,&(struct intel_capture_io){f,identify,map,unmap,read,now});
}
int main(void) {
    struct fake f=fresh();struct intel_capture_result r;
    assert(run(&f,&fb,&r));assert(r.status==INTEL_CAPTURE_OBSERVED && f.maps==9 && f.unmaps==9);
    assert(r.sample[0].reads==58 && r.sample[1].reads==58 && r.active_planes==1 && r.linear_relation==1);
    assert(r.sample[1].begin_us-r.sample[0].end_us>=20000 && !r.retained);
    assert(!intel_capture_offset_allowed(0x70184) && !intel_capture_offset_allowed(0x70024));
    assert(!intel_capture_offset_allowed(0xa188) && !intel_capture_offset_allowed(0x800000));
    for(unsigned kind=0;kind<13;kind++) {
        f=fresh();
        switch(kind) {
        case 0:f.id.firmware_match=0;break;case 1:f.id.id=0x0a268086;break;
        case 2:f.id.subsystem++;break;case 3:f.id.class_rev++;break;case 4:f.id.pch++;break;
        case 5:f.id.command=0;break;case 6:f.id.pm=3;break;case 7:f.id.power_valid=0;break;
        case 8:f.id.bar[0]=4;break;case 9:f.id.bar[0]+=0x400000;break;
        case 10:f.id.bar[1]=0x80;break;case 11:f.id.mmio_safe=0;break;case 12:f.id.bar[0]|=8;break;
        }
        assert(!run(&f,&fb,&r) && !f.maps && !f.reads);
    }
    for(unsigned kind=0;kind<8;kind++) {
        f=fresh();struct intel_capture_fb bad=fb;
        switch(kind) {
        case 0:bad.width=4097;break;case 1:bad.pitch=1;break;case 2:bad.bpp=24;break;
        case 3:bad.red_shift=0;break;case 4:bad.physical=UINT64_MAX-100;break;
        case 5:bad.physical=0xc0000000;break;case 6:bad.height=0;break;
        case 7:bad.physical=0xc0800000;break;
        }
        assert(!run(&f,&bad,&r) && !f.maps && !f.reads && r.status==INTEL_CAPTURE_FRAMEBUFFER);
    }
    for(int i=0;i<9;i++) {
        f=fresh();f.fail_map=i;assert(!run(&f,&fb,&r));
        assert(r.status==INTEL_CAPTURE_MAP && f.maps==(unsigned)i && f.unmaps==(unsigned)i && !f.reads);
    }
    f=fresh();f.fail_unmap=4;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_RETAINED_MAPPING && r.retained==(1u<<4));
    f=fresh();f.well_off=true;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_WELL_OFF && f.reads==1 && f.unmaps==9);
    f=fresh();f.well_changes=true;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_UNSTABLE && f.unmaps==9);
    f=fresh();f.bad_offset=0x70180;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_REGISTER && f.unmaps==9);
    for(int call=2;call<=4;call++) {
        f=fresh();f.change_identity=call;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_UNSTABLE && f.unmaps==9);
        if(call==2)assert(!f.reads);
    }
    f=fresh();f.register_changes=true;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_UNSTABLE);
    f=fresh();f.frozen=true;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_CLOCK && f.clocks<=1000010);
    f=fresh();f.backwards=true;assert(!run(&f,&fb,&r) && r.status==INTEL_CAPTURE_CLOCK);
    puts("intel-display-capture: PASS profile/format/range refusal, allowlist, stable endpoints, bounded clock, partial-map cleanup and retained failures");
}
