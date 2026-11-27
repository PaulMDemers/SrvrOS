/* Exactly one borrowed GGTT DWORD. Configuration and display status reads
 * do not establish requester exclusion or retire outstanding traffic. */
enum hsw_candidate_word {
    HC_FRAME, HC_PTE, HC_EDP_CONF, HC_EDP_FUNC, HC_DDI_A, HC_PRIMARY,
    HC_SURF, HC_LIVE, HC_PRIMARY_B, HC_PRIMARY_C, HC_SPRITE_A, HC_SPRITE_B,
    HC_SPRITE_C, HC_CURSOR_A, HC_CURSOR_B, HC_CURSOR_C, HC_PSR,
    HC_IGD_CMD, HC_IGD_PM, HC_IGD_CAP, HC_IGD_DEVSTA,
    HC_HDA_CMD, HC_HDA_PM, HC_HDA_CAP, HC_HDA_DEVSTA, HC_WORDS
};
static const uint32_t hsw_candidate_regs[] = {
    0x7f008, 0x6f400, 0x64000, 0x70180, 0x7019c, 0x701ac, 0x71180, 0x72180,
    0x70280, 0x71280, 0x72280, 0x70080, 0x71080, 0x72080, 0x64800
};
_Static_assert(sizeof(hsw_candidate_regs) / sizeof(hsw_candidate_regs[0]) == HC_PSR - HC_EDP_CONF + 1,
               "Haswell candidate display register list");
static struct {
    uint64_t begin, end;
    unsigned status, rows;
    bool mapped, retired;
    uint32_t sample[2][HC_WORDS];
} hsw_candidate_result;

static bool hsw_candidate_pci(const struct pci_device *dev, uint32_t id, uint32_t *out) {
    struct package_pci_snapshot p;
    package_pci_read(&p, conventional_read, (void *)dev);
    if (p.id != id || p.state != 1 || p.command == UINT16_MAX || !(p.flags & 1) ||
        p.pm == UINT16_MAX) return false;
    uint32_t status = p.pcie_offset ? pci_read_config16(dev, p.pcie_offset + 10) : UINT32_MAX;
    if (status == UINT16_MAX) return false;
    out[0] = p.command;
    out[1] = p.pm;
    out[2] = p.pcie_offset;
    out[3] = status; /* All ones encodes missing capability, not clear pending. */
    return pci_read_config32(dev, 0) == id;
}

static bool hsw_candidate_display_unchanged(const uint32_t *row) {
    return row[HC_EDP_CONF] == 0xc0000018 && row[HC_EDP_FUNC] == 0x82200000 &&
           row[HC_DDI_A] == 0x80000011 && row[HC_PRIMARY] == 0x98004400 &&
           !row[HC_SURF] && !row[HC_LIVE];
}

static bool hsw_candidate_sample(uint32_t *row) {
    if (!hsw_config_valid()) return false;
    uint32_t wells[4];
    for (unsigned i = 0; i < 4; i++) wells[i] = well_read(0x400 + i * 4);
    if (!display_resource_well_on(wells)) return false;

    for (unsigned i = 0; i < sizeof(hsw_candidate_regs) / sizeof(hsw_candidate_regs[0]); i++) {
        row[HC_EDP_CONF + i] = resource_read(hsw_candidate_regs[i]);
        if (row[HC_EDP_CONF + i] == UINT32_MAX) return false;
    }
    if (!hsw_candidate_display_unchanged(row) ||
        !hsw_candidate_pci(gpu, 0x0a268086, &row[HC_IGD_CMD]) ||
        (row[HC_IGD_PM] & 3) || !(row[HC_IGD_CMD] & 2)) return false;

    const struct pci_device *hda = NULL;
#ifdef SRVROS_PLATFORM_HOST_TEST
    extern const struct pci_device *platform_test_candidate_hda(void);
    hda = platform_test_candidate_hda();
#else
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *p = pci_device_at(i);
        if (!p->bus && p->device == 3 && !p->function) hda = p;
    }
#endif
    if (!hda || pci_read_config32(hda, 8) >> 8 != 0x040300 ||
        pci_read_config32(hda, 0x2c) != 0x011a106b ||
        !hsw_candidate_pci(hda, 0x0a0c8086, &row[HC_HDA_CMD])) return false;

    row[HC_FRAME] = resource_read(0x70040);
    if (!hsw_config_valid() || !hsw_read(NULL, HSW_CANDIDATE_MMIO, &row[HC_PTE]) ||
        row[HC_PTE] == UINT32_MAX) return false;
    for (unsigned i = 0; i < 4; i++) if (wells[i] != well_read(0x400 + i * 4)) return false;
    for (unsigned i = HC_EDP_CONF; i <= HC_LIVE; i++)
        if (row[i] != resource_read(hsw_candidate_regs[i - HC_EDP_CONF])) return false;
    return hsw_config_valid();
}

static bool hsw_candidate_observe(void) {
    uint32_t mmio;
    if (!intel_hsw_ggtt_candidate(&hsw_result, HSW_CANDIDATE_GPU, &mmio) ||
        mmio != HSW_CANDIDATE_MMIO) return false;

    console_write("hsw-candidate-progress: sampling one read-only GGTT slot while Pipe A scans; no input needed\n");
    hsw_candidate_result.begin = timer_monotonic_us();
    hsw_candidate_result.status = 2; /* Access or sampling failure. */
    hsw_candidate_window = true;
    if (hsw_candidate_result.begin && hsw_map(NULL, mmio)) {
        hsw_candidate_result.mapped = true;
        uint32_t *before = hsw_candidate_result.sample[0], *after = hsw_candidate_result.sample[1];
        if (hsw_candidate_sample(before)) {
            hsw_candidate_result.rows = 1;
            hsw_candidate_result.status = 4; /* Clock or deadline. */
            bool waited = timer_delay_us(HSW_CANDIDATE_GAP_US);
            uint64_t now = timer_monotonic_us();
            if (waited && now > hsw_candidate_result.begin &&
                now - hsw_candidate_result.begin <= HSW_CANDIDATE_LIMIT_US) {
                hsw_candidate_result.status = 2;
                if (hsw_candidate_sample(after)) {
                    hsw_candidate_result.rows = 2;
                    hsw_candidate_result.status = 3; /* Changed binding or no scan progress. */
                    uint32_t frames = after[HC_FRAME] - before[HC_FRAME];
                    bool stable = frames > 0 && frames <= 8 && before[HC_PTE] == after[HC_PTE];
                    for (unsigned i = HC_EDP_CONF; i < HC_WORDS; i++)
                        if (i != HC_IGD_DEVSTA && i != HC_HDA_DEVSTA && before[i] != after[i]) stable = false;
                    if (stable) hsw_candidate_result.status = 1;
                }
            }
        }
        hsw_candidate_result.retired = hsw_unmap(NULL);
        if (!hsw_candidate_result.retired) hsw_candidate_result.status = 5;
    }
    if (hsw_window_poison) hsw_candidate_result.status = 5;
    hsw_candidate_window = false;
    hsw_candidate_result.end = timer_monotonic_us();
    if (hsw_candidate_result.status == 1 &&
        (hsw_candidate_result.end <= hsw_candidate_result.begin ||
         hsw_candidate_result.end - hsw_candidate_result.begin < HSW_CANDIDATE_GAP_US ||
         hsw_candidate_result.end - hsw_candidate_result.begin > HSW_CANDIDATE_LIMIT_US))
        hsw_candidate_result.status = 4;
    return hsw_candidate_result.status == 1;
}

static void hsw_candidate_report(void) {
    console_printf("hsw-candidate: v=1 status=%u rows=%u slot=4096 mmio=0x204000 begin=%u end=%u mapped=%u retired=%u writes=0 authority=none\n",
                   (uint64_t)hsw_candidate_result.status, (uint64_t)hsw_candidate_result.rows,
                   hsw_candidate_result.begin, hsw_candidate_result.end,
                   (uint64_t)hsw_candidate_result.mapped, (uint64_t)hsw_candidate_result.retired);
    for (unsigned i = 0; i < hsw_candidate_result.rows; i++) {
        uint8_t bytes[4 * HC_WORDS];
        char data[sizeof(bytes) * 4 / 3 + 5];
        unsigned n = 0;
        for (unsigned j = 0; j < HC_WORDS; j++) hda_put(bytes, &n, hsw_candidate_result.sample[i][j], 4);
        hsw_b64(bytes, n, data);
        console_printf("hsw-candidate-row: phase=%u data=%s\n", (uint64_t)i, data);
    }
}
