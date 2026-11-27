#include <srvros/intel_bdw_boot_scanout.h>
#include <stddef.h>

static bool same_binding(const struct bdw_boot_page_binding *a, const struct bdw_boot_page_binding *b)
{
    return a->driver == b->driver && a->epoch == b->epoch && a->root == b->root &&
           a->mmio == b->mmio && a->aperture == b->aperture && a->aperture_bytes == b->aperture_bytes &&
           a->fence_end == b->fence_end && a->display.owner == b->display.owner &&
           a->display.epoch == b->display.epoch && a->index == b->index && a->plane == b->plane &&
           a->surface == b->surface && a->original == b->original && a->backing == b->backing &&
           a->marker[0] == b->marker[0] && a->marker[1] == b->marker[1];
}

static bool live(const struct bdw_boot_page *p)
{
    return p->object == p && p->state == BDW_PAGE_RUNNING && p->source->object == p->source &&
           p->source->consumer == p && p->source->consumed && same_binding(&p->binding,&p->source->binding) &&
           p->source->driver_range == p->driver_range && p->source->scratch == p->scratch &&
           display_access_held(&p->binding.display);
}

static bool interval_held(const struct bdw_boot_page *p)
{
    const struct bdw_boot_page_binding *b = &p->binding;
    return p->interval.lender == p->driver_range &&
           scanout_interval_held(&p->interval,b->mmio,b->aperture,b->aperture_bytes,
                                 b->fence_end,(struct scanout_space_range){b->index,1});
}

static uint64_t page_clock(void *context)
{
    struct bdw_boot_page *p = context;
    if (!live(p)) return 0;
    uint64_t at = p->io.now(p->io.ctx);
    if (!at || at < p->last || (p->begin && at - p->begin > SCANOUT_PUBLISH_DEADLINE_US) ||
        !live(p)) return 0;
    if (!p->begin) p->begin = at;
    p->last = at;
    return at;
}

static bool prove(struct bdw_boot_page *p)
{
    struct bdw_boot_page_proof proof = {0};
    const struct bdw_boot_page_binding *b = &p->binding;
    uint64_t before = page_clock(p);
    if (!before || !p->io.prove(p->io.ctx,p->source,&proof)) return false;
    uint64_t after = page_clock(p);
    return after && proof.at >= before && proof.at <= after && proof.source == p->source &&
           proof.driver == b->driver && proof.epoch == b->epoch && proof.root == b->root &&
           proof.driver_range == p->driver_range && proof.scratch == p->scratch &&
           proof.index == b->index && proof.rights == BDW_PAGE_REQUIRED && proof.plane == b->plane &&
           (proof.programmed & ~8u) == b->surface && proof.live == b->surface;
}

static bool guard(void *context)
{
    struct bdw_boot_page *p = context;
    return interval_held(p) && prove(p) && interval_held(p);
}

static bool read_pte(void *context, uint32_t index, uint64_t *word)
{
    struct bdw_boot_page *p = context;
    return index == p->binding.index && guard(p) && p->io.read_pte(p->io.ctx,p->entry.mmio_offset,word) &&
           *word != UINT64_MAX && guard(p);
}

static bool write_pte(void *context, uint32_t index, uint64_t word)
{
    struct bdw_boot_page *p = context;
    if (index != p->binding.index || !guard(p)) return false;
    uint64_t expected;
    if (!p->stores && p->publication.state == PUB_WRITING && word == p->entry.replacement)
        expected = p->entry.original;
    else if (p->stores == 1 && p->publication.state == PUB_RESTORING && word == p->entry.original)
        expected = p->entry.replacement;
    else return false;
    if (p->io.exchange(p->io.ctx,p->entry.mmio_offset,expected,word) != GT_WRITE_DONE) return false;
    ++p->stores;
    return guard(p);
}

static bool aperture(struct bdw_boot_page *p, uint64_t words[2])
{
    if (!guard(p) || !p->io.read_aperture(p->io.ctx,p->binding.aperture + p->entry.gpu_address,words))
        return false;
    ++p->apertures;
    return guard(p);
}

static bool pair_equal(const uint64_t a[2], const uint64_t b[2])
{
    return a[0] == b[0] && a[1] == b[1];
}

static bool invalidate(void *context)
{
    struct bdw_boot_page *p = context;
    if (p->stores != p->invalidations + 1 || !guard(p) || !p->io.invalidate(p->io.ctx)) return false;
    ++p->invalidations;

    /* Publication must see the actual translation change before it can
     * report success or retire the restored range. */
    uint64_t words[2];
    const uint64_t *expected = p->stores == 1 ? p->binding.marker : p->baseline;
    return aperture(p,words) && pair_equal(words,expected);
}

static bool retained(struct bdw_boot_page *p)
{
    (void)scanout_space_quarantine(&p->space,&p->range);
    p->state = BDW_PAGE_RETAINED;
    return false;
}

bool intel_bdw_boot_page_run(struct bdw_boot_page *p, struct bdw_boot_page_source *source,
                            const struct bdw_boot_page_io *io)
{
    if (!p || p->object || !source || source->object != source || source->consumer || source->consumed || !io ||
        !io->ctx || !io->prove || !io->now || !io->read_pte || !io->exchange || !io->invalidate ||
        !io->read_aperture) return false;
    const struct bdw_boot_page_binding *b = &source->binding;
    if (!b->driver || !b->epoch || !b->root || (b->root & 4095) || b->aperture_bytes != 4096ull * SCANOUT_SPACE_PAGES ||
        !b->aperture || (b->aperture & (b->aperture_bytes - 1)) || b->aperture > UINT64_MAX - b->aperture_bytes ||
        !b->mmio || (b->mmio & 0xffffff) || !b->fence_end || (b->fence_end & 4095) ||
        !(b->plane & (1u << 31)) || (b->plane & (1u << 9)) || b->surface >= b->fence_end || (b->surface & 4095) ||
        !(b->original & 1) || b->marker[0] == b->marker[1] ||
        !scanout_interval_boot_framebuffer_matches(b->mmio,b->aperture,b->aperture_bytes,b->fence_end)) return false;
    const struct scanout_interval *owner = source->driver_range;
    uint64_t scratch = source->scratch;
    if (owner) {
        if (!scratch || (scratch & 4095) || scratch >= 0x100000000ull || b->original != (scratch | 1) ||
            !scanout_interval_held(owner,b->mmio,b->aperture,b->aperture_bytes,b->fence_end,owner->pages)) return false;
    } else if (scratch) return false;
    struct scanout_transfer_manifest m = {.first=b->index,.pages=1,.original=&b->original,
        .aperture_bytes=b->aperture_bytes,.fence_end=b->fence_end};
    struct intel_bdw_ggtt_page entry;
    if (!intel_bdw_ggtt_page_plan(&m,b->backing,&entry) || entry.original == entry.replacement) return false;

    p->object = p;
    p->source = source;
    p->binding = *b;
    p->driver_range = owner;
    p->scratch = scratch;
    p->io = *io;
    p->entry = entry;
    p->state = BDW_PAGE_RUNNING;
    source->consumed = true;
    source->consumer = p;
    b = &p->binding;
    if ((!owner && !prove(p)) || !display_access_pin(&b->display)) goto refuse;
    struct scanout_space_range managed = {b->index,1}, protected = {0,b->fence_end/4096};
    bool claimed = owner ? scanout_interval_borrow_driver(&p->interval,owner,b->driver,managed) :
        scanout_interval_claim(&p->interval,b->mmio,b->aperture,b->aperture_bytes,b->fence_end,managed);
    if (!claimed) {
        if (!display_access_unpin(&b->display)) return retained(p);
        goto refuse;
    }
    /* Establish the first proof after holding the exact child, so a provider
     * cannot release the parent between admission and reservation. */
    if (owner && !guard(p)) {
        if (!interval_held(p) || !display_access_unpin(&b->display)) return retained(p);
        scanout_interval_release(&p->interval);
        goto refuse;
    }

    struct scanout_space_pool spaces = {0};
    struct scanout_publish_pool publications = {0};
    struct scanout_publish_token publication;
    struct scanout_publish_io writer = {p,guard,read_pte,write_pte,invalidate,guard,page_clock};
    if (!scanout_space_open_transferred(&p->space,&spaces,b->display.owner,SCANOUT_SPACE_PAGES,
                                       managed,protected,&b->original) ||
        !scanout_space_reserve(&p->space,b->display.owner,p->space.epoch,1,1,&p->range) ||
        !scanout_publish_prepare(&p->publication,&publications,b->display.owner,&p->space,&p->range,
                                 &p->entry.replacement,1,&writer,&publication) ||
        !aperture(p,p->baseline) || pair_equal(p->baseline,b->marker) ||
        !scanout_publish_apply(&p->publication,&publication) || !scanout_publish_restore(&p->publication,&publication) ||
        !scanout_space_release(&p->space,&p->range) || !scanout_space_close(&p->space,b->display.owner,p->space.epoch) ||
        !display_access_unpin(&b->display)) return retained(p);
    scanout_interval_release(&p->interval);
    source->consumer = NULL;
    p->state = BDW_PAGE_RETURNED;
    return true;

refuse:
    if (source->consumer == p) source->consumer = NULL;
    p->state = BDW_PAGE_REFUSED;
    return false;
}
