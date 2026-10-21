#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/sci_observe.h"
int main(void) {
    struct sci_observer o={0};
    const struct sci_observe_ports p={0x1800,0x1802,0x1889,0x1899};
    sci_observe_read(&o,&p,p.fixed_status,16,7,7);
    assert(!o.current.valid && !sci_observe_row(&o,0));
    sci_observe_end(&o,3,false,false);assert(o.total==0);
    assert(sci_observe_begin(&o,10,1,false));
    assert(!sci_observe_begin(&o,11,2,true));
    sci_observe_read(&o,&p,p.fixed_status,8,255,255); /* Wrong width. */
    sci_observe_read(&o,&p,0x62,8,255,255); /* EC data is not an event read. */
    assert(o.current.valid==0);
    sci_observe_read(&o,&p,p.fixed_status,16,0x801,0x801);
    sci_observe_read(&o,&p,p.fixed_enable,16,1,1);
    sci_observe_read(&o,&p,p.ec_status,8,0x44,4);
    sci_observe_read(&o,&p,p.ec_enable,8,4,4);
    sci_observe_read(&o,&p,p.ec_status,8,0,0); /* Keep pre-handler first read. */
    sci_observe_end(&o,13,true,true);
    const struct sci_observation *r=sci_observe_row(&o,0);
    assert(r && r->sequence==1 && r->phase==1 && r->valid==15);
    assert(r->fixed_status==0x801 && r->fixed_enable==1 && r->ec_status==4 && r->ec_raw_status==0x44);
    assert(r->handled && !r->previous_valid && !r->query_before && r->query_after && r->duration_us==3);
    assert(sci_observe_begin(&o,14,1,true));
    sci_observe_read(&o,&p,p.ec_enable,8,0,0); /* Zero known; other reads unknown. */
    sci_observe_end(&o,15,false,true);r=sci_observe_row(&o,1);
    assert(r->valid==8 && r->ec_enable==0 && r->previous_valid && r->previous_handled && r->previous_gap_us==4);
    for(unsigned i=2;i<20;i++) {
        assert(sci_observe_begin(&o,20+i,2,false));sci_observe_end(&o,21+i,false,false);
    }
    assert(o.count==SCI_OBSERVE_CAPACITY && o.total==20);
    for(unsigned i=0;i<o.count;i++)assert(sci_observe_row(&o,i)->sequence==13+i);
    assert(!sci_observe_row(&o,o.count));
    assert(sci_observe_begin(&o,1,0,false));sci_observe_end(&o,0,false,false);
    r=sci_observe_row(&o,o.count-1);assert(r->duration_us==0 && r->previous_gap_us==0 && r->valid==0);
    o.total=UINT64_MAX;assert(sci_observe_begin(&o,50,1,false));sci_observe_end(&o,51,true,false);
    assert(o.total==UINT64_MAX);
    puts("sci-observe: PASS first-read/width/address gating, unknown vs zero, filtering, timing, claims, bounded ring and saturation");
}
