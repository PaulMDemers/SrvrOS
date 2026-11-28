/* Endpoint observations of access prerequisites only. This path has no
 * wake owner, engine-register access or requester control callbacks. */
static struct {
    unsigned rows, maps, retired;
    struct {
        uint64_t begin, end;
        uint32_t ack, thread, core, command, pm, smi;
        struct package_pci_af af;
    } row[2];
} hsw_engine_prereqs;

static bool hsw_engine_ack(uint32_t *ack) {
    hsw_wake_ack_window = true;
    bool mapped_ack = hsw_map(NULL, HSW_WAKE_ACK_PAGE);
    if (mapped_ack) hsw_engine_prereqs.maps++;
    bool read = mapped_ack && hsw_survey_tick() && hsw_read(NULL, HSW_WAKE_ACK, ack) &&
                *ack != UINT32_MAX;
    if (mapped_ack && hsw_unmap(NULL)) hsw_engine_prereqs.retired++;
    hsw_wake_ack_window = false;
    if (hsw_window_poison) {
        hsw_survey.status = HSW_SURVEY_RETAINED;
        return false;
    }
    return read && hsw_survey_tick();
}

static bool hsw_engine_prereqs_sample(unsigned phase) {
    if (!hsw_survey_display()) return false;
    hsw_engine_prereqs.row[phase].begin = hsw_survey.end;
    uint32_t pci[4];
    if (!hsw_candidate_pci(gpu, 0x0a268086, pci, &hsw_engine_prereqs.row[phase].af) ||
        !(pci[0] & 2) || (pci[1] & 3) ||
        !hsw_smi_count(&hsw_engine_prereqs.row[phase].smi) ||
        !hsw_engine_ack(&hsw_engine_prereqs.row[phase].ack)) return false;
    hsw_engine_prereqs.row[phase].command = pci[0];
    hsw_engine_prereqs.row[phase].pm = pci[1];
    hsw_engine_prereqs.row[phase].thread = graphics_read(0x5c);
    hsw_engine_prereqs.row[phase].core = graphics_read(0x60);
    if (hsw_engine_prereqs.row[phase].thread == UINT32_MAX ||
        hsw_engine_prereqs.row[phase].core == UINT32_MAX) return false;
    hsw_survey.frames[phase] = resource_read(0x70040);
    if (!hsw_survey_display()) return false;
    hsw_engine_prereqs.row[phase].end = hsw_survey.end;
    hsw_engine_prereqs.rows++;
    return true;
}

static bool hsw_engine_prereqs_run(void) {
    uint32_t first;
    /* Reuse the exact inherited framebuffer profile check, without
     * mapping or reading the returned candidate PTE address. */
    if (!intel_hsw_ggtt_candidate(&hsw_result, HSW_OWNERSHIP_FIRST * 4096u, &first)) return false;
    console_write("hsw-engine-prereqs-progress: sampling wake and PCI status while Pipe A scans\n");
    hsw_survey.begin = hsw_survey.end = timer_monotonic_us();
    hsw_survey.status = HSW_SURVEY_ACCESS;
    if (!hsw_survey.begin || !hsw_engine_prereqs_sample(0)) return false;
    uint64_t first_end = hsw_engine_prereqs.row[0].end;
    if (!timer_delay_us(HSW_CANDIDATE_GAP_US) || !hsw_survey_tick() ||
        hsw_survey.end - first_end < HSW_CANDIDATE_GAP_US) {
        hsw_survey.status = HSW_SURVEY_DEADLINE;
        return false;
    }
    if (!hsw_engine_prereqs_sample(1)) return false;
    uint32_t frames = hsw_survey.frames[1] - hsw_survey.frames[0];
    hsw_survey.status = frames && frames <= 8 ? HSW_SURVEY_OBSERVED : HSW_SURVEY_CHANGED;
    return hsw_survey.status == HSW_SURVEY_OBSERVED;
}

static void hsw_engine_prereqs_report(void) {
    console_printf("hsw-engine-prereqs: v=1 status=%u rows=%u maps=%u retired=%u begin=%u end=%u frames=%u,%u writes=0 ring_reads=0 protected_reads=0 awake_owner=unavailable requester_hold=unverified\n",
                   (uint64_t)hsw_survey.status, (uint64_t)hsw_engine_prereqs.rows,
                   (uint64_t)hsw_engine_prereqs.maps, (uint64_t)hsw_engine_prereqs.retired,
                   hsw_survey.begin, hsw_survey.end, (uint64_t)hsw_survey.frames[0], (uint64_t)hsw_survey.frames[1]);
    for (unsigned phase = 0; phase < hsw_engine_prereqs.rows; phase++) {
        const struct package_pci_af *af = &hsw_engine_prereqs.row[phase].af;
        console_printf("hsw-engine-prereqs-row: phase=%u begin=%u end=%u ack=%u thread=%u core=%u cmd=%u pm=%u smi=%u af=%u,%u,%u\n",
                       (uint64_t)phase, hsw_engine_prereqs.row[phase].begin, hsw_engine_prereqs.row[phase].end,
                       (uint64_t)hsw_engine_prereqs.row[phase].ack, (uint64_t)hsw_engine_prereqs.row[phase].thread,
                       (uint64_t)hsw_engine_prereqs.row[phase].core, (uint64_t)hsw_engine_prereqs.row[phase].command,
                       (uint64_t)hsw_engine_prereqs.row[phase].pm, (uint64_t)hsw_engine_prereqs.row[phase].smi,
                       (uint64_t)af->offset, (uint64_t)af->header, (uint64_t)af->control_status);
    }
}
