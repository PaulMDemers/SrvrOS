#include <srvros/thermal.h>
bool thermal_cpu_supported(uint32_t b,uint32_t d,uint32_t c,uint32_t sig,uint32_t fc,uint32_t fd,uint32_t ta) {
    unsigned family=(sig>>8)&15,model=((sig>>4)&15)|((sig>>12)&0xf0);
    return b==0x756e6547 && d==0x49656e69 && c==0x6c65746e && family==6 && model==0x3d &&
        !(fc&(1u<<31)) && (fd&(1u<<5)) && (ta&1);
}
void thermal_decode(struct srv_thermal_status *s,uint64_t target,uint64_t core,uint64_t package,bool has_package) {
    s->flags=0;s->core_mc=s->package_mc=s->target_mc=0;
    s->raw_target=target;s->raw_core=core;s->raw_package=has_package?package:0;
    /* Keep the first board profile to its observed zero TCC-offset mode. A
     * different firmware configuration is reported, never silently adjusted. */
    unsigned reference=(target>>16)&255;
    if(!reference || (target&0x0f000000)) return;
    s->target_mc=(int32_t)reference*1000;s->flags=SRV_THERMAL_TARGET_VALID;
    if(core&(1ull<<31)) { s->core_mc=((int32_t)reference-(int32_t)((core>>16)&127))*1000;s->flags|=SRV_THERMAL_CORE_VALID; }
    /* PACKAGE_THERM_STATUS has no architectural Reading Valid bit. */
    if(has_package) { s->package_mc=((int32_t)reference-(int32_t)((package>>16)&127))*1000;s->flags|=SRV_THERMAL_PACKAGE_VALID; }
    uint64_t state=core|(has_package?package:0);
    if(state&1) s->flags|=SRV_THERMAL_THROTTLING;
    if(state&16) s->flags|=SRV_THERMAL_CRITICAL;
}
