#include <srvros/intel_bdw_blitter.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum fault { OK, UNPUBLISHED, WRONG_BINDING, LOST_HOLD, NO_COMPLETION, OTHER_CONTEXT,
             PREEMPT, BUSY, WRONG_DATA, SCRIBBLE, EXPIRED, WRONG_MODE, WRONG_HWSP, STALE_CURSOR, DETACH_PENDING };
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
            /* Start at CSB slot four; the completing entry wraps to zero. */
            regs[(0x370+5*8)/4]=1;
            regs[0x370/4]=fault==PREEMPT ? 2 : 0x818;
            regs[0x374/4]=image.config.context_id+(fault==OTHER_CONTEXT);
            if (fault!=NO_COMPLETION)regs[0x3a0/4]=0x500;
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
    return ctx==&space && w->pte==ggtt && w->physical==0xc0800000ull+(FIRST/512)*4096 &&
           t->manifest.mmio==0xc0000000 && t->state==TRANSFER_ACTIVE;
}

enum mapping_case { MAP_OK, MAP_PREEXISTING_ALIAS, MAP_MISSING_COMPLETION };
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
    memset(regs,0,sizeof(regs));active=&mapping.request;posted=0;clock_us=1000000;fault=which==MAP_MISSING_COMPLETION?NO_COMPLETION:OK;
    intel_bdw_bcs_fixture_new_device();
    for (unsigned i=0;i<PAGES;i++)originals[i]=entries[i]=0x110000001ull+i*4096;
    if (which==MAP_PREEXISTING_ALIAS)originals[PAGES-1]=entries[PAGES-1]=(BASE+12*4096)|1u;
    assert(display_access_acquire(7,&exclusion));
    assert(scanout_transfer_open(&transfer,&transfers,&ranges,&exclusion,&manifest,
                                 (struct scanout_transfer_io){&space,validate_transfer,now},&grant));
    struct intel_bdw_bcs_context_config c={.ring_gpu=FIRST*4096,.context_gpu=(FIRST+1)*4096,
        .completion_gpu=(FIRST+3)*4096,.aperture_bytes=0x10000000,.fence_end=manifest.fence_end,
        .nonce=grant.epoch,.context_id=23,.vendor=0x8086,.device=0x1626};
    assert(intel_bdw_blitter_prepare(&image,&c,ram.global,&ram.vm,BASE,grant.epoch));
    struct bdw_blitter_device device={.registers=regs,.owner=&space,.held=hardware_held,.now=now,
        .mmio=manifest.mmio,.root=0x9000,.outer=1000000,.gt=1000000,.hda=1000000,.prepare=1000000,
        .original_hwsp=(FIRST+32)*4096,.csb_head=4};
    regs[0x80/4]=device.original_hwsp;regs[0x29c/4]=1u<<15;regs[0x234/4]=1;regs[0x3a0/4]=0x504;

    struct bdw_blitter_window window={.pte=ggtt,.physical=0xc0800000ull+(FIRST/512)*4096,
        .owner=&space,.held=window_held,.complete=yes,.readers_retired=readers_retired};
    bool done=intel_bdw_blitter_map_and_run(&mapping,&image,&device,&transfer,&grant,&window);
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

int main(void)
{
    for (enum fault i=OK;i<=DETACH_PENDING;i++)run(i);
    run_transfer(MAP_PREEXISTING_ALIAS);run_transfer(MAP_OK);run_transfer(MAP_MISSING_COMPLETION);
    puts("PASS Broadwell blitter driver (18 cases)");
    return 0;
}
