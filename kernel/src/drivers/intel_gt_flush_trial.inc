/* Broadwell PRM volume 2c, pp. 403-405, 587-588, 805-814, 859, 994 and 1034.
 * The two GAM observations describe engine memory access, not completion. */
enum { GT_FLUSH_ENGINES = 5, GT_FLUSH_WORDS = 25, GT_FLUSH_SAMPLES = 4 };
static const uint32_t gt_flush_registers[GT_FLUSH_WORDS] = {
    0x40d0, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8038, 0x8040,
    0x12870, 0x12878, 0x1c870, 0x1c878, 0xa090, 0xa094, 0xa0b8,
    0x8038, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8040, 0x40d0
};

/* Each span contains consecutive observations of one raw request. Split
 * even identical values across a reset pulse, where MMIO coverage pauses. */
struct gt_request_span {
    uint64_t first_begin, first_end, last_begin, last_end;
    uint32_t word;
    unsigned pulse, count;
};
static struct {
    struct gt_request_span spans[32];
    unsigned used, reads, omitted;
    uint64_t max_gap, previous_begin;
} gt_request_trace;

static bool gt_trace_request(void) {
    const struct intel_bdw_gt_bootstrap *r = &gt_bootstrap_native.reset;
    if (gt_bootstrap_native.command_writes != 1 || gt_bootstrap_native.pci_samples < 2 ||
        r->reset_attempts != r->reset_completed)
        return true;
    /* The caller just validated slot 8 (0x4000), forcewake, PCI, and all
     * owner clocks. Do not recurse into admission or touch an active reset. */
    uint64_t begin = gt_bootstrap_native.last;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    uint32_t word = intel_display_test_gam_request();
#else
    uint32_t word = *(volatile const uint32_t *)(WAKE_VA + 8 * 4096 + 0xd0);
#endif
    if (word == UINT32_MAX || !bootstrap_tick())
        return false;
    uint64_t end = gt_bootstrap_native.last;
    if (gt_request_trace.reads && end - gt_request_trace.previous_begin > gt_request_trace.max_gap)
        gt_request_trace.max_gap = end - gt_request_trace.previous_begin;
    gt_request_trace.previous_begin = begin;
    ++gt_request_trace.reads;
    struct gt_request_span *last = gt_request_trace.used ? &gt_request_trace.spans[gt_request_trace.used - 1] : NULL;
    if (!last || last->word != word || last->pulse != r->reset_completed || gt_request_trace.omitted) {
        if (gt_request_trace.used == sizeof(gt_request_trace.spans) / sizeof(gt_request_trace.spans[0])) {
            ++gt_request_trace.omitted;
            return true;
        }
        last = &gt_request_trace.spans[gt_request_trace.used++];
        last->word = word;
        last->pulse = r->reset_completed;
        last->first_begin = begin;
        last->first_end = end;
    }
    ++last->count;
    last->last_begin = begin;
    last->last_end = end;
    return true;
}

static struct {
    struct gt_display_access access;
    uint32_t raw[GT_FLUSH_SAMPLES][GT_FLUSH_WORDS];
    uint32_t slice[GT_FLUSH_SAMPLES][3];
    uint64_t begin[GT_FLUSH_SAMPLES], end[GT_FLUSH_SAMPLES];
    unsigned samples;
} gt_flush;

static bool gt_flush_offset_allowed(uint32_t offset) {
    for (unsigned i = 0; i < GT_FLUSH_WORDS; ++i)
        if (offset == gt_flush_registers[i]) return true;
    return false;
}

static bool gt_flush_live(void) {
    if (!bootstrap_parent() || !wake_irq_off() || !gt_slot_live() || gt_range.mapped ||
        vmm_current_address_space() != gt_bootstrap_native.root ||
        gt_bootstrap_native.command_writes != 1 || gt_bootstrap_native.pci_samples != 2)
        return false;
    if (gt_flush.samples == 2)
        return gt_range.completed == 1 && gt_bootstrap_native.reset.complete &&
            gt_bootstrap_native.epoch_calls == 1 && !gt_bootstrap_native.post_bound;
    if (gt_flush.samples == 3)
        return gt_primary.complete && !gt_primary.access.mapped && !gt_aux.access.mapped &&
            gt_bootstrap_native.epoch_calls == 1 && gt_primary_live();
    unsigned prepared = gt_flush.samples ? GT_FLUSH_ENGINES : 0;
    return gt_range.completed == 1 && !gt_bootstrap_native.reset.object &&
        !gt_bootstrap_native.epoch_calls && gt_bootstrap_native.issued == prepared &&
        gt_bootstrap_native.prepared == prepared;
}

/* Use the parent's existing UC aliases for fuse topology and live slice
 * status. Fuses describe availability even when the slices are power gated. */
static bool gt_flush_slice_read(unsigned slot, uint32_t offset, uint32_t *value) {
    if (!gt_flush_live() || !bootstrap_tick() ||
        wake_pages[slot] != (offset & ~4095u) || !completion_mapping(gt_bootstrap_native.a, slot))
        return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_primary_read(offset, value)) return false;
#else
    *value = *(volatile const uint32_t *)(WAKE_VA + slot * 4096 + (offset & 4095));
#endif
    return *value != UINT32_MAX && gt_flush_live() && bootstrap_tick();
}

#include "intel_gt_l3_banks.inc"

static bool gt_flush_sample(unsigned n) {
    if (n >= GT_FLUSH_SAMPLES || n != gt_flush.samples || !gt_flush_live() || !bootstrap_tick())
        return false;
    struct gt_display_access *access = &gt_flush.access;
    gt_flush.begin[n] = gt_bootstrap_native.last;
    uint32_t *slice = gt_flush.slice[n];
    if (!gt_flush_slice_read(9, 0x9120, &slice[0]) ||
        !gt_flush_slice_read(2, 0x138064, &slice[1])) return false;
    if (n == 1 && ((slice[1] & 7u) != 3 || !gt_bank_capture())) return false;
    /* One guarded alias per MMIO page, inside the same observation window. */
    static const unsigned group_ends[] = { 1, 9, 11, 13, 16, 24, GT_FLUSH_WORDS };
    const unsigned pages = sizeof(group_ends) / sizeof(group_ends[0]);
    unsigned start = 0;
    for (unsigned group = 0; group < pages; ++group) {
        unsigned end = group_ends[group];
        if (!gt_held_read_alias(access, gt_flush_registers + start, gt_flush.raw[n] + start,
                                end - start, gt_flush_live, gt_flush_offset_allowed) ||
            gt_bootstrap_native.last <= gt_flush.begin[n] ||
            gt_bootstrap_native.last - gt_flush.begin[n] > BDW_BOOT_PHASE_US)
            return false;
        start = end;
    }
    if (access->mapped || access->maps != pages * (n + 1) || access->retired != access->maps ||
        access->reads != GT_FLUSH_WORDS * (n + 1))
        return false;
    if (!gt_flush_slice_read(2, 0x138064, &slice[2]) ||
        (n == 1 && (slice[2] & 7u) != 3) ||
        ((slice[1] | slice[2]) & 7u & ~(slice[0] >> 25)) ||
        gt_bootstrap_native.last - gt_flush.begin[n] > BDW_BOOT_PHASE_US) return false;
    gt_flush.end[n] = gt_bootstrap_native.last;
    ++gt_flush.samples;
    /* Bits 23:16 are current requests. Capacity and latency are historical
     * or configuration data and cannot establish completion. */
    for (unsigned i = 9; i < 13; ++i)
        if (gt_flush.raw[n][i] & 0x00ff0000u) return false;
    return true;
}

static void gt_flush_report(void) {
    console_printf("intel-gam-trace: v=1 reads=%u spans=%u omitted=%u max_gap_us=%u\n",
        (uint64_t)gt_request_trace.reads, (uint64_t)gt_request_trace.used,
        (uint64_t)gt_request_trace.omitted, gt_request_trace.max_gap);
    for (unsigned i = 0; i < gt_request_trace.used; ++i) {
        const struct gt_request_span *s = &gt_request_trace.spans[i];
        console_printf("intel-gam-span: v=1 n=%u pulse=%u word=%x count=%u first_begin=%u first_end=%u last_begin=%u last_end=%u\n",
            (uint64_t)i, (uint64_t)s->pulse, (uint64_t)s->word, (uint64_t)s->count,
            s->first_begin, s->first_end, s->last_begin, s->last_end);
    }
    gt_bank_report();
    console_printf("intel-flush-summary: v=9 samples=%u reads=%u maps=%u retired=%u live=%x authority=none\n",
        (uint64_t)gt_flush.samples, (uint64_t)gt_flush.access.reads, (uint64_t)gt_flush.access.maps,
        (uint64_t)gt_flush.access.retired, gt_flush.access.mapped);
    for (unsigned n = 0; n < GT_FLUSH_SAMPLES; ++n) {
        const uint32_t *w = gt_flush.raw[n];
        if (n < gt_flush.samples)
            console_printf("intel-l3-power: v=1 n=%u fuse=%x before=%x after=%x\n",
                (uint64_t)n, (uint64_t)gt_flush.slice[n][0],
                (uint64_t)gt_flush.slice[n][1], (uint64_t)gt_flush.slice[n][2]);
        console_printf("intel-flush-sample: v=5 n=%u begin=%u end=%u rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x l3=%x gam_go=%x gam_eco=%x vcs0_ref=%x vcs0_prefetch=%x vcs1_ref=%x vcs1_prefetch=%x gam_req=%x rc_control=%x rc_target=%x rc6_threshold=%x l3_close=%x go_close=%x rcs_close=%x vcs0_close=%x vecs_close=%x vcs1_close=%x bcs_close=%x eco_close=%x req_close=%x\n",
            (uint64_t)n, gt_flush.begin[n], gt_flush.end[n], (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4], (uint64_t)w[5], (uint64_t)w[6], (uint64_t)w[7], (uint64_t)w[1], (uint64_t)w[8],
            (uint64_t)w[9], (uint64_t)w[10], (uint64_t)w[11], (uint64_t)w[12], (uint64_t)w[0], (uint64_t)w[13], (uint64_t)w[14], (uint64_t)w[15],
            (uint64_t)w[16], (uint64_t)w[17], (uint64_t)w[18], (uint64_t)w[19], (uint64_t)w[20], (uint64_t)w[21],
            (uint64_t)w[22], (uint64_t)w[23], (uint64_t)w[24]);
    }
}
