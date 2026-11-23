#include <srvros/intel_bdw_blitter.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_bdw_bcs_port.h>

enum { PAGE = 4096, HWS_PGA = 0x80, ELSP = 0x230, STATUS = 0x234,
       ACTIVE_ID = 0x238, MODE = 0x29c, CSB = 0x370, CSB_PTR = 0x3a0,
       HWSTAM = 0x98, RING_CTL = 0x3c, MI_MODE = 0x9c, STOP_RING = 1u<<8,
       CSB_SLOTS = 6, EXEC_LISTS = 1u<<15, QUEUE_BUSY = (3u<<14)|0x1cu };

static uint64_t vm_base(const struct bdw_blitter_image *image)
{
    return image->physical + BDW_BLITTER_GLOBAL_PAGES*PAGE;
}

static bool layout(const struct bdw_blitter_image *image)
{
    const struct intel_bdw_bcs_context_config *c = &image->config;
    return image->global && !((uintptr_t)image->global & (PAGE-1)) && image->vm &&
           (uintptr_t)image->global <= UINTPTR_MAX-(BDW_BLITTER_GLOBAL_PAGES+BDW_PRIVATE_PAGES)*PAGE &&
           (uintptr_t)image->vm == (uintptr_t)image->global+BDW_BLITTER_GLOBAL_PAGES*PAGE &&
           image->physical >= 0x100000 && !(image->physical & (PAGE-1)) &&
           image->physical <= (1ull<<32)-(BDW_BLITTER_GLOBAL_PAGES+BDW_PRIVATE_PAGES)*PAGE &&
           c->ring_gpu <= (1ull<<32)-BDW_BLITTER_GLOBAL_PAGES*PAGE &&
           c->ring_gpu+BDW_BLITTER_GLOBAL_PAGES*PAGE <= c->aperture_bytes &&
           (c->ring_gpu>=c->fence_end || c->ring_gpu+BDW_BLITTER_GLOBAL_PAGES*PAGE<=c->fence_begin) &&
           c->context_gpu == c->ring_gpu+PAGE && c->completion_gpu == c->ring_gpu+3*PAGE &&
           c->nonce == image->epoch;
}

bool intel_bdw_blitter_prepare(struct bdw_blitter_image *image,const struct intel_bdw_bcs_context_config *config,
                              volatile uint32_t *global,volatile struct bdw_private_vm_image *vm,
                              uint64_t physical,uint64_t epoch)
{
    if (!image || !config || image->global) return false;
    struct bdw_blitter_image next = {.config=*config,.global=global,.vm=vm,.physical=physical,.epoch=epoch};
    if (!layout(&next) || !intel_bdw_marker_vm_build(vm,vm_base(&next),epoch) ||
        !intel_bdw_bcs_marker_context_build(config,vm_base(&next)+BDW_PRIVATE_PD*PAGE,global,&next.descriptor))
        return false;
    *image=next;
    return true;
}

bool intel_bdw_blitter_verify(const struct bdw_blitter_image *image)
{
    return image && layout(image) && intel_bdw_marker_vm_verify(image->vm,vm_base(image),image->epoch) &&
           intel_bdw_bcs_marker_context_verify(&image->config,vm_base(image)+BDW_PRIVATE_PD*PAGE,
                                               image->global,image->descriptor);
}

static uint32_t reg(const struct bdw_blitter_request *r,unsigned offset)
{
    return r->device.registers[offset/4];
}

static bool reset_matches(const struct bdw_blitter_device *device)
{
    const struct intel_bdw_gt_bootstrap *reset = device->reset;
    return intel_bdw_reset_epoch_live(reset,device->post_reset) &&
           reset->binding.blitter_port_owner == device->port_owner &&
           reset->source->binding.blitter_port_owner == device->port_owner &&
           reset->binding.root == device->root && reset->binding.mmio == device->mmio &&
           reset->binding.vendor == 0x8086 && reset->binding.device == 0x1626 &&
           reset->binding.provider == device->owner && reset->binding.outer == device->outer &&
           reset->binding.hda == device->hda && reset->binding.gt == device->gt &&
           reset->binding.prepare == device->prepare;
}

static bool held(const struct bdw_blitter_request *r)
{
    const struct scanout_publication *p=r->pages;
    return r->device.held(r->device.owner,r->image) && (!r->device.reset || (reset_matches(&r->device) &&
           intel_bdw_bcs_port_lent(r->device.port_owner,&r->port))) && p->state==PUB_PUBLISHED &&
           p->object==p && !p->busy && p->readers==&r->reader && !r->reader.next &&
           p->epoch==r->reader.binding.epoch && (!p->hold_live || p->hold_live(p)) &&
           r->reader.binding.object==p && p->io.guard(p->io.ctx);
}

/* Verify live translations as well as the publication's recorded values.
 * The HWSP is a global address, distinct from the context's status page. */
static bool translations_held(struct bdw_blitter_request *r)
{
    struct scanout_publication *p=r->pages;
    if (!held(r) || p->range.first!=r->image->config.ring_gpu/PAGE ||
        p->pages!=BDW_BLITTER_GLOBAL_PAGES || !p->io.read) return false;
    for (unsigned i=0;i<BDW_BLITTER_GLOBAL_PAGES;i++) {
        uint64_t value,expected=(r->image->physical+i*PAGE)|1;
        if (p->after[i]!=expected || !p->io.read(p->io.ctx,p->range.first+i,&value) || value!=expected)
            return false;
    }
    return held(r);
}

static bool bound(struct bdw_blitter_request *r,uint32_t hwsp)
{
    if (!translations_held(r)) return false;
    uint32_t mode=reg(r,MODE);
    return mode!=UINT32_MAX && (mode&EXEC_LISTS) &&
           reg(r,HWS_PGA)==hwsp;
}

static uint32_t request_hwsp(const struct bdw_blitter_request *r)
{
    return (uint32_t)r->image->config.ring_gpu+6*PAGE;
}

static bool owner_clock(struct bdw_blitter_request *r)
{
    const struct bdw_blitter_device *d=&r->device;
    uint64_t now=d->now(d->owner);
    if (!now || now<r->last || now<d->prepare || now-d->prepare>INTEL_DIAGNOSTIC_PREPARE_US ||
        now-d->gt>INTEL_DIAGNOSTIC_GT_TOTAL_US || now-d->hda>INTEL_DIAGNOSTIC_HDA_US ||
        now-d->outer>INTEL_DIAGNOSTIC_OUTER_US) return false;
    r->last=now;
    return true;
}

static bool switch_hwsp(struct bdw_blitter_request *r,uint32_t from,uint32_t to)
{
    if (!bound(r,from) || (reg(r,STATUS)&QUEUE_BUSY) || !owner_clock(r)) return false;
    r->device.registers[HWS_PGA/4]=to;
    return reg(r,HWS_PGA)==to && held(r);
}

/* The hardware mask is in the high half of BLT_MODE and MI_MODE. Only
 * change execlist mode once, in the full-reset epoch before first submit.
 * The published private status page remains a GPU writer after retirement. */
static bool cold_queue_idle(struct bdw_blitter_request *r)
{
    return translations_held(r) && !(reg(r,RING_CTL)&1) && !(reg(r,STATUS)&QUEUE_BUSY) &&
           reg(r,ACTIVE_ID)==0 && owner_clock(r);
}

static bool reset_queue_matches(const struct bdw_blitter_device *d,const struct intel_completion_result *p)
{
    /* The native census orders RCS, VCS0, VECS, VCS1 and BCS. Compare
     * the live BCS geometry and queue identity with its post-reset sample. */
    static const unsigned offsets[]={0x30,0x34,0x38,RING_CTL,STATUS,ACTIVE_ID};
    for (unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++)
        if (d->registers[offsets[i]/4]!=p->words[1][4][i]) return false;
    uint32_t mi=d->registers[MI_MODE/4];
    return (mi&(1u<<9)) && mi==p->context[1][3] &&
           d->registers[MODE/4]==p->context[1][5] && d->registers[HWSTAM/4]==UINT32_MAX;
}

static bool start_cold_queue(struct bdw_blitter_request *r)
{
    const uint32_t reset_cursors = (CSB_SLOTS-1)*0x101u;
    if (!cold_queue_idle(r) || !reset_queue_matches(&r->device,r->device.post_reset) || reg(r,HWS_PGA) || (reg(r,MODE)&EXEC_LISTS)) return false;
    r->device.registers[HWS_PGA/4]=request_hwsp(r);
    if (reg(r,HWS_PGA)!=request_hwsp(r) || !cold_queue_idle(r)) return false;
    r->device.registers[CSB_PTR/4]=0xffff0000u | reset_cursors;
    if ((reg(r,CSB_PTR)&0xffffu)!=reset_cursors || !cold_queue_idle(r)) return false;
    r->device.registers[MODE/4]=((uint32_t)EXEC_LISTS<<16) | EXEC_LISTS;
    if (!bound(r,request_hwsp(r)) || !cold_queue_idle(r)) return false;
    if (reg(r,MI_MODE)&STOP_RING) {
        r->device.registers[MI_MODE/4]=STOP_RING<<16;
        if ((reg(r,MI_MODE)&STOP_RING) || !cold_queue_idle(r)) return false;
    }
    if ((reg(r,CSB_PTR)&0xffffu)!=reset_cursors || !bound(r,request_hwsp(r))) return false;
    r->csb_start=r->csb_read=CSB_SLOTS-1;
    return true;
}

static uint64_t clock_now(void *ctx)
{
    struct bdw_blitter_request *r=ctx;
    return r->device.now(r->device.owner);
}

static bool port_proof(void *ctx,const struct bcs_submit_source *s,unsigned prefix,struct bcs_submit_proof *proof)
{
    struct bdw_blitter_request *r=ctx;
    if (s!=&r->source || !bound(r,request_hwsp(r))) return false;
    uint32_t status=reg(r,STATUS),id=reg(r,ACTIVE_ID);
    if (reg(r,STATUS)!=status) return false;
    const struct bcs_submit_binding *b=&s->binding;
    *proof=(struct bcs_submit_proof){.source=s,.owner=b->owner,.provider=r,.lifetime=b->lifetime,
        .context=b->context,.root=b->root,.epoch=b->epoch,.sequence=b->sequence,.descriptor=b->descriptor,
        .checked_at=clock_now(r),.requirements=BCS_SUBMIT_REQUIRED,.prefix=prefix,
        .queue_status=status,.running_context=id};
    return true;
}

static enum intel_gt_write post(void *ctx,uint32_t offset,uint32_t value)
{
    struct bdw_blitter_request *r=ctx;
    if (offset!=BCS_SUBMIT_PORT || !held(r) || !owner_clock(r) ||
        r->last<r->port.p.last || r->last-r->port.p.begin>BCS_SUBMIT_US) return GT_WRITE_NONE;
    r->device.registers[ELSP/4]=value;
    /* Ordered UC read drains the posted DWORD. Queue acceptance is checked
     * independently by the submit core after the fourth write. */
    return reg(r,STATUS)==UINT32_MAX ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static bool result_matches(const struct bdw_blitter_request *r)
{
    uint64_t marker=(r->image->epoch<<8)|(BDW_BARRIER_BCS+1);
    volatile const uint32_t *words=r->image->vm->result[BDW_BARRIER_BCS];
    for (unsigned i=0;i<PAGE/4;i++) {
        uint64_t expected=i/2==0 ? ~marker : i/2==BDW_QUEUE_DRAIN/8 ? marker : 0;
        if (words[i]!=(uint32_t)(expected>>(32*(i&1)))) return false;
    }
    return true;
}

static bool within_wait(const struct bdw_blitter_request *r,uint64_t at,uint64_t previous)
{
    return at && at>=previous && at-r->port.end<=BCS_FLUSH_WAIT_US &&
           at-r->device.prepare<=INTEL_DIAGNOSTIC_PREPARE_US && at-r->device.gt<=INTEL_DIAGNOSTIC_GT_TOTAL_US &&
           at-r->device.hda<=INTEL_DIAGNOSTIC_HDA_US && at-r->device.outer<=INTEL_DIAGNOSTIC_OUTER_US;
}

static bool retire(struct bdw_blitter_request *r)
{
    uint64_t last=r->port.end;
    for (unsigned poll=0;poll<BCS_FLUSH_POLLS;poll++) {
        if (!held(r)) return false;
        uint64_t at=clock_now(r);
        if (!within_wait(r,at,last)) return false;
        last=at;
        uint32_t ptr=reg(r,CSB_PTR), tail=ptr&15u;
        if (tail>=CSB_SLOTS || ((ptr>>8)&15u)!=r->csb_read) return false;
        /* One list, one context, no other submitter: idle-to-active followed
         * by complete + active-to-idle. A stale entry at the old tail, an
         * element switch or preemption cannot retire this image. */
        unsigned start=(r->csb_start+1)%CSB_SLOTS, end=(start+1)%CSB_SLOTS;
        if (tail!=r->csb_start && tail!=start && tail!=end) return false;
        if (tail!=end) continue;
        uint32_t enter=reg(r,CSB+start*8), exit=reg(r,CSB+end*8);
        if ((enter&0x801fu)!=1 || (exit&0x801fu)!=0x18 ||
            reg(r,CSB+end*8+4)!=r->image->config.context_id || reg(r,CSB_PTR)!=ptr ||
            (reg(r,STATUS)&QUEUE_BUSY) || !bound(r,request_hwsp(r)) || !result_matches(r)) return false;
        /* Record the consumed cursor for the queue owner. Only engine
         * initialization resets the hardware CSB pointers. */
        if (!held(r) || reg(r,CSB_PTR)!=ptr || (reg(r,STATUS)&QUEUE_BUSY)) return false;
        uint64_t closing=clock_now(r);
        if (!within_wait(r,closing,last)) return false;
        r->last=closing;
        r->csb_end=tail;
        return true;
    }
    return false;
}

static bool device_ready(const struct bdw_blitter_device *d,const struct bdw_blitter_image *image)
{
    if (!d || !image || !d->owner || !d->registers || ((uintptr_t)d->registers & (PAGE-1)) ||
        !d->mmio || (d->mmio&(PAGE-1)) || !d->root || (d->root&(PAGE-1)) || !d->outer ||
        d->gt<d->outer || d->hda<d->outer || d->prepare<d->gt || d->prepare<d->hda ||
        !d->held || !d->now || !d->held(d->owner,image) || !intel_bdw_blitter_verify(image) ||
        !d->held(d->owner,image)) return false;
    uint32_t ptr=d->registers[CSB_PTR/4],mode=d->registers[MODE/4];
    uint64_t gpu=image->config.ring_gpu;
    if (d->reset || d->post_reset) {
        return reset_matches(d) && d->reset->binding.aperture_bytes==image->config.aperture_bytes &&
               reset_queue_matches(d,d->post_reset) && intel_bdw_bcs_port_held(d->port_owner) && !d->original_hwsp && !d->registers[HWS_PGA/4] &&
               !(mode&EXEC_LISTS) && !(d->registers[RING_CTL/4]&1) &&
               !(d->registers[STATUS/4]&QUEUE_BUSY) && !d->registers[ACTIVE_ID/4];
    }
    return !d->port_owner && d->original_hwsp && !(d->original_hwsp & (PAGE-1)) &&
           (d->original_hwsp<gpu || d->original_hwsp>=gpu+BDW_BLITTER_GLOBAL_PAGES*PAGE) &&
           d->registers[HWS_PGA/4]==d->original_hwsp && mode!=UINT32_MAX && (mode&EXEC_LISTS) &&
           !(d->registers[STATUS/4]&QUEUE_BUSY) && (ptr&15u)==d->csb_head &&
           d->csb_head<CSB_SLOTS && ((ptr>>8)&15u)<CSB_SLOTS;
}

static bool run_request(struct bdw_blitter_request *r,const struct bdw_blitter_image *image,
                          const struct bdw_blitter_device *device,struct scanout_publication *p,
                          const struct scanout_publish_token *token)
{
    if (!r || r->state!=BDW_BLITTER_NEW || !p || !p->io.guard || !p->space ||
        !p->space->originals || p->readers || !device_ready(device,image) ||
        !scanout_publish_pin_reader(p,token,&r->reader)) return false;
    r->image=image; r->device=*device; r->pages=p; r->state=BDW_BLITTER_PINNED;
    if (device->reset) {
        r->last=device->post_reset->end_us;
        if (!start_cold_queue(r)) goto retained;
    } else {
        if (!bound(r,device->original_hwsp)) goto retained;
        uint32_t ptr=reg(r,CSB_PTR);
        r->csb_start=ptr&15u;
        r->csb_read=(ptr>>8)&15u;
        if (r->csb_start>=CSB_SLOTS || r->csb_read>=CSB_SLOTS ||
            r->csb_start!=device->csb_head || (reg(r,STATUS)&QUEUE_BUSY) ||
            !switch_hwsp(r,device->original_hwsp,request_hwsp(r))) goto retained;
    }
    const struct intel_bdw_bcs_context_config *c=&image->config;
    r->source.object=&r->source;
    r->source.binding=(struct bcs_submit_binding){.owner=device->owner,.provider=r,.lifetime=p,.context=image,
        .root=device->root,.epoch=token->epoch,.sequence=image->epoch,.descriptor=image->descriptor,
        .ring_gpu=c->ring_gpu,.ring_physical=image->physical,.context_gpu=c->context_gpu,
        .context_physical=image->physical+PAGE,.completion_gpu=c->completion_gpu,
        .completion_physical=image->physical+3*PAGE,.aperture_bytes=c->aperture_bytes,.fence_begin=c->fence_begin,
        .fence_end=c->fence_end,.prepare=device->prepare,.gt=device->gt,.hda=device->hda,.outer=device->outer,
        .admitted_at=clock_now(r),.vendor=c->vendor,.device=c->device,.tail=24};
    struct bcs_submit_io io={r,port_proof,post,clock_now};
    bool issued=device->reset ? intel_bdw_bcs_submit_lent(&r->port,&r->source,io,device->port_owner)
                              : intel_bdw_bcs_submit_run(&r->port,&r->source,io);
    if (!issued) goto retained;
    r->state=BDW_BLITTER_ISSUED;
    if (!retire(r)) goto retained;
    if (device->reset) {
        r->state=BDW_BLITTER_STATUS_PINNED;
        return true;
    }
    if (!switch_hwsp(r,request_hwsp(r),device->original_hwsp) ||
        !p->io.readers_retired || !p->io.readers_retired(p->io.ctx) ||
        !bound(r,device->original_hwsp) || !owner_clock(r) || !scanout_publish_unpin_reader(p,&r->reader)) goto retained;
    r->state=BDW_BLITTER_RETIRED;
    return true;
retained:
    r->state=BDW_BLITTER_RETAINED;
    return false;
}

bool intel_bdw_blitter_run(struct bdw_blitter_request *r,const struct bdw_blitter_image *image,
                          const struct bdw_blitter_device *device,struct scanout_publication *pages,
                          const struct scanout_publish_token *token)
{
    /* Only the mapping owner can detach a cold engine's status page. */
    return device && !device->reset && !device->post_reset && run_request(r,image,device,pages,token);
}

/* A cold mapping outlives its first command. During final rollback check the
 * replacement reset epoch, its clocks and the now-disabled status writer on
 * every access to the seven PTEs, including the flush and final readback. */
static bool mapping_epoch_held(struct bdw_blitter_mapping *m)
{
    if (!m->device.reset) return true;
    if (!intel_bdw_bcs_port_lent(m->device.port_owner,&m->request.port)) return false;
    if (!m->detach_reset) return reset_matches(&m->device);
    const struct bdw_boot_binding *b=&m->detach_reset->binding;
    const struct intel_completion_result *post=m->detach_post;
    if (!intel_bdw_reset_epoch_live(m->detach_reset,post) || b->blitter_port_owner!=m->device.port_owner ||
        !reset_queue_matches(&m->device,post) || reg(&m->request,HWS_PGA) ||
        (reg(&m->request,MODE)&EXEC_LISTS) || (reg(&m->request,RING_CTL)&1) ||
        (reg(&m->request,STATUS)&QUEUE_BUSY) || reg(&m->request,ACTIVE_ID)) return false;
    uint64_t at=m->device.now(m->device.owner);
    if (at<m->detach_clock || at<post->end_us || at-post->end_us>SCANOUT_TRANSFER_DEADLINE_US ||
        at-b->prepare>INTEL_DIAGNOSTIC_PREPARE_US || at-b->gt>INTEL_DIAGNOSTIC_GT_TOTAL_US ||
        at-b->hda>INTEL_DIAGNOSTIC_HDA_US || at-b->outer>INTEL_DIAGNOSTIC_OUTER_US) return false;
    m->detach_clock=at;
    return true;
}

static bool window_held(void *ctx)
{
    struct bdw_blitter_mapping *m=ctx;
    return m->transfer->device_publication==&m->pages &&
           scanout_transfer_check(m->transfer,&m->grant) && m->device.held(m->device.owner,m->image) &&
           m->window.held(m->window.owner,&m->window,m->transfer) && mapping_epoch_held(m);
}

static bool read_entry(void *ctx,uint32_t index,uint64_t *value)
{
    struct bdw_blitter_mapping *m=ctx;
    if (index-m->range.first>=BDW_BLITTER_GLOBAL_PAGES || !window_held(m)) return false;
    *value=m->window.pte[index&511u];
    return true;
}

static bool exchange_entry(void *ctx,uint32_t index,uint64_t value)
{
    struct bdw_blitter_mapping *m=ctx;
    unsigned page=index-m->range.first;
    if (page>=BDW_BLITTER_GLOBAL_PAGES || !m->pages.busy || !m->pages.exposed ||
        !window_held(m)) return false;
    bool restore=m->pages.state==PUB_RESTORING;
    if (!restore && m->pages.state!=PUB_WRITING) return false;
    uint64_t original;
    if (!scanout_transfer_original(m->transfer,&m->grant,index,&original)) return false;
    uint64_t installed=(m->image->physical+page*PAGE)|1;
    if (value!=(restore?original:installed) || m->pages.before[page]!=original ||
        m->pages.after[page]!=installed) return false;
    return intel_bdw_ggtt_exchange(&m->window.pte[index&511u],restore?installed:original,value)==GT_WRITE_DONE;
}

static bool complete_entries(void *ctx)
{
    struct bdw_blitter_mapping *m=ctx;
    return window_held(m) && m->window.complete(m->window.owner) && window_held(m);
}

static bool readers_done(void *ctx)
{
    struct bdw_blitter_mapping *m=ctx;
    return window_held(m) && m->window.readers_retired(m->window.owner) && window_held(m);
}

static uint64_t mapping_clock(void *ctx)
{
    struct bdw_blitter_mapping *m=ctx;
    return m->device.now(m->device.owner);
}

static bool return_mapping(struct bdw_blitter_mapping *m)
{
    struct scanout_transfer *t=m->transfer;
    if (!scanout_transfer_check(t,&m->grant) || !scanout_publish_restore(&m->pages,&m->publication) ||
        !window_held(m) ||
        !scanout_space_release(&t->space,&m->range)) return false;
    t->device_publication=0;
    return true;
}

bool intel_bdw_blitter_map_and_run(struct bdw_blitter_mapping *m,const struct bdw_blitter_image *image,
                                  const struct bdw_blitter_device *device,struct scanout_transfer *t,
                                  const struct scanout_transfer_token *grant,const struct bdw_blitter_window *window)
{
    if (!m || m->range.object || m->pages.object || !t || !grant || t->state!=TRANSFER_ACTIVE ||
        t->device_publication || !device || !device->held || !image ||
        !window || !window->pte || ((uintptr_t)window->pte&(PAGE-1)) || !window->owner ||
        !window->held || !window->complete || !window->readers_retired ||
        (t->manifest.first&511u)>512-BDW_BLITTER_GLOBAL_PAGES ||
        image->epoch!=grant->epoch || t->manifest.mmio!=device->mmio || !device_ready(device,image) ||
        (uint64_t)t->manifest.first*PAGE!=image->config.ring_gpu ||
        t->manifest.pages<BDW_BLITTER_GLOBAL_PAGES || t->manifest.aperture_bytes!=image->config.aperture_bytes ||
        t->manifest.fence_begin!=image->config.fence_begin || t->manifest.fence_end!=image->config.fence_end ||
        !scanout_transfer_check(t,grant)) return false;
    uint64_t offset=0x800000ull+(t->manifest.first/512ull)*PAGE;
    if (offset>UINT64_MAX-t->manifest.mmio || window->physical!=t->manifest.mmio+offset ||
        !window->held(window->owner,window,t)) return false;
    struct scanout_publish_io io={m,window_held,read_entry,exchange_entry,complete_entries,readers_done,mapping_clock};
    uint64_t entries[BDW_BLITTER_GLOBAL_PAGES];
    for (unsigned i=0;i<BDW_BLITTER_GLOBAL_PAGES;i++) entries[i]=(image->physical+i*PAGE)|1;
    /* An inherited PTE that already reaches our private RAM invalidates the
     * fresh-allocation premise, even if it occurs in a neighbouring slot. */
    for (unsigned i=0;i<t->manifest.pages;i++) {
        uint64_t old;
        if (!scanout_transfer_original(t,grant,t->manifest.first+i,&old)) return false;
        uint64_t frame=old & ((1ull<<39)-PAGE); /* Air7,2 physical address bits 38:12 */
        if ((old&1) && frame>=image->physical && frame<vm_base(image)+sizeof(*image->vm)) return false;
    }
    if (!scanout_space_reserve(&t->space,t->owner,t->arena_epoch,BDW_BLITTER_GLOBAL_PAGES,1,&m->range))
        return false;
    if (m->range.first!=t->manifest.first) {
        scanout_space_release(&t->space,&m->range);
        return false;
    }
    if (device->reset && !intel_bdw_bcs_port_lend(device->port_owner,&m->request.port)) {
        scanout_space_release(&t->space,&m->range);
        return false;
    }
    /* Keep transfer close from releasing the parent across provider callbacks,
     * including a partially issued PTE store or uncertain context save. */
    m->image=image; m->device=*device; m->window=*window; m->grant=*grant; m->transfer=t;
    t->device_publication=&m->pages;
    if (!scanout_publish_prepare(&m->pages,&m->pool,t->owner,&t->space,&m->range,entries,
                                 BDW_BLITTER_GLOBAL_PAGES,&io,&m->publication) ||
        !scanout_publish_apply(&m->pages,&m->publication)) {
        if (!m->pages.exposed && t->device_publication==&m->pages && scanout_transfer_check(t,grant) &&
            scanout_space_release(&t->space,&m->range)) {
            if (!device->reset || intel_bdw_bcs_port_return(device->port_owner,&m->request.port))
                t->device_publication=0;
        }
        return false;
    }
    if (!scanout_transfer_check(t,grant) ||
        !run_request(&m->request,image,device,&m->pages,&m->publication)) return false;
    return m->request.state==BDW_BLITTER_STATUS_PINNED || return_mapping(m);
}

bool intel_bdw_blitter_detach(struct bdw_blitter_mapping *m,const struct intel_bdw_gt_bootstrap *reset,
                              const struct intel_completion_result *post)
{
    if (!m || m->request.state!=BDW_BLITTER_STATUS_PINNED || !intel_bdw_reset_epoch_live(reset,post))
        return false;
    struct bdw_blitter_request *r=&m->request;
    const struct intel_bdw_gt_bootstrap *original=r->device.reset;
    if (!original || original==reset || reset->binding.epoch!=original->binding.epoch+1 ||
        original->binding.owner!=reset->binding.owner || original->binding.provider!=reset->binding.provider ||
        reset->binding.root!=r->device.root || reset->binding.mmio!=r->device.mmio ||
        reset->binding.blitter_port_owner!=r->device.port_owner ||
        reset->source->binding.blitter_port_owner!=r->device.port_owner ||
        reset->begin<=r->last || !r->pages || r->pages!=&m->pages ||
        (m->detach_reset && (reset!=m->detach_reset || post!=m->detach_post))) return false;
    m->detach_reset=reset;
    m->detach_post=post;
    /* The new full reset must have removed both the private HWSP and the
     * active queue. Drain outstanding readers before removing its last pin. */
    if (!readers_done(m) || !window_held(m) || !scanout_publish_unpin_reader(&m->pages,&r->reader))
        return false;
    if (!return_mapping(m) || !intel_bdw_bcs_port_return(r->device.port_owner,&r->port)) {
        r->state=BDW_BLITTER_RETAINED;
        return false;
    }
    r->state=BDW_BLITTER_RETIRED;
    return true;
}
