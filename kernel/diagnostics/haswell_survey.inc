/* Four fixed GGTT table pages at 32, 96, 160 and 224 MiB of GPU address
 * space. Only their 32-bit entries are read, never the referenced memory. */
#define HSW_SURVEY_PAGES 4u
#define HSW_SURVEY_WORDS 1024u
#define HSW_SURVEY_CHUNK 128u
static const uint32_t hsw_survey_first[HSW_SURVEY_PAGES] = {8192, 24576, 40960, 57344};

enum hsw_survey_status {
    HSW_SURVEY_REFUSED, HSW_SURVEY_OBSERVED, HSW_SURVEY_ACCESS,
    HSW_SURVEY_CHANGED, HSW_SURVEY_DEADLINE, HSW_SURVEY_RETAINED,
};
struct hsw_survey_pass {
    uint64_t begin, end, hash;
    unsigned reads, maps, retired;
};
static struct {
    enum hsw_survey_status status;
    uint64_t begin, end;
    uint32_t frames[2];
    struct hsw_survey_pass pass[2];
    uint32_t words[HSW_SURVEY_PAGES][HSW_SURVEY_WORDS];
} hsw_survey;

static bool hsw_survey_tick(void) {
    uint64_t now = timer_monotonic_us();
    if (!now || now < hsw_survey.end || now < hsw_survey.begin ||
        now - hsw_survey.begin > HSW_CANDIDATE_LIMIT_US) {
        hsw_survey.status = HSW_SURVEY_DEADLINE;
        return false;
    }
    hsw_survey.end = now;
    return true;
}

static bool hsw_survey_display(void) {
    if (!hsw_config_valid()) return false;
    const uint64_t *bound = hsw_result.sample[1].endpoint.v;
    if (pci_read_config32(hsw_host, 0x50) != bound[HW_GGC] ||
        pci_read_config32(hsw_host, 0xb0) != bound[HW_BDSM] ||
        pci_read_config32(hsw_host, 0xb4) != bound[HW_BGSM] ||
        pci_read_config32(gpu, 0x50) != bound[HW_GPU_GGC] ||
        pci_read_config32(gpu, 0x5c) != bound[HW_GPU_BDSM]) return false;
    uint32_t wells[4];
    for (unsigned i = 0; i < 4; i++) wells[i] = well_read(0x400 + i * 4);
    if (!display_resource_well_on(wells) || !hsw_candidate_route()) return false;
    for (unsigned i = HC_PRIMARY_B; i <= HC_CURSOR_C; i++) {
        uint32_t control = resource_read(hsw_candidate_regs[i - HC_EDP_CONF]);
        /* Primary and sprite enable bits are at bit 31. Require the
         * Haswell cursor mode field to be zero on all three pipes. */
        if (control == UINT32_MAX || (control & (i < HC_CURSOR_A ? 1u << 31 : 0x3fu)))
            return false;
    }
    return hsw_survey_tick();
}

static bool hsw_survey_page(unsigned phase, unsigned page, uint32_t mmio) {
    struct hsw_survey_pass *pass = &hsw_survey.pass[phase];
    if (!hsw_survey_display()) return false;
    hsw_survey_window = mmio;
    bool mapped_page = hsw_map(NULL, mmio);
    bool ok = mapped_page && hsw_survey_display();
    if (mapped_page) pass->maps++;
    for (unsigned i = 0; ok && i < HSW_SURVEY_WORDS; i++) {
        if (!(i % 32) && !hsw_survey_tick()) {ok = false; break;}
        uint32_t word;
        if (!hsw_read(NULL, mmio + i * 4, &word)) {ok = false; break;}
        pass->reads++;
        if (word == UINT32_MAX) {ok = false; break;}
        if (phase && word != hsw_survey.words[page][i]) {
            hsw_survey.status = HSW_SURVEY_CHANGED;
            ok = false;
            break;
        }
        if (!phase) hsw_survey.words[page][i] = word;
        for (unsigned byte = 0; byte < 4; byte++)
            pass->hash = (pass->hash ^ ((word >> (8 * byte)) & 255u)) * 1099511628211ull;
    }
    if (mapped_page && hsw_unmap(NULL)) pass->retired++;
    hsw_survey_window = 0;
    if (hsw_window_poison) {
        hsw_survey.status = HSW_SURVEY_RETAINED;
        return false;
    }
    return ok && hsw_survey_display();
}

static bool hsw_survey_run(void) {
    uint32_t mmio[HSW_SURVEY_PAGES];
    for (unsigned i = 0; i < HSW_SURVEY_PAGES; i++) {
        uint32_t start = hsw_survey_first[i], end = start + HSW_SURVEY_WORDS;
        uint32_t last;
        if ((start <= 4096 && 4096 < end) || (start <= 16385 && 16385 < end) ||
            !intel_hsw_ggtt_candidate(&hsw_result, start * 4096u, &mmio[i]) ||
            !intel_hsw_ggtt_candidate(&hsw_result, (end - 1) * 4096u, &last) ||
            (mmio[i] & 4095u) || last != mmio[i] + 4092u) return false;
    }
    console_write("hsw-survey-progress: two read-only GGTT passes with inherited Pipe A; no input needed\n");
    hsw_survey.begin = hsw_survey.end = timer_monotonic_us();
    hsw_survey.status = HSW_SURVEY_ACCESS;
    if (!hsw_survey.begin || !hsw_survey_display()) return false;
    hsw_survey.frames[0] = resource_read(0x70040);
    for (unsigned phase = 0; phase < 2; phase++) {
        struct hsw_survey_pass *pass = &hsw_survey.pass[phase];
        if (phase && (!timer_delay_us(HSW_CANDIDATE_GAP_US) || !hsw_survey_tick() ||
                      hsw_survey.end - hsw_survey.pass[0].end < HSW_CANDIDATE_GAP_US)) {
            hsw_survey.status = HSW_SURVEY_DEADLINE;
            return false;
        }
        pass->begin = hsw_survey.end;
        pass->hash = 14695981039346656037ull;
        for (unsigned page = 0; page < HSW_SURVEY_PAGES; page++)
            if (!hsw_survey_page(phase, page, mmio[page])) return false;
        pass->end = hsw_survey.end;
    }
    hsw_survey.frames[1] = resource_read(0x70040);
    if (!hsw_survey_display()) return false;
    uint32_t advance = hsw_survey.frames[1] - hsw_survey.frames[0];
    hsw_survey.status = advance && advance <= 8 ? HSW_SURVEY_OBSERVED : HSW_SURVEY_CHANGED;
    return hsw_survey.status == HSW_SURVEY_OBSERVED;
}

static void hsw_survey_report(void) {
    console_printf("hsw-survey: v=1 status=%u begin=%u end=%u frames=%u,%u writes=0 authority=none\n",
                   (uint64_t)hsw_survey.status, hsw_survey.begin, hsw_survey.end,
                   (uint64_t)hsw_survey.frames[0], (uint64_t)hsw_survey.frames[1]);
    for (unsigned i = 0; i < 2; i++) {
        const struct hsw_survey_pass *pass = &hsw_survey.pass[i];
        console_printf("hsw-survey-pass: phase=%u begin=%u end=%u reads=%u maps=%u retired=%u hash=%u\n",
                       (uint64_t)i, pass->begin, pass->end, (uint64_t)pass->reads,
                       (uint64_t)pass->maps, (uint64_t)pass->retired, pass->hash);
    }
    if (hsw_survey.status != HSW_SURVEY_OBSERVED) return;
    for (unsigned page = 0; page < HSW_SURVEY_PAGES; page++)
        for (unsigned i = 0; i < HSW_SURVEY_WORDS; i += HSW_SURVEY_CHUNK) {
            uint8_t raw[HSW_SURVEY_CHUNK * 4];
            char data[sizeof(raw) * 4 / 3 + 5];
            unsigned n = 0;
            for (unsigned j = 0; j < HSW_SURVEY_CHUNK; j++) hda_put(raw, &n, hsw_survey.words[page][i+j], 4);
            hsw_b64(raw, n, data);
            console_printf("hsw-survey-data: first=%u data=%s\n", (uint64_t)hsw_survey_first[page] + i, data);
        }
}
