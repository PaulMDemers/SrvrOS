#include <srvros/scanout_transfer.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_bdw_scanout_mapping.h>
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
    bool late_restore_change;
} model;
static struct scanout_transfer_io provider(void);
static uint64_t now(void *ctx) {
    assert(ctx==&model);
    if(model.clock_fail)return 0;
    if(model.regress)return 1;
    if(model.post_open_clock && transfer.space.state==SPACE_ACTIVE)return 0;
    if(model.step_deadline)model.time+=SCANOUT_TRANSFER_DEADLINE_US;
    if(model.late_restore_change && a.publication.state==PUB_RESTORING) {
        model.late_restore_change=false;
        a.publication.before[0]^=1ull<<36;
    }
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
    assert(ctx==&model && index>=model.first && index<model.first+model.pages);model.reads++;*out=table[index];return true;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t value) {
    assert(ctx==&model && index>=model.first && index<model.first+model.pages);table[index]=value;
    return ++model.writes!=model.write_fail;
}
static bool flush(void *ctx) {assert(ctx==&model);model.flushes++;return true;}
static bool readers(void *ctx) {assert(ctx==&model);return !model.pending;}
static struct scanout_memory_binding binding(void) {
    return (struct scanout_memory_binding){&model,encode,{&model,guard,read_entry,write_entry,flush,readers,now}};
}
static bool prepare_memory(struct scanout_memory *m,struct scanout_memory_token *out,uint32_t tail_pages) {
    struct scanout_memory_layout layout;
    assert(model.memory_pages==1?scanout_memory_layout(1,1,64,false,&layout):
        scanout_memory_layout(1440,900,6144,model.memory_pages!=1478,&layout));
    assert(scanout_memory_create_with_tail(m,&memories,7,&layout,tail_pages,0x100000000ull,
        (struct scanout_memory_io){&model,allocate,backing,release},out));
    if(tail_pages) {
        struct scanout_publish_io io=binding().publication;
        return intel_bdw_scanout_prepare(m,out,&transfer,&token,&publications,io);
    }
    return scanout_memory_prepare(m,out,&transfer.space,&publications,tail_pages?64:1,binding());
}
/* Model visible pixels through X tiles with no address swizzle in this
 * fixture. CRC-32 distinguishes its images; the hardware tap has its own
 * polynomial. The expected checksum never consults a page-table entry. */
static uint32_t image[1484][1024], crc_table[256];

static uint32_t image_crc(uint32_t base, bool reference)
{
    uint32_t crc = UINT32_MAX;
    for (unsigned y=0; y<900; ++y) {
        for (unsigned x=0; x<1440; ++x) {
            unsigned tile = (y/8)*12 + x/128;
            unsigned word = (y%8)*128 + x%128;
            uint32_t pixel = tile*1024 + word;
            if (!reference) {
                uint64_t pte = table[base/4096 + tile];
                assert((pte & 4095) == 1 && pte >= a.physical);
                uint64_t page = (pte - a.physical) / 4096;
                assert(page < a.layout.pages);
                pixel = image[page][word];
            }
            for (unsigned byte=0; byte<4; ++byte) {
                crc = crc_table[(crc ^ pixel) & 255] ^ (crc >> 8);
                pixel >>= 8;
            }
        }
    }
    return ~crc;
}
static uint32_t scan_crc(uint32_t base)
{
    return base ? image_crc(base,false) : 0x12344321;
}
static bool display_mapping_held(void)
{
    return a.state==MEM_GPU && !a.closing && a.publication.state==PUB_PUBLISHED &&
           model.pending && transfer.state==TRANSFER_ACTIVE && guard(&model);
}
bool test_flip_on_published_surface(const struct scanout_memory *,bool (*)(void),
                                    uint32_t (*)(uint32_t),uint32_t);

static void mapped_flip(bool corrupt)
{
    struct scanout_memory_layout layout;
    assert(scanout_memory_layout(1440,900,6144,true,&layout));
    assert(scanout_memory_create_with_tail(&a,&memories,7,&layout,128,0x100000000ull,
           (struct scanout_memory_io){&model,allocate,backing,release},&at));
    struct scanout_memory_cpu cpu;
    assert(scanout_memory_cpu_begin(&a,&at,&cpu) && cpu.bytes==sizeof(image));
    for (unsigned page=0;page<1484;++page)
        for (unsigned word=0;word<1024;++word)
            image[page][word]=page*1024+word;
    if (corrupt)image[0][0]^=1;
    assert(scanout_memory_cpu_end(&a,&cpu));

    struct scanout_publish_io wrong=binding().publication;
    wrong.ctx=&table;
    unsigned checks=model.calls[TRANSFER_LIVE];
    assert(!intel_bdw_scanout_prepare(&a,&at,&transfer,&token,&publications,wrong));
    struct scanout_transfer_token stale=token;
    ++stale.epoch;
    assert(!intel_bdw_scanout_prepare(&a,&at,&transfer,&stale,&publications,binding().publication));
    assert(!a.range.object && !model.reads && !model.writes && checks==model.calls[TRANSFER_LIVE]);
    for (unsigned i=0;i<256;++i) {
        uint32_t crc=i;
        for (unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);
        crc_table[i]=crc;
    }
    assert(intel_bdw_scanout_prepare(&a,&at,&transfer,&token,&publications,binding().publication));
    assert(scanout_memory_apply(&a,&at));
    model.pending=true;
    assert(test_flip_on_published_surface(&a,display_mapping_held,scan_crc,image_crc(0,true)) == !corrupt);
    assert(!scanout_memory_close(&a,&at) && !model.releases && model.writes==1484);
    model.pending=false;
    assert(scanout_memory_close(&a,&at) && model.releases==1 && model.writes==2968);
    for(unsigned i=0;i<manifest.pages;++i)assert(table[model.first+i]==input[i]);
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
    bool linear_tail=!strcmp(mode,"linear-tail");
    bool tiled_tail=!strcmp(mode,"tiled-tail") || !strcmp(mode,"tiled-tail-partial") ||
                    !strcmp(mode,"tiled-tail-no-owner") || !strcmp(mode,"tiled-tail-invalid-original") || !strcmp(mode,"tiled-tail-high-ram");
    bool flip=!strcmp(mode,"tiled-flip") || !strcmp(mode,"tiled-flip-wrong-pixels");
    tiled_tail |= flip;
    bool full_tail=linear_tail || tiled_tail;
    bool tail_changed=!strcmp(mode,"linear-tail-original-change");
    bool before_changed=!strcmp(mode,"one-page-before-change");
    bool apply_changed=!strcmp(mode,"one-page-apply-change");
    bool restore_changed=!strcmp(mode,"one-page-restore-snapshot");
    bool late_restore_changed=!strcmp(mode,"one-page-restore-clock");
    if(full_tail || tail_changed) {
        model.memory_pages=tiled_tail?1484:1478;
        model.pages=manifest.pages=1536+model.memory_pages;
        if(!strcmp(mode,"tiled-tail-invalid-original"))
            input[1483]=table[model.first+1483]=UINT64_MAX;
    }
    if(!strcmp(mode,"other-pin")){assert(display_access_pin(&exclusion));model.other_pin=true;}
    if(!strcmp(mode,"one-page") || before_changed || apply_changed || restore_changed || late_restore_changed)
        model.memory_pages=model.pages=manifest.pages=1;
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
    bool admission_failure=!strcmp(mode,"failed-open") || !strcmp(mode,"clock-open") || !strcmp(mode,"stuck-open") ||
        !strcmp(mode,"deadline-open") || !strcmp(mode,"post-open-clock") || !strcmp(mode,"source-open") ||
        !strcmp(mode,"changed-first") || !strcmp(mode,"changed-last") || !strcmp(mode,"pool-exhausted");
    if(admission_failure) {
        /* Require refusal, not merely a plausible unwind IF it happened.
         * A provider or deadline bypass must not fall into the happy path. */
        assert(!ok);
        assert(transfer.state==TRANSFER_CLOSED && !transfer.pinned && !token.object);
        assert(display_access_release(&exclusion,true));puts("PASS read-only admission unwind");return 0;
    }
    assert(ok && transfer.state==TRANSFER_ACTIVE && transfer.pinned && model.calls[TRANSFER_ADMIT]==1);
    uint64_t value=0;assert(scanout_transfer_original(&transfer,&token,model.first+model.pages-1,&value) && value==table[model.first+model.pages-1]);
    assert(!scanout_transfer_original(&transfer,&token,model.first+model.pages,&value));
    assert(!scanout_transfer_original(&transfer,&token,model.first-1,&value));
    assert(!scanout_transfer_original(&transfer,&token,8704,&transfer.original[0]));
    copy=transfer;struct scanout_transfer_token rebound=token;rebound.object=&copy;
    copy.manifest.original=copy.original;copy.space.object=&copy.space;
    unsigned before_copy=model.calls[TRANSFER_LIVE]+model.calls[TRANSFER_RETIRE];
    assert(!scanout_transfer_check(&copy,&rebound) && !scanout_transfer_close(&copy,&rebound));
    assert(model.calls[TRANSFER_LIVE]+model.calls[TRANSFER_RETIRE]==before_copy);
    assert(!scanout_transfer_original(&copy,&rebound,8704,&value));
    assert(!scanout_transfer_open(&copy,&pool,&spaces,&exclusion,&manifest,provider(),&rebound));
    assert(!display_access_release(&transfer.exclusion,true));
    if(before_changed || apply_changed || tail_changed) {
        uint32_t changed=tail_changed?model.first+model.pages-1:model.first;
        if(apply_changed || tail_changed)assert(prepare_memory(&a,&at,tail_changed?128:0));
        table[changed]^=1ull<<36;
        uint64_t foreign=table[changed];
        if(before_changed)assert(!prepare_memory(&a,&at,0));
        if(apply_changed)assert(!scanout_memory_apply(&a,&at));
        if(tail_changed) {
            assert(!prepare_memory(&b,&bt,128));
            assert(b.publication.error==PUB_CHANGED && b.publication.reads==1478);
            assert(!scanout_memory_apply(&a,&at) && scanout_memory_close(&b,&bt));
        } else assert(a.publication.error==PUB_CHANGED);
        assert(transfer.space.state==SPACE_FAULT && !model.writes && !model.flushes);
        assert(scanout_memory_close(&a,&at));
        assert(!scanout_transfer_close(&transfer,&token) && transfer.pinned && transfer.state==TRANSFER_FAULT);
        assert(!model.calls[TRANSFER_RETIRE] && table[changed]==foreign);
        assert(!display_access_release(&transfer.exclusion,true));
        puts("PASS foreign PTE faults the range before exposure");return 0;
    }
    if(restore_changed || late_restore_changed) {
        assert(prepare_memory(&a,&at,0) && scanout_memory_apply(&a,&at));
        if(restore_changed)a.publication.before[0]^=1ull<<36;
        else model.late_restore_change=true;
        assert(!scanout_memory_close(&a,&at) && a.publication.error==PUB_CHANGED);
        assert(!model.late_restore_change);
        assert(model.writes==1 && model.flushes==1 && !model.releases && table[model.first]==(a.physical|1));
        assert(!scanout_transfer_close(&transfer,&token) && transfer.pinned && a.state==MEM_FAULT);
        assert(!display_access_release(&transfer.exclusion,true));
        puts("PASS changed restoration snapshot retains exposed backing");return 0;
    }
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
        model.fail=false;model.clock_fail=false;
        assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&transfer.exclusion,true));
        puts("PASS failed retirement keeps exclusion");return 0;
    }
    if(flip) {
        mapped_flip(!strcmp(mode,"tiled-flip-wrong-pixels"));close_clean();
        puts("PASS mapped surface flip, pixel CRC, rollback and reader retirement");return 0;
    }
    if(!strcmp(mode,"one-page")) {
        assert(prepare_memory(&a,&at,0));assert(a.range.first==8704 && a.range.pages==1);
        assert(scanout_memory_apply(&a,&at));assert(table[8704]==(a.physical|1));
        assert(!scanout_transfer_close(&transfer,&token) && !model.calls[TRANSFER_RETIRE]);
        assert(scanout_memory_close(&a,&at) && model.releases==1 && table[8704]==input[0]);
        assert(model.writes==2 && model.flushes==2);
    }
    if(!strcmp(mode,"tiled-tail-high-ram")) {
        struct scanout_memory_layout layout;
        model.physical=0x100000000ull-(1484-1)*4096ull;
        assert(scanout_memory_layout(1440,900,6144,true,&layout));
        assert(scanout_memory_create_with_tail(&a,&memories,7,&layout,128,UINT64_MAX,
               (struct scanout_memory_io){&model,allocate,backing,release},&at));
        assert(!intel_bdw_scanout_prepare(&a,&at,&transfer,&token,&publications,binding().publication));
        assert(!a.range.object && !model.writes && !model.reads);
        assert(scanout_memory_close(&a,&at));
        close_clean();puts("PASS last physical page above 4 GiB rejected before reservation");return 0;
    }
    if(!strcmp(mode,"tiled-tail-invalid-original") || !strcmp(mode,"tiled-tail-no-owner")) {
        if(!strcmp(mode,"tiled-tail-no-owner"))++model.source_epoch;
        assert(!prepare_memory(&a,&at,128));
        assert(!model.writes && !model.reads && !model.flushes && !a.publication.exposed);
        assert(scanout_memory_close(&a,&at) && model.releases==1);
        if(model.source_epoch==1)close_clean();
        else assert(transfer.state==TRANSFER_FAULT && transfer.pinned && !a.range.object);
        puts("PASS unavailable scanout mapping refuses before publication");return 0;
    }
    if(full_tail || !strcmp(mode,"full-buffers") || !strcmp(mode,"partial-write") || !strcmp(mode,"pending-readers") ||
       !strcmp(mode,"partial-restore") || !strcmp(mode,"lost-mapped-owner")) {
        uint32_t tail=full_tail?128:0;
        assert(prepare_memory(&a,&at,tail) && prepare_memory(&b,&bt,tail));
        assert(a.range.first==8704 && b.range.first==(full_tail?10240:10060));
        if(!strcmp(mode,"partial-write"))model.write_fail=1356;
        if(!strcmp(mode,"tiled-tail-partial"))model.write_fail=model.memory_pages;
        bool published=scanout_memory_apply(&a,&at);
        if(!strcmp(mode,"partial-write") || !strcmp(mode,"tiled-tail-partial")) {
            assert(!published && a.state==MEM_FAULT && a.publication.exposed);
            assert(!scanout_transfer_close(&transfer,&token) && transfer.pinned);
            assert(!scanout_memory_close(&a,&at) && !model.releases);
            assert(!display_access_release(&transfer.exclusion,true));puts("PASS uncertain mapping retains RAM/range/exclusion");return 0;
        }
        assert(published && scanout_memory_apply(&b,&bt));
        if(full_tail) {
            assert(a.range.pages==model.memory_pages && b.range.pages==model.memory_pages);
            assert(b.physical==a.physical+model.memory_pages*4096ull);
            assert(a.layout.tiled==tiled_tail && a.layout.rows==(tiled_tail?904u:900u));
            for(unsigned i=0;i<model.memory_pages;i++) {
                assert(table[a.range.first+i]==(a.physical+(uint64_t)i*4096+1));
                assert(table[b.range.first+i]==(b.physical+(uint64_t)i*4096+1));
            }
            for(uint32_t i=a.range.first+a.range.pages;i<b.range.first;i++)
                assert(table[i]==input[i-model.first]);
        }
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
        assert(scanout_transfer_original(&transfer,&token,model.first+model.pages-1,&value));
        struct scanout_space_token third;assert(!scanout_space_reserve(&transfer.space,7,transfer.arena_epoch,1,1,&third));
        if(full_tail || !strcmp(mode,"pending-readers")) {
            model.pending=true;unsigned writes=model.writes;
            assert(!scanout_memory_close(&a,&at) && model.writes==writes && !model.releases);
            assert(!scanout_transfer_close(&transfer,&token) && model.calls[TRANSFER_RETIRE]==calls);
            model.pending=false;
        }
        assert(scanout_memory_close(&a,&at) && model.releases==1);
        assert(!scanout_transfer_close(&transfer,&token) && model.calls[TRANSFER_RETIRE]==calls);
        assert(scanout_memory_close(&b,&bt) && model.releases==2 && model.flushes==4);
        for(unsigned i=0;i<model.pages;i++)assert(table[model.first+i]==transfer.original[i]);
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
