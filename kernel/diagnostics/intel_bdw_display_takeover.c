#include <srvros/intel_bdw_display_takeover.h>
#include <srvros/intel_dma_observe.h>

struct takeover_attempt {
    struct bdw_display_takeover *result;
    const struct bdw_display_takeover_io *io;
    struct display_access_token display;
};

static bool overlaps_backing(const struct bdw_edp_source_image *image, uint64_t frame)
{
    return frame >= image->backing_physical && frame - image->backing_physical < image->mapped_bytes;
}

static bool reserve_range(struct takeover_attempt *a, const struct bdw_edp_source_image *image,
                          const struct bdw_flr_pci_image *pci, const struct intel_vtd_drain_binding *drain)
{
    struct bdw_display_takeover *t = a->result;
    const struct bdw_edp_source_io *d = &a->io->display;
    struct bdw_display_takeover_grant grant = {0};
    if (!a->io->range(d->context,image,&t->reset,&grant) || !display_access_held(&a->display) ||
        !d->held(d->context,image)) return false;
    const struct bdw_boot_page_binding *b = &grant.binding;
    if (b->driver != drain->owner || b->root != drain->root || b->epoch <= drain->epoch ||
        b->mmio != (pci->bar[0] & ~15u) || b->aperture != (pci->bar[2] & ~15u) ||
        b->display.owner != a->display.owner || b->display.epoch != a->display.epoch || b->surface ||
        b->original || b->backing != grant.scratch || !intel_dma_ordinary_low_page(grant.scratch) ||
        !intel_dma_ordinary_low_page(grant.private_backing) || grant.private_backing == grant.scratch ||
        overlaps_backing(image,grant.scratch) || overlaps_backing(image,grant.private_backing) ||
        b->marker[0] == b->marker[1] || grant.private_marker[0] == grant.private_marker[1] ||
        (grant.private_marker[0] == b->marker[0] && grant.private_marker[1] == b->marker[1]))
        return false;

    struct scanout_interval *parent = &t->driver_range;
    struct scanout_space_range slot = {b->index, 1};
    struct scanout_transfer_manifest manifest = {.first=b->index,.pages=1,.original=&b->original,
        .aperture_bytes=b->aperture_bytes,.fence_end=b->fence_end};
    struct intel_bdw_ggtt_page page;
    if (!intel_bdw_ggtt_page_plan(&manifest,grant.scratch,&page) ||
        !scanout_interval_claim_driver(parent,b->driver,b->mmio,b->aperture,b->aperture_bytes,b->fence_end,slot) ||
        !scanout_interval_borrow_driver(&t->recovery_loan,parent,b->driver,slot)) return false;

    uint64_t pte;
    if (!a->io->page.read_pte(a->io->page.ctx,page.mmio_offset,&pte) || pte) return false;

    t->seed_source = (struct bdw_boot_page_source){.object=&t->seed_source,.binding=*b,
        .driver_range=parent,.scratch=grant.scratch};
    t->private_source = (struct bdw_boot_page_source){.object=&t->private_source,.binding=*b,
        .driver_range=parent,.scratch=grant.scratch};
    t->private_source.binding.original = grant.scratch | 1;
    t->private_source.binding.backing = grant.private_backing;
    t->private_source.binding.marker[0] = grant.private_marker[0];
    t->private_source.binding.marker[1] = grant.private_marker[1];
    return true;
}

static enum bdw_edp_dark_result take_while_dark(void *opaque, const struct bdw_edp_source_image *image)
{
    struct takeover_attempt *a = opaque;
    struct bdw_display_takeover *t = a->result;
    const struct bdw_edp_source_io *d = &a->io->display;
    struct bdw_display_takeover_reset reset = {0};
    if (!display_access_held(&a->display) || !d->held(d->context,image)) return BDW_EDP_DARK_REFUSED;
    bool prepared = a->io->prepare_reset(d->context,image,&t->reset,&reset);
    if (!display_access_held(&a->display) || !d->held(d->context,image)) return BDW_EDP_DARK_REFUSED;
    if (!prepared) return BDW_EDP_DARK_UNCHANGED;
    const struct bdw_flr_pci_image pci = reset.pci;
    if (!intel_bdw_flr(&reset.io,&pci,reset.drain,&t->reset)) {
        const struct bdw_flr_report *r = &t->reset;
        return r->retained || r->block_issued || r->reset_issued || r->stores ?
            BDW_EDP_DARK_REFUSED : BDW_EDP_DARK_UNCHANGED;
    }
    const struct intel_vtd_drain_binding drained = reset.drain->result->binding;
    if (!display_access_held(&a->display) || !a->io->recover(d->context,image,&t->reset) ||
        !display_access_held(&a->display) || !d->held(d->context,image))
        return BDW_EDP_DARK_REFUSED;
    (void)reserve_range(a,image,&pci,&drained);
    return display_access_held(&a->display) && d->held(d->context,image) ?
        BDW_EDP_DARK_RESET_RECOVERED : BDW_EDP_DARK_REFUSED;
}

static bool finish(struct takeover_attempt *a, enum bdw_display_takeover_state state)
{
    if (state != BDW_TAKEOVER_RETAINED) {
        if (display_access_unpin(&a->display)) {
            scanout_interval_release(&a->result->recovery_loan);
            scanout_interval_release(&a->result->driver_range);
        } else
            state = BDW_TAKEOVER_RETAINED;
    }
    a->result->state = state;
    return state == BDW_TAKEOVER_READY;
}

bool intel_bdw_display_takeover(struct bdw_display_takeover *t, const struct display_access_token *display,
                                const struct bdw_display_takeover_io *io,
                                const struct bdw_edp_source_image *image, const struct bdw_edp_baseline_report *reference)
{
    if (!t || t->object || !image || !image->backing_physical || (image->backing_physical & 4095) ||
        image->mapped_bytes > UINT64_MAX - image->backing_physical ||
        !io || !io->prepare_reset || !io->recover || !io->range || !io->display.held ||
        !io->page.ctx || !io->page.prove || !io->page.now || !io->page.read_pte ||
        !io->page.exchange || !io->page.invalidate || !io->page.read_aperture ||
        !display_access_pin(display)) return false;
    t->object = t;
    t->state = BDW_TAKEOVER_RUNNING;
    struct takeover_attempt a = {t,io,*display};
    if (!intel_bdw_edp_source_cycle(&io->display,image,reference,take_while_dark,&a,&t->cycle))
        return finish(&a,t->cycle.retained ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);
    const struct bdw_boot_page_binding *b = &t->seed_source.binding;
    if (t->cycle.dark_result != BDW_EDP_DARK_RESET_RECOVERED || !t->seed_source.object)
        return finish(&a,BDW_TAKEOVER_REFUSED);
    if (!scanout_interval_held(&t->recovery_loan,b->mmio,b->aperture,b->aperture_bytes,b->fence_end,
                               (struct scanout_space_range){b->index,1}))
        return finish(&a,BDW_TAKEOVER_RETAINED);
    /* Keep the same loan across both transactions and all provider callbacks. */
    if (!intel_bdw_boot_page_in_loan(&t->seed,&t->seed_source,&io->page,&t->recovery_loan,true))
        return finish(&a,t->seed.state == BDW_PAGE_RETAINED ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);
    if (!intel_bdw_boot_page_in_loan(&t->page,&t->private_source,&io->page,&t->recovery_loan,false))
        return finish(&a,t->page.state == BDW_PAGE_RETAINED ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);
    return finish(&a,BDW_TAKEOVER_READY);
}
