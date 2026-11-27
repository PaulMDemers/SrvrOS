#include <srvros/intel_bdw_boot_drain.h>
#include <srvros/intel_bdw_traffic.h>

static bool interval_held(const struct bdw_boot_drain *d)
{
    const struct bdw_boot_page_binding *b = &d->binding;
    return d->object == d && display_access_held(&b->display) &&
           scanout_interval_held(&d->interval, b->mmio, b->aperture, b->aperture_bytes,
                                 b->fence_end, (struct scanout_space_range){b->index, 1});
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

static bool originals_live(struct bdw_boot_drain *d)
{
    const struct bdw_boot_page_binding *b = &d->binding;
    uint32_t control, programmed, live;
    uint64_t pte;
    return sample(d, 0x70180, &control) && sample(d, 0x7019c, &programmed) && sample(d, 0x701ac, &live) &&
           control == b->plane && (programmed & ~8u) == b->surface && live == b->surface &&
           d->io.read_pte(d->io.ctx, b->index, &pte) && pte == b->original && sample_guard(d);
}

static bool wait_for_drain(struct bdw_boot_drain *d)
{
    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    for (unsigned n = 0; n < GT_ACCESS_POLL_CAP; ++n) {
        bool drained;
        ++d->polls;
        if (!intel_bdw_traffic_poll(&traffic, &drained)) return false;
        if (drained)
            return originals_live(d) && intel_bdw_traffic_still_blocked(d, sample) && d->last > d->begin;
        d->io.pause(d->io.ctx);
    }
    return false;
}

static bool wait_for_return(struct bdw_boot_drain *d)
{
    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    for (unsigned n = 0; n < GT_ACCESS_POLL_CAP; ++n) {
        bool reopened;
        ++d->return_polls;
        if (!bdw_traffic_reopened(&traffic, &reopened)) return false;
        if (reopened) return originals_live(d) && intel_bdw_traffic_open(&traffic) && d->last > d->release_begin;
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

bool intel_bdw_boot_drain_run(struct bdw_boot_drain *d, const struct bdw_boot_page_binding *b,
                             const struct bdw_boot_drain_io *io)
{
    if (!d || d->object || !b || !b->driver || !b->epoch || !b->root || (b->root & 4095u) ||
        !(b->plane & (1u << 31)) || (b->plane & (1u << 9)) || (b->surface & 4095u) || b->surface >= b->fence_end ||
        !(b->original & 1u) || b->original == UINT64_MAX || !io || !io->ctx || !io->parent || !io->now ||
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
    if (!scanout_interval_claim(&d->interval, b->mmio, b->aperture, b->aperture_bytes, b->fence_end,
                                (struct scanout_space_range){b->index, 1})) return finish(d, BDW_DRAIN_REFUSED);

    struct bdw_traffic_sample traffic = {.ctx=d, .read=sample};
    if (!originals_live(d) || !intel_bdw_traffic_open(&traffic)) return finish(d, BDW_DRAIN_REFUSED);
    enum intel_gt_write result = d->io.request(d->io.ctx, &d->ticket);
    if (result == GT_WRITE_NONE && !d->ticket && originals_live(d) && intel_bdw_traffic_open(&traffic))
        return finish(d, BDW_DRAIN_REFUSED);
    if (result != GT_WRITE_DONE || !d->ticket) return finish(d, BDW_DRAIN_RETAINED);
    d->state = BDW_DRAIN_WAITING;
    if (!wait_for_drain(d)) return finish(d, BDW_DRAIN_RETAINED);
    d->release_begin = d->last;
    if (d->io.release(d->io.ctx, d->ticket) != GT_WRITE_DONE) return finish(d, BDW_DRAIN_RETAINED);
    d->state = BDW_DRAIN_RELEASING;
    return finish(d, wait_for_return(d) ? BDW_DRAIN_RETURNED : BDW_DRAIN_RETAINED);
}
