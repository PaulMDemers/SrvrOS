#include <stddef.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_ggtt_window.h>
#include <srvros/intel_ggtt_invalidate.h>
#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/vmm.h>
#include <srvros/intel_bdw_traffic.h>
#include <srvros/x86_cache.h>

/* Only the first PTE beyond the captured 34 MiB fence. Its register is the
 * first QWord in the one 4 KiB modification page audited by the cache probe. */

/* CPU0 only. Keep the original publication, guard and register alias together
 * across both stores and the publisher's final device-reader retirement. */
static struct page_borrow {
    const struct scanout_publication *publication;
    const struct vmm_reserved_guard *guard;
    uint64_t epoch, guard_epoch, root, aperture, alias, reg;
    struct scanout_transfer *transfer;
    struct intel_bdw_scanout_traffic traffic;
    uint64_t block_epoch, transfer_epoch;
    uint32_t plane_control, live_surface;
    bool plane_sampled, drained, retiring;
} borrow;

static bool page_pinned(const struct scanout_publication *p)
{
    return p && borrow.publication == p && borrow.epoch == p->epoch &&
           borrow.transfer->device_publication == p && borrow.transfer->pinned &&
           borrow.transfer->epoch == borrow.transfer_epoch &&
           display_access_held(&borrow.transfer->exclusion) &&
           borrow.guard_epoch == borrow.guard->epoch &&
           vmm_reserved_guard_register_pinned(borrow.guard, borrow.root, borrow.aperture,
                                              borrow.reg, borrow.alias, &borrow);
}

static bool page_hold_live(const struct scanout_publication *p)
{
    return page_pinned(p) && borrow.drained &&
           borrow.block_epoch && borrow.traffic.held_epoch(borrow.transfer,p) == borrow.block_epoch;
}



static bool mapped_register(uint64_t root, uint64_t alias, uint64_t physical)
{
    struct vmm_alias_observation a;
    return vmm_diagnostic_kernel_alias(root, alias, physical, 1, 39, &a, NULL) &&
           a.pages == 1 && a.leaves == 1 && a.leaf_sizes == 1 && a.pat_index == 2 &&
           a.writable == 1 && !a.user && a.no_execute == 1;
}

struct traffic_check {
    uint64_t begin, last, epoch;
};

static bool traffic_live(struct traffic_check *check)
{
    if (!page_pinned(borrow.publication) ||
        borrow.traffic.held_epoch(borrow.transfer,borrow.publication)!=check->epoch) return false;
    uint64_t now=borrow.transfer->io.now(borrow.transfer->io.ctx);
    if (!now || now==UINT64_MAX || now<check->last ||
        (borrow.publication->busy && (now<borrow.publication->phase_us ||
         now-borrow.publication->phase_us>SCANOUT_PUBLISH_DEADLINE_US)) ||
        now-check->begin>=BDW_TRAFFIC_BLOCK_US) return false;
    check->last=now;
    return page_pinned(borrow.publication) &&
           borrow.traffic.held_epoch(borrow.transfer,borrow.publication)==check->epoch;
}

static bool read_traffic(void *ctx,uint32_t offset,uint32_t *word)
{
    struct traffic_check *check=ctx;
    *word=UINT32_MAX;
    return traffic_live(check) && borrow.traffic.read(borrow.transfer,offset,word) &&
           *word!=UINT32_MAX &&
           /* Every reported reset reason is foreign to a reset-free store. */
           (offset != 0x40d0 || !(*word & 0xff00u)) && traffic_live(check);
}

static bool select_traffic_l3(void *ctx,uint32_t from,uint32_t to)
{
    (void)ctx;
    return borrow.traffic.select_l3(borrow.transfer,from,to)==GT_WRITE_DONE;
}

static bool l3_flush_covers_all_lines(struct traffic_check *check)
{
    struct bdw_traffic_sample sample={.ctx=check,.read=read_traffic};
    return intel_bdw_traffic_l3_policy(&sample,select_traffic_l3);
}

/* Keep both the queued scanout and the actual scanout on the inherited
 * surface. SURF bit 3 records the previous flip source, not an address. */
static bool original_plane(struct traffic_check *check)
{
    uint32_t control, programmed, live;
    if (!read_traffic(check,0x70180,&control) || !(control & (1u<<31)) ||
        !read_traffic(check,0x7019c,&programmed) || !read_traffic(check,0x701ac,&live) ||
        (live & 4095u) || (programmed & ~8u)!=live || live>=borrow.transfer->manifest.fence_end)
        return false;
    if (borrow.plane_sampled)
        return control==borrow.plane_control && live==borrow.live_surface;
    borrow.plane_control=control;
    borrow.live_surface=live;
    borrow.plane_sampled=true;
    return true;
}

static bool acquire_traffic(struct traffic_check *check)
{
    struct bdw_traffic_sample sample={.ctx=check,.read=read_traffic};
    if (!original_plane(check) || !l3_flush_covers_all_lines(check) ||
        !intel_bdw_traffic_open(&sample) || !traffic_live(check) ||
        *(volatile const uint64_t *)(uintptr_t)borrow.alias!=borrow.publication->before[0]) return false;
    uint64_t requested_at = check->last, epoch = 0;
    enum intel_gt_write issued=borrow.traffic.request(borrow.transfer,borrow.publication,&epoch);
    if (issued!=GT_WRITE_DONE || !epoch) return false;
    borrow.block_epoch=check->epoch=epoch;
    for (unsigned n=0;n<GT_ACCESS_POLL_CAP;++n) {
        bool drained;
        if (!intel_bdw_traffic_poll(&sample,&drained)) return false;
        if (drained && check->last > requested_at) return true;
    }
    return false;
}

bool intel_bdw_scanout_retire_test_page(const struct scanout_memory *m,const struct scanout_memory_token *token)
{
    if (!m || !token || m->object!=m || token->object!=m || token->epoch!=m->epoch ||
        token->owner!=m->owner || m->busy || m->state!=MEM_RELEASED ||
        m->publication.state!=PUB_RETIRED || m->publication.busy ||
        borrow.retiring || borrow.publication!=&m->publication) return false;
    /* Provider calls can reenter. Consume this retirement before consulting
     * the hold or clock, including when either callback reports uncertainty. */
    borrow.retiring=true;
    if (!page_hold_live(&m->publication)) return false;
    uint64_t now=borrow.transfer->io.now(borrow.transfer->io.ctx);
    struct traffic_check check={now,now,borrow.block_epoch};
    if (!now || now < m->publication.last_us || !original_plane(&check) ||
        !l3_flush_covers_all_lines(&check) || !intel_bdw_traffic_still_blocked(&check, read_traffic) ||
        !traffic_live(&check)) return false;
    uint64_t released_at = check.last;
    if (borrow.traffic.release(borrow.transfer,borrow.publication,borrow.block_epoch)!=GT_WRITE_DONE)
        return false;
    check.epoch=0;
    struct bdw_traffic_sample sample={.ctx=&check,.read=read_traffic};
    for (unsigned n=0;n<GT_ACCESS_POLL_CAP;++n) {
        /* A failed read or owner check is terminal. An asynchronous release
         * may take time to reach all six clients and five engines. */
        bool arbiter, pipelines;
        uint32_t l3;
        if (!bdw_traffic_gam(&sample,BDW_GAM_CLIENTS,&arbiter) ||
            !bdw_traffic_engines(&sample,false,&pipelines) || !read_traffic(&check,0x8038,&l3))
            return false;
        if (!arbiter || !pipelines || (l3 & 1u) || check.last == released_at) continue;
        if (!original_plane(&check) || !traffic_live(&check) || !vmm_reserved_guard_unpin_io_page(
                borrow.guard,borrow.root,borrow.aperture,&borrow)) return false;
        borrow.transfer->device_publication=NULL;
        borrow=(struct page_borrow){0};
        return true;
    }
    return false;
}

bool intel_bdw_scanout_store_test_page(struct scanout_memory *m, const struct scanout_memory_token *token,
                                      struct scanout_transfer *t, const struct scanout_transfer_token *grant,
                                      struct intel_bdw_scanout_traffic traffic, const struct vmm_reserved_guard *cpu_guard, uint64_t root, uint64_t alias, uint32_t index, uint64_t value)
{
    uint64_t original;
    if (!root || !alias || (alias & 4095) || !intel_bdw_scanout_page_store_active(m, token, t, index) ||
        !scanout_transfer_original(t, grant, index, &original)) return false;

    struct scanout_publication *p = &m->publication;
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
    if (!traffic.invalidate_ggtt || !traffic.held_epoch || !traffic.read || !traffic.request || !traffic.release || !traffic.select_l3 ||
        !scanout_transfer_check(t,grant)) return false;
    if (restore && (borrow.transfer!=t || !page_hold_live(p) ||
        borrow.traffic.invalidate_ggtt!=traffic.invalidate_ggtt ||
        borrow.traffic.held_epoch!=traffic.held_epoch || borrow.traffic.read!=traffic.read ||
        borrow.traffic.request!=traffic.request || borrow.traffic.release!=traffic.release ||
        borrow.traffic.select_l3!=traffic.select_l3)) return false;
    uint64_t offset = (uint64_t)index * 4096;
    if (t->manifest.aperture > UINT64_MAX - offset || !cpu_guard) return false;
    uint64_t aperture = t->manifest.aperture + offset;
    if (restore) {
        if (borrow.publication != p || borrow.epoch != p->epoch || borrow.guard != cpu_guard ||
            borrow.guard_epoch != cpu_guard->epoch || borrow.root != root || borrow.alias != alias ||
            borrow.aperture != aperture || !vmm_reserved_guard_register_pinned(cpu_guard, root, aperture, physical, alias, &borrow))
            return false;
    } else {
        if (p->hold_live || borrow.publication || t->device_publication || !vmm_reserved_guard_pin_register(cpu_guard, root, aperture, physical, alias, &borrow)) return false;
        borrow = (struct page_borrow){.publication=p,.guard=cpu_guard,.epoch=p->epoch,.guard_epoch=cpu_guard->epoch,
            .root=root,.aperture=aperture,.alias=alias,.reg=physical,.transfer=t,.traffic=traffic,.transfer_epoch=t->epoch};
        t->device_publication=p;
        p->hold_live=page_hold_live;
    }
    uint64_t start=t->io.now(t->io.ctx);
    if (!start || start<t->last_us || start<p->phase_us || start-p->phase_us>SCANOUT_PUBLISH_DEADLINE_US)
        return false;
    struct traffic_check check={start,start,borrow.block_epoch};
    if (!restore && !acquire_traffic(&check)) return false;
    if (!original_plane(&check) || !l3_flush_covers_all_lines(&check) ||
        !intel_bdw_traffic_still_blocked(&check, read_traffic) || !mapped_register(root, alias, physical))
        return false;
    borrow.drained=true;
    if (!traffic_live(&check) ||
        !vmm_reserved_guard_register_pinned(cpu_guard, root, aperture, physical, alias, &borrow)) return false;
    volatile uint64_t *reg = (volatile uint64_t *)(uintptr_t)alias;
    uint64_t expected = restore ? entry.replacement : original;
    if (*reg != expected) return false;
    uint32_t posted;
    if (!read_traffic(&check, INTEL_GGTT_INVALIDATE_REGISTER, &posted) || posted) return false;
    uint64_t now = t->io.now(t->io.ctx);
    if (!intel_bdw_scanout_page_store_active(m, token, t, index) ||
        !scanout_transfer_original(t, grant, index, &fresh) || fresh != original ||
        m->physical != entry.physical || t->manifest.mmio != physical - entry.mmio_offset ||
        restore != (p->state == PUB_RESTORING) || p->before[0] != original || p->after[0] != entry.replacement ||
        !scanout_space_verify_original(m->space, &p->range, 0, original) ||
        value != (restore ? original : entry.replacement) || t->last_us < p->last_us ||
        t->last_us < p->phase_us || now < t->last_us || now<check.last ||
        now - p->phase_us > SCANOUT_PUBLISH_DEADLINE_US || now-check.begin >= BDW_TRAFFIC_BLOCK_US) return false;

    /* Pinned UC alias on x86-64. Compare immediately before the single
     * aligned QWord store. A changed entry is left to its original owner. */
    if (!original_plane(&check) || !intel_bdw_traffic_still_blocked(&check, read_traffic) ||
        p->hold_live != page_hold_live || !page_hold_live(p)) return false;
    if (intel_bdw_ggtt_exchange(reg, expected, value) != GT_WRITE_DONE || !original_plane(&check) ||
        !intel_bdw_traffic_still_blocked(&check, read_traffic)) return false;
    /* This WO register always reads zero. The read orders the preceding
     * System Agent request. Engine and cache retirement have their own proof. */
    if (borrow.traffic.invalidate_ggtt(t, p, check.epoch, INTEL_GGTT_INVALIDATE_REGISTER, 1) != GT_WRITE_DONE) return false;
    return read_traffic(&check, INTEL_GGTT_INVALIDATE_REGISTER, &posted) && !posted &&
           original_plane(&check) && intel_bdw_traffic_still_blocked(&check, read_traffic) &&
           *reg == value;
}
