#include <srvros/scanout_transfer.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/intel_ggtt_invalidate.h>
#include <srvros/vmm.h>
#include <srvros/x86_cache.h>
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
static uint64_t input[SCANOUT_TRANSFER_PAGES];
static _Alignas(4096) uint64_t table[SCANOUT_SPACE_PAGES];
static const char *page_failure = "";
static struct vmm_reserved_guard cpu_guard;
bool vmm_reserved_guard_io_page_held(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page) {
    return g==&cpu_guard && root==4096 && page==manifest.aperture+8704ull*4096 &&
           strcmp(page_failure,"aperture");
}
static const void *borrower;
bool vmm_reserved_guard_pin_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
    if (!child || borrower || !vmm_reserved_guard_io_page_held(g,root,page)) return false;
    borrower=child;
    return true;
}
bool vmm_reserved_guard_io_page_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
    return child && borrower==child && vmm_reserved_guard_io_page_held(g,root,page);
}
bool vmm_reserved_guard_pin_register(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,
 uint64_t reg,uint64_t alias,const void *child) {
    return reg==manifest.mmio+0x811000 && alias==(uintptr_t)&table[8704] &&
           strcmp(page_failure,"register-alias") && vmm_reserved_guard_pin_io_page(g,root,page,child);
}
bool vmm_reserved_guard_register_pinned(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,
 uint64_t reg,uint64_t alias,const void *child) {
    return reg==manifest.mmio+0x811000 && alias==(uintptr_t)&table[8704] &&
           strcmp(page_failure,"register-alias") && vmm_reserved_guard_io_page_pinned(g,root,page,child);
}
bool vmm_reserved_guard_unpin_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,const void *child) {
    if (!vmm_reserved_guard_io_page_pinned(g,root,page,child)) return false;
    borrower=NULL;
    return true;
}
static unsigned store_alias_walks;
static uint64_t cached_translation;
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
/* Separate pool and object, as used by an independent kernel client. */
static struct scanout_transfer contender;
static struct scanout_transfer_pool contender_pool;
static struct scanout_handoff small;
static struct scanout_space_pool small_pool;
static unsigned small_checks;
static bool check_small(void *ctx,const struct scanout_handoff_snapshot *s) {
    assert(ctx==&small && s->original[0]==table[s->first]);
    ++small_checks;
    return true;
}
static uint32_t open_small(unsigned first) {
    struct scanout_handoff_snapshot s={.mmio=manifest.mmio,.aperture=manifest.aperture,
        .aperture_bytes=manifest.aperture_bytes,.fence_end=manifest.fence_end,.first=first,
        .pages=1,.assurances=HANDOFF_ALL,.original={table[first]}};
    return scanout_handoff_open(&small,&small_pool,&exclusion,&s,(struct scanout_handoff_io){&small,check_small});
}
static void expect_small_conflict(void) {
    unsigned before=small_checks;
    assert(open_small(manifest.first+manifest.pages-1)==HANDOFF_SPACE && small_checks==before);
}

static void expect_conflict(struct scanout_transfer_manifest request)
{
    struct scanout_transfer_token untouched={0};
    uint64_t epoch=contender_pool.epoch;
    assert(!scanout_transfer_reserve(&contender,&contender_pool,&exclusion,&request,&untouched));
    assert(!scanout_transfer_open(&contender,&contender_pool,&spaces,&exclusion,&request,provider(),&untouched));
    assert(!untouched.object && contender_pool.epoch==epoch);
}

static struct scanout_interval firmware;
static bool reserve_then_activate(bool imported)
{
    struct scanout_transfer_manifest observed=manifest;
    observed.assurances=0;observed.obligations=255;
    if(imported) {
        struct scanout_space_range span={8704,3072};
        assert(scanout_interval_import_firmware(&firmware,manifest.mmio,manifest.aperture,
                                                manifest.aperture_bytes,manifest.fence_end,span));
        struct scanout_interval copy=firmware,child={0};
        assert(!scanout_interval_borrow(&child,&copy,manifest.mmio,manifest.aperture,manifest.aperture_bytes,
                                        manifest.fence_end,span));
        assert(!scanout_interval_borrow(&child,&firmware,manifest.mmio,manifest.aperture,manifest.aperture_bytes,
                                        manifest.fence_end,(struct scanout_space_range){11775,2}));
        scanout_interval_release(&firmware);
        expect_conflict(manifest);
        assert(scanout_transfer_reserve_imported(&transfer,&pool,&exclusion,&observed,&firmware,&token));
        assert(!scanout_interval_borrow(&child,&firmware,manifest.mmio,manifest.aperture,manifest.aperture_bytes,
                                        manifest.fence_end,(struct scanout_space_range){8704,1}));
    } else assert(scanout_transfer_reserve(&transfer,&pool,&exclusion,&observed,&token));
    assert(transfer.state==TRANSFER_RESERVED && transfer.pinned && !transfer.arena_epoch);
    expect_conflict(manifest);
    expect_small_conflict();
    assert(!scanout_transfer_activate(&transfer,&token,&spaces,&observed,provider()));
    assert(!model.calls[TRANSFER_ADMIT] && !transfer.space.object && !display_access_release(&exclusion,true));
    return scanout_transfer_activate(&transfer,&token,&spaces,&manifest,provider());
}

static void reservation_cases(const char *mode)
{
    assert(!open_small(manifest.first+manifest.pages-1));
    expect_conflict(manifest);
    assert(scanout_handoff_close(&small,(struct scanout_handoff_io){&small,check_small}));
    assert(scanout_transfer_reserve(&transfer,&pool,&exclusion,&manifest,&token));
    assert(scanout_transfer_reservation_matches(&transfer,&token,&manifest));
    if(!strcmp(mode,"reservation-removed")) {
        scanout_interval_release(&transfer.interval);
        assert(!scanout_transfer_reservation_matches(&transfer,&token,&manifest));
        assert(!scanout_transfer_activate(&transfer,&token,&spaces,&manifest,provider()));
        assert(transfer.pinned && transfer.state==TRANSFER_FAULT && !display_access_release(&exclusion,true));
        return;
    }
    struct scanout_transfer_manifest other=manifest;
    other.pages=1;other.first+=manifest.pages-1;
    expect_conflict(other);
    other.mmio+=4096;expect_conflict(other);
    other.aperture+=4096;expect_conflict(other);
    other=manifest;other.aperture+=4096;expect_conflict(other);
    struct scanout_transfer_token stale=token;
    ++stale.epoch;
    uint64_t raw;
    assert(!scanout_transfer_original(&transfer,&token,manifest.first,&raw));
    assert(!scanout_transfer_check(&transfer,&token) && !scanout_transfer_close(&transfer,&stale));
    assert(!scanout_transfer_reservation_matches(&transfer,&stale,&manifest));
    assert(!scanout_transfer_activate(&transfer,&stale,&spaces,&manifest,provider()));
    assert(!scanout_transfer_activate(&transfer,&token,(struct scanout_space_pool *)(void *)&pool,
                                      &manifest,provider()));
    other=manifest;++other.source_epoch;
    assert(!scanout_transfer_activate(&transfer,&token,&spaces,&other,provider()));
    other=manifest;other.first+=other.pages;
    struct scanout_transfer_token adjacent;
    assert(scanout_transfer_reserve(&contender,&contender_pool,&exclusion,&other,&adjacent));
    assert(scanout_transfer_close(&contender,&adjacent));
    if(!strcmp(mode,"reservation-promote-clock") || !strcmp(mode,"reservation-promote-provider")) {
        model.clock_fail=!strcmp(mode,"reservation-promote-clock");
        model.fail=!model.clock_fail;
        assert(!scanout_transfer_activate(&transfer,&token,&spaces,&manifest,provider()));
        assert(transfer.state==TRANSFER_FAULT && transfer.pinned && !model.writes);
        expect_conflict(manifest);
        expect_small_conflict();
        assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&exclusion,true));
        return;
    }
    if(!strcmp(mode,"reservation-stale-original")) {
        input[manifest.pages-1]^=1;
        assert(!scanout_transfer_reservation_matches(&transfer,&token,&manifest));
        assert(!scanout_transfer_activate(&transfer,&token,&spaces,&manifest,provider()));
        assert(transfer.state==TRANSFER_RESERVED && transfer.pinned && !model.calls[TRANSFER_ADMIT]);
        expect_conflict(manifest);
        assert(scanout_transfer_close(&transfer,&token));
    } else {
        assert(scanout_transfer_activate(&transfer,&token,&spaces,&manifest,provider()));
        expect_conflict(manifest);
        expect_small_conflict();
        model.fail=!strcmp(mode,"reservation-fault");
        if(model.fail) {
            assert(!scanout_transfer_close(&transfer,&token));
            expect_conflict(manifest);
            expect_small_conflict();
            assert(transfer.state==TRANSFER_FAULT && !display_access_release(&exclusion,true));
            return;
        }
        assert(scanout_transfer_close(&transfer,&token));
    }
    assert(scanout_transfer_reserve(&contender,&contender_pool,&exclusion,&manifest,&adjacent));
    assert(scanout_transfer_close(&contender,&adjacent) && display_access_release(&exclusion,true));
}
static void boot_framebuffer_case(void)
{
    uint64_t mmio=manifest.mmio, base=manifest.aperture, bytes=manifest.aperture_bytes;
    uint64_t fence=manifest.fence_end;
    struct scanout_transfer_manifest short_fence=manifest;
    short_fence.fence_end-=4096;
    short_fence.first--;
    short_fence.pages=1;
    /* Pre-existing inconsistent owners prevent registration, without eviction. */
    struct scanout_transfer_token old;
    assert(scanout_transfer_reserve(&contender,&contender_pool,&exclusion,&short_fence,&old));
    assert(!scanout_interval_protect_boot_framebuffer(mmio,base,bytes,fence));
    assert(scanout_transfer_close(&contender,&old));
    assert(scanout_interval_protect_boot_framebuffer(mmio,base,bytes,fence));
    assert(!scanout_interval_protect_boot_framebuffer(mmio,base,bytes,fence-4096));
    assert(!scanout_interval_protect_boot_framebuffer(mmio,base+4096,bytes,fence));

    for(unsigned cycle=0;cycle<2;cycle++) {
        assert(scanout_interval_protect_boot_framebuffer(mmio,base,bytes,fence));
        expect_conflict(short_fence);
        manifest.fence_end=short_fence.fence_end;
        unsigned before=small_checks;
        assert(open_small(short_fence.first)==HANDOFF_SPACE && before==small_checks);
        manifest.fence_end=fence;
        assert(scanout_transfer_reserve(&transfer,&pool,&exclusion,&manifest,&token));
        assert(scanout_transfer_close(&transfer,&token));
        assert(!open_small(manifest.first));
        assert(scanout_handoff_close(&small,(struct scanout_handoff_io){&small,check_small}));
    }
    struct scanout_interval independent;
    assert(scanout_interval_claim(&independent,0xd0000000,0x80000000,bytes,4096,
                                  (struct scanout_space_range){1,1}));
    scanout_interval_release(&independent);
    assert(display_access_release(&exclusion,true));
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
/* The native QWord store runs against a page-aligned register image. Only
 * the machine-specific kernel-root and cache checks are substituted here. */
bool intel_cache_probe_uc_page(uint64_t physical) {
    return physical == manifest.mmio + 0x811000 && strcmp(page_failure,"cache");
}
bool vmm_diagnostic_kernel_alias(uint64_t root,uint64_t va,uint64_t pa,uint32_t pages,unsigned bits,
                                 struct vmm_alias_observation *out,struct vmm_range_observation *unused) {
    assert(!unused && pages==1 && bits==39);
    if(root!=4096 || va!=(uintptr_t)&table[8704] || pa!=manifest.mmio+0x811000)return false;
    *out=(struct vmm_alias_observation){.pages=1,.leaves=1,.pat_index=2,.writable=1,.no_execute=1,.leaf_sizes=1};
    ++store_alias_walks;
    if(!strcmp(page_failure,"alias"))out->user=1;
    if(!strcmp(page_failure,"late-alias") && store_alias_walks==2)out->no_execute=0;
    if(!strcmp(page_failure,"foreign") && store_alias_walks==1)table[8704]^=1ull<<36;
    if(!strcmp(page_failure,"deadline") ||
       (!strcmp(page_failure,"late-deadline") && store_alias_walks==2))
        model.time+=SCANOUT_PUBLISH_DEADLINE_US+1;
    return true;
}
static unsigned traffic_reads,block_requests,block_releases,l3_reads,request_reads;
static bool block_revoked,block_active,retire_reentered;
static uint64_t held_block(const struct scanout_transfer *t,const struct scanout_publication *p) {
    assert(t==&transfer && p==&a.publication);
    if (p->state==PUB_RETIRED && !strcmp(page_failure,"reenter") && !retire_reentered) {
        retire_reentered=true;
        assert(!intel_bdw_scanout_retire_test_page(&a,&at));
    }
    if (!block_active || block_revoked || !strcmp(page_failure,"no-block") ||
        (!strcmp(page_failure,"lost-block") && traffic_reads>=6)) return 0;
    return !strcmp(page_failure,"new-block") ? 2 : 1;
}
static enum intel_gt_write request_block(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t *epoch) {
    assert(t==&transfer && p==&a.publication && borrower && !block_active && !block_requests);
    assert(table[8704]==input[0] && p->state==PUB_WRITING);
    if (!strcmp(page_failure,"frozen-request")) model.stuck=true;
    ++block_requests;
    block_active=true;
    *epoch=1;
    return !strcmp(page_failure,"uncertain-request") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}
static enum intel_gt_write release_block(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t epoch) {
    assert(t==&transfer && p==&a.publication && borrower && block_active && !block_releases);
    assert(epoch==1 && p->state==PUB_RETIRED && table[8704]==input[0] && cached_translation==input[0]);
    if (!strcmp(page_failure,"frozen-release")) model.stuck=true;
    ++block_releases;
    block_active=!strcmp(page_failure,"still-held");
    return !strcmp(page_failure,"uncertain-release") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}
/* Keep the System Agent request distinct from PTE readback and the
 * publisher's reader check. Each store must do read -> request -> read. */
static unsigned ggtt_phase, ggtt_posts;
static enum intel_gt_write invalidate_ggtt(const struct scanout_transfer *t,
        const struct scanout_publication *p, uint64_t epoch, uint32_t offset, uint32_t value) {
    assert(t==&transfer && p==&a.publication && offset==0x101008 && value==1);
    assert(epoch==1 && block_active && borrower && ggtt_phase==1);
    assert(table[8704]==(p->state==PUB_RESTORING?p->before[0]:p->after[0]));
    assert(cached_translation!=table[8704]);
    ggtt_phase=2;
    if (!strcmp(page_failure,"sa-no-write")) return GT_WRITE_NONE;
    if (!strcmp(page_failure,"sa-uncertain")) return GT_WRITE_UNCERTAIN;
    if (!strcmp(page_failure,"sa-lost-hold")) block_revoked=true;
    if (!strcmp(page_failure,"sa-expired")) model.time+=SCANOUT_PUBLISH_DEADLINE_US+1;
    return GT_WRITE_DONE;
}

static uint32_t selector = 0x43001122u;
static enum intel_gt_write select_l3(const struct scanout_transfer *t,uint32_t from,uint32_t to) {
    assert(t==&transfer && borrower && selector==from && ((from^to)&~(15u<<24))==0);
    if (!strcmp(page_failure,"l3-steering")) return GT_WRITE_NONE;
    selector=to;
    if (!strcmp(page_failure,"l3-unposted")) selector=from;
    return GT_WRITE_DONE;
}
static bool traffic_word(const struct scanout_transfer *t,uint32_t offset,uint32_t *word) {
    assert(t==&transfer);
    ++traffic_reads;
    if (block_releases && !strcmp(page_failure,"read-failed")) return false;
    if (offset==0x8038 && !strcmp(page_failure,"unfilled-l3")) return true;
    switch(offset) {
    case INTEL_GGTT_INVALIDATE_REGISTER:
        assert(block_active && borrower);
        if (ggtt_phase==0) {
            assert(cached_translation==table[8704]);
            ggtt_phase=1;
            *word=!strcmp(page_failure,"sa-preflight");
        } else {
            assert(ggtt_phase==2 && cached_translation!=table[8704]);
            if (!strcmp(page_failure,"sa-read-error")) return false;
            *word=!strcmp(page_failure,"sa-bad-post");
            if (!*word) {
                ggtt_phase=0;
                ++ggtt_posts;
                if (strcmp(page_failure,"invalidate")) cached_translation=table[8704];
            }
        }
        break;
    case 0x8014:
        *word = block_active ? 0 : 0x10;
        if (!strcmp(page_failure,"pre-store-widi") && ggtt_phase==1) *word=0x10;
        if (!strcmp(page_failure,"sa-store-widi") && ggtt_phase==1 &&
            table[8704]!=cached_translation) *word=0x10;
        if (block_active && !strcmp(page_failure,"sa-post-widi") &&
            ggtt_posts==a.publication.writes) *word=0x10;
        if (block_active && !strcmp(page_failure,"widi-open")) *word = 0x10;
        if (block_active && (!strcmp(page_failure,"widi-idle") ||
            (!strcmp(page_failure,"widi-early-ack") && l3_reads==1))) *word = 1;
        if (block_active && !strcmp(page_failure,"widi-reopened") && l3_reads==2) *word = 0x10;
        if (block_releases && !strcmp(page_failure,"widi-stuck")) *word = 0;
        break;
    case 0xfdc: *word=selector; break;
    case 0x138064:
        *word=!strcmp(page_failure,"l3-off") || (!strcmp(page_failure,"l3-power-lost") &&
            ((selector>>24)&15)==5) ? 1u : 3u;
        break;
    case 0xb11c: {
        unsigned slice=(selector>>26)&3u, bank=(selector>>24)&3u;
        char narrow[32], missing[32];
        snprintf(narrow,sizeof(narrow),"l3-narrow-%u-%u",slice,bank);
        snprintf(missing,sizeof(missing),"l3-missing-%u-%u",slice,bank);
        if (!strcmp(page_failure,missing)) return true;
        *word = !strcmp(page_failure,narrow) || (block_active && slice==1 && bank==3 &&
            !strcmp(page_failure,"changed-policy")) ? 16u : 1u;
        break;
    }
    case 0x9120: *word=(!strcmp(page_failure,"slice-mask")?1u:3u)<<25; break;
    case 0x70180:
        *word=!strcmp(page_failure,"plane-off")?0:1u<<31; break;
    case 0x7019c: case 0x701ac:
        *word=0x00100000;
        if (!strcmp(page_failure,"sa-post-plane") && ggtt_posts==a.publication.writes) *word+=4096;
        if (block_active && !strcmp(page_failure,"plane-moved")) *word+=4096;
        if (offset==0x7019c && !strcmp(page_failure,"queued-flip")) *word+=4096;
        if (block_releases && !strcmp(page_failure,"display-changed")) *word+=4096;
        break;
    case 0x12870: case 0x12878: case 0x1c870: case 0x1c878:
        *word=!strcmp(page_failure,"media-busy") ? 0x10000 : 0; break;
    case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010: {
        /* The last engine closes only after L3's one-shot ACK disappears. */
        *word=(!block_active || !strcmp(page_failure,"engine-go") ||
               (offset==0x8010 && l3_reads==1)) ? 0x10 : 0x20;
        char early_ack[32];
        snprintf(early_ack,sizeof(early_ack),"engine-early-ack-%u",(offset-0x8000)/4);
        if (block_active && l3_reads==1 && !strcmp(page_failure,early_ack)) *word=0x21;
        if (!strcmp(page_failure,"sa-post-engine") && ggtt_posts==a.publication.writes) *word=0x10;
        if (block_releases && offset==0x8010 && !strcmp(page_failure,"engine-stuck")) *word=0x20;
        break;
    }
    case 0x8040: *word=strcmp(page_failure,"gam-busy")!=0; break;
    case 0x8038:
        if(!block_active){*word=!strcmp(page_failure,"stale-l3");break;}
        ++l3_reads;
        *word=!strcmp(page_failure,"l3-unblock")?2:(l3_reads==1 && strcmp(page_failure,"missing-l3"));
        if (!strcmp(page_failure,"sa-post-l3") && ggtt_posts==a.publication.writes) *word=2;
        break;
    case 0x40d0: *word=(!block_active && !strcmp(page_failure,"air-post-reset")) ? 0x7fff :
        ((!block_active || !strcmp(page_failure,"gam-request")) ? 0x5f : 0);
        if (block_active && !strcmp(page_failure,"engine-reset-reason")) *word |= 1u << 11;
        break;
    case 0x8028: *word=(!block_active || !strcmp(page_failure,"gam-response")) ? 0x5f : 0; break;
    default: assert(0); return false;
    }
    if (block_active && offset==0x40d0) ++request_reads;
    if (block_active && request_reads==1) {
        if (!strcmp(page_failure,"l3-before-request")) {
            if (offset==0x40d0 || offset==0x8028) *word=0x5f;
            if (offset>=0x8000 && offset<=0x8014) *word=0x10;
        }
        static const struct { const char *mode; uint32_t bit, message; } early[]={
            {"rcs-before-request",0,0x8000}, {"vcs0-before-request",1,0x8004},
            {"vcs1-before-request",2,0x8008}, {"bcs-before-request",4,0x800c},
            {"vecs-before-request",3,0x8010}, {"widi-before-request",6,0x8014}
        };
        for (unsigned i=0;i<sizeof(early)/sizeof(early[0]);++i) {
            if (strcmp(page_failure,early[i].mode)) continue;
            if (offset==0x40d0 || offset==0x8028) *word |= 1u<<early[i].bit;
            if (offset==early[i].message) *word=offset==0x8014 ? 0 : 0x20;
        }
    }
    return true;
}
static const struct intel_bdw_scanout_traffic traffic_io={
    .held_epoch=held_block,.read=traffic_word,.select_l3=select_l3,
    .request=request_block,.release=release_block,.invalidate_ggtt=invalidate_ggtt
};
static bool write_test_page(void *ctx,uint32_t index,uint64_t value) {
    assert(ctx==&model);
    store_alias_walks=0;
    traffic_reads=0;
    bool ok=intel_bdw_scanout_store_test_page(&a,&at,&transfer,&token,traffic_io,&cpu_guard,4096,
                                             (uintptr_t)&table[8704],index,value);
    if(ok)++model.writes;
    return ok;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t value) {
    assert(ctx==&model && index>=model.first && index<model.first+model.pages);table[index]=value;
    return ++model.writes!=model.write_fail;
}
static bool flush(void *ctx) {assert(ctx==&model);model.flushes++;return true;}
static bool flush_test_page(void *ctx) {
    assert(ctx==&model);
    const struct scanout_publication *p=&a.publication;
    bool restore=p->state==PUB_RESTORING;
    uint64_t expected=restore?p->before[0]:p->after[0];
    assert(table[8704]==expected && ggtt_phase==0 && ggtt_posts==(restore?2u:1u));
    ++model.flushes;
    return cached_translation==expected;
}
static bool readers(void *ctx) {
    assert(ctx==&model);
    if(!strcmp(page_failure,"retirement-block") && a.publication.state==PUB_RESTORING)
        block_revoked=true;
    return !model.pending;
}
static struct scanout_memory_binding binding(void) {
    return (struct scanout_memory_binding){&model,encode,{&model,guard,read_entry,write_entry,flush,readers,now}};
}
static bool prepare_memory(struct scanout_memory *m,struct scanout_memory_token *out,uint32_t tail_pages) {
    struct scanout_memory_layout layout;
    assert(model.memory_pages==1?scanout_memory_layout(1,1,64,false,&layout):
        scanout_memory_layout(1440,900,6144,model.memory_pages!=1478,&layout));
    assert(scanout_memory_create_with_tail(m,&memories,7,&layout,tail_pages,0x100000000ull,
        (struct scanout_memory_io){&model,allocate,backing,release},out));
    if(model.memory_pages==1 && manifest.pages==1) {
        struct scanout_publish_io io=binding().publication;
        io.write=write_test_page;
        io.flush=flush_test_page;
        cached_translation=table[8704];
        return intel_bdw_scanout_prepare_test_page(m,out,&transfer,&token,&publications,io);
    }
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
    expect_conflict(manifest);
    expect_small_conflict();
    model.pending=false;
    assert(scanout_memory_close(&a,&at) && model.releases==1 && model.writes==2968);
    expect_conflict(manifest);
    expect_small_conflict();
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
    if(!strcmp(mode,"boot-framebuffer")) {
        boot_framebuffer_case();puts("PASS inherited frame survives child return");return 0;
    }
    if(!strncmp(mode,"reservation-",12)) {
        reservation_cases(mode);puts("PASS competing reservations, promotion and original-owner return");return 0;
    }
    bool linear_tail=!strcmp(mode,"linear-tail");
    bool tiled_tail=!strcmp(mode,"tiled-tail") || !strcmp(mode,"tiled-tail-partial") ||
                    !strcmp(mode,"tiled-tail-no-owner") || !strcmp(mode,"tiled-tail-invalid-original") || !strcmp(mode,"tiled-tail-high-ram");
    bool flip=!strcmp(mode,"tiled-flip") || !strcmp(mode,"tiled-flip-wrong-pixels");
    tiled_tail |= flip;
    bool full_tail=linear_tail || tiled_tail;
    bool tail_changed=!strcmp(mode,"linear-tail-original-change");
    bool imported=!strcmp(mode,"firmware-page-air");
    bool captured_page=imported || !strcmp(mode,"one-page-air");
    bool unblock=!strncmp(mode,"unblock-",8);
    bool single_page=captured_page || unblock || !strcmp(mode,"one-page");
    if(captured_page)input[0]=table[8704]=0x8e201001ull;
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
    bool store_failure=!strncmp(mode,"pte-store-",10);
    bool missing_fence=!strcmp(mode,"one-page-no-boot-fence");
    if(store_failure || single_page || missing_fence || before_changed || apply_changed || restore_changed || late_restore_changed) {
        model.memory_pages=model.pages=manifest.pages=1;
        if(!missing_fence)assert(scanout_interval_protect_boot_framebuffer(manifest.mmio,manifest.aperture,
                                                                          manifest.aperture_bytes,manifest.fence_end));
    }
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
    bool ok=(flip || single_page || store_failure) ? reserve_then_activate(imported) : scanout_transfer_open(&transfer,&pool,&spaces,&exclusion,&manifest,provider(),&token);
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
    if(missing_fence) {
        assert(!prepare_memory(&a,&at,0) && a.state==MEM_RAM && !a.range.object && !model.reads);
        /* Even if the generic publisher dispatches the native callback, that
         * callback needs the independent registration before it touches a PTE. */
        struct scanout_memory_binding generic=binding();
        generic.publication.write=write_test_page;
        assert(scanout_memory_prepare(&a,&at,&transfer.space,&publications,1,generic));
        assert(!scanout_memory_apply(&a,&at) && !store_alias_walks && !model.writes && !model.releases);
        assert(table[8704]==input[0] && a.state==MEM_FAULT);
        puts("PASS missing boot fence prevents native access");return 0;
    }
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
    if(store_failure) {
        assert(prepare_memory(&a,&at,0));
        assert(!intel_bdw_scanout_store_test_page(&a,&at,&transfer,&token,traffic_io,&cpu_guard,4096,(uintptr_t)&table[8704],8704,a.entries[0]));
        bool restoring=!strncmp(mode+10,"restore-",8);
        if(restoring)assert(scanout_memory_apply(&a,&at));
        page_failure=mode+10+(restoring?8:0);
        if (!strcmp(page_failure,"guard-epoch")) ++cpu_guard.epoch;
        uint64_t previous=table[8704];
        assert(restoring ? !scanout_memory_close(&a,&at) : !scanout_memory_apply(&a,&at));
        bool retired_fault=!strcmp(page_failure,"retirement-block");
        bool written=!strcmp(page_failure,"invalidate") || retired_fault;
        bool store_uncertain=!strncmp(page_failure,"sa-",3) && strcmp(page_failure,"sa-preflight");
        if(written || store_uncertain)assert(table[8704]==(restoring?input[0]:a.entries[0]));
        else assert(table[8704]==(previous^(!strcmp(page_failure,"foreign")?(1ull<<36):0)));
        bool posted=!strncmp(page_failure,"sa-post-",8);
        assert(cached_translation==(posted?table[8704]:retired_fault?input[0]:previous));
        assert(a.state==MEM_FAULT && !model.releases && model.writes==(unsigned)restoring+written);
        assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&exclusion,true));
        assert(!intel_bdw_scanout_retire_test_page(&a,&at));
        if (restoring || written || store_uncertain) assert(borrower);
        if (store_uncertain) assert(!block_releases && block_requests==1 && ggtt_posts==(unsigned)restoring+posted);
        if (!strcmp(page_failure,"sa-store-widi")) assert(ggtt_phase==1);
        if (!strcmp(page_failure,"air-post-reset") || !strcmp(page_failure,"stale-l3"))
            assert(!block_requests);
        if (!strcmp(page_failure,"missing-l3") || !strcmp(page_failure,"uncertain-request") ||
            !strcmp(page_failure,"changed-policy") || !strcmp(page_failure,"widi-reopened"))
            assert(block_requests==1 && !block_releases && borrower);
        puts("PASS hardware access refusal retains page");return 0;
    }
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
    if(single_page) {
        assert(prepare_memory(&a,&at,0));assert(a.range.first==8704 && a.range.pages==1);
        uint64_t left=table[8703],right=table[8705];
        assert(scanout_memory_apply(&a,&at));assert(table[8704]==(a.physical|1));
        assert(selector==0x43001122u);
        assert(borrower && !intel_bdw_scanout_retire_test_page(&a,&at));
        assert(cached_translation==table[8704]);
        assert(table[8703]==left && table[8705]==right);
        assert(!scanout_transfer_close(&transfer,&token) && !model.calls[TRANSFER_RETIRE]);
        assert(scanout_memory_close(&a,&at) && model.releases==1 && table[8704]==input[0]);
        assert(!scanout_transfer_close(&transfer,&token) && !block_releases);
        assert(block_requests==1 && l3_reads>=2);
        page_failure=unblock?mode+8:"reenter";
        if (unblock) {
            assert(!intel_bdw_scanout_retire_test_page(&a,&at));
            assert(!intel_bdw_scanout_retire_test_page(&a,&at));
            assert(block_releases==(unsigned)(strncmp(page_failure,"l3-",3)!=0 && strcmp(page_failure,"engine-reset-reason")!=0 &&
                                               strcmp(page_failure,"widi-open")!=0));
            assert(borrower && transfer.device_publication==&a.publication && transfer.pinned);
            assert(!scanout_transfer_close(&transfer,&token) && !display_access_release(&exclusion,true));
            assert(cached_translation==input[0] && table[8704]==input[0]);
            assert(table[8703]==left && table[8705]==right && model.writes==2 && model.flushes==2);
            puts("PASS failed unblock retains the original publication and exclusion");return 0;
        }
        assert(borrower && intel_bdw_scanout_retire_test_page(&a,&at) && !borrower);
        assert(retire_reentered && block_releases==1 && !transfer.device_publication);
        assert(!intel_bdw_scanout_retire_test_page(&a,&at));
        assert(cached_translation==input[0]);
        assert(table[8703]==left && table[8705]==right);
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
    if(imported) {
        assert(scanout_interval_held(&firmware,manifest.mmio,manifest.aperture,manifest.aperture_bytes,
                                     manifest.fence_end,(struct scanout_space_range){8704,3072}));
        assert(!scanout_interval_claim(&transfer.interval,manifest.mmio,manifest.aperture,manifest.aperture_bytes,
                                       manifest.fence_end,(struct scanout_space_range){8704,1}));
    }
    if(!strcmp(mode,"reuse")) {
        model.calls[TRANSFER_ADMIT]=model.calls[TRANSFER_RETIRE]=0;
        assert(display_access_acquire(7,&exclusion));opened();
        assert(!scanout_transfer_check(&transfer,&stale) && !scanout_transfer_close(&transfer,&stale));
        assert(!scanout_transfer_original(&transfer,&stale,8704,&value));close_clean();
    }
    puts("PASS full-vector transfer lifetime");return 0;
}
