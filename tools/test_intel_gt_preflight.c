#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_gt_preflight.h>
static unsigned calls,fail_at,maps,unmaps,reads,endpoint_calls;
static uint64_t ticks;
static bool mapped,freeze,backward,changed,float_read,bad_endpoint,endpoint_drift;
static uint32_t page;
static bool step(void){return ++calls!=fail_at;}
static bool guard(void *p){assert(!p && !mapped);return step();}
static bool endpoint(void *p,struct intel_gt_endpoint *e) {
    assert(!p && !mapped);endpoint_calls++;
    *e=(struct intel_gt_endpoint){0xc0000000,bad_endpoint?UINT32_MAX:endpoint_drift && endpoint_calls==3?0x12346000:0x12345000,0};return step();
}
static bool map(void *p,uint32_t off){assert(!p && !mapped && (off==0xa000 || off==0x130000));if(!step())return false;mapped=true;page=off;maps++;return true;}
static bool read_reg(void *p,uint32_t off,uint32_t *v) {
    assert(!p && mapped && (off==INTEL_GT_REQUEST || off==INTEL_GT_ACK) && (off&~4095u)==page);reads++;
    *v=float_read?UINT32_MAX:changed && reads==4?4:2;return step();
}
static bool unmap(void *p){assert(!p && mapped);if(!step())return false;mapped=false;unmaps++;return true;}
static uint64_t now(void *p){assert(!p);if(backward)return ticks--;if(!freeze)ticks+=100;return ticks;}
static const struct intel_gt_preflight_io io={NULL,guard,endpoint,map,read_reg,unmap,now};
static void fresh(void){calls=fail_at=maps=unmaps=reads=endpoint_calls=0;ticks=100;mapped=freeze=backward=changed=float_read=bad_endpoint=endpoint_drift=false;}
int main(void) {
    struct intel_gt_preflight_result r;
    fresh();assert(intel_gt_preflight(&r,&io));unsigned total=calls;
    assert(r.status==GTP_OBSERVED && r.maps==4 && !r.retained && maps==unmaps && reads==4 && endpoint_calls==5);
    assert(r.sample[0].requests==2 && r.sample[0].ack==2 && r.sample[1].ack==2);
    assert(r.sample[0].reads==2 && r.sample[1].reads==2 && r.sample[0].end_us<=r.sample[1].begin_us);
    for(unsigned at=1;at<=total;at++) {
        fresh();fail_at=at;assert(!intel_gt_preflight(&r,&io));
        assert(r.maps<=4 && reads<=4 && unmaps<=maps);
        assert(mapped==(r.status==GTP_RETAINED));assert(r.retained==mapped);
    }
    fresh();changed=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_CHANGED && maps==unmaps);
    fresh();bad_endpoint=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_REFUSED && !maps && !reads);
    fresh();endpoint_drift=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_CHANGED && maps==unmaps && maps==2);
    fresh();float_read=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_READ && maps==unmaps);
    fresh();freeze=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_CLOCK && maps==unmaps);
    fresh();backward=true;assert(!intel_gt_preflight(&r,&io) && r.status==GTP_CLOCK && maps==unmaps);
    fresh();assert(!intel_gt_preflight(&r,NULL) && !maps && !reads);
    printf("intel-gt-preflight: PASS exact two-page RO observations, finite clocks, raw foreign requests and %u failure boundaries; no engine or firmware access\n",total);
}
