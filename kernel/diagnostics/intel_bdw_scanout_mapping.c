#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_ggtt_window.h>

bool intel_bdw_scanout_page_store_active(const struct scanout_memory *m, const struct scanout_memory_token *token,
                         const struct scanout_transfer *t, uint32_t index)
{
    if (!m || !token || !t || index != INTEL_WINDOW_FIRST ||
        m->object != m || token->object != m || token->epoch != m->epoch || token->owner != m->owner ||
        !m->busy || m->owner != t->owner || m->backing_pages != 1 || m->layout.pages != 1 ||
        m->layout.tiled || m->cpu_serial || m->dependent || m->registered || m->hold_serial ||
        m->space != &t->space || m->range.first != index || m->range.pages != 1 ||
        t->manifest.first != index || t->manifest.fence_end != (uint64_t)index * 4096 ||
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

static bool prepare(struct scanout_memory *m, const struct scanout_memory_token *token,
                    struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                    struct scanout_publish_pool *pool, struct scanout_publish_io io, bool single_page)
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

    if (single_page) {
        const struct scanout_transfer_manifest *s = &transfer->manifest;
        if (m->layout.pages != 1 || m->layout.tiled || m->backing_pages != 1 ||
            s->first != INTEL_WINDOW_FIRST ||
            !scanout_interval_boot_framebuffer_matches(s->mmio,s->aperture,s->aperture_bytes,s->fence_end))
            return false;
    } else if (m->backing_pages < 128 || m->layout.pages != m->backing_pages - 128) {
        return false;
    }

    if (!scanout_transfer_check(transfer, grant)) return false;

    /* scanout_memory_prepare consumes encode synchronously and copies only
     * the publisher's long-lived I/O into the transaction. */
    struct encode_context encoder = {transfer, *grant, m, single_page};
    struct scanout_memory_binding binding = {&encoder, encode, io};
    return scanout_memory_prepare(m, token, &transfer->space, pool, 64, binding);
}

bool intel_bdw_scanout_prepare(struct scanout_memory *m, const struct scanout_memory_token *token,
                              struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                              struct scanout_publish_pool *pool, struct scanout_publish_io io)
{
    return prepare(m, token, transfer, grant, pool, io, false);
}

bool intel_bdw_scanout_prepare_test_page(struct scanout_memory *m, const struct scanout_memory_token *token,
                                        struct scanout_transfer *transfer, const struct scanout_transfer_token *grant,
                                        struct scanout_publish_pool *pool, struct scanout_publish_io io)
{
    return prepare(m, token, transfer, grant, pool, io, true);
}
