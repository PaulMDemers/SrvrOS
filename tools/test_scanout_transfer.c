#include <srvros/scanout_transfer.h>
#include <srvros/scanout_memory.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct scanout_transfer transfer,copy;
static struct scanout_transfer_pool pool;
static struct scanout_space_pool spaces;
static struct display_access_token exclusion;
static struct scanout_transfer_token token;
static struct scanout_transfer_manifest manifest;
static struct scanout_memory a,b;
static struct scanout_memory_pool memories;
static struct scanout_publish_pool publications;
static struct scanout_memory_token at,bt;
static uint64_t input[SCANOUT_TRANSFER_PAGES],table[SCANOUT_SPACE_PAGES];
static struct {
    uint64_t time,source_epoch,physical;
    unsigned calls[3],releases,allocations,reads,writes,flushes,write_fail,first,pages,memory_pages;
    bool mutate,reenter,poison,pending,fail,stuck,regress,clock_fail,step_deadline,post_open_clock,other_pin;
} model;
static struct scanout_transfer_io provider(void);
static uint64_t now(void *ctx) {
    assert(ctx==&model);
    if(model.clock_fail)return 0;
    if(model.regress)return 1;
    if(model.post_open_clock && transfer.space.state==SPACE_ACTIVE)return 0;
    if(model.step_deadline)model.time+=SCANOUT_TRANSFER_DEADLINE_US;
    return model.stuck?model.time:++model.time;
}
static bool revalidate(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
    assert(ctx==&model && !display_access_allowed());model.calls[phase]++;
    assert(m!=&manifest && m->original==transfer.original && m->source_epoch==1);
    assert(m->first==model.first && m->pages==model.pages && m->assurances==HANDOFF_ALL && !m->obligations);
    assert(!display_access_release(&exclusion,true));
    if(model.poison)assert(!display_access_release(&exclusion,false));
    if(model.reenter) {
        assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
        assert(!scanout_transfer_check(&transfer,&token));
        assert(!scanout_transfer_close(&transfer,&token));
        uint64_t out;assert(!scanout_transfer_original(&transfer,&token,8704,&out));
    }
    if(model.mutate && phase==TRANSFER_ADMIT) {
        manifest.first=1;manifest.pages=1;manifest.assurances=0;input[2711]^=1;exclusion.epoch++;
    }
    if(model.fail || model.source_epoch!=m->source_epoch)return false;
    if(phase!=TRANSFER_LIVE)for(unsigned i=0;i<m->pages;i++)
        if(table[m->first+i]!=m->original[i])return false;
    return true;
}
static struct scanout_transfer_io provider(void) {return (struct scanout_transfer_io){&model,revalidate,now};}
static void setup(void) {
    model.time=100;model.source_epoch=1;model.physical=0x1000000;model.first=8704;model.pages=2712;model.memory_pages=1356;
    for(unsigned i=0;i<SCANOUT_SPACE_PAGES;i++)table[i]=0x200000001ull+(uint64_t)i*4096;
    for(unsigned i=0;i<SCANOUT_TRANSFER_PAGES;i++)input[i]=table[8704+i];
    manifest=(struct scanout_transfer_manifest){.mmio=0xc0000000,.aperture=0xb0000000,
        .aperture_bytes=0x10000000,.fence_end=0x2200000,.source_epoch=1,
        .first=8704,.pages=2712,.assurances=HANDOFF_ALL,.original=input};
    assert(display_access_acquire(7,&exclusion));
}
static void opened(void) {
    assert(scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
    assert(transfer.state==TRANSFER_ACTIVE && transfer.pinned && model.calls[TRANSFER_ADMIT]==1);
    assert(transfer.manifest.original==transfer.original && !display_access_release(&transfer.exclusion,true));
}
static uint64_t allocate(void *ctx,uint32_t pages) {
    assert(ctx==&model && pages==model.memory_pages);model.allocations++;
    uint64_t p=model.physical;model.physical+=(uint64_t)pages*4096;return p;
}
static bool backing(void *ctx,uint64_t pa,uint32_t pages) {
    assert(ctx==&model);return pages==model.memory_pages && !(pa&4095) && pa>=0x1000000 && pa<model.physical;
}
static bool release(void *ctx,uint64_t pa,uint32_t pages) {
    assert(backing(ctx,pa,pages));model.releases++;return true;
}
static bool encode(void *ctx,uint64_t pa,uint64_t *out) {
    assert(ctx==&model);*out=pa|1;return true;
}
static bool guard(void *ctx) {assert(ctx==&model);return scanout_transfer_check(&transfer,&token);}
static bool read_entry(void *ctx,uint32_t index,uint64_t *out) {
    assert(ctx==&model && index>=8704 && index<11416);model.reads++;*out=table[index];return true;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t value) {
    assert(ctx==&model && index>=8704 && index<11416);table[index]=value;
    return ++model.writes!=model.write_fail;
}
static bool flush(void *ctx) {assert(ctx==&model);model.flushes++;return true;}
static bool readers(void *ctx) {assert(ctx==&model);return !model.pending;}
static struct scanout_memory_binding binding(void) {
    return (struct scanout_memory_binding){&model,encode,{&model,guard,read_entry,write_entry,flush,readers,now}};
}
static void memory(struct scanout_memory *m,struct scanout_memory_token *out) {
    struct scanout_memory_layout layout;
    assert(model.memory_pages==1?scanout_memory_layout(1,1,64,false,&layout):scanout_memory_layout(1440,900,6144,true,&layout));
    assert(scanout_memory_create(m,&memories,7,&layout,0x100000000ull,
        (struct scanout_memory_io){&model,allocate,backing,release},out));
    assert(scanout_memory_prepare(m,out,&transfer.space,&publications,1,binding()));
}
static void close_clean(void) {
    assert(scanout_transfer_close(&transfer,&token));
    assert(transfer.state==TRANSFER_CLOSED && !transfer.pinned && model.calls[TRANSFER_RETIRE]==1);
    assert(!scanout_transfer_close(&transfer,&token));
    if(model.other_pin) {
        assert(!display_access_release(&transfer.exclusion,true));
        assert(display_access_unpin(&transfer.exclusion));
    }
    assert(display_access_release(&transfer.exclusion,true));assert(display_access_allowed());
}
int main(int argc,char **argv) {
    assert(argc==2);const char *mode=argv[1];setup();
    if(!strcmp(mode,"other-pin")){assert(display_access_pin(&exclusion));model.other_pin=true;}
    if(!strcmp(mode,"one-page"))model.memory_pages=model.pages=manifest.pages=1;
    if(!strcmp(mode,"capacity") || !strcmp(mode,"last-aperture-page")) {
        model.pages=manifest.pages=SCANOUT_TRANSFER_PAGES;
        if(!strcmp(mode,"last-aperture-page"))model.first=manifest.first=SCANOUT_SPACE_PAGES-SCANOUT_TRANSFER_PAGES;
        for(unsigned i=0;i<model.pages;i++)input[i]=table[model.first+i];
    }
    if(!strcmp(mode,"missing")) {
        struct scanout_transfer_token untouched={.epoch=55},saved=untouched;
        for(unsigned bit=1;bit<=HANDOFF_CACHE;bit<<=1) {
            manifest.assurances=HANDOFF_ALL^bit;
            assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&untouched));
        }
        manifest.assurances=HANDOFF_ALL;
        for(unsigned bit=1;bit<=128;bit<<=1) {
            manifest.obligations=bit;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&untouched));
        }
        assert(!memcmp(&untouched,&saved,sizeof saved) && !pool.epoch && !model.calls[0]);
        assert(display_access_release(&exclusion,true));puts("PASS missing hardware grants refuse before provider");return 0;
    }
    if(!strcmp(mode,"invalid")) {
        struct scanout_transfer_manifest saved=manifest;
        const unsigned firsts[]={0,8703,65536,UINT32_MAX};
        for(unsigned i=0;i<4;i++){manifest.first=firsts[i];assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));}
        manifest=saved;manifest.pages=4097;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
        manifest=saved;manifest.assurances|=32;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
        manifest=saved;manifest.original=transfer.original;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
        manifest=saved;manifest.original=(const uint64_t *)(UINTPTR_MAX-7);assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));
        manifest=saved;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),(void *)&transfer));
        assert(!scanout_transfer_open(&transfer,(void *)input,&spaces,&exclusion,&manifest,provider(),&token));
        assert(!scanout_transfer_open(&transfer,&pool,(void *)input,&exclusion,&manifest,provider(),&token));
        assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),(void *)input));
        assert(!scanout_transfer_open(&transfer,&pool,(void *)&pool,&exclusion,&manifest,provider(),&token));
        assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),(void *)&spaces));
        pool.epoch=UINT64_MAX;assert(!scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token));pool.epoch=0;
        assert(!pool.epoch && !model.calls[0] && display_access_release(&exclusion,true));puts("PASS bounds and aliases");return 0;
    }
    if(!strcmp(mode,"failed-open"))model.fail=true;
    if(!strcmp(mode,"clock-open"))model.clock_fail=true;
    if(!strcmp(mode,"stuck-open"))model.stuck=true;
    if(!strcmp(mode,"deadline-open"))model.step_deadline=true;
    if(!strcmp(mode,"post-open-clock"))model.post_open_clock=true;
    if(!strcmp(mode,"source-open"))model.source_epoch=2;
    if(!strcmp(mode,"changed-first"))table[8704]^=1;
    if(!strcmp(mode,"changed-last"))table[11415]^=1;
    if(!strcmp(mode,"pool-exhausted"))spaces.epoch=UINT64_MAX;
    if(!strcmp(mode,"poison-open"))model.poison=true;
    if(!strcmp(mode,"input-mutation"))model.mutate=true;
    if(!strcmp(mode,"reentry"))model.reenter=true;
    bool ok=scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token);
    if(!strcmp(mode,"poison-open")) {
        assert(!ok && transfer.state==TRANSFER_FAULT && transfer.pinned && transfer.error==TRANSFER_EXCLUSION);
        assert(!display_access_allowed() && !display_access_release(&transfer.exclusion,true));puts("PASS poisoned open stays pinned");return 0;
    }
    if(!ok) {
        assert(!strcmp(mode,"failed-open") || !strcmp(mode,"clock-open") || !strcmp(mode,"stuck-open") ||
            !strcmp(mode,"deadline-open") || !strcmp(mode,"post-open-clock") || !strcmp(mode,"source-open") ||
            !strcmp(mode,"changed-first") || !strcmp(mode,"changed-last") || !strcmp(mode,"pool-exhausted"));
        assert(transfer.state==TRANSFER_CLOSED && !transfer.pinned && !token.object);
        assert(display_access_release(&exclusion,true));puts("PASS read-only admission unwind");return 0;
    }
    assert(transfer.state==TRANSFER_ACTIVE && transfer.pinned && model.calls[TRANSFER_ADMIT]==1);
    uint64_t value=0;assert(scanout_transfer_original(&transfer,&token,model.first+model.pages-1,&value) && value==table[model.first+model.pages-1]);
    assert(!scanout_transfer_original(&transfer,&token,model.first+model.pages,&value));
    assert(!scanout_transfer_original(&transfer,&token,model.first-1,&value));
    assert(!scanout_transfer_original(&transfer,&token,8704,&transfer.original[0]));
    copy=transfer;struct scanout_transfer_token rebound=token;rebound.object=&copy;
    assert(!scanout_transfer_check(&copy,&rebound) && !scanout_transfer_close(&copy,&rebound));
    assert(!scanout_transfer_original(&copy,&rebound,8704,&value));
    assert(!scanout_transfer_open(&copy,&pool,&spaces,&exclusion,&manifest,provider(),&rebound));
    assert(!display_access_release(&transfer.exclusion,true));
    if(!strcmp(mode,"source-live") || !strcmp(mode,"clock-live") || !strcmp(mode,"poison-live") || !strcmp(mode,"regress-live")) {
        struct scanout_space_token range;assert(scanout_space_reserve(&transfer.space,7,transfer.arena_epoch,1,1,&range));
        if(!strcmp(mode,"source-live"))model.source_epoch++;
        if(!strcmp(mode,"clock-live"))model.clock_fail=true;
        if(!strcmp(mode,"poison-live"))model.poison=true;
        if(!strcmp(mode,"regress-live"))model.regress=true;
        assert(!scanout_transfer_check(&transfer,&token) && transfer.state==TRANSFER_FAULT && transfer.pinned);
        assert(!scanout_space_reserve(&transfer.space,7,transfer.arena_epoch,1,1,&range));
        assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&transfer.exclusion,true));
        puts("PASS lost live owner is terminal and stops admission");return 0;
    }
    if(!strcmp(mode,"failed-close") || !strcmp(mode,"clock-close") || !strcmp(mode,"changed-close")) {
        if(!strcmp(mode,"failed-close"))model.fail=true;
        if(!strcmp(mode,"clock-close"))model.clock_fail=true;
        if(!strcmp(mode,"changed-close"))table[11415]^=1;
        assert(!scanout_transfer_close(&transfer,&token) && transfer.state==TRANSFER_FAULT && transfer.pinned);
        assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&transfer.exclusion,true));
        puts("PASS failed retirement keeps exclusion");return 0;
    }
    if(!strcmp(mode,"one-page")) {
        memory(&a,&at);assert(a.range.first==8704 && a.range.pages==1);
        assert(scanout_memory_apply(&a,&at));assert(table[8704]==(a.physical|1));
        assert(!scanout_transfer_close(&transfer,&token) && !model.calls[TRANSFER_RETIRE]);
        assert(scanout_memory_close(&a,&at) && model.releases==1 && table[8704]==input[0]);
        assert(model.writes==2 && model.flushes==2);
    }
    if(!strcmp(mode,"full-buffers") || !strcmp(mode,"partial-write") || !strcmp(mode,"pending-readers") ||
       !strcmp(mode,"partial-restore") || !strcmp(mode,"lost-mapped-owner")) {
        memory(&a,&at);memory(&b,&bt);
        assert(a.range.first==8704 && b.range.first==10060);
        if(!strcmp(mode,"partial-write"))model.write_fail=1356;
        bool published=scanout_memory_apply(&a,&at);
        if(!strcmp(mode,"partial-write")) {
            assert(!published && a.state==MEM_FAULT && a.publication.exposed);
            assert(!scanout_transfer_close(&transfer,&token) && transfer.pinned);
            assert(!scanout_memory_close(&a,&at) && !model.releases);
            assert(!display_access_release(&transfer.exclusion,true));puts("PASS uncertain mapping retains RAM/range/exclusion");return 0;
        }
        assert(published && scanout_memory_apply(&b,&bt));
        if(!strcmp(mode,"partial-restore") || !strcmp(mode,"lost-mapped-owner")) {
            if(!strcmp(mode,"partial-restore"))model.write_fail=model.writes+1356;
            else model.source_epoch++;
            assert(!scanout_memory_close(&a,&at) && a.state==MEM_FAULT && !model.releases);
            assert(!scanout_transfer_close(&transfer,&token) && transfer.state==TRANSFER_FAULT && transfer.pinned);
            assert(!display_access_release(&transfer.exclusion,true));puts("PASS mapped-owner/restore failure retains buffers and exclusion");return 0;
        }
        unsigned calls=model.calls[TRANSFER_RETIRE];
        assert(!scanout_transfer_close(&transfer,&token) && transfer.state==TRANSFER_CLOSING);
        assert(model.calls[TRANSFER_RETIRE]==calls);
        assert(scanout_transfer_original(&transfer,&token,11415,&value));
        struct scanout_space_token third;assert(!scanout_space_reserve(&transfer.space,7,transfer.arena_epoch,1,1,&third));
        if(!strcmp(mode,"pending-readers")) {
            model.pending=true;unsigned writes=model.writes;
            assert(!scanout_memory_close(&a,&at) && model.writes==writes && !model.releases);
            assert(!scanout_transfer_close(&transfer,&token) && model.calls[TRANSFER_RETIRE]==calls);
            model.pending=false;
        }
        assert(scanout_memory_close(&a,&at) && model.releases==1);
        assert(!scanout_transfer_close(&transfer,&token) && model.calls[TRANSFER_RETIRE]==calls);
        assert(scanout_memory_close(&b,&bt) && model.releases==2 && model.flushes==4);
        for(unsigned i=0;i<2712;i++)assert(table[8704+i]==transfer.original[i]);
    }
    struct scanout_transfer_token stale=token;
    close_clean();
    if(!strcmp(mode,"reuse")) {
        model.calls[TRANSFER_ADMIT]=model.calls[TRANSFER_RETIRE]=0;
        assert(display_access_acquire(7,&exclusion));opened();
        assert(!scanout_transfer_check(&transfer,&stale) && !scanout_transfer_close(&transfer,&stale));
        assert(!scanout_transfer_original(&transfer,&stale,8704,&value));close_clean();
    }
    puts("PASS full-vector transfer lifetime");return 0;
}
