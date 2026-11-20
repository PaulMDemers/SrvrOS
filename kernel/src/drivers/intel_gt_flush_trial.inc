/* Broadwell PRM volume 2c, pp. 403-405, 587-588, 805-814, 859, 994 and 1034.
 * The two GAM observations describe engine memory access, not completion. */
enum { GT_FLUSH_ENGINES = 5, GT_FLUSH_MEDIA_END = 12, GT_FLUSH_WORDS = 16, GT_FLUSH_SAMPLES = 3 };
static const uint32_t gt_flush_registers[GT_FLUSH_WORDS] = {
    0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8038, 0x8028, 0x8040,
    0x12870, 0x12878, 0x1c870, 0x1c878,
    /* GO_GAM_REQ, RC_CTRL0, RC_CTRL1 and the RC6 promotion timer. */
    0x40d0, 0xa090, 0xa094, 0xa0b8
};
static struct {
    struct gt_display_access access;
    uint32_t raw[GT_FLUSH_SAMPLES][GT_FLUSH_WORDS];
    uint64_t begin[GT_FLUSH_SAMPLES], end[GT_FLUSH_SAMPLES];
    unsigned samples;
} gt_flush;

static bool gt_flush_offset_allowed(uint32_t offset) {
    for (unsigned i = 0; i < GT_FLUSH_WORDS; ++i)
        if (offset == gt_flush_registers[i]) return true;
    return false;
}

static bool gt_flush_live(void) {
    if (!bootstrap_parent() || !wake_irq_off() || gt_range.mapped ||
        vmm_current_address_space() != gt_bootstrap_native.root ||
        gt_bootstrap_native.command_writes != 1 || gt_bootstrap_native.pci_samples != 2)
        return false;
    if (gt_flush.samples == 2)
        return gt_primary.complete && !gt_primary.access.mapped && !gt_aux.access.mapped &&
            gt_bootstrap_native.epoch_calls == 1 && gt_primary_live();
    unsigned prepared = gt_flush.samples ? GT_FLUSH_ENGINES : 0;
    return gt_range.completed == 1 && !gt_bootstrap_native.reset.object &&
        !gt_bootstrap_native.epoch_calls && gt_bootstrap_native.issued == prepared &&
        gt_bootstrap_native.prepared == prepared;
}

static bool gt_flush_sample(unsigned n) {
    if (n >= GT_FLUSH_SAMPLES || n != gt_flush.samples || !gt_flush_live() || !bootstrap_tick())
        return false;
    struct gt_display_access *access = &gt_flush.access;
    gt_flush.begin[n] = gt_bootstrap_native.last;
    /* One guarded alias per MMIO page, inside the same observation window. */
    static const unsigned group_ends[] = { 8, 10, GT_FLUSH_MEDIA_END, 13, GT_FLUSH_WORDS };
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
    gt_flush.end[n] = gt_bootstrap_native.last;
    ++gt_flush.samples;
    /* Bits 23:16 are current requests. Capacity and latency are historical
     * or configuration data and cannot establish completion. */
    for (unsigned i = 8; i < GT_FLUSH_MEDIA_END; ++i)
        if (gt_flush.raw[n][i] & 0x00ff0000u) return false;
    return true;
}

static void gt_flush_report(void) {
    console_printf("intel-flush-summary: v=5 samples=%u reads=%u maps=%u retired=%u live=%x authority=none\n",
        (uint64_t)gt_flush.samples, (uint64_t)gt_flush.access.reads, (uint64_t)gt_flush.access.maps,
        (uint64_t)gt_flush.access.retired, gt_flush.access.mapped);
    for (unsigned n = 0; n < GT_FLUSH_SAMPLES; ++n) {
        const uint32_t *w = gt_flush.raw[n];
        console_printf("intel-flush-sample: v=4 n=%u begin=%u end=%u rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x l3=%x gam_go=%x gam_eco=%x vcs0_ref=%x vcs0_prefetch=%x vcs1_ref=%x vcs1_prefetch=%x gam_req=%x rc_control=%x rc_target=%x rc6_threshold=%x\n",
            (uint64_t)n, gt_flush.begin[n], gt_flush.end[n], (uint64_t)w[0], (uint64_t)w[1],
            (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4], (uint64_t)w[5], (uint64_t)w[6], (uint64_t)w[7],
            (uint64_t)w[8], (uint64_t)w[9], (uint64_t)w[10], (uint64_t)w[11],
            (uint64_t)w[12], (uint64_t)w[13], (uint64_t)w[14], (uint64_t)w[15]);
    }
}
