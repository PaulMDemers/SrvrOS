#include <srvros/intel_bdw_edp_baseline.h>
#include <srvros/display_access.h>
#include <srvros/display_present.h>

extern bool intel_cache_probe_edp_uc_page(uint64_t physical, uint32_t offset);

#define EDP_VA (CAPTURE_VA + 0x180000)
#define EDP_RENDERED_PTES (BDW_EDP_BACKING_PAGES - 128u)
#define EDP_TAIL_PAGE 0x802000u
#define EDP_LEASE 0x454450435243ull

static const uint32_t edp_pages[] = {
    0x41000, 0x43000, 0x44000, 0x45000, 0x46000, 0x60000,
    0x64000, 0x6f000, 0x70000, 0x7f000, 0x130000
};

struct edp_adapter {
    struct backing_adapter backing;
    struct display_access_token lease;
    unsigned mapped, smi, reason;
    bool pinned, map_fault;
};
static struct edp_adapter edp_session;

static bool edp_smi_supported(void)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0),"c"(0));
    if (a < 1 || b != 0x756e6547 || d != 0x49656e69 || c != 0x6c65746e)
        return false;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    return a == 0x306d4 && (d & (1u << 5)) && !(c & (1u << 31));
}

static unsigned edp_smi(void)
{
    uint32_t count, high;
    __asm__ volatile("lfence; rdmsr; lfence":"=a"(count),"=d"(high):"c"(0x34):"memory");
    return count;
}

static bool edp_held(void *context)
{
    struct edp_adapter *a = context;
    return a->pinned && display_access_held(&a->lease) && display_present_handoff_idle() &&
           vmm_current_address_space() == a->backing.common.cr3 &&
           intel_boot_check(INTEL_BOOT_OBSERVER, a->backing.common.gpu) &&
           edp_smi() == a->smi;
}

static bool edp_unmap_all(struct edp_adapter *a)
{
    while (a->mapped) {
        unsigned i = a->mapped - 1;
        uint64_t phys;
        uint64_t va = EDP_VA + 4096ull * i;
        if (vmm_current_address_space() != a->backing.common.cr3 ||
            !vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &phys) ||
            phys != result.mmio_base + edp_pages[i] ||
            !vmm_unmap_page_in_space(a->backing.common.cr3, va, &phys) ||
            phys != result.mmio_base + edp_pages[i])
            return false;
        --a->mapped;
    }
    return true;
}

static bool edp_map_all(struct edp_adapter *a)
{
    for (unsigned i = 0; i < sizeof(edp_pages) / sizeof(edp_pages[0]); ++i) {
        uint32_t page = edp_pages[i];
        uint64_t va = EDP_VA + 4096ull * i;
        uint64_t physical = result.mmio_base + page, existing;
        if (!edp_held(a) || !backing_range_reserved(physical, 4096) ||
            !intel_cache_probe_edp_uc_page(physical, page) ||
            vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &existing))
            return false;
        uint64_t flags = VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE;
        if (page == 0x44000 || page == 0x60000)
            flags |= VMM_PAGE_WRITABLE;
        if (!vmm_map_page_in_space(a->backing.common.cr3, va, physical, flags)) {
            a->map_fault = vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &existing);
            return false;
        }
        ++a->mapped;
    }
    return true;
}

static volatile uint32_t *edp_mmio(struct edp_adapter *a, uint32_t offset)
{
    if ((offset & 3) || !edp_held(a))
        return NULL;
    for (unsigned i = 0; i < a->mapped; ++i) {
        if (edp_pages[i] != (offset & ~4095u))
            continue;
        uint64_t phys, va = EDP_VA + 4096ull * i;
        if (vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &phys) &&
            phys == result.mmio_base + edp_pages[i])
            return (volatile uint32_t *)(va + (offset & 4095u));
        break;
    }
    a->map_fault = true;
    return NULL;
}

static bool edp_read(void *context, uint32_t offset, uint32_t *out)
{
    volatile const uint32_t *word = edp_mmio(context, offset);
    if (!word)
        return false;
    *out = *word;
    return true;
}

static enum intel_gt_write edp_write(void *context, uint32_t offset, uint32_t value)
{
    if (offset == BDW_EDP_CRC_CTL) {
        if (value && value != BDW_EDP_CRC_ENABLE)
            return GT_WRITE_NONE;
    } else if (offset == BDW_EDP_PIPE_IIR) {
        if (value != BDW_EDP_CRC_DONE)
            return GT_WRITE_NONE;
    } else if (offset != BDW_EDP_PIPE_IMR) {
        return GT_WRITE_NONE;
    }
    volatile uint32_t *word = edp_mmio(context, offset);
    if (!word || (offset == BDW_EDP_PIPE_IMR && ((*word ^ value) & ~BDW_EDP_CRC_DONE)))
        return GT_WRITE_NONE;
    if (!edp_held(context))
        return GT_WRITE_NONE;
    *word = value;
    return GT_WRITE_DONE;
}

static bool edp_expect(struct edp_adapter *a, uint32_t reg, uint32_t mask, uint32_t value)
{
    uint32_t word;
    return edp_read(a, reg, &word) && (word & mask) == value;
}

static bool edp_check_backing(struct edp_adapter *a, uint64_t *hash)
{
    struct scanout_profile profile;
    if (!edp_held(a) || !intel_scanout_trial_profile(&profile) ||
        profile.width != 1440 || profile.height != 900 || profile.pitch != 6144 ||
        backing.offset || backing.count != EDP_RENDERED_PTES ||
        !intel_scanout_trial_revalidate())
        return false;
    const uint64_t first = backing.pte[0];
    if ((first & 4095u) != 1 || first < backing.stolen_base ||
        first >= backing.stolen_end ||
        backing.stolen_end - (first & ~4095ull) < BDW_EDP_BACKING_PAGES * 4096ull)
        return false;
    for (unsigned i = 0; i < EDP_RENDERED_PTES; ++i)
        if (backing.pte[i] != first + i * 4096ull)
            return false;

    if (!edp_held(a) || !intel_cache_probe_edp_uc_page(result.mmio_base + EDP_TAIL_PAGE, EDP_TAIL_PAGE) ||
        !backing_map(&a->backing, EDP_TAIL_PAGE))
        return false;
    uint64_t digest = backing.sample[1].hash;
    bool stable_tail = true;
    for (unsigned i = EDP_RENDERED_PTES; i < BDW_EDP_BACKING_PAGES; ++i) {
        uint32_t low, high, again, offset = 0x800000u + i * 8u;
        if (!edp_held(a) || !backing_read(&a->backing, offset + 4, &high) ||
            !backing_read(&a->backing, offset, &low) || !backing_read(&a->backing, offset + 4, &again) ||
            high != again || (((uint64_t)high << 32) | low) != first + i * 4096ull) {
            stable_tail = false;
            break;
        }
        digest = (digest ^ low) * 1099511628211ull;
        digest = (digest ^ high) * 1099511628211ull;
    }
    if (!backing_unmap(&a->backing)) {
        a->map_fault = true;
        return false;
    }
    *hash = digest;
    return stable_tail && edp_held(a);
}

static bool edp_snapshot(void *context, struct bdw_edp_baseline_sample *s)
{
    struct edp_adapter *a = context;
    a->reason = 4;
    if (!edp_check_backing(a, &s->backing_hash))
        return false;
    a->reason = 5;
    for (unsigned i = 0; i < 4; ++i)
        if (!edp_expect(a, 0x45400 + 4 * i, UINT32_MAX, result.sample[1].well_after[i]))
            return false;
    a->reason = 6;
    if (!edp_read(a, 0x46100, &s->port) || !edp_read(a, 0x64000, &s->buffer) ||
        !edp_read(a, 0x64040, &s->transport) || !edp_read(a, 0x130040, &s->lcpll) ||
        !edp_read(a, 0x46200, &s->cdclk))
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        if (!edp_read(a, 0x64e00u + 8 * i, &s->phy[i][0]) ||
            !edp_read(a, 0x64e04u + 8 * i, &s->phy[i][1]))
            return false;
    a->reason = 7;
    if (!edp_expect(a, 0x41000, 0x80000000u, 0x80000000u) || /* VGA disabled */
        !edp_expect(a, 0x43208, 0x80000000u, 0) ||            /* FBC disabled */
        !edp_expect(a, 0x43214, 0xfffu, 0) ||
        !edp_expect(a, 0x43408, 0x80000000u, 0) ||            /* IPS disabled */
        !edp_expect(a, 0x6f800, 0x80000000u, 0) ||           /* PSR disabled */
        !edp_expect(a, 0x6f840, 0xe0000000u, 0) ||
        !edp_expect(a, 0x6f400, UINT32_MAX, 0x82200000u) ||
        !edp_expect(a, 0x7f008, UINT32_MAX, 0xc0000000u) ||
        !edp_expect(a, 0x70180, 0xbffffbffu, 0x98000000u) ||
        !edp_expect(a, 0x70180, 0x400u, 0x400u))
        return false;
    if (s->port != 0x20000000u || (s->buffer & 0x8000008eu) != 0x80000000u ||
        (s->transport & ~0x40u) != 0x80040300u || (s->lcpll & 0xc0000000u) != 0x40000000u ||
        s->cdclk > 675)
        return false;
    a->reason = 0;
    return edp_held(a);
}

static void edp_pause(void *context, uint32_t us)
{
    (void)context;
    timer_delay_us(us);
}

static bool edp_try(struct bdw_edp_baseline_report *r)
{
    struct edp_adapter *a = &edp_session;
    *a = (struct edp_adapter){0};
    if (!edp_smi_supported() || !trial_adapter(&a->backing)) {
        r->reason = 1;
        return false;
    }
    if (!console_sync() || !display_access_acquire(EDP_LEASE, &a->lease)) {
        r->reason = 2;
        return false;
    }
    a->smi = edp_smi();
    a->pinned = display_access_pin(&a->lease);
    bool ok = false;
    a->reason = 3;
    if (a->pinned && edp_map_all(a)) {
        const struct bdw_edp_baseline_io io = {
            a, edp_held, edp_snapshot, edp_read, edp_write, now, edp_pause
        };
        ok = intel_bdw_edp_baseline(&io, r);
    }
    bool retired = !r->retained && !a->map_fault && !trial_mapping_fault && edp_unmap_all(a);
    if (retired && a->pinned)
        retired = display_access_unpin(&a->lease);
    if (!retired || !display_access_release(&a->lease, true))
        r->retained = true;
    if (r->retained)
        r->status = BDW_EDP_BASELINE_INCOMPLETE;
    r->reason = r->retained ? 9 : a->reason ? a->reason : ok ? 0 : 8;
    return ok && !r->retained;
}

bool intel_edp_baseline_probe(bool *retained)
{
    static bool attempted;
    if (attempted || !retained)
        return false;
    attempted = true;
    struct bdw_edp_baseline_report r[2] = {0};
    console_write("intel-edp-baseline: frame A ===============================\n");
    bool first = edp_try(&r[0]);
    bool second = false;
    if (first) {
        console_write("intel-edp-baseline: frame B +++++ data +++++ data +++++\n"
                      "intel-edp-baseline: frame B [::::::::] [::::::::] [::::::::]\n");
        second = edp_try(&r[1]);
    }
    *retained = r[0].retained || r[1].retained;
    bool same = first && second && intel_bdw_edp_baseline_same(&r[0], &r[1]);
    bool changed = same && r[0].crc[2] != r[1].crc[2];
    for (unsigned i = 0; i < 2; ++i) {
        const struct bdw_edp_baseline_sample *s = &r[i].sample;
        console_printf("intel-edp-window: v=1 sample=%u status=%u reason=%u events=%u writes=%u restored=%u retained=%u hash=%x port=%x buf=%x tp=%x pll=%x cdclk=%x\n",
            (uint64_t)i, (uint64_t)r[i].status, (uint64_t)r[i].reason, (uint64_t)r[i].events, (uint64_t)r[i].writes,
            (uint64_t)r[i].restored, (uint64_t)r[i].retained, s->backing_hash, (uint64_t)s->port,
            (uint64_t)s->buffer, (uint64_t)s->transport, (uint64_t)s->lcpll, (uint64_t)s->cdclk);
        console_printf("intel-edp-poll: v=1 sample=%u iir=%x\n",
            (uint64_t)i, (uint64_t)r[i].poll_iir);
        for (unsigned e = 0; e < BDW_EDP_CRC_FRAMES; ++e)
            console_printf("intel-edp-crc: v=1 sample=%u event=%u frame=%u value=%x\n",
                (uint64_t)i, (uint64_t)e, (uint64_t)r[i].frame[e], (uint64_t)r[i].crc[e]);
        for (unsigned p = 0; p < BDW_EDP_PHY_ENTRIES; ++p)
            console_printf("intel-edp-phy: v=1 sample=%u entry=%u low=%x high=%x\n",
                (uint64_t)i, (uint64_t)p, (uint64_t)s->phy[p][0], (uint64_t)s->phy[p][1]);
    }
    console_printf("intel-edp-baseline: v=1 complete=%u same=%u changed=%u retained=%u pte_writes=0 reset=0\n",
        (uint64_t)changed, (uint64_t)same, (uint64_t)changed, (uint64_t)*retained);
    return changed;
}
