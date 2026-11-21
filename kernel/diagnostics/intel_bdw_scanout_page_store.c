#include <stddef.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_ggtt_window.h>
#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/vmm.h>
#include <srvros/x86_cache.h>

/* Only the first PTE beyond the captured 34 MiB fence. Its register is the
 * first QWord in the one 4 KiB modification page audited by the cache probe. */

static bool store_active(const struct scanout_memory *m, const struct scanout_memory_token *token,
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

static bool mapped_register(uint64_t root, uint64_t alias, uint64_t physical)
{
    struct vmm_alias_observation a;
    return vmm_diagnostic_kernel_alias(root, alias, physical, 1, 39, &a, NULL) &&
           a.pages == 1 && a.leaves == 1 && a.leaf_sizes == 1 && a.pat_index == 2 &&
           a.writable == 1 && !a.user && a.no_execute == 1;
}

bool intel_bdw_scanout_store_test_page(struct scanout_memory *m, const struct scanout_memory_token *token,
                                      struct scanout_transfer *t, const struct scanout_transfer_token *grant,
                                      const struct vmm_reserved_guard *cpu_guard, uint64_t root, uint64_t alias, uint32_t index, uint64_t value)
{
    uint64_t original;
    if (!root || !alias || (alias & 4095) || !store_active(m, token, t, index) ||
        !scanout_transfer_original(t, grant, index, &original)) return false;

    const struct scanout_publication *p = &m->publication;
    bool restore = p->state == PUB_RESTORING;
    struct intel_bdw_ggtt_page entry;
    if (!intel_bdw_ggtt_plan_entry(&t->manifest, index, m->physical, &entry) ||
        entry.original != original || (original & 4095) != 1 || original >> 39 ||
        !(original & ~4095ull) || (original & ~4095ull) == m->physical ||
        t->manifest.mmio > UINT64_MAX - entry.mmio_offset) return false;

    uint64_t physical = t->manifest.mmio + entry.mmio_offset;
    if (!m->allocator.validate || !m->allocator.validate(m->allocator.ctx, entry.physical, 1) ||
        !intel_cache_probe_uc_page(physical) || !mapped_register(root, alias, physical)) return false;

    /* Charge the alias and cache checks to the original publication deadline.
     * Recheck the operation and saved owner after the last provider callback. */
    uint64_t fresh;
    if (!scanout_transfer_check(t, grant) || !mapped_register(root, alias, physical)) return false;
    uint64_t now = t->io.now(t->io.ctx);
    if (!store_active(m, token, t, index) ||
        !scanout_transfer_original(t, grant, index, &fresh) || fresh != original ||
        m->physical != entry.physical || t->manifest.mmio != physical - entry.mmio_offset ||
        restore != (p->state == PUB_RESTORING) || p->before[0] != original || p->after[0] != entry.replacement ||
        !scanout_space_verify_original(m->space, &p->range, 0, original) ||
        value != (restore ? original : entry.replacement) || t->last_us < p->last_us ||
        t->last_us < p->phase_us || now < t->last_us ||
        now - p->phase_us > SCANOUT_PUBLISH_DEADLINE_US) return false;

    /* Pinned UC alias on x86-64. Compare immediately before the single
     * aligned QWord store. A changed entry is left to its original owner. */
    uint64_t offset = (uint64_t)index * 4096;
    if (t->manifest.aperture > UINT64_MAX - offset ||
        !vmm_reserved_guard_io_page_held(cpu_guard, root, t->manifest.aperture + offset)) return false;
    volatile uint64_t *reg = (volatile uint64_t *)(uintptr_t)alias;
    if (*reg != (restore ? entry.replacement : original)) return false;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    *reg = value;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return *reg == value;
}
