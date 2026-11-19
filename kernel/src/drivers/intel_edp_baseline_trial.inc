#include <srvros/intel_bdw_edp_baseline.h>
#include <srvros/intel_bdw_edp_platform.h>
#include <srvros/display_access.h>
#include <srvros/display_present.h>

extern bool intel_cache_probe_edp_uc_page(uint64_t physical, uint32_t offset);

#define EDP_VA (CAPTURE_VA + 0x180000)
#define EDP_RENDERED_PTES (BDW_EDP_BACKING_PAGES - 128u)
#define EDP_LEASE 0x454450435243ull
#define EDP_BASELINE_PAGES 11u

static const uint32_t edp_pages[] = {
    0x41000, 0x43000, 0x44000, 0x45000, 0x46000, 0x60000,
    0x64000, 0x6f000, 0x70000, 0x7f000, 0x130000,
    0x42000, 0x48000, 0x65000, 0x68000, 0xc2000, 0xc7000, 0xc8000
};

struct edp_cycle_observation {
    uint64_t backing_hash;
    uint32_t imr, ier, iir, aux, fitter, audio, ddi[4], wm[3], blockers;
    bool complete;
};

struct edp_adapter {
    struct backing_adapter backing;
    struct display_access_token lease;
    struct bdw_edp_baseline_report *crc;
    struct edp_cycle_observation *cycle;
    unsigned mapped, smi, reason;
    bool pinned, map_fault, platform, well_requested;
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
    unsigned pages = a->platform ? sizeof(edp_pages) / sizeof(edp_pages[0]) : EDP_BASELINE_PAGES;
    for (unsigned i = 0; i < pages; ++i) {
        uint32_t page = edp_pages[i];
        uint64_t va = EDP_VA + 4096ull * i;
        uint64_t physical = result.mmio_base + page, existing;
        if (!edp_held(a) || !backing_range_reserved(physical, 4096) ||
            !intel_cache_probe_edp_uc_page(physical, page) ||
            vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &existing))
            return false;
        uint64_t flags = VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE;
        if (page == 0x44000 || page == 0x60000 || (a->platform && page == 0x45000))
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

static bool edp_read_pair(struct edp_adapter *a, uint32_t offset, uint64_t *pair)
{
    uint32_t low, high, again;
    if (!edp_held(a) || !backing_read(&a->backing, offset + 4, &high) ||
        !backing_read(&a->backing, offset, &low) ||
        !backing_read(&a->backing, offset + 4, &again) || high != again)
        return false;
    *pair = ((uint64_t)high << 32) | low;
    return true;
}

static bool edp_map_backing(struct edp_adapter *a, uint32_t page)
{
    if (!edp_held(a) || !intel_cache_probe_edp_uc_page(result.mmio_base + page, page))
        return false;
    if (backing_map(&a->backing, page))
        return true;
    uint64_t existing;
    a->map_fault |= vmm_virt_to_phys_in_space(a->backing.common.cr3, BACKING_VA, &existing);
    return false;
}

static bool edp_unmap_backing(struct edp_adapter *a)
{
    if (backing_unmap(&a->backing))
        return true;
    a->map_fault = true;
    return false;
}

static bool edp_range_valid(void)
{
    const uint64_t first = backing.pte[0];
    return backing.offset == 0 && backing.count == EDP_RENDERED_PTES &&
           (first & 4095u) == 1 && first >= backing.stolen_base && first < backing.stolen_end &&
           backing.stolen_end - (first & ~4095ull) >= BDW_EDP_BACKING_PAGES * 4096ull;
}

static bool edp_pte_page(struct edp_adapter *a, unsigned first, unsigned end, uint64_t *digest)
{
    uint32_t page = (0x800000u + first * 8u) & ~4095u;
    if (!edp_map_backing(a, page))
        return false;
    bool match = true;
    for (unsigned i = first; i < end; ++i) {
        uint64_t value;
        if (!edp_read_pair(a, 0x800000u + i * 8u, &value) ||
            value != backing.pte[0] + i * 4096ull ||
            (i < EDP_RENDERED_PTES && value != backing.pte[i])) {
            match = false;
            break;
        }
        if (i >= EDP_RENDERED_PTES) {
            *digest = (*digest ^ (uint32_t)value) * 1099511628211ull;
            *digest = (*digest ^ (value >> 32)) * 1099511628211ull;
        }
    }
    return edp_unmap_backing(a) && match && edp_held(a);
}

static bool edp_check_backing(struct edp_adapter *a, uint64_t *hash)
{
    struct scanout_profile profile;
    struct intel_capture_result owned;
    const struct intel_capture_result *capture = &result;
    if (a->well_requested) {
        owned = result;
        owned.sample[1].well_after[1] |= BDW_EDP_WELL_REQUEST;
        capture = &owned;
    }
    if (!edp_held(a) || !intel_scanout_trial_profile(&profile) ||
        profile.width != 1440 || profile.height != 900 || profile.pitch != 6144 ||
        !edp_range_valid() ||
        !trial_revalidate_capture(capture))
        return false;
    for (unsigned i = 0; i < EDP_RENDERED_PTES; ++i)
        if (backing.pte[i] != backing.pte[0] + i * 4096ull)
            return false;
    *hash = backing.sample[1].hash;
    return edp_pte_page(a, EDP_RENDERED_PTES, BDW_EDP_BACKING_PAGES, hash);
}

static bool edp_fences_preserved(struct edp_adapter *a)
{
    if (!edp_map_backing(a, 0x100000u))
        return false;
    bool match = true;
    for (unsigned i = 0; i < INTEL_BACKING_FENCES; ++i) {
        uint64_t value;
        if (!edp_read_pair(a, 0x100000u + 8 * i, &value) || value != backing.sample[1].fence[i]) {
            match = false;
            break;
        }
    }
    return edp_unmap_backing(a) && match && edp_held(a);
}

/* No route registers. This census can be used with the panel and pipe off
 * once a native display-cycle provider owns their entire lifetime. */
static bool edp_pinned_translations(struct edp_adapter *a, uint64_t *hash)
{
    struct intel_backing_endpoint endpoint;
    uint64_t begin = timer_monotonic_us();
    if (!begin || !edp_range_valid() || !edp_held(a) ||
        !backing_endpoint(&a->backing, &endpoint) || !trial_endpoint_same(&endpoint) ||
        !edp_fences_preserved(a))
        return false;
    *hash = backing.sample[1].hash;
    for (unsigned i = 0; i < BDW_EDP_BACKING_PAGES; i += 512) {
        unsigned end = i + 512 < BDW_EDP_BACKING_PAGES ? i + 512 : BDW_EDP_BACKING_PAGES;
        if (!edp_pte_page(a, i, end, hash))
            return false;
    }
    return edp_fences_preserved(a) && backing_endpoint(&a->backing, &endpoint) &&
           trial_endpoint_same(&endpoint) && edp_held(a) && timer_monotonic_us() - begin < 100000u;
}

static bool edp_observe_cycle(struct edp_adapter *a)
{
    struct edp_cycle_observation *o = a->cycle;
    if (!edp_pinned_translations(a, &o->backing_hash) ||
        !edp_read(a, BDW_EDP_PIPE_IMR, &o->imr) ||
        !edp_read(a, BDW_EDP_PIPE_IER, &o->ier) ||
        !edp_read(a, BDW_EDP_PIPE_IIR, &o->iir) ||
        !edp_read(a, 0x64010u, &o->aux) || !edp_read(a, 0x68080u, &o->fitter) ||
        !edp_read(a, 0x650c0u, &o->audio))
        return false;
    uint32_t other_ddi = 0, wm = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (!edp_read(a, 0x64100u + 0x100 * i, &o->ddi[i]))
            return false;
        other_ddi |= o->ddi[i];
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (!edp_read(a, 0x45108u + 4 * i, &o->wm[i]))
            return false;
        wm |= o->wm[i];
    }
    o->blockers = (!!(o->imr >> 31)) | ((!!(o->ier >> 31)) << 1) |
                  ((!!(o->iir >> 31)) << 2) | ((!!(o->aux >> 31)) << 3) |
                  ((!!(o->fitter >> 31)) << 4) | ((!!(o->audio & 0x444u)) << 5) |
                  ((!!(other_ddi >> 31)) << 6) | ((!!(wm >> 31)) << 7);
    o->complete = edp_held(a);
    return o->complete;
}

static bool edp_snapshot(void *context, struct bdw_edp_baseline_sample *s)
{
    struct edp_adapter *a = context;
    a->reason = 4;
    if (!edp_check_backing(a, &s->backing_hash))
        return false;
    a->reason = 5;
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t well = result.sample[1].well_after[i];
        if (i == 1 && a->well_requested)
            well |= BDW_EDP_WELL_REQUEST;
        if (!edp_expect(a, 0x45400 + 4 * i, UINT32_MAX, well))
            return false;
    }
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

static enum intel_gt_write edp_well_request(void *context, bool enable)
{
    struct edp_adapter *a = context;
    if (!a->platform || enable == a->well_requested ||
        !edp_expect(a, 0x45400, UINT32_MAX, BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON))
        return GT_WRITE_NONE;
    volatile uint32_t *reg = edp_mmio(a, BDW_EDP_WELL_DRIVER);
    if (!reg || !edp_held(a))
        return GT_WRITE_NONE;
    uint32_t before = *reg;
    uint32_t expected = BDW_EDP_WELL_ON | (enable ? 0 : BDW_EDP_WELL_REQUEST);
    if (before != expected || !edp_held(a))
        return GT_WRITE_NONE;
    *reg = before ^ BDW_EDP_WELL_REQUEST;
    a->well_requested = enable;
    return GT_WRITE_DONE;
}

static bool edp_capture_crc(void *context, bool *retained)
{
    struct edp_adapter *a = context;
    if (a->cycle && !edp_observe_cycle(a)) {
        a->reason = 11;
        *retained = a->map_fault || trial_mapping_fault;
        return false;
    }
    const struct bdw_edp_baseline_io io = {
        a, edp_held, edp_snapshot, edp_read, edp_write, now, edp_pause
    };
    bool ok = intel_bdw_edp_baseline(&io, a->crc);
    *retained = a->crc->retained || a->map_fault || trial_mapping_fault;
    return ok;
}

static bool edp_try(struct bdw_edp_baseline_report *r, struct bdw_edp_platform_report *platform,
                    struct edp_cycle_observation *cycle)
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
    a->platform = platform != NULL;
    a->crc = r;
    a->cycle = cycle;
    bool ok = false;
    a->reason = 3;
    if (a->pinned && edp_map_all(a)) {
        if (platform) {
            const struct bdw_edp_platform_io io = {
                a, edp_held, edp_read, edp_well_request, now, edp_capture_crc
            };
            uint64_t before, after;
            a->reason = 10;
            if (edp_check_backing(a, &before)) {
                ok = intel_bdw_edp_platform_trial(&io, platform);
                if (platform->released &&
                    (!edp_check_backing(a, &after) || before != after))
                    r->retained = true;
            }
            r->retained |= platform->retained;
        } else {
            ok = edp_capture_crc(a, &r->retained);
        }
    }
    a->crc = NULL;
    a->cycle = NULL;
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
    static const char *const pattern[] = {
        "SrvrOS display check - reference frame\n"
        "=====================================\n\n"
        "Automatic test. If unchanged for 60 seconds, photograph and restart.\n",
        "SrvrOS display check - alternate frame\n"
        "+++++ data +++++ data +++++ data +++++\n"
        "[::::::::] [::::::::] [::::::::]\n"
        "Automatic test. If unchanged for 60 seconds, photograph and restart.\n",
    };
    const unsigned frames[] = {0, 1, 0};
    struct bdw_edp_baseline_report r[3] = {0};
    struct bdw_edp_platform_report platform = {0};
    struct edp_cycle_observation cycle = {0};
    for (unsigned i = 0; i < 3; ++i) {
        /* Start from identical glyph, cursor and margin state every time.
         * edp_try drains presentation before it pins out other CPU writers. */
        console_clear();
        console_write(pattern[frames[i]]);
        if (!edp_try(&r[i], i == 2 ? &platform : NULL, i == 2 ? &cycle : NULL))
            break;
    }
    for (unsigned i = 0; i < platform.samples; ++i) {
        const uint32_t *w = platform.words[i];
        console_printf("intel-edp-platform-word: v=1 phase=%u bios=%x driver=%x kvmr=%x debug=%x fuse=%x pll=%x cdclk=%x gate=%x chicken=%x pps_status=%x pps_ctl=%x pps_on=%x pps_off=%x pps_div=%x cpu_pwm=%x cpu_duty=%x pch_pwm=%x pch_duty=%x\n",
            (uint64_t)i, (uint64_t)w[0], (uint64_t)w[1], (uint64_t)w[2], (uint64_t)w[3],
            (uint64_t)w[4], (uint64_t)w[5], (uint64_t)w[6], (uint64_t)w[7], (uint64_t)w[8],
            (uint64_t)w[9], (uint64_t)w[10], (uint64_t)w[11], (uint64_t)w[12], (uint64_t)w[13],
            (uint64_t)w[14], (uint64_t)w[15], (uint64_t)w[16], (uint64_t)w[17]);
    }
    console_printf("intel-edp-platform: v=1 samples=%u writes=%u reason=%u acquired=%u released=%u retained=%u begin=%u end=%u\n",
        (uint64_t)platform.samples, (uint64_t)platform.writes, (uint64_t)platform.reason,
        (uint64_t)platform.acquired, (uint64_t)platform.released, (uint64_t)platform.retained,
        platform.begin_us, platform.end_us);
    console_printf("intel-edp-cycle-preflight: v=1 complete=%u hash=%x blockers=%x imr=%x ier=%x iir=%x aux=%x fitter=%x audio=%x ddi_b=%x ddi_c=%x ddi_d=%x ddi_e=%x wm1=%x wm2=%x wm3=%x\n",
        (uint64_t)cycle.complete, cycle.backing_hash, (uint64_t)cycle.blockers,
        (uint64_t)cycle.imr, (uint64_t)cycle.ier, (uint64_t)cycle.iir, (uint64_t)cycle.aux,
        (uint64_t)cycle.fitter, (uint64_t)cycle.audio, (uint64_t)cycle.ddi[0],
        (uint64_t)cycle.ddi[1], (uint64_t)cycle.ddi[2], (uint64_t)cycle.ddi[3],
        (uint64_t)cycle.wm[0], (uint64_t)cycle.wm[1], (uint64_t)cycle.wm[2]);
    *retained = r[0].retained || r[1].retained || r[2].retained;
    uint32_t first_gap = r[1].frame[0] - r[0].frame[3];
    uint32_t last_gap = r[2].frame[0] - r[1].frame[3];
    bool same = intel_bdw_edp_baseline_same(&r[0], &r[1]) &&
                intel_bdw_edp_baseline_same(&r[0], &r[2]) &&
                first_gap && first_gap < (1u << 31) && last_gap && last_gap < (1u << 31);
    bool changed = same && r[0].crc[2] != r[1].crc[2];
    bool repeat = same && r[0].crc[2] == r[2].crc[2];
    bool complete = changed && repeat && cycle.complete &&
                    cycle.backing_hash == r[2].sample.backing_hash;
    for (unsigned i = 0; i < 3; ++i) {
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
    console_printf("intel-edp-pattern: v=1 repeat=%u\n", (uint64_t)repeat);
    console_printf("intel-edp-baseline: v=1 complete=%u same=%u changed=%u retained=%u pte_writes=0 reset=0\n",
        (uint64_t)complete, (uint64_t)same, (uint64_t)changed, (uint64_t)*retained);
    return complete;
}
