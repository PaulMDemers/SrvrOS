/* Bounded borrowed GGTT window. Configuration and display status reads
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

enum hsw_fbc_word { HF_CTL, HF_BASE, HF_STATUS, HF_WORDS };
static struct {
    unsigned fbc_rows, entries, fbc_maps, fbc_retired;
    uint32_t fbc[2][HF_WORDS];
    uint32_t neighbors[2][HSW_ACCESS_SLOTS];
} hsw_access_result;

static bool hsw_candidate_pci(const struct pci_device *dev, uint32_t id, uint32_t *out,
                              struct package_pci_af *af) {
    struct package_pci_snapshot p;
    package_pci_read_with_af(&p, af, conventional_read, (void *)dev);
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

/* requester[] brackets the isolated VT-d sampler. gt[] is captured
 * much later, in the established candidate bracket. Neither observes a
 * command engine queue or claims uninterrupted DMA/firmware exclusion. */
static struct {
    unsigned requester_rows, gt_rows;
    struct { uint64_t begin, end; uint32_t words[4]; } requester[2];
    struct { uint32_t core, rc6; } gt[2];
    struct package_pci_af af[2][2]; /* Phase, then IGD/display HDA. */
    uint32_t smi_begin, smi_end;
    bool slot_probe;
} hsw_live;

static bool hsw_smi_count(uint32_t *count) {
#ifdef SRVROS_PLATFORM_HOST_TEST
    extern bool platform_test_hsw_smi_count(uint32_t *);
    return platform_test_hsw_smi_count(count);
#else
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    if (a < 1 || b != 0x756e6547 || d != 0x49656e69 || c != 0x6c65746e) return false;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    if (a != 0x40651 || b >> 24 || !(d & (1u << 5))) return false;
    __asm__ volatile("rdmsr" : "=a"(*count), "=d"(d) : "c"(0x34u) : "memory");
    return true;
#endif
}

static bool hsw_live_snapshot(const struct pci_device *dev) {
    unsigned phase = hsw_live.requester_rows;
    if (!dev || phase >= 2 || dev->bus || dev->device != 2 || dev->function ||
        pci_read_config32(dev, 8) != 0x03000009 || pci_read_config32(dev, 0x2c) != 0x011a106b)
        return false;
    hsw_live.requester[phase].begin = timer_monotonic_us();
    uint32_t *row = hsw_live.requester[phase].words;
    if (!hsw_candidate_pci(dev, 0x0a268086, row, NULL) || !(row[0] & 2) || (row[1] & 3)) return false;
    hsw_live.requester[phase].end = timer_monotonic_us();
    if (!hsw_live.requester[phase].begin ||
        hsw_live.requester[phase].end < hsw_live.requester[phase].begin) return false;
    hsw_live.requester_rows++;
    for (unsigned i = 0; phase && i < 3; i++)
        if (row[i] != hsw_live.requester[0].words[i]) return false;
    return true;
}

bool platform_haswell_live_requester(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *dev = pci_device_at(i);
        if (dev && !dev->bus && dev->device == 2 && !dev->function)
            return hsw_live_snapshot(dev);
    }
    return false;
}

static bool hsw_live_gt(void) {
    unsigned i = hsw_live.gt_rows;
    if (i >= 2) return false;
    uint32_t core = graphics_read(0x60), rc6 = graphics_read(0x108);
    if (core == UINT32_MAX || rc6 == UINT32_MAX) return false;
    hsw_live.gt[i].core = core;
    hsw_live.gt[i].rc6 = rc6;
    hsw_live.gt_rows++;
    return true;
}

static bool hsw_live_match(void) {
    if (hsw_live.requester_rows != 2 || hsw_live.gt_rows != 2 ||
        hsw_candidate_result.rows != 2 ||
        hsw_candidate_result.begin <= hsw_live.requester[1].end) return false;

    for (unsigned phase = 0; phase < 2; phase++)
        for (unsigned word = 0; word < 3; word++)
            if (hsw_live.requester[phase].words[word] !=
                hsw_candidate_result.sample[phase][HC_IGD_CMD + word]) return false;
    return true;
}

static void hsw_live_report(bool ok) {
    console_printf("hsw-live: v=1 observed=%u requester_rows=%u gt_rows=%u writes=0 authority=none\n",
                   (uint64_t)ok, (uint64_t)hsw_live.requester_rows, (uint64_t)hsw_live.gt_rows);
    for (unsigned phase = 0; phase < hsw_live.requester_rows; phase++) {
        const uint32_t *row = hsw_live.requester[phase].words;
        console_printf("hsw-live-requester: phase=%u begin=%u end=%u cmd=%u pm=%u cap=%u devsta=%u\n",
                       (uint64_t)phase, hsw_live.requester[phase].begin, hsw_live.requester[phase].end,
                       (uint64_t)row[0], (uint64_t)row[1], (uint64_t)row[2], (uint64_t)row[3]);
    }
    for (unsigned phase = 0; phase < hsw_live.gt_rows; phase++)
        console_printf("hsw-live-gt: phase=%u core=%u rc6=%u\n", (uint64_t)phase,
                       (uint64_t)hsw_live.gt[phase].core, (uint64_t)hsw_live.gt[phase].rc6);
    console_printf("hsw-live-observe: v=1 complete=%u cpu=0 begin=%u end=%u smi=%u,%u writes=0 authority=none\n",
                   (uint64_t)ok, ok ? hsw_candidate_result.begin : 0, ok ? hsw_candidate_result.end : 0,
                   (uint64_t)(ok ? hsw_live.smi_begin : 0), (uint64_t)(ok ? hsw_live.smi_end : 0));
    if (ok) for (unsigned phase = 0; phase < 2; phase++) {
        const struct package_pci_af *igd = &hsw_live.af[phase][0], *hda = &hsw_live.af[phase][1];
        console_printf("hsw-live-af: phase=%u igd=%u,%u,%u hda=%u,%u,%u\n", (uint64_t)phase,
                       (uint64_t)igd->offset, (uint64_t)igd->header, (uint64_t)igd->control_status,
                       (uint64_t)hda->offset, (uint64_t)hda->header, (uint64_t)hda->control_status);
    }
}

static bool hsw_candidate_display_unchanged(const uint32_t *row) {
    return row[HC_EDP_CONF] == 0xc0000018 && row[HC_EDP_FUNC] == 0x82200000 &&
           row[HC_DDI_A] == 0x80000011 && row[HC_PRIMARY] == 0x98004400 &&
           !row[HC_SURF] && !row[HC_LIVE];
}

static bool hsw_candidate_route(void) {
    return resource_read(0x7f008) == 0xc0000018 && resource_read(0x6f400) == 0x82200000 &&
           resource_read(0x64000) == 0x80000011 && resource_read(0x70180) == 0x98004400 &&
           !resource_read(0x7019c) && !resource_read(0x701ac);
}

/* Take the FBC sample before/after the candidate bracket. The only borrowed
 * alias switches pages after a verified retirement. The register offset is
 * the compressed-buffer base, never an address to dereference. */
static bool hsw_fbc_sample(void) {
    if (!hsw_config_valid() || hsw_access_result.fbc_rows >= 2) return false;
    uint32_t wells[4];
    for (unsigned i = 0; i < 4; i++) wells[i] = well_read(0x400 + i * 4);
    if (!display_resource_well_on(wells) || !hsw_candidate_route() || !hsw_map(NULL, HSW_FBC_PAGE)) return false;
    hsw_access_result.fbc_maps++;

    uint32_t *row = hsw_access_result.fbc[hsw_access_result.fbc_rows];
    uint32_t closing_ctl;
    bool ok = hsw_config_valid() && hsw_read(NULL, HSW_FBC_CTL, &row[HF_CTL]) &&
              hsw_read(NULL, HSW_FBC_BASE, &row[HF_BASE]) &&
              hsw_read(NULL, HSW_FBC_STATUS, &row[HF_STATUS]) &&
              hsw_read(NULL, HSW_FBC_CTL, &closing_ctl) && closing_ctl == row[HF_CTL];
    for (unsigned i = 0; i < HF_WORDS; i++) if (row[i] == UINT32_MAX) ok = false;
    for (unsigned i = 0; i < 4; i++) if (wells[i] != well_read(0x400 + i * 4)) ok = false;
    ok = ok && hsw_candidate_route() && hsw_config_valid();
    if (!hsw_unmap(NULL)) return false;
    hsw_access_result.fbc_retired++;
    if (ok) hsw_access_result.fbc_rows++;
    return ok;
}

static bool hsw_access_entries(uint32_t pte) {
    if (hsw_access_result.entries >= 2) return false;
    uint32_t *row = hsw_access_result.neighbors[hsw_access_result.entries];
    row[0] = pte;
    for (unsigned i = 1; i < HSW_ACCESS_SLOTS; i++)
        if (!hsw_read(NULL, HSW_CANDIDATE_MMIO + 4 * i, &row[i]) || row[i] == UINT32_MAX)
            return false;
    uint32_t closing;
    if (!hsw_read(NULL, HSW_CANDIDATE_MMIO, &closing) || closing != pte) return false;
    hsw_access_result.entries++;
    return true;
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
    struct package_pci_af *af = hsw_live.slot_probe ? hsw_live.af[hsw_candidate_result.rows] : NULL;
    if (!hsw_candidate_display_unchanged(row) ||
        !hsw_candidate_pci(gpu, 0x0a268086, &row[HC_IGD_CMD], af ? &af[0] : NULL) ||
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
        !hsw_candidate_pci(hda, 0x0a0c8086, &row[HC_HDA_CMD], af ? &af[1] : NULL)) return false;

    row[HC_FRAME] = resource_read(0x70040);
    if (!hsw_config_valid() || !hsw_read(NULL, hsw_candidate_window, &row[HC_PTE]) ||
        row[HC_PTE] == UINT32_MAX ||
        (hsw_access_window && !hsw_access_entries(row[HC_PTE])) ||
        (hsw_live.slot_probe && !hsw_live_gt())) return false;
    for (unsigned i = 0; i < 4; i++) if (wells[i] != well_read(0x400 + i * 4)) return false;
    for (unsigned i = HC_EDP_CONF; i <= HC_LIVE; i++)
        if (row[i] != resource_read(hsw_candidate_regs[i - HC_EDP_CONF])) return false;
    return hsw_config_valid();
}

static bool hsw_candidate_in_budget(void) {
    uint64_t now = timer_monotonic_us();
    return now > hsw_candidate_result.begin &&
           now - hsw_candidate_result.begin <= HSW_CANDIDATE_LIMIT_US;
}

static bool hsw_candidate_observe_access(bool access, bool boot_slot) {
    uint32_t mmio;
    if ((boot_slot && (access || hsw_live.slot_probe)) ||
        !intel_hsw_ggtt_candidate(&hsw_result, boot_slot ? HSW_BOOT_SLOT_GPU : HSW_CANDIDATE_GPU, &mmio) ||
        mmio != (boot_slot ? HSW_BOOT_SLOT_MMIO : HSW_CANDIDATE_MMIO)) return false;

    console_write("hsw-candidate-progress: sampling one read-only GGTT slot while Pipe A scans; no input needed\n");
    hsw_candidate_result.begin = timer_monotonic_us();
    hsw_candidate_result.status = 2; /* Access or sampling failure. */
    hsw_candidate_window = mmio;
    hsw_access_window = access;
    if (hsw_candidate_result.begin && (!hsw_live.slot_probe || hsw_smi_count(&hsw_live.smi_begin)) &&
        (!access || (hsw_fbc_sample() && hsw_candidate_in_budget())) &&
        hsw_map(NULL, mmio & ~4095u)) {
        hsw_candidate_result.mapped = true;
        uint32_t *before = hsw_candidate_result.sample[0], *after = hsw_candidate_result.sample[1];
        if (hsw_candidate_sample(before)) {
            hsw_candidate_result.rows = 1;
            hsw_candidate_result.status = 4; /* Clock or deadline. */
            bool waited = timer_delay_us(HSW_CANDIDATE_GAP_US);
            if (waited && hsw_candidate_in_budget()) {
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
        if (access && hsw_candidate_result.status == 1) {
            if (!hsw_candidate_in_budget()) hsw_candidate_result.status = 4;
            else if (!hsw_fbc_sample()) hsw_candidate_result.status = 2;
            else if (hsw_access_result.fbc[0][HF_CTL] != hsw_access_result.fbc[1][HF_CTL] ||
                     hsw_access_result.fbc[0][HF_BASE] != hsw_access_result.fbc[1][HF_BASE])
                hsw_candidate_result.status = 3;
            for (unsigned i = 0; i < HSW_ACCESS_SLOTS; i++)
                if (hsw_access_result.neighbors[0][i] != hsw_access_result.neighbors[1][i] &&
                    hsw_candidate_result.status == 1) hsw_candidate_result.status = 3;
        }
    }
    if (hsw_window_poison) hsw_candidate_result.status = 5;
    if (hsw_live.slot_probe && hsw_candidate_result.status == 1 && !hsw_smi_count(&hsw_live.smi_end))
        hsw_candidate_result.status = 2;
    hsw_access_window = false;
    hsw_candidate_window = 0;
    hsw_candidate_result.end = timer_monotonic_us();
    if (hsw_candidate_result.status == 1 &&
        (hsw_candidate_result.end <= hsw_candidate_result.begin ||
         hsw_candidate_result.end - hsw_candidate_result.begin < HSW_CANDIDATE_GAP_US ||
         hsw_candidate_result.end - hsw_candidate_result.begin > HSW_CANDIDATE_LIMIT_US))
        hsw_candidate_result.status = 4;
    return hsw_candidate_result.status == 1;
}

static void hsw_candidate_report(bool boot_slot) {
    console_printf("hsw-candidate: v=1 status=%u rows=%u %s begin=%u end=%u mapped=%u retired=%u writes=0 authority=none\n",
                   (uint64_t)hsw_candidate_result.status, (uint64_t)hsw_candidate_result.rows,
                   boot_slot ? "slot=16385 mmio=0x210004" : "slot=4096 mmio=0x204000",
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

static void hsw_access_report(bool observed) {
    console_printf("hsw-access: v=1 observed=%u fbc_rows=%u entry_rows=%u fbc_maps=%u fbc_retired=%u writes=0 authority=none\n",
                   (uint64_t)observed, (uint64_t)hsw_access_result.fbc_rows, (uint64_t)hsw_access_result.entries,
                   (uint64_t)hsw_access_result.fbc_maps, (uint64_t)hsw_access_result.fbc_retired);
    for (unsigned phase = 0; phase < hsw_access_result.fbc_rows; phase++) {
        uint8_t raw[HF_WORDS * 4];
        char data[sizeof(raw) * 4 / 3 + 5];
        unsigned n = 0;
        for (unsigned i = 0; i < HF_WORDS; i++) hda_put(raw, &n, hsw_access_result.fbc[phase][i], 4);
        hsw_b64(raw, n, data);
        console_printf("hsw-access-fbc: phase=%u data=%s\n", (uint64_t)phase, data);
    }
    for (unsigned phase = 0; phase < hsw_access_result.entries; phase++) {
        uint8_t raw[HSW_ACCESS_SLOTS * 4];
        char data[sizeof(raw) * 4 / 3 + 5];
        unsigned n = 0;
        for (unsigned i = 0; i < HSW_ACCESS_SLOTS; i++) hda_put(raw, &n, hsw_access_result.neighbors[phase][i], 4);
        hsw_b64(raw, n, data);
        console_printf("hsw-access-entries: phase=%u data=%s\n", (uint64_t)phase, data);
    }
}
