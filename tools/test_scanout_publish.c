#include <srvros/scanout_publish.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define PAGES SCANOUT_SPACE_PAGES
static struct scanout_space space;
static struct scanout_space_pool ranges;
static struct scanout_publication pub;
static struct scanout_publish_pool epochs;
static struct scanout_space_token lease;
static struct scanout_publish_token token;
static uint64_t desired[SCANOUT_PUBLISH_PAGES];
static struct {
    uint64_t table[PAGES],tlb[PAGES],posted[PAGES],time;
    bool queued[PAGES],readers,reenter,frozen;
    unsigned op,fail_op,close_op,clock,rollback,freeze_at,late_at,tamper_read,read_count,reentries;
    unsigned write_calls,flush_calls,retire_calls,prepared_clock,published_clock,corrupt_write;
} m;
static uint64_t initial(unsigned i){return 0x120000001ull+4096ull*i;}
static void reentry(void) {
    assert(pub.busy);
    if(!m.reenter)return;
    struct scanout_publish_token fake={&pub,pub.epoch,pub.owner},out;
    assert(!scanout_publish_apply(&pub,&fake));assert(!scanout_publish_restore(&pub,&fake));
    assert(!scanout_publish_cancel(&pub,&fake));
    assert(!scanout_publish_prepare(&pub,&epochs,7,&space,&lease,desired,lease.pages,&pub.io,&out));
    m.reentries++;
}
static bool pass(void){m.op++;reentry();if(m.op==m.close_op)assert(scanout_space_close(&space,7,space.epoch));return m.op!=m.fail_op;}
static bool guard(void *ctx){assert(ctx==&m);return pass();}
static bool read_pte(void *ctx,uint32_t i,uint64_t *v) {
    assert(ctx==&m && i>=lease.first && i<lease.first+lease.pages);
    if(!pass())return false;
    *v=m.table[i];if(++m.read_count==m.tamper_read)*v^=0x1000;
    return true;
}
static bool write_pte(void *ctx,uint32_t i,uint64_t v) {
    assert(ctx==&m && i>=lease.first && i<lease.first+lease.pages);
    assert(pub.exposed && space.leases[lease.slot].use==SPACE_EXPOSED && !scanout_space_release(&space,&lease));
    bool ok=pass();m.write_calls++;
    /* Failure can already have queued a torn low-half write. */
    m.posted[i]=ok?v:((m.table[i]&0xffffffff00000000ull)|(v&0xffffffff));m.queued[i]=true;
    if(m.write_calls==m.corrupt_write)m.posted[i]^=0x1000;
    return ok;
}
static bool flush(void *ctx) {
    assert(ctx==&m && pub.exposed);bool ok=pass();m.flush_calls++;
    for(unsigned i=lease.first;i<lease.first+lease.pages;i++)if(m.queued[i]) {
        m.table[i]=m.posted[i];m.queued[i]=false;
        if(!ok)break; /* Partial posted completion; stale translations remain. */
    }
    if(ok)for(unsigned i=lease.first;i<lease.first+lease.pages;i++)m.tlb[i]=m.table[i];
    return ok;
}
static bool retired(void *ctx){assert(ctx==&m);m.retire_calls++;return pass() && m.readers;}
static uint64_t now(void *ctx) {
    assert(ctx==&m);reentry();m.clock++;
    if(m.clock==m.rollback)return m.time-100;
    if(m.clock==m.freeze_at)m.frozen=true;
    if(m.clock==m.late_at)m.time+=SCANOUT_PUBLISH_DEADLINE_US+1;
    if(!m.frozen)m.time++;
    return m.time;
}
static struct scanout_publish_io io={&m,guard,read_pte,write_pte,flush,retired,now};
static void setup(unsigned pages) {
    memset(&space,0,sizeof(space));memset(&pub,0,sizeof(pub));memset(&m,0,sizeof(m));
    ranges=(struct scanout_space_pool){0};epochs=(struct scanout_publish_pool){0};token=(struct scanout_publish_token){0};
    struct scanout_space_range all={0,PAGES},protected={0,8704};
    assert(scanout_space_open(&space,&ranges,7,PAGES,&all,1,&protected,1));
    assert(scanout_space_reserve(&space,7,space.epoch,pages,1,&lease) && lease.first==8704);
    for(unsigned i=0;i<PAGES;i++)m.table[i]=m.tlb[i]=initial(i);
    for(unsigned i=0;i<pages;i++)desired[i]=0x300000001ull+4096ull*i;
    m.time=1000;m.readers=true;
}
static bool prepare(void){return scanout_publish_prepare(&pub,&epochs,7,&space,&lease,desired,lease.pages,&io,&token);}
static void oracle(bool published) {
    for(unsigned i=0;i<PAGES;i++) {
        uint64_t expected=published && i>=lease.first && i<lease.first+lease.pages?desired[i-lease.first]:initial(i);
        assert(m.table[i]==expected && m.tlb[i]==expected && !m.queued[i]);
    }
}
static void fault_or_cancel(void) {
    assert(!pub.busy);
    if(pub.exposed) {
        assert(pub.state==PUB_FAULT && space.state==SPACE_FAULT && space.leases[lease.slot].use==SPACE_QUARANTINED);
        assert(!scanout_space_release(&space,&lease));
    } else {
        assert(pub.state==PUB_CANCELLED && space.leases[lease.slot].use==SPACE_RESERVED && !m.write_calls);
        assert(scanout_space_release(&space,&lease));
    }
    unsigned calls=m.op,clocks=m.clock;
    assert(!scanout_publish_apply(&pub,&token) && !scanout_publish_restore(&pub,&token) && !scanout_publish_cancel(&pub,&token));
    assert(m.op==calls && m.clock==clocks);
}
static bool full(void) {
    if(!prepare())return false;
    m.prepared_clock=m.clock;
    assert(pub.state==PUB_PREPARED && !pub.exposed && space.leases[lease.slot].use==SPACE_RESERVED);
    if(!scanout_publish_apply(&pub,&token))return false;
    m.published_clock=m.clock;
    oracle(true);assert(pub.state==PUB_PUBLISHED && !scanout_space_release(&space,&lease));
    if(!scanout_publish_restore(&pub,&token))return false;
    oracle(false);assert(pub.state==PUB_RETIRED && space.leases[lease.slot].use==SPACE_RETIRED);
    return true;
}
int main(void) {
    uint64_t bytes=9;uint32_t pages=9;
    assert(scanout_linear_storage(1440,900,6144,&bytes,&pages) && bytes==5529600 && pages==1350);
    bytes=9;pages=9;assert(!scanout_linear_storage(0,900,6144,&bytes,&pages));
    assert(!scanout_linear_storage(1440,900,5759,&bytes,&pages));
    assert(!scanout_linear_storage(UINT32_MAX,UINT32_MAX,UINT32_MAX-63,&bytes,&pages));assert(bytes==9 && pages==9);
    setup(4);m.reenter=true;assert(full() && m.reentries && pub.writes==8 && pub.flushes==2);
    unsigned operations=m.op,clocks=m.clock,reads=m.read_count,apply_origin=m.prepared_clock+1,restore_origin=m.published_clock+1;
    assert(scanout_space_release(&space,&lease));
    for(unsigned fail=1;fail<=operations;fail++) {
        setup(4);m.fail_op=fail;
        assert(!full());
        if(pub.state==PUB_PUBLISHED) { /* Busy reader refusal made no restore write. */
            assert(m.retire_calls==1 && m.write_calls==4);oracle(true);
            m.fail_op=0;assert(scanout_publish_restore(&pub,&token));oracle(false);
        } else fault_or_cancel();
    }
    for(unsigned close=1;close<=operations;close++) {
        setup(4);m.close_op=close;
        if(full()) {assert(space.state==SPACE_CLOSING && scanout_space_release(&space,&lease) && space.state==SPACE_CLOSED);}
        else fault_or_cancel();
    }
    for(unsigned fail=1;fail<=clocks;fail++) {
        setup(4);m.rollback=fail;if(full()){assert(fail==1);}else fault_or_cancel();
        setup(4);m.late_at=fail;
        if(full()) {assert(fail==1 || fail==apply_origin || fail==restore_origin);}else fault_or_cancel(); /* Each phase has its own origin. */
    }
    for(unsigned fail=1;fail<=reads;fail++) {
        setup(4);m.tamper_read=fail;
        if(full()) { /* Snapshot can be arbitrary; the later comparison catches it. */
            assert(!"mismatching snapshot accepted");
        } else fault_or_cancel();
    }
    for(unsigned bad=1;bad<=8;bad++) {
        setup(4);m.corrupt_write=bad;assert(!full() && pub.error==PUB_CHANGED);fault_or_cancel();
    }
    setup(4);assert(prepare());struct scanout_publish_token foreign=token;
    unsigned untouched=m.op;foreign.owner++;assert(!scanout_publish_apply(&pub,&foreign));
    foreign=token;foreign.epoch++;assert(!scanout_publish_apply(&pub,&foreign));
    foreign=token;foreign.object=(void *)1;assert(!scanout_publish_apply(&pub,&foreign));assert(m.op==untouched);
    assert(scanout_publish_apply(&pub,&token));untouched=m.op;
    assert(!scanout_publish_cancel(&pub,&token) && !scanout_publish_apply(&pub,&token) && m.op==untouched);
    assert(scanout_publish_restore(&pub,&token));oracle(false);
    setup(4);m.frozen=true;assert(!prepare() && pub.error==PUB_CLOCK);fault_or_cancel();
    setup(4);assert(prepare());m.frozen=true;assert(!scanout_publish_apply(&pub,&token) && !pub.exposed);fault_or_cancel();
    setup(4);assert(prepare() && scanout_publish_apply(&pub,&token));m.readers=false;
    unsigned writes=m.write_calls;assert(!scanout_publish_restore(&pub,&token) && pub.state==PUB_PUBLISHED && m.write_calls==writes);
    assert(scanout_space_close(&space,7,space.epoch));m.readers=true;
    assert(scanout_publish_restore(&pub,&token) && scanout_space_release(&space,&lease) && space.state==SPACE_CLOSED);oracle(false);
    setup(4);assert(prepare());m.table[lease.first]^=0x1000;
    assert(!scanout_publish_apply(&pub,&token) && !pub.exposed && !m.write_calls);fault_or_cancel();
    setup(4);assert(prepare() && scanout_publish_apply(&pub,&token));m.table[lease.first]^=0x1000;
    assert(!scanout_publish_restore(&pub,&token) && pub.writes==4);fault_or_cancel();
    setup(4);assert(prepare() && scanout_publish_apply(&pub,&token));m.frozen=true;writes=m.write_calls;
    assert(!scanout_publish_restore(&pub,&token) && m.write_calls==writes);fault_or_cancel();
    setup(4);assert(prepare());struct scanout_publish_token old=token;
    assert(scanout_publish_cancel(&pub,&token) && scanout_space_release(&space,&lease));
    assert(scanout_space_reserve(&space,7,space.epoch,4,1,&lease));assert(prepare() && token.epoch!=old.epoch);
    unsigned calls=m.op;assert(!scanout_publish_apply(&pub,&old) && m.op==calls);
    assert(scanout_publish_cancel(&pub,&token));
    struct scanout_publish_pool reset_pool={0};assert(!scanout_publish_prepare(&pub,&reset_pool,7,&space,&lease,desired,4,&io,&token));
    setup(4);assert(prepare());assert(scanout_space_close(&space,7,space.epoch));
    assert(!scanout_publish_apply(&pub,&token) && !m.write_calls);fault_or_cancel();
    setup(4);struct scanout_publish_token unchanged={(void *)1,2,3};token=unchanged;
    assert(!scanout_publish_prepare(&pub,&epochs,7,&space,&lease,pub.after,4,&io,&token));
    assert(!m.op && !pub.epoch && token.object==unchanged.object && token.epoch==2);
    struct scanout_space_token wrong=lease;wrong.serial++;
    assert(!scanout_publish_prepare(&pub,&epochs,7,&space,&wrong,desired,4,&io,&token) && !m.op);
    epochs.epoch=UINT64_MAX;assert(!prepare() && !m.op);epochs.epoch=0;
    assert(prepare());uint64_t copied=desired[0];desired[0]^=0x4000;
    assert(scanout_publish_apply(&pub,&token) && m.table[lease.first]==copied && m.tlb[lease.first]==copied);
    assert(scanout_publish_restore(&pub,&token) && scanout_space_release(&space,&lease));oracle(false);
    setup(SCANOUT_PUBLISH_PAGES);assert(full() && pub.writes==2*SCANOUT_PUBLISH_PAGES);assert(scanout_space_release(&space,&lease));
    printf("scanout-publish: PASS full-fence isolation, posted writes/TLB oracle, %u callback faults, %u clock points, %u read corruptions, stale identity, reentry and 1536-page restore\n",operations,clocks,reads);
}
