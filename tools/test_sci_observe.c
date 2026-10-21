#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/sci_observe.h"
static void query_tests(void) {
    struct sci_query_state q={0};
    struct sci_query_snapshot snap=sci_query_snapshot(&q,100);
    assert(snap.valid && !snap.sequence && !snap.age_us && !snap.phase_age_us);
    sci_query_start(&q,100);
    snap=sci_query_snapshot(&q,110);
    assert(snap.sequence==1 && snap.phase==SCI_QUERY_QUEUED && snap.age_us==10);
    sci_query_phase(&q,SCI_QUERY_TRANSFER,115);
    snap=sci_query_snapshot(&q,120);
    assert(snap.transfers==1 && !snap.code_valid && snap.age_us==20 && snap.phase_age_us==5);
    q.code=18;q.code_valid=true;sci_query_phase(&q,SCI_QUERY_AML,130);
    struct sci_observer o={0};assert(sci_observe_begin(&o,140,1,true));
    o.current.query_entry=sci_query_snapshot(&q,140);sci_observe_end(&o,145,false,true);
    sci_query_phase(&q,SCI_QUERY_CHECK,150);sci_query_phase(&q,SCI_QUERY_QUEUED,160);
    sci_query_phase(&q,SCI_QUERY_TRANSFER,170);
    assert(q.transfers==2 && !q.code_valid && !q.code);
    const struct sci_query_snapshot *r=&sci_observe_row(&o,0)->query_entry;
    assert(r->phase==SCI_QUERY_AML && r->code==18 && r->code_valid);
    assert(r->age_us==40 && r->phase_age_us==10 && r->transfers==1);
    q.code_valid=true;sci_query_phase(&q,SCI_QUERY_RESUME,180);
    sci_query_phase(&q,SCI_QUERY_IDLE,181);sci_query_start(&q,190);
    assert(q.sequence==2 && !q.transfers && !q.code_valid);
    snap=sci_query_snapshot(&q,180);assert(!snap.age_us && !snap.phase_age_us);
    q.sequence=UINT64_MAX;sci_query_start(&q,200);assert(q.sequence==UINT64_MAX);
    q.transfers=UINT64_MAX;sci_query_phase(&q,SCI_QUERY_TRANSFER,201);assert(q.transfers==UINT64_MAX);
}
int main(void) {
    query_tests();
    struct sci_observer o={0};
    const struct sci_observe_ports p={0x1800,0x1802,0x1889,0x1899};
    const struct sci_controller_observation before={.valid=7,.vector=41,.gsi=9,
        .lapic_bits=5,.lint0=0x10700,.route_low=0xe029,.pic_masks=0xffff};
    struct sci_controller_observation after=before;after.lapic_bits=7;after.route_low=0x1e029;
    sci_observe_controller(&o,false,before);
    assert(!o.current.before.valid);
    sci_observe_read(&o,&p,p.fixed_status,16,7,7);
    assert(!o.current.valid && !sci_observe_row(&o,0));
    sci_observe_end(&o,3,false,false);assert(o.total==0);
    assert(sci_observe_begin(&o,10,1,false));
    sci_observe_controller(&o,false,before);
    assert(!sci_observe_begin(&o,11,2,true));
    sci_observe_controller(&o,true,after);
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
    assert(!r->post_eoi_observed && o.post_pending);
    sci_observe_post_eoi(&o,after,15,true);
    assert(r->post_eoi_observed && r->post_eoi_elapsed_us==5 && r->query_post_eoi);
    assert(r->post_eoi.lapic_bits==7 && !o.post_pending);
    sci_observe_post_eoi(&o,before,16,false);assert(r->post_eoi.lapic_bits==7);
    assert(r->before.valid==7 && r->before.lapic_bits==5 && r->before.vector==41);
    assert(r->after.valid==7 && r->after.lapic_bits==7 && r->after.route_low==0x1e029);
    assert(r->before.route_low==0xe029 && r->before.gsi==9 && r->before.pic_masks==0xffff);
    sci_observe_controller(&o,false,after);assert(r->before.lapic_bits==5);
    assert(r && r->sequence==1 && r->phase==1 && r->valid==15);
    assert(r->fixed_status==0x801 && r->fixed_enable==1 && r->ec_status==4 && r->ec_raw_status==0x44);
    assert(r->handled && !r->previous_valid && !r->query_before && r->query_after && r->duration_us==3);
    assert(sci_observe_begin(&o,14,1,true));
    assert(!o.current.before.valid && !o.current.after.valid && !o.current.post_eoi_observed);
    sci_observe_post_eoi(&o,before,17,false);assert(!o.current.post_eoi_observed);
    sci_observe_read(&o,&p,p.ec_enable,8,0,0); /* Zero known; other reads unknown. */
    sci_observe_end(&o,15,false,true);r=sci_observe_row(&o,1);
    assert(r->valid==8 && r->ec_enable==0 && r->previous_valid && r->previous_handled && r->previous_gap_us==4);
    for(unsigned i=2;i<20;i++) {
        assert(sci_observe_begin(&o,20+i,2,false));sci_observe_end(&o,21+i,false,false);
    }
    assert(o.count==SCI_OBSERVE_CAPACITY && o.total==20);
    for(unsigned i=0;i<o.count;i++) {
        r=sci_observe_row(&o,i);assert(r->sequence==13+i);
        assert(!r->before.valid && !r->after.valid && !r->before.lapic_bits && !r->post_eoi_observed);
    }
    assert(!sci_observe_row(&o,o.count));
    assert(sci_observe_begin(&o,1,0,false));sci_observe_end(&o,0,false,false);
    r=sci_observe_row(&o,o.count-1);assert(r->duration_us==0 && r->previous_gap_us==0 && r->valid==0);
    o.total=UINT64_MAX;assert(sci_observe_begin(&o,50,1,false));sci_observe_end(&o,51,true,false);
    assert(o.total==UINT64_MAX);
    puts("sci-observe: PASS first-read/width/address gating, unknown vs zero, filtering, timing, claims, bounded ring and saturation");
}
