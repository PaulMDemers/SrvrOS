#include <srvros/intel_bdw_display_takeover.h>

struct takeover_attempt {
    struct bdw_display_takeover *result;
    const struct bdw_display_takeover_io *io;
    struct display_access_token display;
};

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
    if (!intel_bdw_flr(&reset.io,&reset.pci,reset.drain,&t->reset)) {
        const struct bdw_flr_report *r = &t->reset;
        return r->retained || r->block_issued || r->reset_issued || r->stores ?
            BDW_EDP_DARK_REFUSED : BDW_EDP_DARK_UNCHANGED;
    }
    const void *owner = reset.drain->source->binding.owner;
    uint64_t root = reset.drain->source->binding.root, epoch = reset.drain->source->binding.epoch;
    if (!display_access_held(&a->display) || !a->io->recover(d->context,image,&t->reset) ||
        !display_access_held(&a->display) || !d->held(d->context,image))
        return BDW_EDP_DARK_REFUSED;
    const struct bdw_boot_page_binding *page = &t->range.seed.binding;
    t->granted = a->io->range(d->context,image,&t->range) && page->driver == owner &&
                 page->root == root && page->epoch > epoch;
    return display_access_held(&a->display) ? BDW_EDP_DARK_RESET_RECOVERED : BDW_EDP_DARK_REFUSED;
}

static bool usable_range(const struct bdw_display_takeover_range *r, const struct display_access_token *d)
{
    const struct bdw_boot_page_source *seed = &r->seed;
    return seed->binding.display.owner == d->owner && seed->binding.display.epoch == d->epoch &&
           seed->binding.surface == 0 && seed->driver_range &&
           r->private_backing && !(r->private_backing & 4095u) && r->private_backing < 0x100000000ull &&
           r->private_backing != seed->scratch && r->private_marker[0] != r->private_marker[1] &&
           (r->private_marker[0] != seed->binding.marker[0] || r->private_marker[1] != seed->binding.marker[1]);
}

static bool finish(struct takeover_attempt *a, enum bdw_display_takeover_state state)
{
    if (state != BDW_TAKEOVER_RETAINED && !display_access_unpin(&a->display))
        state = BDW_TAKEOVER_RETAINED;
    a->result->state = state;
    return state == BDW_TAKEOVER_READY;
}

bool intel_bdw_display_takeover(struct bdw_display_takeover *t, const struct display_access_token *display,
                                const struct bdw_display_takeover_io *io,
                                const struct bdw_edp_source_image *image, const struct bdw_edp_baseline_report *reference)
{
    if (!t || t->object || !io || !io->prepare_reset || !io->recover || !io->range || !io->display.held ||
        !io->page.ctx || !io->page.prove || !io->page.now || !io->page.read_pte ||
        !io->page.exchange || !io->page.invalidate || !io->page.read_aperture ||
        !display_access_pin(display)) return false;
    t->object = t;
    t->state = BDW_TAKEOVER_RUNNING;
    struct takeover_attempt a = {t,io,*display};
    if (!intel_bdw_edp_source_cycle(&io->display,image,reference,take_while_dark,&a,&t->cycle))
        return finish(&a,t->cycle.retained ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);
    if (t->cycle.dark_result != BDW_EDP_DARK_RESET_RECOVERED || !t->granted ||
        !usable_range(&t->range,&a.display)) return finish(&a,BDW_TAKEOVER_REFUSED);
    if (!intel_bdw_boot_page_seed(&t->seed,&t->range.seed,&io->page))
        return finish(&a,t->seed.state == BDW_PAGE_RETAINED ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);

    struct bdw_boot_page_source *private = &t->private_source;
    *private = (struct bdw_boot_page_source){.object=private,.binding=t->seed.binding,
        .driver_range=t->range.seed.driver_range,.scratch=t->seed.scratch};
    private->binding.original = t->seed.entry.replacement;
    private->binding.backing = t->range.private_backing;
    private->binding.marker[0] = t->range.private_marker[0];
    private->binding.marker[1] = t->range.private_marker[1];
    if (!intel_bdw_boot_page_run(&t->page,private,&io->page))
        return finish(&a,t->page.state == BDW_PAGE_RETAINED ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED);
    return finish(&a,BDW_TAKEOVER_READY);
}
