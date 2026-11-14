#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/scanout_handoff.h>

static struct scanout_handoff h;
static struct scanout_space_pool pool;
static struct display_access_token token;
static struct scanout_handoff_snapshot snapshot;
static const char *mode;
static unsigned calls, depth;
static bool closing, fail;
static bool validate(void *,const struct scanout_handoff_snapshot *);
static struct scanout_handoff_io io(void) {
    return (struct scanout_handoff_io){&snapshot,validate};
}
static bool validate(void *ctx,const struct scanout_handoff_snapshot *s) {
    assert(ctx==&snapshot);calls++;
    if(!depth++) {
        if(!strcmp(mode,"reenter-open") && !closing)
            assert(scanout_handoff_open(&h,&pool,&token,s,io())==HANDOFF_INVALID);
        if(!strcmp(mode,"reenter-close") && closing)
            assert(!scanout_handoff_close(&h,io()));
        if(!strcmp(mode,"release-during-open") && !closing)
            assert(!display_access_release(&token,true));
        if(!strcmp(mode,"snapshot-input") && !closing) {
            /* The provider validated the captured input, then a shared caller
             * changes its original storage. It must not change the import. */
            assert(s->first==8704);snapshot.first=1;snapshot.assurances=0;
        }
        if(!strcmp(mode,"token-input") && !closing)token.epoch++;
        if(!strcmp(mode,"poison-open") && !closing)
            assert(!display_access_release(&token,false));
        if(!strcmp(mode,"poison-close") && closing)
            assert(!display_access_release(&token,false));
    }
    depth--;
    return !fail;
}
static bool other(void *ctx,const struct scanout_handoff_snapshot *s) {
    (void)ctx;(void)s;assert(!"different provider must not run");return true;
}
int main(int argc,char **argv) {
    assert(argc==2);mode=argv[1];
    snapshot=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,
        .aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16,
        .assurances=HANDOFF_ALL};
    for(unsigned i=0;i<HANDOFF_PAGES;i++)snapshot.original[i]=0x8e201001ull+i*4096;
    assert(display_access_acquire(1,&token));struct display_access_token saved=token;
    if(!strcmp(mode,"other-pin"))assert(display_access_pin(&saved));
    if(!strcmp(mode,"failed-open"))fail=true;
    if(!strcmp(mode,"pool-exhausted"))pool.epoch=UINT64_MAX;
    uint32_t result=scanout_handoff_open(&h,&pool,&token,&snapshot,io());
    if(!strcmp(mode,"poison-open")) {
        assert(result==HANDOFF_EXCLUSION && !h.active);
        assert(!display_access_allowed() && !display_access_release(&saved,true));
        assert(scanout_handoff_open(&h,&pool,&saved,&snapshot,io())==HANDOFF_INVALID);
        puts("PASS retained poisoned admission");return 0;
    }
    if(fail || !strcmp(mode,"pool-exhausted")) {
        assert(result==(fail?HANDOFF_SNAPSHOT:HANDOFF_SPACE) && !h.active);
        assert(display_access_release(&saved,true));
        puts("PASS clean admission unwind");return 0;
    }
    assert(!result && h.active && !display_access_release(&saved,true));
    if(!strcmp(mode,"snapshot-input")) {
        assert(h.snapshot.first==8704 && h.snapshot.assurances==HANDOFF_ALL);
        struct scanout_space_token a;
        assert(scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&a) && a.first==8704);
        assert(scanout_space_release(&h.space,&a));
        snapshot=h.snapshot;
    }
    token=saved;closing=true;
    if(!strcmp(mode,"copy-close")) {
        struct scanout_handoff copy=h;unsigned before=calls;
        assert(!scanout_handoff_close(&copy,io()) && calls==before);
        assert(h.active && !display_access_release(&saved,true));
    }
    if(!strcmp(mode,"provider-context") || !strcmp(mode,"provider-function")) {
        struct scanout_handoff_io wrong=io();unsigned before=calls;
        if(!strcmp(mode,"provider-context"))wrong.context=&pool;
        else wrong.revalidate=other;
        assert(!scanout_handoff_close(&h,wrong) && calls==before);
        assert(h.active && !display_access_release(&saved,true));
    }
    if(!strcmp(mode,"close-admission")) {
        struct scanout_space_token a,b;unsigned before=calls;
        assert(scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&a));
        assert(!scanout_handoff_close(&h,io()) && calls==before);
        assert(!scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&b));
        assert(scanout_space_release(&h.space,&a));
    }
    if(!strcmp(mode,"failed-close")) {
        fail=true;assert(!scanout_handoff_close(&h,io()));
        struct scanout_space_token out;
        assert(!scanout_space_reserve(&h.space,1,h.space.epoch,1,1,&out));
        assert(!display_access_release(&saved,true));fail=false;
    }
    if(!strcmp(mode,"poison-close")) {
        assert(!scanout_handoff_close(&h,io()) && h.active);
        assert(!display_access_allowed() && !display_access_release(&saved,true));
        assert(!scanout_handoff_close(&h,io()));
        puts("PASS retained poisoned retirement");return 0;
    }
    assert(scanout_handoff_close(&h,io()));
    assert(!scanout_handoff_close(&h,io()));
    if(!strcmp(mode,"other-pin")) {
        assert(!display_access_release(&saved,true));
        assert(display_access_unpin(&saved));
    }
    assert(display_access_release(&saved,true) && display_access_allowed());
    if(!strcmp(mode,"copy-reopen")) {
        struct scanout_handoff copy=h;assert(display_access_acquire(1,&token));
        unsigned before=calls;
        assert(scanout_handoff_open(&copy,&pool,&token,&snapshot,io())==HANDOFF_INVALID && calls==before);
        assert(!scanout_handoff_open(&h,&pool,&token,&snapshot,io()));
        assert(scanout_handoff_close(&h,io()) && display_access_release(&token,true));
    }
    puts("PASS handoff lifetime");
}
