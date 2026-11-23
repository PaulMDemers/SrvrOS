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
                    assert(mapping.verified_pages[1]==PAGES && invalidates==2);
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
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
        return;
    }
    if (which==MAP_PREEXISTING_ALIAS) {
        assert(!done && !posted && !mapping.pages.writes && !mapping.range.object && !transfer.device_publication);
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
        return;
    }
    assert(done==(which==MAP_OK) && posted==4 && mapping.pages.pages==PAGES);
    if (done) {
        assert(!memcmp(entries,originals,sizeof(originals)) && regs[0x80/4]==device.original_hwsp);
        assert(mapping.pages.state==PUB_RETIRED && !transfer.device_publication && mapping.pages.writes==2*PAGES);
        assert(scanout_transfer_close(&transfer,&grant) && display_access_release(&exclusion,true));
    } else {
        assert(transfer.device_publication==&mapping.pages && !scanout_transfer_close(&transfer,&grant));
        assert(mapping.request.state==BDW_BLITTER_RETAINED && mapping.pages.readers==&mapping.request.reader);
        assert(!scanout_publish_restore(&mapping.pages,&mapping.publication) && mapping.pages.writes==PAGES);
    }
}

int main(int argc,char **argv)
{
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
