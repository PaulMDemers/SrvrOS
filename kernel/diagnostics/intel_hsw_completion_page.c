#include <srvros/intel_hsw_completion_page.h>

#define HSW_COMPLETION_LIMIT_US 4000u

static bool bound(struct hsw_completion_page *p, const struct hsw_completion_io *io) {
    if (!io->valid(io->ctx, p->ticket, &p->binding)) return false;
    uint64_t now = io->now(io->ctx);
    if (now < p->last || now - p->begin > HSW_COMPLETION_LIMIT_US) return false;
    p->last = now;
    return true;
}

static bool observe(struct hsw_completion_page *p, const struct hsw_completion_io *io,
                    uint32_t pte, uint32_t pga) {
    struct hsw_completion_sample sample;
    return bound(p, io) && io->read(io->ctx, &p->binding, &sample) && bound(p, io) &&
           sample.pte == pte && sample.pga == pga && sample.hwstam == UINT32_MAX && sample.imr == UINT32_MAX;
}

static bool replace_pte(struct hsw_completion_page *p, const struct hsw_completion_io *io, uint32_t pte) {
    if (!bound(p, io)) return false;
    p->pte_attempts++;
    return io->write_pte(io->ctx, &p->binding, pte) && bound(p, io) &&
           io->translation_visible(io->ctx, &p->binding, pte) && observe(p, io, pte, 0);
}

static bool replace_pga(struct hsw_completion_page *p, const struct hsw_completion_io *io, uint32_t pga) {
    if (!bound(p, io)) return false;
    p->pga_attempts++;
    return io->write_pga(io->ctx, &p->binding, pga) && observe(p, io, p->page.replacement, pga);
}

bool intel_hsw_completion_page_run(struct hsw_completion_page *p, const struct hwb_result *backing,
                                   const struct hsw_completion_binding *binding,
                                   const struct hsw_completion_io *io, uint64_t ticket) {
    if (!p || p->state != HSW_COMPLETION_NEW) return false;
    p->state = HSW_COMPLETION_REFUSED;
    p->reason = "firmware-exclusion";
    if (!ticket || !binding || !io || !io->valid || !io->now || !io->read || !io->zero_and_verify ||
        !io->write_pte || !io->write_pga || !io->translation_visible || !io->readers_retired) return false;

    p->binding = *binding;
    p->ticket = ticket;
    p->begin = p->last = io->now(io->ctx);
    if (!p->binding.boot_epoch || !p->begin || !bound(p, io)) return false;

    p->reason = "binding";
    if (p->binding.gpu_offset == HSW_CANDIDATE_GPU || p->binding.gpu_offset == HSW_BOOT_SLOT_GPU ||
        !intel_hsw_ggtt_plan_page(backing, p->binding.gpu_offset, 0, p->binding.dma_address, &p->page) ||
        !observe(p, io, 0, 0)) return false;

    p->reason = "zero-page";
    p->zero_attempts++;
    if (!io->zero_and_verify(io->ctx, &p->binding) || !observe(p, io, 0, 0)) return false;

    p->state = HSW_COMPLETION_RETAINED;
    p->reason = "publish";
    if (!replace_pte(p, io, p->page.replacement) || !replace_pga(p, io, p->binding.gpu_offset)) return false;

    p->reason = "detach";
    if (!replace_pga(p, io, 0) || !io->readers_retired(io->ctx, &p->binding) ||
        !observe(p, io, p->page.replacement, 0)) return false;

    p->reason = "restore-pte";
    if (!replace_pte(p, io, p->page.original)) return false;
    p->state = HSW_COMPLETION_RETURNED;
    p->reason = "none";
    return true;
}
