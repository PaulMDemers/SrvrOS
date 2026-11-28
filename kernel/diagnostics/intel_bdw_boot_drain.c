#include <srvros/intel_bdw_boot_drain.h>
#include <srvros/intel_bdw_traffic.h>
#include <srvros/intel_dma_observe.h>
#include <stddef.h>

static bool interval_held(const struct bdw_boot_drain *d)
{
    const struct bdw_boot_page_binding *b = &d->binding;
    const struct scanout_interval *lender = d->driver_range ? d->driver_range : d->firmware_range;
    return d->object == d && display_access_held(&b->display) &&
           d->interval.lender == lender && !d->interval.firmware &&
           d->interval.driver == (d->driver_range ? b->driver : NULL) &&
           scanout_interval_held(&d->interval, b->mmio, b->aperture, b->aperture_bytes,
                                 b->fence_end, (struct scanout_space_range){b->index, 1}) &&
           (!lender || (!lender->lender && lender->firmware == (lender == d->firmware_range) &&
             lender->driver == (d->driver_range ? b->driver : NULL) &&
             scanout_interval_held(lender, b->mmio, b->aperture, b->aperture_bytes, b->fence_end, d->lender_pages)));
}

static bool tick(struct bdw_boot_drain *d)
{
    if (!interval_held(d)) return false;
    uint64_t at = d->io.now(d->io.ctx);
    uint64_t phase = d->state == BDW_DRAIN_RELEASING ? d->release_begin : d->begin;
    if (!at || at < d->last || (phase && at - phase > BDW_TRAFFIC_BLOCK_US) || !interval_held(d)) return false;
    if (!d->begin) d->begin = at;
    d->last = at;
    return true;
}

static bool authorized(struct bdw_boot_drain *d)
{
    if (!d->io.parent(d->io.ctx, &d->binding)) return false;
    uint64_t active = d->io.held(d->io.ctx);
    return active == (d->state == BDW_DRAIN_WAITING ? d->ticket : 0) && interval_held(d);
}

static bool sample_guard(struct bdw_boot_drain *d)
{
    return tick(d) && authorized(d) && tick(d) && authorized(d);
}

static bool sample(void *ctx, uint32_t reg, uint32_t *word)
{
    struct bdw_boot_drain *d = ctx;
    return sample_guard(d) && d->io.read(d->io.ctx, reg, word) && *word != UINT32_MAX && sample_guard(d);
}

static bool originals_live(struct bdw_boot_drain *d, uint64_t expected)
{
    const struct bdw_boot_page_binding *b = &d->binding;
    uint32_t control, programmed, live;
    uint64_t pte;
    return sample(d, 0x70180, &control) && sample(d, 0x7019c, &programmed) && sample(d, 0x701ac, &live) &&
           control == b->plane && (programmed & ~8u) == b->surface && live == b->surface &&
           d->io.read_pte(d->io.ctx, b->index, &pte) && pte == expected && sample_guard(d);
}

static bool wait_for_drain(struct bdw_boot_drain *d)
{
    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    for (unsigned n = 0; n < GT_ACCESS_POLL_CAP; ++n) {
        bool drained;
        ++d->polls;
        if (!intel_bdw_traffic_poll(&traffic, &drained)) return false;
        d->engine_acks = traffic.engine_ack;
        d->l3_ack = traffic.l3_ack;
        if (drained)
            return originals_live(d,d->binding.original) && intel_bdw_traffic_still_blocked(d, sample) &&
                   d->last > d->requested_at;
        d->io.pause(d->io.ctx);
    }
    return false;
}

static bool wait_for_return(struct bdw_boot_drain *d, uint64_t expected)
{
    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    for (unsigned n = 0; n < GT_ACCESS_POLL_CAP; ++n) {
        bool reopened;
        ++d->return_polls;
        if (!bdw_traffic_reopened(&traffic, &reopened)) return false;
        if (reopened) return originals_live(d,expected) && intel_bdw_traffic_open(&traffic) &&
                             d->last > d->release_begin;
        d->io.pause(d->io.ctx);
    }
    return false;
}

static bool finish(struct bdw_boot_drain *d, unsigned state)
{
    if (state != BDW_DRAIN_RETAINED) {
        if (display_access_unpin(&d->binding.display)) scanout_interval_release(&d->interval);
        else state = BDW_DRAIN_RETAINED;
    }
    d->state = state;
    return state == BDW_DRAIN_RETURNED;
}

static bool held_for_page(struct bdw_boot_bind *b)
{
    struct bdw_boot_drain *d = &b->hold;
    return d->state == BDW_DRAIN_WAITING && d->engine_acks == 31u && d->l3_ack &&
           intel_bdw_traffic_still_blocked(d,sample);
}

static bool page_proof(void *context, const struct bdw_boot_page_source *source, struct bdw_boot_page_proof *proof)
{
    struct bdw_boot_bind *b = context;
    return held_for_page(b) && b->page_io.prove(b->page_io.ctx,source,proof) && held_for_page(b);
}

static uint64_t page_now(void *context)
{
    struct bdw_boot_bind *b = context;
    return sample_guard(&b->hold) ? b->hold.last : 0;
}

static bool page_pte(void *context, uint32_t offset, uint64_t *word)
{
    struct bdw_boot_bind *b = context;
    return b->page_io.read_pte(b->page_io.ctx,offset,word);
}

static enum intel_gt_write page_exchange(void *context, uint32_t offset, uint64_t before, uint64_t after)
{
    struct bdw_boot_bind *b = context;
    return held_for_page(b) ? b->page_io.exchange(b->page_io.ctx,offset,before,after) : GT_WRITE_NONE;
}

static bool page_invalidate(void *context)
{
    struct bdw_boot_bind *b = context;
    return held_for_page(b) && b->page_io.invalidate(b->page_io.ctx);
}

static bool page_aperture(void *context, uint64_t address, uint64_t words[2])
{
    struct bdw_boot_bind *b = context;
    return b->page_io.read_aperture(b->page_io.ctx,address,words);
}

static bool bind_pages(struct bdw_boot_bind *b)
{
    const struct bdw_boot_page_io io = {b,page_proof,page_now,page_pte,page_exchange,page_invalidate,page_aperture};
    const struct scanout_interval *loan = &b->hold.interval;
    bool needs_seed = b->hold.driver_range && !b->private_source.inherited_backing;
    return (!needs_seed || intel_bdw_boot_page_in_loan(&b->seed,&b->scratch_source,&io,loan,true)) &&
           intel_bdw_boot_page_in_loan(&b->page,&b->private_source,&io,loan,false);
}

static bool run(struct bdw_boot_drain *d, const struct bdw_boot_page_binding *b,
                const struct scanout_interval *driver_range, const struct bdw_boot_drain_io *io,
                struct bdw_boot_bind *bind, const struct scanout_interval *firmware_range)
{
    bool needs_empty = driver_range && (!bind || !bind->private_source.inherited_backing);
    if (!d || d->object || !b || !b->driver || !b->epoch || !b->root || (b->root & 4095u) ||
        !(b->plane & (1u << 31)) || (b->plane & (1u << 9)) || (b->surface & 4095u) || b->surface >= b->fence_end ||
        (needs_empty ? b->original != 0 : !(b->original & 1u) || b->original == UINT64_MAX) ||
        !io || !io->ctx || !io->parent || !io->now ||
        !io->pause || !io->read || !io->read_pte || !io->request || !io->held || !io->release ||
        !scanout_interval_boot_framebuffer_matches(b->mmio, b->aperture, b->aperture_bytes, b->fence_end)) return false;
    d->object = d;
    d->binding = *b;
    d->io = *io;
    d->state = BDW_DRAIN_ADMITTING;
    if (!display_access_pin(&b->display)) {
        d->state = BDW_DRAIN_REFUSED;
        return false;
    }
    const struct scanout_space_range page = {b->index, 1};
    bool claimed = driver_range ?
        scanout_interval_borrow_driver(&d->interval,driver_range,b->driver,page) :
        firmware_range ? scanout_interval_borrow(&d->interval,firmware_range,b->mmio,b->aperture,
                                                 b->aperture_bytes,b->fence_end,page) :
        scanout_interval_claim(&d->interval,b->mmio,b->aperture,b->aperture_bytes,b->fence_end,page);
    if (!claimed) return finish(d, BDW_DRAIN_REFUSED);
    d->driver_range = driver_range;
    d->firmware_range = firmware_range;
    if (d->interval.lender) d->lender_pages = d->interval.lender->pages;

    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    uint64_t expected = d->binding.original;
    if (!originals_live(d,expected) || !intel_bdw_traffic_open(&traffic)) return finish(d, BDW_DRAIN_REFUSED);
    d->requested_at = d->last;
    enum intel_gt_write result = d->io.request(d->io.ctx, &d->ticket);
    if (result == GT_WRITE_NONE && !d->ticket && originals_live(d,expected) && intel_bdw_traffic_open(&traffic))
        return finish(d, BDW_DRAIN_REFUSED);
    if (result != GT_WRITE_DONE || !d->ticket) return finish(d, BDW_DRAIN_RETAINED);
    d->state = BDW_DRAIN_WAITING;
    if (!wait_for_drain(d)) return finish(d, BDW_DRAIN_RETAINED);
    if (bind) {
        if (!bind_pages(bind)) return finish(d, BDW_DRAIN_RETAINED);
        if (bind->seed.state == BDW_PAGE_SEEDED) expected = bind->seed.entry.replacement;
        if (!originals_live(d,expected) || !held_for_page(bind)) return finish(d, BDW_DRAIN_RETAINED);
    }
    d->release_begin = d->last;
    if (d->io.release(d->io.ctx, d->ticket) != GT_WRITE_DONE) return finish(d, BDW_DRAIN_RETAINED);
    d->state = BDW_DRAIN_RELEASING;
    return finish(d, wait_for_return(d,expected) ? BDW_DRAIN_RETURNED : BDW_DRAIN_RETAINED);
}

bool intel_bdw_boot_drain_run(struct bdw_boot_drain *d, const struct bdw_boot_page_binding *b,
                             const struct bdw_boot_drain_io *io)
{
    return run(d,b,NULL,io,NULL,NULL);
}

bool intel_bdw_boot_drain_run_driver(struct bdw_boot_drain *d, const struct bdw_boot_page_binding *b,
                                    const struct scanout_interval *driver_range, const struct bdw_boot_drain_io *io)
{
    return driver_range && run(d,b,driver_range,io,NULL,NULL);
}

bool intel_bdw_boot_drain_run_occupied(struct bdw_boot_drain *d, const struct bdw_boot_page_binding *b,
                                      const struct scanout_interval *firmware_range, const struct bdw_boot_drain_io *io)
{
    return firmware_range && run(d,b,NULL,io,NULL,firmware_range);
}

bool intel_bdw_boot_bind(struct bdw_boot_bind *t, const struct bdw_boot_page_binding *b,
                         struct scanout_interval *range, uint64_t private_frame, const uint64_t private_marker[2],
                         const struct bdw_boot_drain_io *io, const struct bdw_boot_page_io *page_io)
{
    if (!t || t->hold.object || t->seed.object || t->page.object || !b || b->original || !range ||
        !intel_dma_ordinary_low_page(b->backing) || !intel_dma_ordinary_low_page(private_frame) ||
        private_frame == b->backing ||
        !private_marker || private_marker[0] == private_marker[1] || b->marker[0] == b->marker[1] ||
        (private_marker[0] == b->marker[0] && private_marker[1] == b->marker[1]) ||
        !page_io || !page_io->ctx || !page_io->prove || !page_io->read_pte || !page_io->exchange ||
        !page_io->invalidate || !page_io->read_aperture) return false;
    t->page_io = *page_io;
    t->scratch_source = (struct bdw_boot_page_source){.object=&t->scratch_source,.binding=*b,
        .driver_range=range,.scratch=b->backing};
    t->private_source = (struct bdw_boot_page_source){.object=&t->private_source,.binding=*b,
        .driver_range=range,.scratch=b->backing};
    t->private_source.binding.original = b->backing | 1u;
    t->private_source.binding.backing = private_frame;
    t->private_source.binding.marker[0] = private_marker[0];
    t->private_source.binding.marker[1] = private_marker[1];
    return run(&t->hold,b,range,io,t,NULL);
}

static bool bind_occupied(struct bdw_boot_bind *t, const struct bdw_boot_page_binding *b,
                          struct scanout_interval *driver_range, uint64_t framebuffer_first,
                          const struct scanout_interval *firmware_range,
                          const struct bdw_boot_drain_io *io, const struct bdw_boot_page_io *page_io)
{
    if (!t || t->hold.object || t->seed.object || t->page.object || !b || (!driver_range && !firmware_range) ||
        !intel_dma_ordinary_low_page(b->backing) || !(b->original & 1u) ||
        b->original < 4096 || b->original >= 0x100000000ull ||
        b->backing == (b->original & ~4095ull) ||
        (driver_range && (!framebuffer_first || (framebuffer_first & 4095u) ||
                          b->original != (framebuffer_first | 1u))) ||
        b->marker[0] == b->marker[1] || !page_io || !page_io->ctx || !page_io->prove ||
        !page_io->read_pte || !page_io->exchange || !page_io->invalidate || !page_io->read_aperture)
        return false;
    t->page_io = *page_io;
    t->private_source = (struct bdw_boot_page_source){.object=&t->private_source,.binding=*b,
        .driver_range=driver_range,.firmware_range=firmware_range,.inherited_backing=framebuffer_first};
    return run(&t->hold,b,driver_range,io,t,firmware_range);
}

bool intel_bdw_boot_bind_inherited(struct bdw_boot_bind *t, const struct bdw_boot_page_binding *b,
                                   struct scanout_interval *driver_range, uint64_t framebuffer_first,
                                   const struct bdw_boot_drain_io *io, const struct bdw_boot_page_io *page_io)
{
    return bind_occupied(t,b,driver_range,framebuffer_first,NULL,io,page_io);
}

bool intel_bdw_boot_bind_occupied(struct bdw_boot_bind *t, const struct bdw_boot_page_binding *b,
                                  const struct scanout_interval *firmware_range,
                                  const struct bdw_boot_drain_io *io, const struct bdw_boot_page_io *page_io)
{
    return bind_occupied(t,b,NULL,0,firmware_range,io,page_io);
}
