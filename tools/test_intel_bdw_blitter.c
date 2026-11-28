#include <srvros/intel_bdw_blitter.h>
#include <srvros/intel_bdw_bcs_port.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum fault { OK, UNPUBLISHED, WRONG_BINDING, LOST_HOLD, NO_COMPLETION, OTHER_CONTEXT,
             PREEMPT, BUSY, WRONG_DATA, SCRIBBLE, EXPIRED, WRONG_MODE, WRONG_HWSP, STALE_CURSOR, DETACH_PENDING, LOST_DURING_INIT };
enum { FIRST=11744, BASE=0x1000000, PAGES=BDW_BLITTER_GLOBAL_PAGES };
static struct {
    _Alignas(4096) uint32_t global[PAGES*1024];
    struct bdw_private_vm_image vm;
} ram;
static _Alignas(4096) uint32_t regs[1024];
static struct scanout_space space;
static struct scanout_publication pub;
static struct scanout_space_pool ranges;
static struct scanout_publish_pool publications;
static struct bdw_blitter_image image;
static struct bdw_blitter_request request;
static struct bdw_blitter_request *active=&request;
static _Alignas(4096) uint64_t ggtt[512];
#define entries (&ggtt[FIRST&511u])
static uint64_t originals[PAGES], clock_us;
static unsigned posted;
/* Count the clocks inside the fourth port-write callback. Replay the same
 * successful path and revoke one lease on its final clock before the store. */
static unsigned trigger_clocks, revoke_at;
static bool revoked;
enum completion_fault { COMPLETION_OK, EARLY_MARKER, EARLY_CURSOR, CLOSING_DEVICE,
                        CLOSING_WIDI, CLOSING_MARKER, CLOSING_CURSOR, CLOSING_WIDI_GO,
                        CLOSING_WIDI_RESET };
static enum completion_fault completion_fault;
static unsigned completion_clocks, closing_fault_at;
static _Alignas(4096) uint32_t invalidate_regs[1024], aperture[PAGES*1024];
static uint64_t translations[PAGES];
enum translation_fault { NORMAL_TRANSLATION, STALE_SEVENTH, SWAPPED, UNPOSTED, STALE_RESTORE };
static enum translation_fault translation_fault;
static unsigned invalidates;
static void model_aperture(void)
{
    if (invalidate_regs[2]==1 && translation_fault!=UNPOSTED) {
        invalidate_regs[2]=0;
        ++invalidates;
        for (unsigned i=0;i<PAGES;i++) {
            if (translation_fault==STALE_SEVENTH && i==6) continue;
            if (translation_fault==STALE_RESTORE && invalidates==2 && i==2) continue;
            unsigned source=(translation_fault==SWAPPED && i<2)?1-i:i;
            translations[i]=entries[source];
        }
    }
    for (unsigned i=0;i<PAGES;i++) {
        uint64_t frame=translations[i]&~4095ull;
        aperture[i*1024+1023]=frame>=BASE && frame<BASE+PAGES*4096 ?
            ram.global[(frame-BASE)/4+1023] : 0;
    }
}
static struct intel_gt_access gt;
static bool lose_rollback_epoch;
static enum fault fault;

static bool yes(void *ctx) {return ctx==&space;}
static bool read_entry(void *ctx,uint32_t index,uint64_t *value)
{
    assert(ctx==&space && index>=FIRST && index<FIRST+PAGES);
    *value=entries[index-FIRST];
    return true;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t value)
{
    assert(ctx==&space && index>=FIRST && index<FIRST+PAGES);
    entries[index-FIRST]=value;
    return true;
}

static bool readers_retired(void *ctx)
{
    assert(ctx==&space);
    return fault!=DETACH_PENDING || !posted;
}

static bool hardware_held(void *ctx,const struct bdw_blitter_image *p)
{
    assert(ctx==&space && p==&image);
    if (fault==LOST_DURING_INIT && regs[0x80/4]) return false;
    return fault!=LOST_HOLD || posted<2;
}

static uint64_t now(void *ctx)
{
    assert(ctx==&space);
    if (active->port.p.attempts==4 && active->port.p.writes==3 && ++trigger_clocks==revoke_at) {
        if (completion_fault==EARLY_MARKER) {
            uint64_t marker=(image.epoch<<8)|2;
            ram.vm.result[BDW_BARRIER_BCS][16]=(uint32_t)marker;
            ram.vm.result[BDW_BARRIER_BCS][17]=(uint32_t)(marker>>32);
        } else if (completion_fault==EARLY_CURSOR) {
            regs[0x3a0/4]^=1;
        } else {
            revoked=true;
        }
    }
    if (posted<active->port.p.writes) {
        const uint32_t expected[]={0,0,image.config.context_id,(uint32_t)image.descriptor};
        assert(active->port.p.writes==posted+1 && regs[0x230/4]==expected[posted]);
        ++posted;
        regs[0x234/4]=posted<4 ? 17 : 2;
        if (posted==4) {
            uint64_t marker=(image.epoch<<8)|2;
            uint32_t *result=ram.vm.result[BDW_BARRIER_BCS];
            result[0]=(uint32_t)~marker;result[1]=(uint32_t)(~marker>>32);
            result[16]=(uint32_t)marker;result[17]=(uint32_t)(marker>>32);
            if (fault==WRONG_DATA)result[1]^=1;
            if (fault==SCRIBBLE)result[1023]=1;
            unsigned enter=(active->csb_start+1)%6, end=(enter+1)%6;
            regs[(0x370+8*enter)/4]=1;
            regs[(0x370+8*end)/4]=fault==PREEMPT ? 2 : 0x818;
            regs[(0x374+8*end)/4]=image.config.context_id+(fault==OTHER_CONTEXT);
            if (fault!=NO_COMPLETION)regs[0x3a0/4]=0x500|end;
        }
    }
    if (active->state==BDW_BLITTER_ISSUED) {
        if (++completion_clocks==closing_fault_at) {
            if (completion_fault==CLOSING_MARKER) ram.vm.result[BDW_BARRIER_BCS][16]^=1;
            else if (completion_fault==CLOSING_CURSOR) regs[0x3a0/4]^=1;
            else revoked=true;
        }
        if (fault==EXPIRED)clock_us+=BCS_FLUSH_WAIT_US+1;
        if (fault==BUSY)regs[0x234/4]|=1u<<14;
    }
    return ++clock_us;
}

static void run(enum fault injected)
{
    memset(&space,0,sizeof(space));memset(&pub,0,sizeof(pub));memset(regs,0,sizeof(regs));
    memset(&request,0,sizeof(request));memset(&image,0,sizeof(image));memset(entries,0,sizeof(originals));
    intel_bdw_bcs_fixture_new_device();
    clock_us=1000000;posted=0;fault=injected;
    struct intel_bdw_bcs_context_config c={.ring_gpu=FIRST*4096,.context_gpu=(FIRST+1)*4096,
        .completion_gpu=(FIRST+3)*4096,.aperture_bytes=0x10000000,.fence_begin=0x100000,.fence_end=0x2300000,
        .nonce=0x123456789abc,.context_id=23,.vendor=0x8086,.device=0x1626};
    assert(intel_bdw_blitter_prepare(&image,&c,ram.global,&ram.vm,BASE,c.nonce));
    assert(intel_bdw_blitter_verify(&image));
    uint64_t desired[PAGES];
    for(unsigned i=0;i<PAGES;i++)desired[i]=(BASE+i*4096)|1;
    struct scanout_space_range allowed={FIRST,PAGES},protected={0,8704};
    struct scanout_space_token lease;
    struct scanout_publish_token token={0};
    assert(scanout_space_open_transferred(&space,&ranges,7,65536,allowed,protected,originals));
    assert(scanout_space_reserve(&space,7,space.epoch,PAGES,1,&lease));
    struct scanout_publish_io io={&space,yes,read_entry,write_entry,yes,readers_retired,now};
    assert(scanout_publish_prepare(&pub,&publications,7,&space,&lease,desired,PAGES,&io,&token));
    if (fault!=UNPUBLISHED)assert(scanout_publish_apply(&pub,&token));
    if (fault==WRONG_BINDING)entries[6]^=4096;
    regs[0x29c/4]=fault==WRONG_MODE ? 0 : 1u<<15;
    regs[0x80/4]=(FIRST+32+(fault==WRONG_HWSP))*4096;
    regs[0x234/4]=1;regs[0x3a0/4]=0x504;regs[0x370/4]=0x18;regs[0x374/4]=23;
    struct bdw_blitter_device device={.registers=regs,.owner=&space,.held=hardware_held,.now=now,
        .mmio=0xc0000000,.root=0x9000,.outer=1000000,.gt=1000000,.hda=1000000,.prepare=1000000,.original_hwsp=(FIRST+32)*4096,.csb_head=fault==STALE_CURSOR?5u:4u};
    bool done=intel_bdw_blitter_run(&request,&image,&device,&pub,&token);
    assert(done==(fault==OK));
    if (done) {
        assert(regs[0x80/4]==device.original_hwsp);
        assert(posted==4 && request.csb_end==0 && request.state==BDW_BLITTER_RETIRED && !pub.readers);
        assert(scanout_publish_restore(&pub,&token));
        for(unsigned i=0;i<PAGES;i++)assert(!entries[i]);
    } else if (fault==UNPUBLISHED || fault==WRONG_MODE || fault==WRONG_HWSP || fault==STALE_CURSOR) {
        assert(!posted && !pub.readers && request.state==BDW_BLITTER_NEW);
    } else {
        assert(request.state==BDW_BLITTER_RETAINED && pub.readers==&request.reader);
        assert(!scanout_publish_restore(&pub,&token));
        if (fault==WRONG_BINDING || fault==WRONG_MODE || fault==WRONG_HWSP || fault==STALE_CURSOR)assert(!posted);
        else assert(posted==(fault==LOST_HOLD?2u:4u));
    }
}
static bool validate_transfer(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase)
{
    if (ctx!=&space || m->first!=FIRST || m->pages!=PAGES || m->assurances!=HANDOFF_ALL || m->obligations)
        return false;
    return phase==TRANSFER_LIVE || !memcmp(entries,m->original,sizeof(originals));
}

static bool window_held(void *ctx,const struct bdw_blitter_window *w,const struct scanout_transfer *t)
{
    model_aperture();
    if (lose_rollback_epoch && t->device_publication && t->device_publication->writes>PAGES &&
        entries[0]==originals[0]) {
        ++gt.engine_epoch;
        lose_rollback_epoch=false;
    }
    return ctx==&space && w->pte==ggtt && w->physical==0xc0800000ull+(FIRST/512)*4096 &&
           t->manifest.mmio==0xc0000000 && t->state==TRANSFER_ACTIVE;
}

enum mapping_case { MAP_OK, MAP_PREEXISTING_ALIAS, MAP_MISSING_COMPLETION,
    COLD_OK, COLD_RETURNED_BORROW, COLD_STALE_EPOCH, COLD_RING_BUSY, COLD_PORT_BUSY,
    COLD_LOST_COMPLETION, COLD_STALE_DETACH, COLD_ATTACHED_HWSP, COLD_UNDRAINED, COLD_LOST_INIT, COLD_WRONG_OWNER, COLD_CHANGED_HEAD, COLD_UNMASKED_STATUS, COLD_PARSER_ALREADY_RUNNING, COLD_EPOCH_DURING_ROLLBACK };

static struct intel_gt_borrow borrow;
static struct bdw_boot_source sources[2];
static struct intel_bdw_gt_bootstrap resets[2];
static struct intel_completion_result audits[2];
static char fifo,other_request;

static void reset_epoch(unsigned n,const struct display_access_token *exclusion,struct scanout_transfer *t)
{
    struct bdw_boot_source *s=&sources[n];
    struct intel_bdw_gt_bootstrap *r=&resets[n];
    gt=(struct intel_gt_access){.object=&gt,.exclusion=*exclusion,.pinned=true,
        .state=GT_ACCESS_AWAKE,.engine_epoch=10+n,.borrows=1,.dependents=&borrow};
    borrow=(struct intel_gt_borrow){.object=&borrow,.parent=&gt,.active=true};
    s->binding=(struct bdw_boot_binding){.owner=&gt,.lifetime=&borrow,.provider=&space,
        .aperture_bytes=0x10000000,.blitter_port_owner=&fifo,.command_pages=t,.command_pages_epoch=t->epoch,.root=0x9000,.mmio=0xc0000000,.epoch=gt.engine_epoch-1,.vendor=0x8086,.device=0x1626,
        .outer=1000000,.hda=1000000,.gt=1000000,.prepare=1000000};
    s->object=s;s->consumer=r;
    *r=(struct intel_bdw_gt_bootstrap){.object=r,.source=s,.binding=s->binding,.stage=BDW_BOOT_DONE,
        .epoch_advanced=true,.preserved=true,.complete=true,.reset_completed=2,.begin=++clock_us};
    r->end=++clock_us;
    audits[n]=(struct intel_completion_result){.object=&audits[n],.parent=&gt,.engine_epoch=gt.engine_epoch,
        .stable=true,.complete=true,.begin_us=++clock_us};
    audits[n].end_us=++clock_us;
    audits[n].words[1][4][4]=1;
    audits[n].context[1][3]=n ? 0x200 : 0x300;
}
enum traffic_fault { TRAFFIC_OK, LOST_ENGINE_ACK, EARLY_L3, NO_NEW_L3, WIDI_ESCAPES,
                     LOST_BLOCK_EPOCH, MOVED_PLANE, STALE_SECOND_DRAIN, AIR_RESET_REASONS, PARTIAL_GO,
                     UNOWNED_CLOCK, AUDIO_REOPEN, LOST_RELEASE, PEER_MISSING, PEER_OPEN, PEER_ROLLBACK, PEER_DURING_SUBMIT, WRITER_ALIAS, WRITER_RETURN, FINAL_DEVICE_LEASE, FINAL_WIDI_LEASE, WIDI_CACHED, WIDI_INVALIDATING, WIDI_CACHED_ROLLBACK, EARLY_ENGINE_GO, EARLY_ENGINE_REPLY,
                     WIDI_GO_WHEN_REOPENED, WIDI_GO_BEFORE_TRIGGER, WIDI_GO_DURING_ROLLBACK,
                     WIDI_GO_AT_RECLOSE, WIDI_GO_IN_DRAIN_PACKET, WIDI_PREPARE_AT_EXECUTION,
                     WIDI_RESET_BEFORE_TRIGGER, WIDI_RESET_DURING_ROLLBACK };
static enum traffic_fault traffic_fault;
static struct {
    const struct scanout_transfer *transfer;
    const struct scanout_publication *publication;
    uint64_t epoch,widi_epoch;
    uint32_t selector;
    bool bcs;
    unsigned drains,tlb_controls,l3_reads,widi_reclose_reads;
    bool injected_early;
} controller;

static uint64_t block_epoch(const struct scanout_transfer *t,const struct scanout_publication *p)
{
    assert(t==controller.transfer && p==controller.publication);
    return revoked && (traffic_fault==FINAL_DEVICE_LEASE || completion_fault==CLOSING_DEVICE) ? 0 : controller.epoch;
}
static bool traffic_read(const struct scanout_transfer *t,uint32_t offset,uint32_t *out)
{
    assert(t==controller.transfer);
    bool blocked=controller.epoch!=0, bcs=controller.bcs;
    bool reclose_fault=traffic_fault==WIDI_GO_AT_RECLOSE || traffic_fault==WIDI_GO_IN_DRAIN_PACKET;
    uint32_t allowed=blocked && !bcs?0:0x5f;
    switch (offset) {
    case 0x40d0: case 0x8028:
        *out=traffic_fault==AIR_RESET_REASONS && offset==0x40d0 ? 0x7fff :
            (traffic_fault==PARTIAL_GO && bcs ? 0x10 : allowed);
        if (traffic_fault==EARLY_ENGINE_REPLY && offset==0x8028 && blocked && !bcs && !controller.injected_early) {
            *out|=1u<<4;
            controller.injected_early=true;
        }
        break;
    case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010:
        *out=!blocked || bcs?0x10:0x20;
        if (traffic_fault==EARLY_ENGINE_GO && offset==0x800c && blocked && !bcs && !controller.injected_early) {
            *out=0x30;
            controller.injected_early=true;
        }
        if (traffic_fault==LOST_ENGINE_ACK && offset==0x8008 && blocked) *out=0;
        break;
    case 0x8104: case 0x802c: *out=controller.widi_epoch &&
        !(traffic_fault==WIDI_ESCAPES && bcs && offset==0x802c) ? 0x20 : 0; break;
    case 0x8014:
        *out=!blocked || traffic_fault==EARLY_L3 || (traffic_fault==WIDI_ESCAPES && bcs)?0x1f:0;
        if (bcs && (traffic_fault==WIDI_GO_WHEN_REOPENED ||
                    (revoked && (traffic_fault==WIDI_GO_BEFORE_TRIGGER || completion_fault==CLOSING_WIDI_GO))))
            *out|=0x10;
        if (traffic_fault==WIDI_GO_DURING_ROLLBACK && controller.drains==2 && entries[0]==originals[0])
            *out|=0x10;
        if (reclose_fault && controller.drains==2 &&
            ++controller.widi_reclose_reads==(traffic_fault==WIDI_GO_AT_RECLOSE?1u:3u))
            *out|=0x10;
        break;
    case 0x4278:
        *out=++controller.tlb_controls==2 && traffic_fault==WIDI_INVALIDATING;
        break;
    case 0x4dc0: *out=!invalidates || traffic_fault==WIDI_CACHED ? 0x40000000u : 0; break;
    case 0x4dc4: *out=traffic_fault==WIDI_CACHED_ROLLBACK && invalidates==2 ? 1 : 0; break;
    case 0x8038: *out=blocked && !bcs &&
            (++controller.l3_reads==(reclose_fault && controller.drains==2?3u:2u) || traffic_fault==EARLY_L3) &&
            traffic_fault!=NO_NEW_L3 &&
            !(traffic_fault==STALE_SECOND_DRAIN && controller.drains==2); break;
    case 0x8040: *out=1; break;
    case 0x8030:
        *out=traffic_fault==WIDI_PREPARE_AT_EXECUTION && controller.widi_epoch ? 1u<<5 : 0;
        break;
    case 0x941c:
        *out=(revoked && (traffic_fault==WIDI_RESET_BEFORE_TRIGGER || completion_fault==CLOSING_WIDI_RESET)) ||
             (traffic_fault==WIDI_RESET_DURING_ROLLBACK && controller.drains==2 && entries[0]==originals[0]) ?
             1u<<6 : 0;
        break;
    case 0x8108: case 0x12870: case 0x12878: case 0x1c870: case 0x1c878: case 0xb11c: *out=0; break;
    case 0x9120: *out=3u<<25; break;
    case 0x138064: *out=3; break;
    case 0xfdc: *out=controller.selector; break;
    case 0x70180: *out=1u<<31; break;
    case 0x7019c: case 0x701ac: *out=traffic_fault==MOVED_PLANE && bcs?4096:0; break;
    default: assert(0);return false;
    }
    return true;
}
static enum intel_gt_write select_bank(const struct scanout_transfer *t,uint32_t from,uint32_t to)
{
    assert(t==controller.transfer && from==controller.selector);
    controller.selector=to;return GT_WRITE_DONE;
}
static enum intel_gt_write block_request(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t *epoch)
{
    assert(!block_epoch(t,p) && controller.selector==0);
    *epoch=controller.epoch=93;controller.drains=1;
    return GT_WRITE_DONE;
}
static enum intel_gt_write permit_bcs(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t epoch,bool enable)
{
    assert(epoch==93 && block_epoch(t,p)==epoch && enable!=controller.bcs);
    if (enable) assert(invalidates==1 && !posted && p->writes==PAGES);
    else {
        ++controller.drains;
        controller.l3_reads=0;
    }
    controller.bcs=enable;
    if (enable && traffic_fault==LOST_BLOCK_EPOCH) ++controller.epoch;
    return GT_WRITE_DONE;
}
static enum intel_gt_write block_release(const struct scanout_transfer *t,const struct scanout_publication *p,uint64_t epoch)
{
    assert(epoch==block_epoch(t,p) && !controller.bcs && (!p->exposed || invalidates==2));
    if (traffic_fault==LOST_RELEASE) return GT_WRITE_UNCERTAIN;
    controller.epoch=controller.widi_epoch=0;return GT_WRITE_DONE;
}
static uint64_t widi_epoch(const struct scanout_transfer *t,const struct scanout_publication *p)
{
    assert(t==controller.transfer && p==controller.publication);
    if (revoked && (traffic_fault==FINAL_WIDI_LEASE || completion_fault==CLOSING_WIDI)) return 0;
    return traffic_fault==UNOWNED_CLOCK && controller.bcs ? 0 : controller.widi_epoch;
}
static enum intel_gt_write isolate_widi(const struct scanout_transfer *t,const struct scanout_publication *p,
                                         uint64_t epoch,uint64_t *lease)
{
    assert(epoch==block_epoch(t,p) && !widi_epoch(t,p) && !controller.bcs);
    *lease=controller.widi_epoch=37;
    return GT_WRITE_DONE;
}
static const struct bdw_command_control command_control={
    .traffic={.held_epoch=block_epoch,.read=traffic_read,.select_l3=select_bank,.request=block_request,.release=block_release},
    .isolate_widi=isolate_widi,.widi_epoch=widi_epoch,.set_execution=permit_bcs
};
static struct vmm_reserved_guard route_guard={&route_guard,1};
static const void *port_pin;
bool vmm_reserved_guard_pin_writer(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,uint64_t alias,const void *owner)
{
    assert(g==&route_guard && root==0x9000 && page==0xc0022000 && alias==(uintptr_t)regs);
    if (!owner || port_pin || traffic_fault==WRITER_ALIAS) return false;
    port_pin=owner;
    return true;
}
bool vmm_reserved_guard_writer_held(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,uint64_t alias,const void *owner)
{
    return owner && owner==port_pin && g==&route_guard && root==0x9000 && page==0xc0022000 && alias==(uintptr_t)regs;
}
bool vmm_reserved_guard_check_writer(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,uint64_t alias,const void *owner)
{
    assert(controller.epoch && !controller.bcs && invalidates==2);
    return vmm_reserved_guard_writer_held(g,root,page,alias,owner) && traffic_fault!=WRITER_RETURN;
}
bool vmm_reserved_guard_unpin_writer(const struct vmm_reserved_guard *g,uint64_t root,uint64_t page,uint64_t alias,const void *owner)
{
    assert(vmm_reserved_guard_writer_held(g,root,page,alias,owner));
    assert(!invalidates || !controller.epoch);
    port_pin=NULL;
    return true;
}
static bool peer_protected(uint64_t pa)
{
    switch (pa) {
    case 0xc0002000: return traffic_fault!=PEER_MISSING;
    case 0xc0012000: return !(controller.bcs && traffic_fault==PEER_OPEN);
    case 0xc001a000: return !(controller.drains==2 && traffic_fault==PEER_ROLLBACK);
    case 0xc001c000: return !(traffic_fault==PEER_DURING_SUBMIT && posted==3);
    default: return false;
    }
}
bool vmm_reserved_guard_readonly_registers_held(const struct vmm_reserved_guard *g,uint64_t root,const uint64_t *pages,unsigned count)
{
    assert(g==&route_guard && root==0x9000 && count==4);
    for (unsigned i=0;i<count;i++) if (!peer_protected(pages[i])) return false;
    return true;
}
bool vmm_reserved_guard_excludes_io_page(const struct vmm_reserved_guard *g,uint64_t root,uint64_t pa)
{
    assert(g==&route_guard && root==0x9000);
    if (pa==0xc0065000) return traffic_fault!=AUDIO_REOPEN || !controller.bcs;
    return pa==0xc0014000 || pa==0xc006e000;
}
static void run_transfer(enum mapping_case which)
{
    static struct bdw_blitter_mapping mapping;
    static struct scanout_transfer transfer;
    static struct scanout_transfer_pool transfers;
    struct display_access_token exclusion;
    struct scanout_transfer_token grant;
    struct scanout_transfer_manifest manifest={.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,
        .fence_end=0x2300000,.source_epoch=47,.first=FIRST,.pages=PAGES,.assurances=HANDOFF_ALL,.original=originals};
    memset(&mapping,0,sizeof(mapping));memset(&transfer,0,sizeof(transfer));memset(&image,0,sizeof(image));
    memset(regs,0,sizeof(regs));active=&mapping.request;posted=0;clock_us=1000000;fault=which==MAP_MISSING_COMPLETION || which==COLD_LOST_COMPLETION ? NO_COMPLETION : OK;
    intel_bdw_bcs_fixture_new_device();
    for (unsigned i=0;i<PAGES;i++)originals[i]=entries[i]=0x110000001ull+i*4096;
    memset(invalidate_regs,0,sizeof(invalidate_regs)); invalidates=0;
    memcpy(translations,originals,sizeof(translations));
    if (which==MAP_PREEXISTING_ALIAS)originals[PAGES-1]=entries[PAGES-1]=(BASE+12*4096)|1u;
    assert(display_access_acquire(7,&exclusion));
    bool cold=which>=COLD_OK;
    manifest.source_epoch=exclusion.epoch;
    if (cold) {
        struct scanout_transfer_manifest reservation=manifest;
        reservation.assurances=0;
        assert(scanout_transfer_reserve(&transfer,&transfers,&exclusion,&reservation,&grant));
    } else assert(scanout_transfer_open(&transfer,&transfers,&ranges,&exclusion,&manifest,
                                 (struct scanout_transfer_io){&space,validate_transfer,now},&grant));
    struct intel_bdw_bcs_context_config c={.ring_gpu=FIRST*4096,.context_gpu=(FIRST+1)*4096,
        .completion_gpu=(FIRST+3)*4096,.aperture_bytes=0x10000000,.fence_end=manifest.fence_end,
        .nonce=grant.epoch,.context_id=23,.vendor=0x8086,.device=0x1626};
    assert(intel_bdw_blitter_prepare(&image,&c,ram.global,&ram.vm,BASE,grant.epoch));
    struct bdw_blitter_device device={.registers=regs,.owner=&space,.held=hardware_held,.now=now,
        .mmio=manifest.mmio,.root=0x9000,.outer=1000000,.gt=1000000,.hda=1000000,.prepare=1000000,
        .original_hwsp=(FIRST+32)*4096,.csb_head=4};
    regs[0x80/4]=device.original_hwsp;regs[0x29c/4]=1u<<15;regs[0x234/4]=1;regs[0x3a0/4]=0x504;

    if (cold) {
        assert(intel_bdw_bcs_port_claim(&fifo));
        reset_epoch(0,&exclusion,&transfer);
        device.reset=&resets[0];device.post_reset=&audits[0];device.port_owner=&fifo;
        device.original_hwsp=0;device.csb_head=0;
        regs[0x80/4]=0;regs[0x29c/4]=0;regs[0x9c/4]=0x300;regs[0x98/4]=UINT32_MAX;regs[0x3a0/4]=0;
        if (which==COLD_RETURNED_BORROW)borrow.active=false;
        if (which==COLD_STALE_EPOCH)++gt.engine_epoch;
        if (which==COLD_PARSER_ALREADY_RUNNING) {
            /* Air post-reset MI_MODE observed on both sides of BME reopening. */
            regs[0x9c/4]=audits[0].context[1][3]=0x200;
        }
        if (which==COLD_LOST_INIT)fault=LOST_DURING_INIT;
        if (which==COLD_WRONG_OWNER)resets[0].binding.blitter_port_owner=&other_request;
        if (which==COLD_CHANGED_HEAD)regs[0x34/4]=8;
        if (which==COLD_UNMASKED_STATUS)regs[0x98/4]^=1;
        if (which==COLD_RING_BUSY)regs[0x3c/4]=1;
        if (which==COLD_PORT_BUSY)assert(intel_bdw_bcs_port_lend(&fifo,&other_request));
    }
    struct bdw_blitter_window window={.pte=ggtt,.physical=0xc0800000ull+(FIRST/512)*4096,
        .owner=&space,.held=window_held,.readers_retired=readers_retired,
        .invalidate_page=invalidate_regs,.invalidate_physical=device.mmio+0x101000,.mmio_bytes=0x1000000,
        .aperture_pages=aperture,.aperture_physical=manifest.aperture+FIRST*4096};
    if (which==MAP_OK) {
        assert(!intel_bdw_blitter_map_and_run(&mapping,&image,&device,&transfer,&grant,&window));
        assert(!posted && !mapping.pages.writes && !transfer.command_hold);
    }
    memset(&controller,0,sizeof(controller));
    controller.transfer=&transfer; controller.publication=&mapping.pages;
    struct bdw_wireless_routes routes={.transfer=&transfer,.guard=&route_guard,.transfer_epoch=grant.epoch,
        .guard_epoch=route_guard.epoch,.root=device.root,.mmio=device.mmio,.strap=1u<<20,
        .well_before=1u<<30,.well_after=1u<<30,.codec_before=0x80862808,.codec_after=0x80862808};
    if (!intel_bdw_command_acquire(&mapping,&transfer,&grant,&routes,command_control,&space,now)) {
        if (traffic_fault==PEER_MISSING) {
            assert(mapping.traffic.phase==BDW_CMD_UNUSED && !controller.epoch && !posted && !mapping.pages.writes &&
                   !transfer.command_hold && scanout_transfer_close(&transfer,&grant));
            assert(display_access_release(&exclusion,true));
            return;
        }
        if (traffic_fault==AIR_RESET_REASONS) {
            assert(mapping.traffic.phase==BDW_CMD_REFUSED && !controller.epoch && !posted &&
                   !mapping.pages.writes && !transfer.command_hold && scanout_transfer_close(&transfer,&grant));
            assert(display_access_release(&exclusion,true));
            return;
        }
        assert(traffic_fault==LOST_ENGINE_ACK || traffic_fault==EARLY_L3 || traffic_fault==NO_NEW_L3 ||
               traffic_fault==EARLY_ENGINE_GO || traffic_fault==EARLY_ENGINE_REPLY);
        assert(mapping.traffic.phase==BDW_CMD_RETAINED && !posted && !mapping.pages.writes &&
               !scanout_transfer_close(&transfer,&grant));
        return;
    }
    assert(mapping.traffic.drains==1 && !scanout_transfer_close(&transfer,&grant));
    if (cold) {
        window.spaces=&ranges;
        window.provider=(struct scanout_transfer_io){&space,validate_transfer,now};
        window.handoff=&manifest;
        if (which==COLD_OK) {
            manifest.assurances &= ~HANDOFF_DMA;
            assert(!intel_bdw_blitter_map_and_run(&mapping,&image,&device,&transfer,&grant,&window));
            manifest.assurances |= HANDOFF_DMA;
            resets[0].binding.command_pages_epoch++;
            assert(!intel_bdw_blitter_map_and_run(&mapping,&image,&device,&transfer,&grant,&window));
            resets[0].binding.command_pages_epoch--;
            assert(transfer.state==TRANSFER_RESERVED && !posted && !mapping.pages.writes);
        }
    }
    bool done=intel_bdw_blitter_map_and_run(&mapping,&image,&device,&transfer,&grant,&window);
    if (completion_fault!=COMPLETION_OK) {
        unsigned expected=completion_fault<=EARLY_CURSOR ? 3 : 4;
        assert(!done && posted==expected && mapping.request.port.p.triggered==(expected==4));
        assert(!mapping.request.csb_end && mapping.request.state==BDW_BLITTER_RETAINED &&
               mapping.traffic.phase==BDW_CMD_RETAINED && port_pin==&mapping);
        assert(mapping.pages.writes==PAGES && mapping.pages.readers==&mapping.request.reader &&
               transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==WRITER_ALIAS) {
        assert(!done && !posted && !port_pin && !invalidates && !mapping.pages.writes &&
               transfer.state==TRANSFER_RESERVED && !transfer.device_publication);
        assert(intel_bdw_command_release(&mapping) && scanout_transfer_close(&transfer,&grant));
        assert(display_access_release(&exclusion,true));
        return;
    }
    if (traffic_fault==WRITER_RETURN) {
        assert(!done && posted==4 && invalidates==2 && port_pin==&mapping && controller.widi_epoch &&
               controller.epoch && mapping.traffic.phase==BDW_CMD_RETAINED &&
               !memcmp(originals,entries,sizeof(originals)) && transfer.device_publication==&mapping.pages &&
               !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==FINAL_DEVICE_LEASE || traffic_fault==FINAL_WIDI_LEASE ||
        traffic_fault==WIDI_GO_BEFORE_TRIGGER || traffic_fault==WIDI_RESET_BEFORE_TRIGGER) {
        assert(revoked && mapping.request.port.p.attempts==4);
    }
    if (traffic_fault==PEER_DURING_SUBMIT || revoked) {
        assert(!done && posted==3 && mapping.request.port.p.writes==3 && !mapping.request.port.p.triggered);
        assert(port_pin==&mapping && mapping.request.state==BDW_BLITTER_RETAINED && mapping.traffic.phase==BDW_CMD_RETAINED);
        assert(mapping.pages.writes==PAGES && mapping.pages.readers==&mapping.request.reader &&
               transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==WIDI_GO_DURING_ROLLBACK || traffic_fault==WIDI_RESET_DURING_ROLLBACK) {
        assert(!done && posted==4 && invalidates==1 && controller.drains==2 &&
               mapping.traffic.phase==BDW_CMD_RETAINED && mapping.pages.state==PUB_FAULT &&
               entries[0]==originals[0] && entries[1]==((BASE+4096u)|1u));
        assert(port_pin==&mapping && transfer.device_publication==&mapping.pages &&
               !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==WIDI_PREPARE_AT_EXECUTION) {
        assert(!done && !controller.bcs && !posted && invalidates==1 &&
               mapping.traffic.phase==BDW_CMD_RETAINED && mapping.pages.state==PUB_PUBLISHED);
        assert(mapping.pages.writes==PAGES && port_pin==&mapping && display_access_held(&exclusion) &&
               transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==WIDI_GO_AT_RECLOSE || traffic_fault==WIDI_GO_IN_DRAIN_PACKET) {
        assert(!done && posted==4 && invalidates==1 && controller.drains==2 && mapping.traffic.drains==1);
        assert(controller.widi_reclose_reads==(traffic_fault==WIDI_GO_AT_RECLOSE?1u:3u));
        assert(mapping.traffic.phase==BDW_CMD_RETAINED && mapping.pages.state==PUB_PUBLISHED &&
               mapping.pages.writes==PAGES && port_pin==&mapping && display_access_held(&exclusion));
        for (unsigned i=0;i<PAGES;i++) assert(entries[i]==((BASE+i*4096u)|1u));
        assert(transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==WIDI_CACHED || traffic_fault==WIDI_INVALIDATING || traffic_fault==WIDI_CACHED_ROLLBACK) {
        bool rollback=traffic_fault==WIDI_CACHED_ROLLBACK;
        assert(!done && posted==(rollback ? 4u : 0u) && invalidates==(rollback ? 2u : 1u));
        assert(mapping.invalidation[rollback].complete && !mapping.verified_pages[rollback]);
        assert(mapping.pages.state==PUB_FAULT && transfer.device_publication==&mapping.pages &&
               mapping.pages.writes==invalidates*PAGES && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault==LOST_RELEASE) {
        assert(!done && posted==4 && invalidates==2 && mapping.verified_pages[1]==PAGES);
        assert(mapping.pages.writes==2*PAGES && !memcmp(originals,entries,sizeof(originals)));
        assert(mapping.traffic.phase==BDW_CMD_RETAINED && mapping.range.object &&
               transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (traffic_fault!=TRAFFIC_OK) {
        bool rollback=traffic_fault==STALE_SECOND_DRAIN || traffic_fault==PEER_ROLLBACK;
        assert(!done && mapping.traffic.phase==BDW_CMD_RETAINED && posted==(rollback?4u:0u));
        assert(mapping.pages.writes==PAGES && invalidates==1 && mapping.traffic.drains==1);
        assert(!scanout_transfer_close(&transfer,&grant));
        return;
    }
    if (translation_fault!=NORMAL_TRANSLATION) {
        bool rollback=translation_fault==STALE_RESTORE;
        assert(!done && posted==(rollback?4u:0u));
        assert(mapping.pages.state==PUB_FAULT && mapping.pages.writes==(rollback?2u:1u)*PAGES);
        assert(transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        if (!rollback) assert(intel_bdw_blitter_verify(&image) && !active->port.p.writes);
        assert(mapping.invalidation[rollback].complete==(translation_fault!=UNPOSTED));
        return;
    }
    if (posted) assert(mapping.verified_pages[0]==PAGES && mapping.invalidation[0].complete);
    if (cold) {
        if (which==COLD_LOST_INIT) {
            assert(!done && !posted && mapping.request.state==BDW_BLITTER_RETAINED);
            assert(mapping.pages.readers==&mapping.request.reader && mapping.pages.writes==PAGES);
            assert(regs[0x80/4]==(FIRST+6)*4096 && !regs[0x29c/4] && !regs[0x3a0/4]);
            assert(!intel_bdw_bcs_port_release(&fifo) && !scanout_transfer_close(&transfer,&grant));
            return;
        }
        if (!posted) {
            assert(which==COLD_RETURNED_BORROW || which==COLD_STALE_EPOCH ||
                   which==COLD_RING_BUSY || which==COLD_PORT_BUSY || which==COLD_WRONG_OWNER ||
                   which==COLD_CHANGED_HEAD || which==COLD_UNMASKED_STATUS);
            assert(!done && !mapping.pages.writes && !transfer.device_publication && !regs[0x29c/4]);
            if (which==COLD_PORT_BUSY)assert(intel_bdw_bcs_port_return(&fifo,&other_request));
        } else {
            assert(posted==4 && intel_bdw_bcs_port_lent(&fifo,&mapping.request.port));
            assert(!intel_bdw_bcs_port_release(&fifo) && !intel_bdw_bcs_port_claim(&other_request));
            assert(!scanout_transfer_close(&transfer,&grant) && mapping.pages.writes==PAGES);
            assert(regs[0x80/4]==(FIRST+6)*4096 && (regs[0x29c/4]&(1u<<15)) && !(regs[0x9c/4]&(1u<<8)));
            assert(!intel_bdw_blitter_detach(&mapping,&resets[0],&audits[0]));
            if (which!=COLD_LOST_COMPLETION) {
                assert(done && mapping.request.state==BDW_BLITTER_STATUS_PINNED && mapping.request.csb_end==1);
                reset_epoch(1,&exclusion,&transfer);
                if (which==COLD_STALE_DETACH)--audits[1].engine_epoch;
                if (which==COLD_UNDRAINED)fault=DETACH_PENDING;
                if (which!=COLD_ATTACHED_HWSP)regs[0x80/4]=0;
                regs[0x29c/4]=0;regs[0x9c/4]=0x200;regs[0x234/4]=1;
                lose_rollback_epoch=which==COLD_EPOCH_DURING_ROLLBACK;
                bool detached=intel_bdw_blitter_detach(&mapping,&resets[1],&audits[1]);
                assert(detached==(which==COLD_OK || which==COLD_PARSER_ALREADY_RUNNING));
                if (which==COLD_EPOCH_DURING_ROLLBACK) {
                    /* Publication counts attempted stores. The second restore
                     * attempt detects the epoch change before touching its PTE. */
                    assert(mapping.request.state==BDW_BLITTER_RETAINED && mapping.pages.writes==PAGES+2);
                    assert(mapping.pages.state==PUB_FAULT && transfer.device_publication==&mapping.pages);
                    assert(entries[0]==originals[0] && entries[1]==((BASE+4096u)|1u));
                    assert(!intel_bdw_bcs_port_release(&fifo) && !scanout_transfer_close(&transfer,&grant));
                    return;
                }
                if (detached) {
                    assert(!port_pin && mapping.verified_pages[1]==PAGES && invalidates==2);
                    assert(!memcmp(entries,originals,sizeof(originals)) && mapping.pages.writes==2*PAGES);
                    assert(!intel_bdw_bcs_port_lent(&fifo,&mapping.request.port));
                }
            } else assert(!done && mapping.request.state==BDW_BLITTER_RETAINED);
            if (which==COLD_STALE_DETACH || which==COLD_ATTACHED_HWSP || which==COLD_UNDRAINED) {
                assert(mapping.pages.writes==PAGES && mapping.pages.readers==&mapping.request.reader);
                audits[1].engine_epoch=gt.engine_epoch;regs[0x80/4]=0;fault=OK;
                assert(intel_bdw_blitter_detach(&mapping,&resets[1],&audits[1]));
            }
            if (which==COLD_LOST_COMPLETION) {
                assert(mapping.pages.writes==PAGES && !scanout_transfer_close(&transfer,&grant));
                assert(mapping.pages.readers==&mapping.request.reader);
                assert(!intel_bdw_bcs_port_release(&fifo));
                return;
            }
        }
        assert(intel_bdw_bcs_port_release(&fifo));
        if (transfer.command_hold) assert(intel_bdw_command_release(&mapping));
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
        return;
    }
    if (which==MAP_PREEXISTING_ALIAS) {
        assert(!done && !posted && !mapping.pages.writes && !mapping.range.object && !transfer.device_publication);
        if (transfer.command_hold) assert(intel_bdw_command_release(&mapping));
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
        return;
    }
    assert(done==(which==MAP_OK) && posted==4 && mapping.pages.pages==PAGES);
    if (done) {
        assert(!memcmp(entries,originals,sizeof(originals)) && regs[0x80/4]==device.original_hwsp);
        assert(mapping.pages.state==PUB_RETIRED && !transfer.device_publication && mapping.pages.writes==2*PAGES);
        if (transfer.command_hold) assert(intel_bdw_command_release(&mapping));
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
    } else {
        assert(transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        assert(mapping.request.state==BDW_BLITTER_RETAINED && mapping.pages.readers==&mapping.request.reader);
        assert(!scanout_publish_restore(&mapping.pages,&mapping.publication) && mapping.pages.writes==PAGES);
    }
}

int main(int argc,char **argv)
{
    const char *const traffic_failures[]={"missing-engine-ack","early-l3","missing-l3","widi-escapes",
                                         "changed-traffic-epoch","moving-scanout","missing-rollback-ack",
                                         "air-reset-reasons","partial-gam-go","unowned-widi-clock","wireless-audio-reopens","uncertain-release",
                                         "peer-missing","peer-open","peer-rollback","peer-during-submit","writer-alias","writer-return","device-lease-before-trigger","widi-lease-before-trigger","widi-cached","widi-invalidate-race","widi-cached-rollback","flush-before-engine-go","flush-before-engine-reply",
                                         "widi-go-reopens","widi-go-before-trigger","widi-go-during-rollback",
                                         "widi-go-at-reclose","widi-go-in-drain-packet","widi-prepare-at-execution",
                                         "widi-reset-before-trigger","widi-reset-during-rollback"};
    for (unsigned i=0;argc==2 && i<sizeof(traffic_failures)/sizeof(traffic_failures[0]);i++) {
        if (strcmp(argv[1],traffic_failures[i])) continue;
        if (i+1==FINAL_DEVICE_LEASE || i+1==FINAL_WIDI_LEASE ||
            i+1==WIDI_GO_BEFORE_TRIGGER || i+1==WIDI_RESET_BEFORE_TRIGGER) {
            run_transfer(COLD_OK);
            revoke_at=trigger_clocks;
            assert(revoke_at>0);
            trigger_clocks=0;
        }
        traffic_fault=(enum traffic_fault)(i+1);
        run_transfer((traffic_fault==STALE_SECOND_DRAIN || traffic_fault==PEER_ROLLBACK ||
                      traffic_fault==LOST_RELEASE || traffic_fault==WRITER_RETURN ||
                      traffic_fault==WIDI_CACHED_ROLLBACK || traffic_fault==WIDI_GO_DURING_ROLLBACK ||
                      traffic_fault==WIDI_GO_AT_RECLOSE || traffic_fault==WIDI_GO_IN_DRAIN_PACKET ||
                      traffic_fault==WIDI_RESET_DURING_ROLLBACK)?MAP_OK:COLD_OK);
        printf("PASS %s\n",traffic_failures[i]);
        return 0;
    }
    const char *const failures[]={"stale-seventh","swapped-translations","unposted-invalidate","stale-restore"};
    for (unsigned i=0;argc==2 && i<sizeof(failures)/sizeof(failures[0]);i++) {
        if (strcmp(argv[1],failures[i])) continue;
        translation_fault=(enum translation_fault)(i+1);
        run_transfer(translation_fault==STALE_RESTORE?MAP_OK:COLD_OK);
        printf("PASS %s\n",failures[i]);
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"warm-lost-completion")) {
        run_transfer(MAP_MISSING_COMPLETION);
        puts("PASS warm mapping retains lost completion");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"cold-init-fault")) {
        run_transfer(COLD_LOST_INIT);
        puts("PASS cold initialization retains selected status page");
        return 0;
    }
    if (argc==2 && !strcmp(argv[1],"cold-rollback-fault")) {
        run_transfer(COLD_EPOCH_DURING_ROLLBACK);
        puts("PASS cold rollback retains changed reset epoch");
        return 0;
    }
    const char *const completion_failures[]={"premature-marker","premature-cursor","completion-device-lease",
        "completion-widi-lease","completion-marker-changed","completion-cursor-changed","completion-widi-go",
        "completion-widi-reset"};
    for (unsigned i=0;argc==2 && i<sizeof(completion_failures)/sizeof(completion_failures[0]);i++) {
        if (strcmp(argv[1],completion_failures[i])) continue;
        run_transfer(COLD_OK);
        const unsigned trigger=trigger_clocks, closing=completion_clocks;
        assert(trigger>0 && closing>0);
        completion_fault=(enum completion_fault)(i+1);
        /* Two early cases target the last callback before the trigger.
         * The rest target the final clock in the completion proof. */
        bool early=completion_fault<=EARLY_CURSOR;
        trigger_clocks=completion_clocks=0;
        revoke_at=early ? trigger : 0;
        closing_fault_at=early ? 0 : closing;
        run_transfer(COLD_OK);
        printf("PASS %s\n",completion_failures[i]);
        return 0;
    }
    assert(argc==1);
    for (enum fault i=OK;i<=DETACH_PENDING;i++)run(i);
    run_transfer(MAP_PREEXISTING_ALIAS);run_transfer(MAP_OK);
    for (enum mapping_case i=COLD_OK;i<=COLD_PARSER_ALREADY_RUNNING;i++)
        if (i!=COLD_LOST_COMPLETION && i!=COLD_LOST_INIT)run_transfer(i);
    /* Each unrecoverable fixture keeps a real display exclusion pinned. */
    run_transfer(COLD_LOST_COMPLETION);
    puts("PASS Broadwell blitter driver (30 cases)");
    return 0;
}
