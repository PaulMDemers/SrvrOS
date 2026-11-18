/* Fresh primary geometry measured while the original post-reset owner
 * still inhibits the requester. This observation grants no reader retirement. */
#include <srvros/intel_bdw_primary_range.h>

struct gt_display_access {
    uint64_t mapped;
    unsigned reads, maps, retired;
};

#ifdef SRVROS_INTEL_GT_AUX
static bool gt_aux_offset_allowed(uint32_t offset);
#endif

static struct {
    struct intel_bdw_primary_input input;
    struct intel_bdw_primary_range result;
    uint32_t wells[2][2][4];
    uint64_t begin[2], end[2];
    struct gt_display_access access;
    bool complete;
} gt_primary;

static bool gt_primary_live(void) {
    return gt_range.completed == 2 && !gt_range.mapped && bootstrap_parent() &&
        wake_irq_off() && vmm_current_address_space() == gt_bootstrap_native.root &&
        gt_range_post_memory();
}

static bool gt_primary_guard(void) {
    return gt_primary_live() && bootstrap_guard();
}

static bool gt_held_read(struct gt_display_access *access, uint64_t address, uint32_t offset,
                         uint32_t *value, bool (*live)(void)) {
    if (!live() || !bootstrap_tick()) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    (void)address;
    if (!intel_display_test_primary_read(offset, value)) return false;
#else
    *value = *(volatile const uint32_t *)address;
    (void)offset;
#endif
    ++access->reads;
    return live() && bootstrap_tick() && *value != UINT32_MAX;
}

/* Each caller admits exact offsets and the stage where the range VA is free. */
static bool gt_held_read_alias(struct gt_display_access *access, const uint32_t *offsets,
                               uint32_t *out, unsigned count, bool (*live)(void),
                               bool (*allowed)(uint32_t)) {
    if (!count || count > 6) return false;
    uint32_t page = offsets[0] & ~4095u;
    for (unsigned i = 0; i < count; ++i)
        if (!allowed(offsets[i]) || (offsets[i] & ~4095u) != page) return false;
    uint64_t actual, physical = result.mmio_base + page;
    unsigned mtrr, pat, effective;
    if (!live() || !bootstrap_guard() || access->mapped) return false;
    if (gt_range_translation(&actual)) {
        access->mapped = actual;
        return false;
    }
    if (!x86_cache_page(&gt_bootstrap_native.cache, physical, 3, &mtrr, &pat, &effective) || effective)
        return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    bool mapped = intel_display_test_range_map(physical);
#else
    if (!backing_range_reserved(physical, 4096)) return false;
    bool mapped = vmm_map_page_in_space(gt_bootstrap_native.root, GT_RANGE_VA, physical,
        VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_WRITE_THROUGH);
#endif
    if (!mapped) {
        if (gt_range_translation(&actual)) access->mapped = actual;
        return false;
    }
    access->mapped = physical;
    ++access->maps;
    struct vmm_alias_observation alias;
    bool ok = live() && bootstrap_tick() &&
        gt_range_translation(&actual) && actual == physical;
    if (ok) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        ok = intel_display_test_range_alias(&alias);
#else
        ok = vmm_diagnostic_kernel_alias(gt_bootstrap_native.root, GT_RANGE_VA, physical, 1,
            gt_bootstrap_native.cache.cpu.physical_bits, &alias, NULL);
#endif
        ok = ok && alias.pages == 1 && alias.leaves == 1 && alias.pat_index == 3 &&
            !alias.writable && !alias.user && alias.no_execute == 1 && alias.leaf_sizes == 1;
    }
    for (unsigned i = 0; ok && i < count; ++i)
        ok = gt_held_read(access, GT_RANGE_VA + (offsets[i] & 4095), offsets[i], &out[i], live);
    if (!live() || !bootstrap_tick() ||
        !gt_range_translation(&actual) || actual != physical) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    bool retired = intel_display_test_range_unmap(&actual);
#else
    bool retired = vmm_unmap_page_in_space(gt_bootstrap_native.root, GT_RANGE_VA, &actual);
#endif
    if (!retired || actual != physical || gt_range_translation(&actual)) return false;
    access->mapped = 0;
    ++access->retired;
    return live() && bootstrap_guard() && ok;
}

static bool gt_primary_offset_allowed(uint32_t offset) {
#ifdef SRVROS_INTEL_GT_AUX
    if (gt_aux_offset_allowed(offset)) return true;
#endif
    return intel_capture_offset_allowed(offset);
}

static bool gt_primary_alias(struct gt_display_access *access, const uint32_t *offsets, uint32_t *out, unsigned count) {
    return count <= 4 && gt_held_read_alias(access, offsets, out, count, gt_primary_live, gt_primary_offset_allowed);
}

static bool gt_primary_wells(uint32_t observed[4]) {
    static const uint32_t offsets[] = {0x45400, 0x45404, 0x45408, 0x4540c};
    if (!gt_primary_alias(&gt_primary.access, offsets, observed, 4)) return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!(observed[i] & (1u << 30)) || observed[i] != result.sample[1].well_after[i]) return false;
    return true;
}

static bool gt_primary_pipe(unsigned pipe, struct intel_bdw_primary_sample *sample) {
    static const uint32_t offsets[13] = {
        0x180, 0x19c, 0x1ac, 0x1b0, 0x1bc, 0x080, 0x084, 0x0ac, 0x280, 0x29c, 0x2ac, 0x2b0, 0x2bc
    };
    static const unsigned reader_for_pipe[] = {0, 1, 2, 5, 6};
    static const unsigned pipe_indices[] = {2, 4, 6, 7, 8};
    if (!gt_primary_guard() || !completion_mapping(gt_bootstrap_native.a, 10 + pipe)) return false;
    uint64_t address = WAKE_VA + (10 + pipe) * 4096;
    uint32_t page = 0x70000 + pipe * 4096;
    for (unsigned i = 0; i < 13; ++i) {
        uint32_t *word = &sample->reader[pipe][i];
        if (!gt_held_read(&gt_primary.access, address + offsets[i], page + offsets[i], word, gt_primary_live) ||
            *word != gt_bootstrap_native.display[1][pipe][i]) return false;
    }
    for (unsigned i = 0; i < 5; ++i)
        sample->pipe[pipe][pipe_indices[i]] = sample->reader[pipe][reader_for_pipe[i]];
    for (unsigned i = 3; i <= 5; i += 2) {
        uint32_t offset = intel_capture_pipe_offsets[i];
        if (!gt_held_read(&gt_primary.access, address + offset, page + offset, &sample->pipe[pipe][i], gt_primary_live)) return false;
    }
    for (unsigned i = 2; i < 9; ++i)
        if (sample->pipe[pipe][i] != result.sample[1].pipe[pipe][i]) return false;
    return gt_primary_guard();
}

#ifdef SRVROS_INTEL_GT_AUX
#include "intel_gt_aux_trial.inc"
#endif

static bool gt_primary_capture(void) {
    if (!gt_primary_guard()) return false;
    struct intel_bdw_primary_input *in = &gt_primary.input;
    in->aperture_bytes = window_result.aperture_bytes;
    in->ggtt_bytes = backing.ggtt_bytes;
    in->fence_begin = window_result.endpoint.fence_begin;
    in->fence_end = window_result.endpoint.fence_end;
    in->first = GT_RANGE_CANDIDATE;
    in->pages = 1;
    for (unsigned n = 0; n < 2; ++n) {
        struct intel_bdw_primary_sample *sample = &in->sample[n];
        gt_primary.begin[n] = gt_bootstrap_native.last;
        if (!gt_primary_wells(gt_primary.wells[n][0])) return false;
#ifdef SRVROS_INTEL_GT_AUX
        if (!n && !gt_aux_sample(n)) return false;
#endif
        for (unsigned p = 0; p < 3; ++p) {
            const uint32_t offset = 0x6001c + p * 4096;
            if (!gt_primary_alias(&gt_primary.access, &offset, &sample->source[p], 1) ||
                sample->source[p] != result.sample[1].source[p] || !gt_primary_pipe(p, sample))
                return false;
        }
#ifdef SRVROS_INTEL_GT_AUX
        if (n && !gt_aux_sample(n)) return false;
#endif
        if (!gt_primary_wells(gt_primary.wells[n][1]) || !gt_primary_guard() ||
            gt_bootstrap_native.last <= gt_primary.begin[n]) return false;
        gt_primary.end[n] = gt_bootstrap_native.last;
        ++in->samples;
    }
    struct intel_bdw_primary_range *out = &gt_primary.result;
    if (gt_primary.access.reads != 112 || gt_primary.access.maps != 10 || gt_primary.access.retired != 10 ||
        gt_primary.access.mapped || !intel_bdw_primary_range_check(in, out) ||
        out->begin != backing.offset || out->end != backing.offset + backing.bytes ||
        out->pte_end != (GT_RANGE_FIRST + GT_RANGE_TAIL) * 4096ull ||
        out->width != result.fb.width || out->height != result.fb.height ||
        out->stride != result.fb.pitch || out->tiled != backing.tiled ||
        !gt_primary_guard()) return false;
    gt_primary.complete = true;
    return true;
}

static void gt_primary_report(void) {
    const struct intel_bdw_primary_range *out = &gt_primary.result;
    console_printf("intel-primary-summary: v=1 complete=%u samples=%u reads=%u maps=%u retired=%u live=%x begin0=%u end0=%u begin1=%u end1=%u image_end=%u pte_end=%u candidate=%u reason=%u authority=none\n",
        (uint64_t)gt_primary.complete, (uint64_t)gt_primary.input.samples, (uint64_t)gt_primary.access.reads,
        (uint64_t)gt_primary.access.maps, (uint64_t)gt_primary.access.retired, gt_primary.access.mapped,
        gt_primary.begin[0], gt_primary.end[0], gt_primary.begin[1], gt_primary.end[1],
        out->end, out->pte_end, out->candidate_begin, (uint64_t)out->reason);
    for (unsigned i = 0; i < gt_primary.input.samples; ++i) {
        const struct intel_bdw_primary_sample *sample = &gt_primary.input.sample[i];
        /* This wire is x86-only. Include source, pipe, reader and well words. */
        _Static_assert(sizeof(*sample) == 69 * sizeof(uint32_t), "canonical primary sample");
        uint32_t packet[77];
        __builtin_memcpy(packet, sample, sizeof(*sample));
        __builtin_memcpy(packet + 69, gt_primary.wells[i], sizeof(gt_primary.wells[i]));
        char encoded[((sizeof(packet) + 2) / 3) * 4 + 1];
        entry_requester_base64((const uint8_t *)packet, sizeof(packet), encoded);
        console_printf("intel-primary-vector: v=1 sample=%u data=%s\n", (uint64_t)i, encoded);
    }
}
