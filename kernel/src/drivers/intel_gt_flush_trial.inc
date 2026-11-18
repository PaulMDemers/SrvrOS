/* Broadwell PRM volume 2c, pp. 805-814 and 859. These are GT messages.
 * Read the acknowledgements, never write the message channel. */
enum { GT_FLUSH_ENGINES = 5, GT_FLUSH_WORDS = 6, GT_FLUSH_SAMPLES = 3 };
static const uint32_t gt_flush_registers[GT_FLUSH_WORDS] = {
    0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8038
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
    if (!gt_held_read_alias(access, gt_flush_registers, gt_flush.raw[n], GT_FLUSH_WORDS,
                            gt_flush_live, gt_flush_offset_allowed) ||
        access->mapped || access->maps != n + 1 || access->retired != n + 1 ||
        access->reads != GT_FLUSH_WORDS * (n + 1) ||
        gt_bootstrap_native.last <= gt_flush.begin[n] ||
        gt_bootstrap_native.last - gt_flush.begin[n] > BDW_BOOT_PHASE_US)
        return false;
    gt_flush.end[n] = gt_bootstrap_native.last;
    ++gt_flush.samples;
    return true;
}

static void gt_flush_report(void) {
    console_printf("intel-flush-summary: v=2 samples=%u reads=%u maps=%u retired=%u live=%x authority=none\n",
        (uint64_t)gt_flush.samples, (uint64_t)gt_flush.access.reads, (uint64_t)gt_flush.access.maps,
        (uint64_t)gt_flush.access.retired, gt_flush.access.mapped);
    for (unsigned n = 0; n < GT_FLUSH_SAMPLES; ++n) {
        const uint32_t *w = gt_flush.raw[n];
        console_printf("intel-flush-sample: v=1 n=%u begin=%u end=%u rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x l3=%x\n",
            (uint64_t)n, gt_flush.begin[n], gt_flush.end[n], (uint64_t)w[0], (uint64_t)w[1],
            (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4], (uint64_t)w[5]);
    }
}
