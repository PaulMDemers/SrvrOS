#include <srvros/intel_bdw_edp_baseline.h>
#include <srvros/intel_bdw_edp_platform.h>
#include <srvros/intel_bdw_pci_hold.h>
#include <srvros/intel_ggtt_window.h>
#include <srvros/x86_cache.h>
#include <srvros/display_access.h>
#include <srvros/display_present.h>

#define EDP_VA (CAPTURE_VA + 0x180000)
#define EDP_RENDERED_PTES (BDW_EDP_BACKING_PAGES - 128u)
#define EDP_LEASE 0x454450435243ull
#define EDP_BASELINE_PAGES 11u

static const uint32_t edp_pages[] = {
    0x41000, 0x43000, 0x44000, 0x45000, 0x46000, 0x60000,
    0x64000, 0x6f000, 0x70000, 0x7f000, 0x130000,
    0x42000, 0x48000, 0x65000, 0x68000, 0xc2000, 0xc7000, 0xc8000, 0x138000
};

struct edp_cycle_observation {
    uint64_t backing_hash;
    uint32_t imr, ier, iir, aux, fitter, audio, ddi[4], wm[3], blockers;
    bool complete;
};

#define EDP_FRAME_BYTES (6144u * 900u)
#define EDP_FRAME_PAGES (EDP_FRAME_BYTES / 4096u)
#define EDP_FRAME_DEADLINE_US 2000000u
#define EDP_FLR_PCI_WORDS 16u
#define EDP_CANDIDATE_SLOT 12292u
#define EDP_CANDIDATE_OFFSET (0x800000u + 8 * EDP_CANDIDATE_SLOT)
#define EDP_CENSUS_LIMIT_US 500000u
#define EDP_CENSUS_FIRST (0x2200000u / 4096u)
#define EDP_CENSUS_END (0x10000000u / 4096u)

struct edp_candidate_sample {
    uint64_t begin, end, pte[3];
    uint32_t command, af;
    unsigned reads;
    bool complete, retired;
};

struct edp_dark_census {
    uint64_t begin, end;
    uint32_t command;
    unsigned entries, empty, first_empty, framebuffer_aliases, maps, retired;
    bool complete;
};

struct edp_flr_preflight {
    uint64_t copy, pixels[3], elapsed[3];
    uint32_t pci[3][EDP_FLR_PCI_WORDS];
    unsigned pixel_samples, pci_samples;
    bool released;
    bool exercise_well, exercise_pci, exercise_pwm, exercise_pps;
    struct bdw_pci_hold_report pci_hold;
    struct bdw_edp_pwm_image pwm_image;
    struct bdw_edp_pwm_report pwm;
    struct bdw_edp_pps_image pps_image;
    struct bdw_edp_pps_report pps;
    uint64_t ram_digest;
    struct edp_candidate_sample candidate[3];
    unsigned candidate_samples;
    struct edp_dark_census census;
    struct {
        uint32_t bios, driver, pps;
    } well[3];
    unsigned well_samples, well_writes;
    uint64_t well_begin, well_elapsed;
};

enum edp_reopen_check {
    REOPEN_OWNER = 1, REOPEN_PLATFORM, REOPEN_FRAME_SETUP, REOPEN_SOURCE,
    REOPEN_TRANSLATION_ADMISSION, REOPEN_ENDPOINT_READ, REOPEN_ENDPOINT_COMMAND,
    REOPEN_ENDPOINT_MATCH, REOPEN_FENCE_MAP, REOPEN_FENCES, REOPEN_FENCE_UNMAP,
    REOPEN_GGTT_MAP, REOPEN_PTES, REOPEN_GGTT_UNMAP, REOPEN_TRANSLATION_CLOSE,
    REOPEN_BACKING_HASH, REOPEN_FRAME_GUARD, REOPEN_FRAME_MAPPING, REOPEN_FRAME_WORDS,
    REOPEN_FRAME_CLOSE, REOPEN_RELEASE_WELL, REOPEN_RECOVER_WELL,
};

struct edp_reopen_entry {
    uint64_t observed, expected;
    uint32_t check, index;
    bool ok, sampled;
};

struct edp_adapter {
    struct backing_adapter backing;
    struct display_access_token lease;
    struct bdw_edp_baseline_report *crc;
    struct edp_cycle_observation *cycle;
    struct edp_flr_preflight *flr;
    unsigned mapped, smi, reason;
    bool pinned, map_fault, platform, well_requested, run_cycle;
    bool decode_closed, pci_backing_check;
    uint64_t cycle_epoch;
    const struct bdw_edp_source_image *image;
    /* Buffered while the display lease is held. Emitted from ordinary RAM
     * after leaving the PCI hold, including on a retained failure. */
    struct edp_reopen_entry reopen[48];
    unsigned reopen_count;
    bool trace_reopen;
};
static struct edp_adapter edp_session;

static bool edp_cache_page(struct edp_adapter *a, uint64_t physical, uint32_t offset);

static bool edp_reopen_record(struct edp_adapter *a, enum edp_reopen_check check,
                              uint32_t index, bool ok, bool sampled,
                              uint64_t observed, uint64_t expected)
{
    if (a->trace_reopen && a->reopen_count < sizeof(a->reopen) / sizeof(a->reopen[0]))
        a->reopen[a->reopen_count++] = (struct edp_reopen_entry){
            .check = check, .index = index, .ok = ok, .sampled = sampled,
            .observed = observed, .expected = expected,
        };
    return ok;
}

static bool edp_reopen_ok(struct edp_adapter *a, enum edp_reopen_check check,
                          uint32_t index, bool ok)
{
    return edp_reopen_record(a, check, index, ok, false, 0, 0);
}

struct edp_native_cycle {
    struct bdw_edp_cycle_report report;
    bool attempted, well_released, retired, retained;
};

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
    if (a->platform && !(a->flr && a->flr->exercise_pci))
        --pages; /* The final, read-only RC6 page belongs to the PCI hold. */
    for (unsigned i = 0; i < pages; ++i) {
        uint32_t page = edp_pages[i];
        uint64_t va = EDP_VA + 4096ull * i;
        uint64_t physical = result.mmio_base + page, existing;
        if (!edp_held(a) || !backing_range_reserved(physical, 4096) ||
            !edp_cache_page(a, physical, page) ||
            vmm_virt_to_phys_in_space(a->backing.common.cr3, va, &existing))
            return false;
        uint64_t flags = VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE;
        if (page == 0x44000 || page == 0x60000 || (a->platform && page == 0x45000) ||
            (a->run_cycle && (page == 0x46000 || page == 0x64000 || page == 0x6f000 ||
                              page == 0x70000 || page == 0x7f000 || page == 0xc7000)) ||
            (a->flr && a->flr->exercise_pwm && page == 0xc8000))
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
    if ((offset & 3) || a->decode_closed || !edp_held(a))
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
    if (!edp_held(a) || !edp_cache_page(a, result.mmio_base + page, page))
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
    if (!edp_reopen_ok(a, REOPEN_GGTT_MAP, page, edp_map_backing(a, page)))
        return false;
    bool match = true;
    for (unsigned i = first; i < end; ++i) {
        uint64_t value = 0, expected = backing.pte[0] + i * 4096ull;
        bool sampled = edp_read_pair(a, 0x800000u + i * 8u, &value);
        if (!sampled || value != expected || (i < EDP_RENDERED_PTES && value != backing.pte[i])) {
            if (sampled && value == expected)
                expected = backing.pte[i];
            edp_reopen_record(a, REOPEN_PTES, i, false, sampled, value, expected);
            match = false;
            break;
        }
        if (i >= EDP_RENDERED_PTES) {
            *digest = (*digest ^ (uint32_t)value) * 1099511628211ull;
            *digest = (*digest ^ (value >> 32)) * 1099511628211ull;
        }
    }
    if (match)
        edp_reopen_record(a, REOPEN_PTES, first, true, true, end - first, end - first);
    bool unmapped = edp_unmap_backing(a);
    return match && edp_reopen_ok(a, REOPEN_GGTT_UNMAP, page, unmapped && edp_held(a));
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
    if (!edp_reopen_ok(a, REOPEN_FENCE_MAP, 0x100000u, edp_map_backing(a, 0x100000u)))
        return false;
    bool match = true;
    for (unsigned i = 0; i < INTEL_BACKING_FENCES; ++i) {
        uint64_t value = 0, expected = backing.sample[1].fence[i];
        bool sampled = edp_read_pair(a, 0x100000u + 8 * i, &value);
        if (!sampled || value != expected) {
            edp_reopen_record(a, REOPEN_FENCES, i, false, sampled, value, expected);
            match = false;
            break;
        }
    }
    if (match)
        edp_reopen_record(a, REOPEN_FENCES, 0, true, true, INTEL_BACKING_FENCES, INTEL_BACKING_FENCES);
    bool unmapped = edp_unmap_backing(a);
    return match && edp_reopen_ok(a, REOPEN_FENCE_UNMAP, 0x100000u, unmapped && edp_held(a));
}

static bool edp_held_endpoint_same(struct edp_adapter *a, struct intel_backing_endpoint *endpoint)
{
    if (!a->flr || !a->pci_backing_check || a->decode_closed)
        return false;
    const struct bdw_pci_hold_report *hold = &a->flr->pci_hold;
    if (!hold->retained || hold->samples != 4 || hold->writes != 3 || !hold->cpu_copy ||
        endpoint->identity.command != 0x402 || backing.sample[1].endpoint.identity.command != 7 ||
        !edp_held(a))
        return false;
    /* Check the complete endpoint after accounting for the one owned change. */
    endpoint->identity.command = 7;
    return trial_endpoint_same(endpoint);
}

static bool edp_cache_page(struct edp_adapter *a, uint64_t physical, uint32_t offset)
{
    uint64_t pages[3];
    if (a->pci_backing_check) {
        struct intel_backing_endpoint endpoint;
        struct scanout_profile profile;
        if (!intel_scanout_trial_profile(&profile) || !backing_endpoint(&a->backing, &endpoint) ||
            !edp_held_endpoint_same(a, &endpoint) || !trial_cache_pages(&endpoint, pages))
            return false;
    } else if (!intel_scanout_trial_cache_pages(pages)) {
        return false;
    }
    return pages[0] >= 0x101000 &&
           intel_cache_probe_edp_uc_page(physical, offset, pages[0] - 0x101000) && edp_held(a);
}

static bool edp_endpoint_same(struct edp_adapter *a, struct intel_backing_endpoint *endpoint,
                              unsigned phase)
{
    if (a->pci_backing_check) {
        if (!edp_reopen_record(a, REOPEN_ENDPOINT_COMMAND, phase, endpoint->identity.command == 0x402,
                               true, endpoint->identity.command, 0x402))
            return false;
        return edp_reopen_ok(a, REOPEN_ENDPOINT_MATCH, phase, edp_held_endpoint_same(a, endpoint));
    }
    return edp_reopen_ok(a, REOPEN_ENDPOINT_MATCH, phase, trial_endpoint_same(endpoint));
}

static bool edp_check_endpoint(struct edp_adapter *a, unsigned phase)
{
    struct intel_backing_endpoint endpoint;
    return edp_reopen_ok(a, REOPEN_ENDPOINT_READ, phase, backing_endpoint(&a->backing, &endpoint)) &&
           edp_endpoint_same(a, &endpoint, phase);
}

/* No route registers. This census can be used with the panel and pipe off
 * once a native display-cycle provider owns their entire lifetime. */
static bool edp_pinned_translations(struct edp_adapter *a, uint64_t *hash)
{
    uint64_t begin = timer_monotonic_us();
    if (!edp_reopen_ok(a, REOPEN_TRANSLATION_ADMISSION, 0,
                       begin && !a->decode_closed && edp_range_valid() && edp_held(a)) ||
        !edp_check_endpoint(a, 0) ||
        !edp_fences_preserved(a))
        return false;
    *hash = backing.sample[1].hash;
    for (unsigned i = 0; i < BDW_EDP_BACKING_PAGES; i += 512) {
        unsigned end = i + 512 < BDW_EDP_BACKING_PAGES ? i + 512 : BDW_EDP_BACKING_PAGES;
        if (!edp_pte_page(a, i, end, hash))
            return false;
    }
    return edp_fences_preserved(a) && edp_check_endpoint(a, 1) &&
           edp_reopen_ok(a, REOPEN_TRANSLATION_CLOSE, 0, edp_held(a) && timer_monotonic_us() - begin < 100000u);
}

static bool edp_observe_cycle(struct edp_adapter *a, struct edp_cycle_observation *o)
{
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

/* Only the known profile owns these global endpoints, kept live through
 * the display-only cycle. PPS timing/PWM and clocks are read-only here. */
static bool edp_platform_word(struct edp_adapter *a, uint32_t offset, uint32_t mask, uint32_t expected)
{
    uint32_t value = 0;
    bool sampled = edp_read(a, offset, &value);
    return edp_reopen_record(a, REOPEN_PLATFORM, offset, sampled && (value & mask) == expected,
                             sampled, value, expected);
}

static bool edp_cycle_platform(struct edp_adapter *a, bool driver)
{
    static const uint32_t regs[][2] = {
        {0x45400, 0xc0000000}, {0x45408, 0x40000000}, {0x4540c, 0x40000000},
        {0x130040, 0x4c000000}, {0x46200, 0x2a2}, {0xc2020, 0x1000}, {0xc2004, 0x24},
        {BDW_EDP_PPS_ON_DELAYS, BDW_EDP_PPS_ON_AIR72}, {BDW_EDP_PPS_OFF_DELAYS, BDW_EDP_PPS_OFF_AIR72},
        {BDW_EDP_PPS_DIVISOR, BDW_EDP_PPS_DIV_AIR72},
    };
    for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); ++i)
        if (!edp_platform_word(a, regs[i][0], UINT32_MAX, regs[i][1]))
            return false;
    if (!edp_platform_word(a, BDW_EDP_WELL_DRIVER, UINT32_MAX,
                           BDW_EDP_WELL_ON | (driver ? BDW_EDP_WELL_REQUEST : 0)) ||
        !edp_platform_word(a, 0x48250, 1u << 31, 0) ||
        !edp_platform_word(a, 0xc8250, 0xc0000000u, 0xc0000000u))
        return false;
    uint32_t pwm = 0;
    bool sampled = edp_read(a, 0xc8254, &pwm);
    return edp_reopen_record(a, REOPEN_PLATFORM, 0xc8254,
                             sampled && (pwm >> 16) == 0x0ad9u && (pwm & 0xffffu) <= (pwm >> 16),
                             sampled, pwm, 0x0ad90000u);
}

static bool edp_source_held(void *context, const struct bdw_edp_source_image *image)
{
    struct edp_adapter *a = context;
    return a->run_cycle && a->well_requested && a->image && image->backing == &backing &&
           image->epoch == a->cycle_epoch && image->backing_hash == a->image->backing_hash &&
           image->backing_physical == (backing.pte[0] & ~4095ull) &&
           image->mapped_bytes == BDW_EDP_BACKING_PAGES * 4096ull &&
           image->reference_crc == a->image->reference_crc &&
           edp_expect(a, BDW_EDP_WELL_DRIVER, UINT32_MAX, BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON) &&
           edp_expect(a, 0x45400, UINT32_MAX, BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON);
}

static bool edp_source_backing(void *context, const struct bdw_edp_source_image *image,
                               uint64_t *hash)
{
    return edp_reopen_ok(context, REOPEN_SOURCE, 0, edp_source_held(context, image)) &&
           edp_pinned_translations(context, hash);
}

static enum intel_gt_write edp_well_request(void *context, bool enable);

static bool edp_dark_well_sample(struct edp_adapter *a, bool requested)
{
    struct edp_flr_preflight *f = a->flr;
    unsigned n = f->well_samples;
    if (!edp_held(a) || !edp_cycle_platform(a, requested) ||
        !edp_read(a, 0x45400, &f->well[n].bios) ||
        !edp_read(a, BDW_EDP_WELL_DRIVER, &f->well[n].driver) ||
        !edp_read(a, 0xc7200, &f->well[n].pps) ||
        !edp_expect(a, 0xc7204, UINT32_MAX, 0))
        return false;
    uint64_t end = now(a);
    if (end < f->well_begin || end - f->well_begin < f->well_elapsed ||
        end - f->well_begin >= 100000 ||
        f->well[n].bios != (BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON) ||
        f->well[n].driver != (BDW_EDP_WELL_ON | (requested ? BDW_EDP_WELL_REQUEST : 0)) ||
        (f->well[n].pps & 0xb0000000u) || !edp_held(a))
        return false;
    f->well_elapsed = end - f->well_begin;
    ++f->well_samples;
    return true;
}

/* The original CPU and backing pin remain live while the driver releases
 * and reclaims its own bit. Firmware must keep the physical well powered. */
static bool edp_dark_well(struct edp_adapter *a)
{
    struct edp_flr_preflight *f = a->flr;
    f->well_begin = now(a);
    if (!f->well_begin || !edp_dark_well_sample(a, true))
        return false;
    for (unsigned step = 0; step < 2; ++step) {
        bool enable = step != 0;
        uint64_t before = now(a);
        if (before < f->well_begin || before - f->well_begin >= 100000)
            return false;
        enum intel_gt_write status = edp_well_request(a, enable);
        if (status != GT_WRITE_NONE)
            ++f->well_writes;
        if (status != GT_WRITE_DONE || !edp_dark_well_sample(a, enable))
            return false;
    }
    return f->well_elapsed != 0;
}

static bool edp_flr_config(struct edp_adapter *a)
{
    static const uint8_t offsets[EDP_FLR_PCI_WORDS] = {
        0, 8, 0x2c, 4, 0x10, 0x14, 0x18, 0x1c, 0x20, 0x34, 0x90, 0xd0, 0xd4, 0xa4, 0xa8, 0xfc
    };
    struct edp_flr_preflight *f = a->flr;
    unsigned n = f->pci_samples;
    if (n >= 3)
        return false;
    uint32_t *w = f->pci[n];
    for (unsigned i = 0; i < EDP_FLR_PCI_WORDS; ++i) {
        if (!edp_source_held(a, a->image))
            return false;
        w[i] = pci_read_config32(a->backing.common.gpu, offsets[i]);
        if (w[i] == UINT32_MAX || (n && w[i] != f->pci[0][i]))
            return false;
        if ((i == 0 && w[i] != 0x16268086u) || (i == 1 && w[i] != 0x03000009u) ||
            (i == 2 && w[i] != 0x011b106bu) || (i == 3 && (w[i] & 0x100007u) != 0x100007u) ||
            (i == 9 && (w[i] & 255u) != 0x90u) || (i == 10 && (w[i] & 65535u) != 0xd005u) ||
            (i == 11 && (w[i] & 0x7ffffu) != 0x2a401u) || (i == 12 && (w[i] & 3u)) ||
            (i == 13 && w[i] != 0x03060013u) || (i == 14 && (w[i] & ~0x100u)))
            return false;
    }
    if (!edp_source_held(a, a->image))
        return false;
    ++f->pci_samples;
    return true;
}

/* Clone the proven CPU aperture view into ordinary RAM while decode is
 * usable. This backup remains CPU-owned across any later PCI reset. Each
 * aperture load is covered by the original held translation census. */
static bool edp_flr_pixels(struct edp_adapter *a)
{
    struct edp_flr_preflight *f = a->flr;
    unsigned n = f->pixel_samples;
    uint64_t va = (uint64_t)a->backing.fb->address, hash, begin = now(a);
    if (!edp_reopen_ok(a, REOPEN_FRAME_SETUP, 0, n < 3 && begin && !(va & 4095u) &&
                       !(result.fb.physical & 4095u) && va <= UINT64_MAX - EDP_FRAME_BYTES) ||
        !edp_source_backing(a, a->image, &hash) ||
        !edp_reopen_record(a, REOPEN_BACKING_HASH, 0, hash == a->image->backing_hash, true,
                           hash, a->image->backing_hash))
        return false;
    if (!f->copy) {
        f->copy = pmm_alloc_frames(EDP_FRAME_PAGES);
        if (!f->copy)
            return false;
    }
    uint32_t *copy = pmm_phys_to_virt(f->copy);
    uint64_t pixels = 14695981039346656037ull;
    uint32_t variation = 0, first = 0;
    /* Fence completed CPU aperture stores before copying the reference. */
    __asm__ volatile("mfence" ::: "memory");
    for (unsigned page = 0; page < EDP_FRAME_PAGES; ++page) {
        uint64_t physical = 0, source = va + 4096ull * page;
        uint64_t expected = result.fb.physical + 4096ull * page;
        if (!edp_source_held(a, a->image) || now(a) - begin >= EDP_FRAME_DEADLINE_US)
            return edp_reopen_ok(a, REOPEN_FRAME_GUARD, page, false);
        bool mapped = vmm_virt_to_phys_in_space(a->backing.common.cr3, source, &physical);
        if (!mapped || physical != expected)
            return edp_reopen_record(a, REOPEN_FRAME_MAPPING, page, false, mapped,
                                     mapped ? physical : 0, expected);
        volatile const uint32_t *words = (volatile const uint32_t *)source;
        for (unsigned i = 0; i < 1024; ++i) {
            uint32_t word = words[i];
            if (!page && !i)
                first = word;
            variation |= word ^ first;
            if (!n)
                *copy = word;
            else if (*copy != word)
                return edp_reopen_ok(a, REOPEN_FRAME_WORDS, page * 1024 + i, false);
            ++copy;
            pixels = (pixels ^ word) * 1099511628211ull;
        }
    }
    edp_reopen_record(a, REOPEN_FRAME_GUARD, 0, true, true, EDP_FRAME_PAGES, EDP_FRAME_PAGES);
    edp_reopen_record(a, REOPEN_FRAME_MAPPING, 0, true, true, EDP_FRAME_PAGES, EDP_FRAME_PAGES);
    edp_reopen_record(a, REOPEN_FRAME_WORDS, 0, true, true, EDP_FRAME_BYTES / 4, EDP_FRAME_BYTES / 4);
    uint64_t done = now(a);
    if (!edp_reopen_record(a, REOPEN_FRAME_CLOSE, 0,
                           variation && pixels && done > begin && done - begin < EDP_FRAME_DEADLINE_US &&
                           (!n || pixels == f->pixels[0]) && edp_source_held(a, a->image),
                           true, pixels, n ? f->pixels[0] : pixels))
        return false;
    f->pixels[n] = pixels;
    f->elapsed[n] = done - begin;
    ++f->pixel_samples;
    return true;
}

static void edp_pause(void *context, uint32_t us);

static bool edp_pci_hold_held(void *context)
{
    struct edp_adapter *a = context;
    return a->flr && a->flr->exercise_pci && a->image && a->well_requested &&
           !a->map_fault && !trial_mapping_fault && edp_held(a);
}

static bool edp_pci_hold_sample(void *context, struct bdw_pci_hold_sample *s)
{
    struct edp_adapter *a = context;
    const struct pci_device *p = a->backing.common.gpu;
    if (!edp_pci_hold_held(a) || p->bus || p->device != 2 || p->function ||
        pci_read_config32(p, 0) != 0x16268086u || !edp_pci_hold_held(a))
        return false;
    s->command = pci_read_config32(p, 4);
    s->af = pci_read_config32(p, 0xa8);
    return edp_pci_hold_held(a);
}

static bool edp_read_dark_rc6(void *context, struct bdw_pci_rc6_sample *s)
{
    struct edp_adapter *a = context;
    return edp_pci_hold_held(a) &&
           edp_read(a, 0x138060, &s->core_status) &&
           edp_read(a, 0x138108, &s->residency) &&
           edp_read(a, INTEL_GT_ACK, &s->wake_ack) && edp_pci_hold_held(a);
}

static enum intel_gt_write edp_pci_hold_command(void *context, uint16_t before, uint16_t after)
{
    static const uint16_t commands[] = {7, 0x403, 0x400, 0x402, 7};
    struct edp_adapter *a = context;
    struct bdw_pci_hold_sample s;
    if (!edp_pci_hold_held(a))
        return GT_WRITE_NONE;
    unsigned n = a->flr->pci_hold.writes;
    const struct pci_device *p = a->backing.common.gpu;
    if (n >= 4 || before != commands[n] || after != commands[n+1] ||
        pci_read_config32(p, 0x90) != a->flr->pci[0][10] ||
        pci_read_config32(p, 0xd4) != a->flr->pci[0][12] ||
        !edp_pci_hold_sample(a, &s) || s.command != ((a->flr->pci[0][3] & 0xffff0000u) | before) ||
        (n ? s.af != 0 : (s.af & ~0x100u)))
        return GT_WRITE_NONE;
    if (after == 0x400)
        a->decode_closed = true;
    pci_write_config16(p, 4, after);
    return GT_WRITE_DONE;
}

static bool edp_pci_hold_copy(void *context)
{
    struct edp_adapter *a = context;
    if (!edp_pci_hold_held(a) || !a->decode_closed || !a->flr->copy ||
        a->flr->pixel_samples != 1)
        return false;
    const uint32_t *words = pmm_phys_to_virt(a->flr->copy);
    uint64_t digest = 14695981039346656037ull;
    for (unsigned i = 0; i < EDP_FRAME_BYTES / sizeof(*words); ++i)
        digest = (digest ^ words[i]) * 1099511628211ull;
    a->flr->ram_digest = digest;
    return digest == a->flr->pixels[0] && edp_pci_hold_held(a);
}

/* The outer PCI hold and CPU backup span the temporary loss of the driver
 * request. This guard must therefore be independent of edp_source_held(). */
static bool edp_recovery_held(void *context)
{
    struct edp_adapter *a = context;
    if (!a->pci_backing_check || a->decode_closed || !a->flr || !a->image ||
        a->image->epoch != a->cycle_epoch || a->image->backing != &backing ||
        a->map_fault || trial_mapping_fault || !edp_held(a))
        return false;
    const struct edp_flr_preflight *f = a->flr;
    const struct bdw_pci_hold_report *h = &f->pci_hold;
    const struct pci_device *p = a->backing.common.gpu;
    return f->exercise_pci && f->pci_samples == 1 && f->copy && f->pixel_samples == 1 &&
           h->cpu_copy && h->retained && h->samples == 4 && h->writes == 3 &&
           !p->bus && p->device == 2 && !p->function &&
           pci_read_config32(p, 0) == 0x16268086u &&
           pci_read_config32(p, 4) == ((f->pci[0][3] & 0xffff0000u) | 0x402u) &&
           pci_read_config32(p, 0x10) == f->pci[0][4] && pci_read_config32(p, 0x14) == f->pci[0][5] &&
           pci_read_config32(p, 0xa8) == 0 && edp_held(a);
}

static enum intel_gt_write edp_recovery_write(void *context, uint32_t reg, uint32_t value)
{
    if (reg != BDW_EDP_WELL_DRIVER || value != BDW_EDP_WELL_REQUEST || !edp_recovery_held(context))
        return GT_WRITE_NONE;
    return edp_well_request(context, true);
}

static bool edp_recover_held_well(struct edp_adapter *a)
{
    uint32_t driver = 0;
    bool released = edp_recovery_held(a) && edp_expect(a, 0xc7204, UINT32_MAX, 0) &&
                    edp_expect(a, 0xc7200, 0xf0000000u, 0) &&
                    edp_well_request(a, false) == GT_WRITE_DONE &&
                    edp_recovery_held(a) && edp_read(a, BDW_EDP_WELL_DRIVER, &driver);
    if (!edp_reopen_record(a, REOPEN_RELEASE_WELL, 0, released && driver == BDW_EDP_WELL_ON,
                           released, driver, BDW_EDP_WELL_ON))
        return false;
    const struct bdw_edp_dark_io io = {a, edp_recovery_held, edp_read, edp_recovery_write, now, edp_pause};
    struct bdw_edp_reopen_report r;
    bool ok = intel_bdw_edp_reopen_resources(&io, &r) && r.writes == 1 &&
              r.phase == BDW_EDP_REOPEN_READY && !r.retained && edp_recovery_held(a);
    return edp_reopen_record(a, REOPEN_RECOVER_WELL, r.phase, ok, true, r.driver_after,
                             BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON);
}

static bool edp_candidate_guard(struct edp_adapter *a, unsigned phase, struct bdw_pci_hold_sample *pci)
{
    bool dark = phase == 1;
    const struct edp_flr_preflight *f = a->flr;
    if (!edp_source_held(a,a->image) || (dark && !edp_recovery_held(a)) ||
        !edp_pci_hold_sample(a,pci) || pci->af ||
        pci->command != ((f->pci[0][3] & 0xffff0000u) | (dark ? 0x402u : 7u)) ||
        !edp_expect(a,0xc7204,UINT32_MAX,dark ? 0u : 7u) ||
        !edp_expect(a,0xc7200,0xb0000000u,dark ? 0u : 0x80000000u) ||
        !edp_expect(a,0x70180,1u << 31,dark ? 0u : 1u << 31)) return false;
    return dark || (edp_expect(a,0x7019c,~8u,0) && edp_expect(a,0x701ac,UINT32_MAX,0));
}

/* The trial reads a fixed occupied candidate and both neighbors. The raw
 * PTEs supply no ownership. A clean sample failure must permit recovery. */
static void edp_candidate_sample(struct edp_adapter *a, unsigned phase)
{
    struct edp_flr_preflight *f = a->flr;
    if (f->candidate_samples != phase || phase >= 3) return;
    struct edp_candidate_sample *s = &f->candidate[f->candidate_samples++];
    struct bdw_pci_hold_sample before, after;
    s->begin = now(a);
    if (!s->begin || !edp_candidate_guard(a,phase,&before)) return;
    s->command = before.command;
    s->af = before.af;
    if (!edp_map_backing(a,EDP_CANDIDATE_OFFSET & ~4095u)) return;
    for (unsigned i = 0; i < 3; ++i) {
        uint64_t word;
        if (!edp_read_pair(a,EDP_CANDIDATE_OFFSET - 8 + 8 * i,&word) || word == UINT64_MAX) break;
        s->pte[s->reads++] = word;
    }
    s->retired = edp_unmap_backing(a);
    bool stable = s->retired && edp_candidate_guard(a,phase,&after) && before.command == after.command;
    s->end = now(a);
    s->complete = stable && s->reads == 3 && s->end > s->begin && s->end - s->begin < 100000u;
}

static bool edp_candidate_preserved(const struct edp_flr_preflight *f)
{
    return f->candidate_samples == 3 && f->candidate[0].complete && f->candidate[1].complete &&
           f->candidate[2].complete &&
           !__builtin_memcmp(f->candidate[0].pte,f->candidate[1].pte,sizeof(f->candidate[0].pte)) &&
           !__builtin_memcmp(f->candidate[0].pte,f->candidate[2].pte,sizeof(f->candidate[0].pte));
}

static bool edp_census_clock(struct edp_adapter *a, struct edp_dark_census *census)
{
    uint64_t at = now(a);
    if (at < census->end) return false;
    census->end = at;
    return at - census->begin < EDP_CENSUS_LIMIT_US;
}

/* This diagnostic never maps the aperture or writes a translation. Sample the
 * GGTT only after recovery verified the inherited backing with BME still off. */
static void edp_dark_census(struct edp_adapter *a)
{
    struct edp_dark_census *c = &a->flr->census;
    const struct pci_device *gpu = a->backing.common.gpu;
    const uint32_t *cfg = a->flr->pci[0];
    uint64_t raw = ((uint64_t)cfg[7] << 32) | cfg[6], aperture, bytes;
    struct bdw_pci_hold_sample pci;
    c->begin = c->end = now(a);
    if (!c->begin || !edp_candidate_guard(a,1,&pci) ||
        !intel_ggtt_aperture(pci_read_config8(gpu,0x62),raw,&aperture,&bytes) ||
        pci_read_config32(gpu,0x18) != cfg[6] || pci_read_config32(gpu,0x1c) != cfg[7] ||
        bytes != EDP_CENSUS_END * 4096ull || result.mmio_base != 0xc0000000u ||
        backing.ggtt_bytes < EDP_CENSUS_END * 8ull || backing.sample[1].fence[0] != 0x021ff02f00000001ull)
        return;
    c->command = pci.command;
    const unsigned entries_per_page = 4096u / sizeof(uint64_t);
    for (unsigned first = EDP_CENSUS_FIRST; first < EDP_CENSUS_END; first += entries_per_page) {
        const uint32_t offset = 0x800000 + first * 8u;
        const uint64_t physical = result.mmio_base + offset;
        struct x86_cache_snapshot cache;
        struct vmm_alias_observation alias;
        unsigned mtrr, pat, effective;
        if (!edp_census_clock(a,c) || !edp_candidate_guard(a,1,&pci) ||
            !intel_cache_probe_context(&cache) || !x86_cache_page(&cache,physical,2,&mtrr,&pat,&effective) ||
            mtrr || effective) return;
        if (!backing_map(&a->backing,offset)) {
            uint64_t existing;
            a->map_fault |= vmm_virt_to_phys_in_space(a->backing.common.cr3,BACKING_VA,&existing);
            return;
        }
        ++c->maps;
        bool ok = vmm_diagnostic_kernel_alias(a->backing.common.cr3,BACKING_VA,physical,1,
                                               cache.cpu.physical_bits,&alias,NULL) &&
                  alias.pages == 1 && alias.leaves == 1 && alias.pat_index == 2 &&
                  !alias.writable && !alias.user && alias.no_execute == 1 && alias.leaf_sizes == 1;
        for (unsigned i = 0; ok && i < entries_per_page; ++i) {
            uint64_t word, again;
            const uint32_t entry = offset + 8 * i;
            ok = ((i & 31u) || edp_census_clock(a,c)) && edp_read_pair(a,entry,&word) && word != UINT64_MAX;
            if (!ok) break;
            if (!word) {
                ok = edp_read_pair(a,entry,&again) && !again;
                if (!ok) break;
                if (!c->empty) c->first_empty = first + i;
                ++c->empty;
            }
            c->framebuffer_aliases += word == backing.pte[0];
            ++c->entries;
        }
        if (!edp_unmap_backing(a)) return;
        ++c->retired;
        if (!ok) return;
    }
    c->complete = edp_candidate_guard(a,1,&pci) && edp_census_clock(a,c) && c->end > c->begin;
}

static bool edp_pci_hold_backing(void *context)
{
    struct edp_adapter *a = context;
    a->trace_reopen = true;
    if (!edp_reopen_ok(a, REOPEN_OWNER, 0, a->decode_closed && edp_pci_hold_held(a))) {
        a->trace_reopen = false;
        return false;
    }
    /* The caller has positively read back MEM on with BME still off. */
    a->decode_closed = false;
    a->pci_backing_check = true;
    bool ok = edp_cycle_platform(a, true) && edp_recover_held_well(a);
    if (ok) edp_candidate_sample(a,1);
    ok = ok && !a->map_fault && edp_flr_pixels(a);
    if (ok) edp_dark_census(a);
    ok = ok && !a->map_fault;
    a->pci_backing_check = false;
    a->trace_reopen = false;
    return ok;
}

static bool edp_pwm_held(void *context)
{
    struct edp_adapter *a = context;
    return a->flr && a->flr->exercise_pwm && a->image && edp_source_held(a, a->image);
}

static enum intel_gt_write edp_pwm_write(void *context, uint32_t offset, uint32_t value)
{
    struct edp_adapter *a = context;
    static const uint32_t registers[] = {
        BDW_EDP_PWM_DATA, BDW_EDP_PWM_CONTROL, BDW_EDP_PWM_DATA, BDW_EDP_PWM_CONTROL, BDW_EDP_PWM_DATA
    };
    if (!edp_pwm_held(a))
        return GT_WRITE_NONE;
    const struct bdw_edp_pwm_image *saved = &a->flr->pwm_image;
    const uint32_t period = saved->data & 0xffff0000u;
    const uint32_t values[] = {period, 0x40000000u, period, saved->control, saved->data};
    unsigned n = a->flr->pwm.writes;
    if (n >= sizeof(registers) / sizeof(registers[0]) || registers[n] != offset || values[n] != value)
        return GT_WRITE_NONE;
    volatile uint32_t *word = edp_mmio(a, offset);
    if (!word || !edp_pwm_held(a))
        return GT_WRITE_NONE;
    *word = value;
    return GT_WRITE_DONE;
}

static bool edp_rehearse_pwm(struct edp_adapter *a)
{
    const struct bdw_edp_pwm_io io = {a, edp_pwm_held, edp_read, edp_pwm_write, now};
    return intel_bdw_edp_pwm_restore(&io, a->flr->pwm_image, true, &a->flr->pwm);
}

static bool edp_pps_held(void *context)
{
    struct edp_adapter *a = context;
    return a->flr && a->flr->exercise_pps && a->image && edp_source_held(a, a->image);
}

static enum intel_gt_write edp_pps_write(void *context, uint32_t offset, uint32_t value)
{
    struct edp_adapter *a = context;
    const uint32_t regs[] = {
        BDW_EDP_PPS_ON_DELAYS, BDW_EDP_PPS_OFF_DELAYS,
        BDW_EDP_PPS_ON_DELAYS, BDW_EDP_PPS_OFF_DELAYS, BDW_EDP_PPS_DIVISOR,
    };
    const uint32_t values[] = {0, 0, BDW_EDP_PPS_ON_AIR72, BDW_EDP_PPS_OFF_AIR72, BDW_EDP_PPS_DIV_AIR72};
    if (!edp_pps_held(a))
        return GT_WRITE_NONE;
    unsigned n = a->flr->pps.writes;
    if (n >= sizeof(regs) / sizeof(regs[0]) || offset != regs[n] || value != values[n])
        return GT_WRITE_NONE;
    volatile uint32_t *word = edp_mmio(a, offset);
    if (!word || !edp_expect(a, 0xc7204, UINT32_MAX, 0) ||
        !edp_expect(a, 0xc7200, 0xf8000000u, 0) || !edp_pps_held(a))
        return GT_WRITE_NONE;
    *word = value;
    return GT_WRITE_DONE;
}

static bool edp_rehearse_pps(struct edp_adapter *a)
{
    const struct bdw_edp_dark_io io = {a, edp_pps_held, edp_read, edp_pps_write, now, edp_pause};
    return intel_bdw_edp_pps_restore(&io, a->flr->pps_image, true, &a->flr->pps);
}

static enum bdw_edp_dark_result edp_flr_dark(void *context, const struct bdw_edp_source_image *image)
{
    struct edp_adapter *a = context;
    if (!edp_source_held(a, image) || (a->flr->exercise_well && !edp_dark_well(a)))
        return BDW_EDP_DARK_REFUSED;
    if (a->flr->exercise_pwm && !edp_rehearse_pwm(a))
        return BDW_EDP_DARK_REFUSED;
    if (a->flr->exercise_pps && !edp_rehearse_pps(a))
        return BDW_EDP_DARK_REFUSED;
    if (a->flr->exercise_pci) {
        const struct bdw_pci_hold_io io = {
            a, edp_pci_hold_held, edp_pci_hold_sample, edp_pci_hold_command, now, edp_pause,
            edp_read_dark_rc6, edp_pci_hold_copy, edp_pci_hold_backing,
        };
        if (a->flr->pci_samples != 1 || (a->flr->pci[0][10] & 0x10000u) ||
            !intel_bdw_pci_hold(&io, &a->flr->pci_hold))
            return BDW_EDP_DARK_REFUSED;
    } else if (!edp_flr_pixels(a)) {
        return BDW_EDP_DARK_REFUSED;
    }
    /* A read-only config refusal can still recover the original display. */
    (void)edp_flr_config(a);
    return BDW_EDP_DARK_UNCHANGED;
}

static enum intel_gt_write edp_source_write(void *context, uint32_t offset, uint32_t value)
{
    struct edp_adapter *a = context;
    if (!a->image || !edp_source_held(a, a->image))
        return GT_WRITE_NONE;
    if (offset == BDW_EDP_CRC_CTL || offset == BDW_EDP_PIPE_IIR)
        return edp_write(a, offset, value);
    if (offset == BDW_EDP_PIPE_IMR) {
        uint32_t before;
        if (!edp_read(a, offset, &before))
            return GT_WRITE_NONE;
        if ((before ^ value) == BDW_EDP_CRC_DONE)
            return edp_write(a, offset, value);
        if ((before ^ value) != BDW_EDP_FIFO_UNDERRUN ||
            (value & ~BDW_EDP_FIFO_UNDERRUN) != (a->cycle->imr & ~BDW_EDP_FIFO_UNDERRUN) ||
            !edp_expect(a, BDW_EDP_PIPE_IER, UINT32_MAX, 0) ||
            !edp_expect(a, BDW_EDP_PIPE_IIR, BDW_EDP_FIFO_UNDERRUN | BDW_EDP_CRC_IRQS, 0))
            return GT_WRITE_NONE;
    } else {
        switch (offset) {
        case 0x45100: case 0x45270: case 0x46100: case 0x6001c:
        case 0x64000: case 0x64010: case 0x64014: case 0x64018:
        case 0x6401c: case 0x64020: case 0x64024: case 0x64040:
        case 0x6f000: case 0x6f004: case 0x6f008: case 0x6f00c:
        case 0x6f010: case 0x6f014: case 0x6f030: case 0x6f034:
        case 0x6f040: case 0x6f044: case 0x6f400: case 0x6f410:
        case 0x70030: case 0x70180: case 0x70188: case 0x7019c: case 0x701a4:
        case 0x7f008:
            break;
        case 0xc7204:
            if (value != 0 && value != 3 && value != 7)
                return GT_WRITE_NONE;
            break;
        default:
            if (offset < 0x64e00 || offset >= 0x64e00 + 8 * BDW_EDP_PHY_ENTRIES ||
                (offset & 3) || value != a->image->phy[(offset - 0x64e00) / 8][(offset / 4) % 2])
                return GT_WRITE_NONE;
            break;
        }
    }
    volatile uint32_t *reg = edp_mmio(a, offset);
    if (!reg || !edp_source_held(a, a->image))
        return GT_WRITE_NONE;
    *reg = value;
    return GT_WRITE_DONE;
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
    if (a->cycle && !edp_observe_cycle(a, a->cycle)) {
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

static bool edp_repeat(const struct bdw_edp_baseline_report r[3])
{
    uint32_t first_gap = r[1].frame[0] - r[0].frame[3];
    uint32_t last_gap = r[2].frame[0] - r[1].frame[3];
    return intel_bdw_edp_baseline_same(&r[0], &r[1]) &&
           intel_bdw_edp_baseline_same(&r[0], &r[2]) &&
           first_gap && first_gap < (1u << 31) && last_gap && last_gap < (1u << 31);
}

static bool edp_native_cycle(struct edp_adapter *a, const struct bdw_edp_baseline_report *ref,
                             struct edp_native_cycle *native)
{
    struct edp_cycle_observation current = {0};
    uint32_t pwm;
    if (!edp_cycle_platform(a, false) || !edp_read(a, 0xc8254, &pwm) ||
        !edp_observe_cycle(a, &current) || current.backing_hash != ref->sample.backing_hash ||
        current.blockers != 1 || a->cycle->blockers != 1 ||
        current.imr != ref->original_mask || current.imr != a->cycle->imr ||
        current.ier || current.iir || ref->sample.cdclk != 674u)
        return false;
    if (a->flr && a->flr->exercise_pwm) {
        a->flr->pwm_image.data = pwm;
        if (!edp_read(a, BDW_EDP_PWM_CONTROL, &a->flr->pwm_image.control))
            return false;
    }
    if (a->flr && a->flr->exercise_pps) {
        struct bdw_edp_pps_image *saved = &a->flr->pps_image;
        if (!edp_read(a, BDW_EDP_PPS_ON_DELAYS, &saved->on) ||
            !edp_read(a, BDW_EDP_PPS_OFF_DELAYS, &saved->off) ||
            !edp_read(a, BDW_EDP_PPS_DIVISOR, &saved->divisor))
            return false;
    }
    struct bdw_edp_source_image image = {
        .backing = &backing, .epoch = now(a), .mapped_bytes = BDW_EDP_BACKING_PAGES * 4096ull,
        .backing_physical = backing.pte[0] & ~4095ull,
        .backing_hash = ref->sample.backing_hash, .reference_crc = ref->crc[2],
        .cdclk_khz = 675000, .ddi_lane_bits = ref->sample.buffer & 0x10010u,
    };
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        for (unsigned j = 0; j < 2; ++j)
            image.phy[i][j] = ref->sample.phy[i][j];
    if (!image.epoch || edp_well_request(a, true) != GT_WRITE_DONE)
        return false;
    a->image = &image;
    a->cycle_epoch = image.epoch;
    native->attempted = true;
    native->retained = true;
    const struct bdw_edp_source_io io = {
        a, edp_source_held, edp_read, edp_source_write, now, edp_pause, edp_source_backing,
    };
    bool flr = a->flr != NULL;
    bool ready = edp_cycle_platform(a, true) && (!flr || (edp_flr_config(a) && edp_flr_pixels(a)));
    if (ready && flr && a->flr->exercise_pci) edp_candidate_sample(a,0);
    bool ok = ready && !a->map_fault && intel_bdw_edp_source_cycle(&io, &image, ref,
                                                  flr ? edp_flr_dark : NULL, a, &native->report);
    if (ok && flr) {
        bool pixels = edp_flr_pixels(a);
        if (a->flr->pci_samples == 2)
            (void)edp_flr_config(a);
        if (!pixels)
            native->report.retained = true;
        if (pixels && a->flr->exercise_pci) edp_candidate_sample(a,2);
    }
    a->image = NULL;
    if (!native->report.retained && !a->map_fault && !trial_mapping_fault &&
        edp_cycle_platform(a, true) && edp_expect(a, 0xc8254, UINT32_MAX, pwm) &&
        edp_well_request(a, false) == GT_WRITE_DONE && edp_cycle_platform(a, false) &&
        edp_expect(a, 0xc8254, UINT32_MAX, pwm)) {
        native->retained = false;
        native->well_released = true;
    }
    return ok && native->well_released;
}

static bool edp_try(struct bdw_edp_baseline_report *r, struct bdw_edp_platform_report *platform,
                    struct edp_cycle_observation *cycle,
                    struct edp_native_cycle *native, struct edp_flr_preflight *flr,
                    struct bdw_edp_baseline_report *windows)
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
    a->flr = flr;
    a->run_cycle = native != NULL;
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
    if (ok && native && !r->retained && edp_repeat(windows) &&
        windows[0].crc[2] == r->crc[2] && windows[1].crc[2] != r->crc[2])
        edp_native_cycle(a, r, native);
    a->crc = NULL;
    a->cycle = NULL;
    if (native && native->retained)
        return false;
    bool retired = !r->retained && !a->map_fault && !trial_mapping_fault && edp_unmap_all(a);
    if (retired && a->pinned)
        retired = display_access_unpin(&a->lease);
    if (!retired || !display_access_release(&a->lease, true)) {
        if (native)
            native->retained = true;
        else
            r->retained = true;
    } else if (native) {
        native->retired = true;
        if (flr && flr->copy) {
            pmm_free_frame_range(flr->copy, EDP_FRAME_PAGES);
            flr->released = true;
        }
    }
    if (r->retained)
        r->status = BDW_EDP_BASELINE_INCOMPLETE;
    r->reason = r->retained ? 9 : a->reason ? a->reason : ok ? 0 : 8;
    return ok && !r->retained;
}

bool intel_edp_baseline_probe(bool *retained, bool display_cycle, bool flr_preflight,
                             bool dark_well, bool pci_hold, bool dark_pwm, bool dark_pps)
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
    struct edp_native_cycle native = {0};
    /* This one-shot report remains available after a retained hold. */
    static struct edp_flr_preflight flr;
    flr.exercise_well = dark_well;
    flr.exercise_pci = pci_hold;
    flr.exercise_pwm = dark_pwm;
    flr.exercise_pps = dark_pps;
    for (unsigned i = 0; i < 3; ++i) {
        /* Start from identical glyph, cursor and margin state every time.
         * edp_try drains presentation before it pins out other CPU writers. */
        console_clear();
        console_write(pattern[frames[i]]);
        if (!edp_try(&r[i], i == 2 ? &platform : NULL, i == 2 ? &cycle : NULL,
                     i == 2 && display_cycle ? &native : NULL,
                     i == 2 && flr_preflight ? &flr : NULL, r))
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
    bool baseline_retained = r[0].retained || r[1].retained || r[2].retained;
    *retained = baseline_retained || native.retained;
    bool same = edp_repeat(r);
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
        (uint64_t)complete, (uint64_t)same, (uint64_t)changed, (uint64_t)baseline_retained);
    if (display_cycle) {
        const struct bdw_edp_cycle_report *c = &native.report;
        bool restored = complete && c->stage == BDW_EDP_CYCLE_COMPLETE && native.retired && !*retained;
        console_printf("intel-edp-cycle: v=1 attempted=%u complete=%u stage=%u shutdown_error=%u restore_error=%u restore_result=%u offset=%x word=%x panel=%u aux=%x dark_hash=%x crc_checks=%u crc0=%x crc1=%x mask_before=%x mask_live=%x mask_after=%x mask_writes=%u mask_restored=%u shutdown_writes=%u restore_writes=%u well_released=%u retired=%u retained=%u\n",
            (uint64_t)native.attempted, (uint64_t)restored, (uint64_t)c->stage,
            (uint64_t)c->shutdown.source_error, (uint64_t)c->restore.source_error, (uint64_t)c->restore.panel.result,
            (uint64_t)(c->stage >= BDW_EDP_CYCLE_RESTORE ? c->restore.offset : c->shutdown.offset),
            (uint64_t)(c->stage >= BDW_EDP_CYCLE_RESTORE ? c->restore.word : c->shutdown.word),
            (uint64_t)c->restore.panel.panel_stage, (uint64_t)c->restore.panel.aux_status, c->dark_hash,
            (uint64_t)c->restore.crc_checks, (uint64_t)c->restore.captured_crc[0], (uint64_t)c->restore.captured_crc[1],
            (uint64_t)c->mask_before, (uint64_t)c->mask_live, (uint64_t)c->mask_after, (uint64_t)c->mask_writes,
            (uint64_t)c->mask_restored, (uint64_t)c->shutdown.source_writes, (uint64_t)c->restore.source_writes,
            (uint64_t)native.well_released, (uint64_t)native.retired, (uint64_t)native.retained);
        const struct bdw_edp_panel_off_report *off = &c->panel_off;
        console_printf("intel-edp-panel-off: v=1 outcome=%u samples=%u first=%x last=%x elapsed_us=%u mask_error=%u\n",
            (uint64_t)off->outcome, (uint64_t)off->samples, (uint64_t)off->first, (uint64_t)off->last,
            off->elapsed_us, (uint64_t)c->mask_error);
        if (flr_preflight) {
            bool ready = restored && flr.pixel_samples == 3 && flr.pci_samples == 3 && flr.released;
            if (dark_pwm) {
                const struct bdw_edp_pwm_report *p = &flr.pwm;
                console_printf("intel-edp-pwm: v=1 complete=%u retained=%u writes=%u elapsed_us=%u before_ctl=%x before_data=%x after_ctl=%x after_data=%x\n",
                    (uint64_t)p->complete, (uint64_t)p->retained, (uint64_t)p->writes, p->elapsed_us,
                    (uint64_t)p->before.control, (uint64_t)p->before.data,
                    (uint64_t)p->after.control, (uint64_t)p->after.data);
                ready = ready && p->complete;
            }
            if (dark_pps) {
                const struct bdw_edp_pps_report *p = &flr.pps;
                const struct bdw_edp_pps_image *images[] = {&p->before, &p->reset, &p->after};
                for (unsigned i = 0; i < p->samples; ++i)
                    console_printf("intel-edp-pps-word: v=1 phase=%u on=%x off=%x div=%x\n",
                        (uint64_t)i, (uint64_t)images[i]->on, (uint64_t)images[i]->off, (uint64_t)images[i]->divisor);
                console_printf("intel-edp-pps: v=1 complete=%u retained=%u samples=%u writes=%u wait_us=%u program_us=%u initial_status=%x idle_status=%x\n",
                    (uint64_t)p->complete, (uint64_t)p->retained, (uint64_t)p->samples, (uint64_t)p->writes, p->wait_us, p->program_us,
                    (uint64_t)p->initial_status, (uint64_t)p->idle_status);
                ready = ready && p->complete;
            }
            if (pci_hold) {
                const struct bdw_pci_hold_report *p = &flr.pci_hold;
                for (unsigned i = 0; i < p->samples; ++i)
                    console_printf("intel-edp-pci-hold-word: v=1 phase=%u command=%x af=%x\n",
                        (uint64_t)i, (uint64_t)p->sample[i].command, (uint64_t)p->sample[i].af);
                for (unsigned i = 0; i < p->rc6_count; ++i) {
                    const struct bdw_pci_rc6_sample *s = &p->rc6[i];
                    console_printf("intel-edp-rc6: v=1 seq=%u begin_us=%u end_us=%u core=%x residency=%x wake_ack=%x\n",
                        (uint64_t)i, s->begin_us, s->end_us, (uint64_t)s->core_status,
                        (uint64_t)s->residency, (uint64_t)s->wake_ack);
                }
                console_printf("intel-edp-pci-hold: v=5 complete=%u samples=%u writes=%u polls=%u elapsed_us=%u settle_us=%u first_clear_us=%u cpu_copy=%u digest=%x backing=%u retained=%u\n",
                    (uint64_t)p->complete, (uint64_t)p->samples, (uint64_t)p->writes, (uint64_t)p->polls,
                    p->elapsed_us, p->settle_us, p->first_clear_us, (uint64_t)p->cpu_copy, flr.ram_digest,
                    (uint64_t)p->backing, (uint64_t)p->retained);
                for (unsigned i = 0; i < edp_session.reopen_count; ++i) {
                    const struct edp_reopen_entry *e = &edp_session.reopen[i];
                    console_printf("intel-edp-reopen: v=1 seq=%u check=%u at=%u ok=%u read=%u got=%u want=%u\n",
                        (uint64_t)i, (uint64_t)e->check, (uint64_t)e->index,
                        (uint64_t)e->ok, (uint64_t)e->sampled, e->observed, e->expected);
                }
                for (unsigned i = 0; i < flr.candidate_samples; ++i) {
                    const struct edp_candidate_sample *s = &flr.candidate[i];
                    console_printf("intel-edp-candidate: v=1 phase=%u slot=%u complete=%u reads=%u retired=%u begin_us=%u end_us=%u command=%x af=%x left=%x target=%x right=%x\n",
                        (uint64_t)i, (uint64_t)EDP_CANDIDATE_SLOT, (uint64_t)s->complete, (uint64_t)s->reads,
                        (uint64_t)s->retired, s->begin, s->end, (uint64_t)s->command, (uint64_t)s->af,
                        s->pte[0], s->pte[1], s->pte[2]);
                }
                const struct edp_dark_census *census = &flr.census;
                if (census->begin)
                    console_printf("intel-edp-dark-census: v=1 first=%u end_slot=%u complete=%u entries=%u empty=%u first_empty=%u framebuffer_aliases=%u maps=%u retired=%u begin_us=%u end_us=%u command=%x pte_writes=0\n",
                        (uint64_t)EDP_CENSUS_FIRST, (uint64_t)EDP_CENSUS_END, (uint64_t)census->complete,
                        (uint64_t)census->entries, (uint64_t)census->empty, (uint64_t)census->first_empty,
                        (uint64_t)census->framebuffer_aliases, (uint64_t)census->maps, (uint64_t)census->retired,
                        census->begin, census->end, (uint64_t)census->command);
                ready = ready && p->complete && edp_candidate_preserved(&flr);
            }
            if (dark_well) {
                bool well_complete = flr.well_samples == 3 && flr.well_writes == 2 && flr.well_elapsed;
                for (unsigned i = 0; i < flr.well_samples; ++i)
                    console_printf("intel-edp-dark-well-word: v=1 phase=%u bios=%x driver=%x pps=%x\n",
                        (uint64_t)i, (uint64_t)flr.well[i].bios, (uint64_t)flr.well[i].driver,
                        (uint64_t)flr.well[i].pps);
                console_printf("intel-edp-dark-well: v=1 complete=%u samples=%u writes=%u elapsed_us=%u\n",
                    (uint64_t)well_complete, (uint64_t)flr.well_samples, (uint64_t)flr.well_writes,
                    flr.well_elapsed);
                ready = ready && well_complete;
            }
            for (unsigned i = 0; i < flr.pixel_samples; ++i)
                console_printf("intel-edp-flr-pixels: v=1 phase=%u digest=%x elapsed_us=%u\n",
                    (uint64_t)i, flr.pixels[i], flr.elapsed[i]);
            for (unsigned i = 0; i < flr.pci_samples; ++i) {
                for (unsigned j = 0; j < EDP_FLR_PCI_WORDS; ++j)
                    console_printf("intel-edp-flr-config: v=1 phase=%u index=%u word=%x\n",
                        (uint64_t)i, (uint64_t)j, (uint64_t)flr.pci[i][j]);
            }
            console_printf("intel-edp-flr-preflight: v=1 complete=%u pixels=%u configs=%u bytes=%u released=%u reset=0\n",
                (uint64_t)ready, (uint64_t)flr.pixel_samples, (uint64_t)flr.pci_samples,
                (uint64_t)(flr.copy ? EDP_FRAME_BYTES : 0), (uint64_t)flr.released);
            return ready;
        }
        return restored;
    }
    return complete;
}
