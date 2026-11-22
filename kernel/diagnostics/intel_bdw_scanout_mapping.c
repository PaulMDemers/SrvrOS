#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_bdw_stolen_scanout.h>
#include <srvros/intel_ggtt_window.h>

static bool page_store_active(const struct scanout_memory *m, const struct scanout_memory_token *token,
                         const struct scanout_transfer *t, uint32_t index)
{
    if (!m || !token || !t ||
        m->object != m || token->object != m || token->epoch != m->epoch || token->owner != m->owner ||
        !m->busy || m->owner != t->owner || m->backing_pages != 1 || m->layout.pages != 1 ||
        m->layout.tiled || m->cpu_serial || m->dependent || m->registered || m->hold_serial ||
        m->space != &t->space || m->range.first != index || m->range.pages != 1 ||
        t->manifest.first != index ||
        t->manifest.assurances != HANDOFF_ALL || t->manifest.obligations) return false;

    const struct scanout_transfer_manifest *s = &t->manifest;
    if (!scanout_interval_boot_framebuffer_matches(s->mmio,s->aperture,s->aperture_bytes,s->fence_end))
        return false;

    const struct scanout_publication *p = &m->publication;
    bool restore = p->state == PUB_RESTORING;
    if (!p->busy || !p->exposed || p->object != p || p->pages != 1 || p->space != m->space ||
        p->owner != m->owner || p->io.ctx != t->io.ctx || p->io.now != t->io.now ||
        p->range.object != m->space || p->range.epoch != t->arena_epoch || p->range.first != index ||
        m->range.object != p->range.object || m->range.epoch != p->range.epoch ||
        p->range.pages != 1 || p->range.serial != m->range.serial || p->range.slot != m->range.slot ||
        p->range.slot >= SCANOUT_SPACE_LEASES || m->space->leases[p->range.slot].use != SPACE_EXPOSED ||
        (restore ? (m->state != MEM_GPU || !m->closing || p->writes != 2) :
                   (p->state != PUB_WRITING || m->state != MEM_PREPARED || m->closing || p->writes != 1)))
        return false;

    return true;
}

bool intel_bdw_scanout_page_store_active(const struct scanout_memory *m, const struct scanout_memory_token *token,
                                        const struct scanout_transfer *t, uint32_t index)
{
    return index == INTEL_WINDOW_FIRST && t && t->manifest.fence_end == (uint64_t)index * 4096 &&
           page_store_active(m, token, t, index);
}

static bool owned_page_range(const struct scanout_transfer_manifest *s,
                             const struct scanout_interval *firmware, const struct bdw_stolen_layout *layout)
{
    uint32_t end = intel_bdw_stolen_import_end(layout, s->fence_end, s->aperture_bytes);
    if (!end || !firmware || !firmware->firmware || s->pages != 1 || s->first < end ||
        firmware->pages.first != INTEL_WINDOW_FIRST || (uint64_t)INTEL_WINDOW_FIRST * 4096 != s->fence_end ||
        end != (uint64_t)firmware->pages.first + firmware->pages.pages)
        return false;
    return scanout_interval_held(firmware, s->mmio, s->aperture, s->aperture_bytes,
                                 s->fence_end, firmware->pages);
}

bool intel_bdw_scanout_owned_page_store_active(const struct scanout_memory *m,
        const struct scanout_memory_token *token, const struct scanout_transfer *t,
        const struct scanout_interval *firmware, const struct bdw_stolen_layout *layout, uint32_t index)
{
    return t && owned_page_range(&t->manifest, firmware, layout) && page_store_active(m, token, t, index);
}

bool intel_bdw_scanout_owned_page_store_matches(const struct scanout_memory *m,
        const struct scanout_memory_token *token, struct scanout_transfer *t,
        const struct scanout_transfer_token *grant, const struct scanout_interval *firmware,
        const struct bdw_stolen_layout *layout, uint32_t index, uint64_t expected, uint64_t value)
{
    if (!intel_bdw_scanout_owned_page_store_active(m, token, t, firmware, layout, index) ||
        !scanout_transfer_check(t, grant)) return false;
    uint64_t original;
    struct intel_bdw_ggtt_page plan;
    if (!intel_bdw_scanout_owned_page_store_active(m, token, t, firmware, layout, index) ||
        !scanout_transfer_original(t, grant, index, &original) ||
        !intel_bdw_ggtt_page_plan(&t->manifest, m->physical, &plan) || plan.original != original ||
        !scanout_space_verify_original(m->space, &m->range, 0, original) ||
        m->publication.before[0] != original || m->publication.after[0] != plan.replacement ||
        original == plan.replacement || t->last_us < m->publication.last_us ||
        t->last_us < m->publication.phase_us ||
        t->last_us - m->publication.phase_us > SCANOUT_PUBLISH_DEADLINE_US) return false;
    bool restore = m->publication.state == PUB_RESTORING;
    return expected == (restore ? plan.replacement : original) &&
           value == (restore ? original : plan.replacement);
}

struct encode_context {
    struct scanout_transfer *transfer;
    struct scanout_transfer_token token;
    struct scanout_memory *memory;
    bool single_page;
};

static bool encode(void *ctx, uint64_t physical, uint64_t *replacement)
{
    const struct encode_context *encoder = ctx;
    const struct scanout_memory *memory = encoder->memory;
    if (physical < memory->physical || (physical - memory->physical) % 4096)
        return false;
    uint64_t offset = (physical - memory->physical) / 4096;
    if (offset >= memory->range.pages)
        return false;

    uint32_t index = memory->range.first + (uint32_t)offset;
    if (encoder->single_page && index != encoder->transfer->manifest.first) return false;
    struct intel_bdw_ggtt_page entry;
    uint64_t original;
    if (!scanout_transfer_original(encoder->transfer, &encoder->token, index, &original) ||
        !intel_bdw_ggtt_plan_entry(&encoder->transfer->manifest, index, physical, &entry) ||
        entry.original != original)
        return false;
    *replacement = entry.replacement;
    return true;
}

enum mapping_kind { FULL_SURFACE, INHERITED_PAGE, DRIVER_PAGE };

static bool prepare(struct scanout_memory *m, const struct scanout_memory_token *token,
                    struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                    struct scanout_publish_pool *pool, struct scanout_publish_io io, enum mapping_kind kind,
                    const struct scanout_interval *firmware, const struct bdw_stolen_layout *layout)
{
    if (!m || !token || !transfer || !grant || !pool ||
        m->object != m || token->object != m || token->epoch != m->epoch ||
        token->owner != m->owner || m->owner != transfer->owner || m->state != MEM_RAM ||
        m->busy || m->closing || m->cpu_serial || m->dependent || m->registered ||
        m->hold_serial || m->range.object || m->layout.pitch > 32768 ||
        m->backing_pages > SCANOUT_PUBLISH_PAGES ||
        !m->physical || (m->physical & 4095) ||
        m->physical > 0x100000000ull - (uint64_t)m->backing_pages * 4096 ||
        transfer->state != TRANSFER_ACTIVE || transfer->manifest.assurances != HANDOFF_ALL ||
        transfer->manifest.obligations || io.ctx != transfer->io.ctx || io.now != transfer->io.now ||
        !io.guard || !io.read || !io.write || !io.flush || !io.readers_retired)
        return false;

    if (kind != FULL_SURFACE) {
        const struct scanout_transfer_manifest *s = &transfer->manifest;
        if (m->layout.pages != 1 || m->layout.tiled || m->backing_pages != 1 ||
            !scanout_interval_boot_framebuffer_matches(s->mmio,s->aperture,s->aperture_bytes,s->fence_end))
            return false;
        if (kind == INHERITED_PAGE && s->first != INTEL_WINDOW_FIRST) return false;
        if (kind == DRIVER_PAGE && !owned_page_range(s, firmware, layout)) return false;
    } else if (m->backing_pages < 128 || m->layout.pages != m->backing_pages - 128) {
        return false;
    }

    if (!scanout_transfer_check(transfer, grant)) return false;

    /* scanout_memory_prepare consumes encode synchronously and copies only
     * the publisher's long-lived I/O into the transaction. */
    struct encode_context encoder = {transfer, *grant, m, kind != FULL_SURFACE};
    struct scanout_memory_binding binding = {&encoder, encode, io};
    return scanout_memory_prepare(m, token, &transfer->space, pool, kind == DRIVER_PAGE ? 1 : 64, binding);
}

bool intel_bdw_scanout_prepare(struct scanout_memory *m, const struct scanout_memory_token *token,
                              struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                              struct scanout_publish_pool *pool, struct scanout_publish_io io)
{
    return prepare(m, token, transfer, grant, pool, io, FULL_SURFACE, 0, 0);
}

bool intel_bdw_scanout_prepare_test_page(struct scanout_memory *m, const struct scanout_memory_token *token,
                                        struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                                        struct scanout_publish_pool *pool, struct scanout_publish_io io)
{
    return prepare(m, token, transfer, grant, pool, io, INHERITED_PAGE, 0, 0);
}

bool intel_bdw_scanout_prepare_owned_page(struct scanout_memory *m, const struct scanout_memory_token *token,
                                         struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                                         const struct scanout_interval *firmware, const struct bdw_stolen_layout *layout,
                                         struct scanout_publish_pool *pool,
                                         struct scanout_publish_io io)
{
    return prepare(m, token, transfer, grant, pool, io, DRIVER_PAGE, firmware, layout);
}
