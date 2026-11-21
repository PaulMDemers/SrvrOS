/* Broadwell display endpoints. No register write or reader retirement. */
enum {
    AUX_VGA_CONTROL,
    AUX_FBC_CONTROL,
    AUX_FBC_STATUS2,
    AUX_EDP_DDI,
    AUX_PSR_CONTROL,
    AUX_PSR_STATUS,
    AUX_EDP_CONFIG,
    AUX_WORDS
};
static const uint32_t gt_aux_registers[AUX_WORDS] = {
    0x41000, 0x43208, 0x43214, 0x6f400, 0x6f800, 0x6f840, 0x7f008
};
static struct {
    struct gt_display_access access;
    uint32_t raw[2][AUX_WORDS], blockers[2];
    uint64_t begin[2], end[2];
    unsigned samples;
    bool active;
} gt_aux;

static bool gt_aux_offset_allowed(uint32_t offset) {
    for (unsigned i = 0; i < AUX_WORDS; ++i)
        if (gt_aux_registers[i] == offset) return true;
    return false;
}

static uint32_t gt_aux_blockers(const uint32_t words[AUX_WORDS]) {
    uint32_t blockers = 0;
    if (!(words[AUX_VGA_CONTROL] & (1u << 31))) blockers |= 1;
    if (words[AUX_FBC_CONTROL] & (1u << 31)) blockers |= 2;
    if (words[AUX_FBC_STATUS2] & 0xfff) blockers |= 4;
    if (words[AUX_PSR_CONTROL] & (1u << 31)) blockers |= 8;
    if (words[AUX_PSR_STATUS] & (7u << 29)) blockers |= 16;
    if (words[AUX_EDP_DDI] != 0x82200000 || words[AUX_EDP_CONFIG] != 0xc0000000 ||
        words[AUX_EDP_DDI] != result.sample[1].trans[3][3] ||
        words[AUX_EDP_CONFIG] != result.sample[1].trans[3][4]) blockers |= 32;
    return blockers;
}

static bool gt_aux_sample(unsigned n) {
    if (n != gt_aux.samples || n > 1 || gt_aux.active || !gt_primary_guard() || gt_primary.access.mapped)
        return false;
    gt_aux.begin[n] = gt_bootstrap_native.last;
    gt_aux.active = true;
    uint32_t *words = gt_aux.raw[n];
    struct gt_display_access *access = &gt_aux.access;
    bool ok = gt_primary_alias(access, gt_aux_registers + AUX_VGA_CONTROL, words + AUX_VGA_CONTROL, 1) &&
        gt_primary_alias(access, gt_aux_registers + AUX_FBC_CONTROL, words + AUX_FBC_CONTROL, 2) &&
        gt_primary_alias(access, gt_aux_registers + AUX_EDP_DDI, words + AUX_EDP_DDI, 3) &&
        gt_primary_alias(access, gt_aux_registers + AUX_EDP_CONFIG, words + AUX_EDP_CONFIG, 1);
    gt_aux.active = false;
    if (!ok || access->mapped || access->maps != 4 * (n + 1) || access->retired != access->maps ||
        access->reads != AUX_WORDS * (n + 1) || gt_bootstrap_native.last <= gt_aux.begin[n]) return false;
    gt_aux.blockers[n] = gt_aux_blockers(words);
    gt_aux.end[n] = gt_bootstrap_native.last;
    ++gt_aux.samples;
    return true;
}

static void gt_aux_report(void) {
    bool stable = gt_aux.samples == 2;
    for (unsigned i = 0; i < AUX_WORDS; ++i)
        stable = stable && gt_aux.raw[0][i] == gt_aux.raw[1][i];
    console_printf("intel-aux-summary: v=1 samples=%u reads=%u maps=%u retired=%u live=%x stable=%u inactive=%u authority=none\n",
        (uint64_t)gt_aux.samples, (uint64_t)gt_aux.access.reads, (uint64_t)gt_aux.access.maps,
        (uint64_t)gt_aux.access.retired, gt_aux.access.mapped, (uint64_t)stable,
        (uint64_t)(stable && !(gt_aux.blockers[0] | gt_aux.blockers[1])));
    for (unsigned n = 0; n < 2; ++n) {
        const uint32_t *w = gt_aux.raw[n];
        console_printf("intel-aux-sample: v=1 n=%u begin=%u end=%u blockers=%u vga=%x fbc=%x fbc_status2=%x ddi=%x psr=%x psr_status=%x trans=%x\n",
            (uint64_t)n, gt_aux.begin[n], gt_aux.end[n], (uint64_t)gt_aux.blockers[n],
            (uint64_t)w[0], (uint64_t)w[1], (uint64_t)w[2], (uint64_t)w[3],
            (uint64_t)w[4], (uint64_t)w[5], (uint64_t)w[6]);
    }
}
