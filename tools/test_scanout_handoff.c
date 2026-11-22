#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/scanout_handoff.h>
static struct scanout_handoff h;
static struct scanout_space_pool pool;
static struct scanout_handoff_snapshot expected;
static unsigned reads;
static bool drift;
static bool revalidate(void *ctx,const struct scanout_handoff_snapshot *s) {
    assert(ctx==&expected);reads++;return !drift && !memcmp(s,&expected,sizeof(*s));
}
int main(void) {
    struct display_access_token t,stale;
    assert(display_access_enter());assert(!display_access_acquire(1,&t));display_access_leave();
    assert(!display_access_acquire(0,&t) && !display_access_acquire(1,NULL));
    assert(display_access_acquire(1,&t));stale=t;stale.epoch++;
    assert(!display_access_allowed() && !display_access_enter() && !display_access_release(&stale,true));
    expected=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,
        .fence_end=0x2200000,.first=8704,.pages=16,.assurances=HANDOFF_ALL};
    /* Populated originals are retained byte-for-byte, not classified as free. */
    for(unsigned i=0;i<16;i++)expected.original[i]=0x8e201001ull+i*4096;
    struct scanout_handoff_io io={&expected,revalidate};
    for(unsigned bit=1;bit<=HANDOFF_CACHE;bit<<=1) {
        struct scanout_handoff_snapshot s=expected;s.assurances^=bit;
        assert(scanout_handoff_open(&h,&pool,&t,&s,io)==bit && !reads && !h.active);
    }
    struct scanout_handoff_snapshot s=expected;s.first=8703;
    assert(scanout_handoff_open(&h,&pool,&t,&s,io)==HANDOFF_INVALID && !reads);
    s=expected;s.pages=17;assert(scanout_handoff_open(&h,&pool,&t,&s,io)==HANDOFF_INVALID);
    s=expected;s.aperture_bytes=UINT64_MAX;assert(scanout_handoff_open(&h,&pool,&t,&s,io)==HANDOFF_INVALID);
    assert(scanout_handoff_open(&h,&pool,&stale,&expected,io)==HANDOFF_EXCLUSION && !reads);
    drift=true;assert(scanout_handoff_open(&h,&pool,&t,&expected,io)==HANDOFF_SNAPSHOT && !h.active);drift=false;
    assert(!scanout_handoff_open(&h,&pool,&t,&expected,io) && h.active);
    assert(!memcmp(&h.snapshot,&expected,sizeof(expected)) && !display_access_release(&t,true));
    struct scanout_space_token a,b;
    assert(scanout_space_reserve(&h.space,1,h.space.epoch,16,1,&a) && a.first==8704);
    assert(!scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&b));
    assert(scanout_space_expose(&h.space,&a));
    assert(!scanout_handoff_close(&h,io) && !display_access_release(&t,true));
    assert(!scanout_space_retire(&h.space,&a,3));
    assert(scanout_space_retire(&h.space,&a,SCANOUT_SPACE_RETIRE_ALL) && scanout_space_release(&h.space,&a));
    drift=true;assert(!scanout_handoff_close(&h,io) && !display_access_release(&t,true));drift=false;
    assert(scanout_handoff_close(&h,io) && display_access_release(&t,true));
    assert(!display_access_held(&t) && display_access_allowed());
    stale=t;assert(display_access_acquire(1,&t) && t.epoch>stale.epoch);
    assert(!display_access_release(&stale,true) && !display_access_enter());
    /* A stale original vector cannot be admitted, even with all assurances. */
    s=expected;s.original[15]^=1;
    assert(scanout_handoff_open(&h,&pool,&t,&s,io)==HANDOFF_SNAPSHOT && !h.active);
    assert(!scanout_handoff_open(&h,&pool,&t,&expected,io));
    assert(scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&a));
    assert(scanout_space_expose(&h.space,&a) && scanout_space_quarantine(&h.space,&a));
    assert(!scanout_handoff_close(&h,io) && !display_access_release(&t,true));
    assert(!display_access_release(&t,false) && !display_access_allowed() && !display_access_enter());
    assert(!display_access_acquire(2,&stale) && !display_access_release(&t,true));
    puts("scanout-handoff: PASS missing assurances, exact originals, inherited fence, exclusion epochs, pinned retirement and quarantine");
}
