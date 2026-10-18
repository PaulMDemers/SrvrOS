#include <assert.h>
#include <stdio.h>
#include <srvros/thermal.h>
int main(void) {
    uint32_t b=0x756e6547,d=0x49656e69,c=0x6c65746e;
    assert(thermal_cpu_supported(b,d,c,0x306d4,0,32,65));
    assert(!thermal_cpu_supported(b,d,c,0x306d4,1u<<31,32,65));
    assert(!thermal_cpu_supported(b,d,c,0x306d4,0,0,65));
    assert(!thermal_cpu_supported(b,d,c,0x306d4,0,32,64));
    assert(!thermal_cpu_supported(b,d,c,0x306c3,0,32,65));
    assert(!thermal_cpu_supported(b,d,c,0x307d4,0,32,65));
    assert(!thermal_cpu_supported(0,d,c,0x306d4,0,32,65));
    struct srv_thermal_status s={.version=1,.samples=2};
    thermal_decode(&s,0x690000,0x88030a80,0x88030800,true); /* Actual read-only Linux capture. */
    assert(s.core_mc==102000&&s.package_mc==102000&&s.target_mc==105000);
    assert(s.flags==(SRV_THERMAL_CORE_VALID|SRV_THERMAL_PACKAGE_VALID|SRV_THERMAL_TARGET_VALID));
    thermal_decode(&s,100u<<16,(1u<<31)|(30u<<16),25u<<16,true);
    assert(s.core_mc==70000&&s.package_mc==75000); /* No invented package valid bit. */
    thermal_decode(&s,100u<<16,30u<<16,25u<<16,false);
    assert(!(s.flags&3)&&!s.core_mc&&!s.package_mc&&!s.raw_package&&s.samples==2);
    thermal_decode(&s,100u<<16,(1u<<31)|16|1,0,true);
    assert((s.flags&(SRV_THERMAL_THROTTLING|SRV_THERMAL_CRITICAL))==12);
    thermal_decode(&s,0,0,0,true);assert(!s.flags);
    thermal_decode(&s,0x01640000,0,0,true);assert(!s.flags); /* Unvalidated TCC offset. */
    thermal_decode(&s,100u<<16,(1u<<31)|(127u<<16),0,false);assert(s.core_mc==-27000);
    puts("thermal: PASS exact CPU/feature/hypervisor gating, actual target fixture, signed readout, independent package semantics, invalid core/target, offset rejection, raw flags and cached metadata");
}
