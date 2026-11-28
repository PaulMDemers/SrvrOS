/* Four fixed GGTT table pages at 32, 96, 160 and 224 MiB of GPU address
 * space. Only their 32-bit entries are read, never the referenced memory. */
#define HSW_SURVEY_PAGES 4u
#define HSW_SURVEY_WORDS 1024u
#define HSW_SURVEY_CHUNK 128u
#define HSW_SURVEY_TABLE 0x200000u
static const uint32_t hsw_survey_first[HSW_SURVEY_PAGES] = {8192, 24576, 40960, 57344};

enum hsw_survey_status {
    HSW_SURVEY_REFUSED, HSW_SURVEY_OBSERVED, HSW_SURVEY_ACCESS,
    HSW_SURVEY_CHANGED, HSW_SURVEY_DEADLINE, HSW_SURVEY_RETAINED, HSW_SURVEY_NOT_POPULATED,
};
struct hsw_survey_pass {
    uint64_t begin, end, hash;
    unsigned reads, maps, retired;
};
struct hsw_survey_batch {
    enum hsw_survey_status status;
    uint64_t begin, end;
    uint32_t frames[2];
    struct hsw_survey_pass pass[2];
};
static struct hsw_survey_batch hsw_survey;
static uint32_t hsw_survey_words[HSW_SURVEY_PAGES][HSW_SURVEY_WORDS];

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

static bool hsw_survey_page(unsigned phase, unsigned page, uint32_t mmio, unsigned words) {
    struct hsw_survey_pass *pass = &hsw_survey.pass[phase];
    if (!hsw_survey_display()) return false;
    hsw_survey_window = mmio;
    hsw_survey_window_end = mmio + words * 4;
    bool mapped_page = hsw_map(NULL, mmio);
    bool ok = mapped_page && hsw_survey_display();
    if (mapped_page) pass->maps++;
    for (unsigned i = 0; ok && i < words; i++) {
        if (!(i % 32) && !hsw_survey_tick()) {ok = false; break;}
        uint32_t off = mmio + i * 4;
        if (hsw_survey_protected(off)) continue;
        uint32_t word;
        if (!hsw_read(NULL, off, &word)) {ok = false; break;}
        pass->reads++;
        if (word == UINT32_MAX) {ok = false; break;}
        if (phase && word != hsw_survey_words[page][i]) {
            hsw_survey.status = HSW_SURVEY_CHANGED;
            ok = false;
            break;
        }
        if (!phase) hsw_survey_words[page][i] = word;
        for (unsigned byte = 0; byte < 4; byte++)
            pass->hash = (pass->hash ^ ((word >> (8 * byte)) & 255u)) * 1099511628211ull;
    }
    if (mapped_page && hsw_unmap(NULL)) pass->retired++;
    hsw_survey_window = 0;
    hsw_survey_window_end = 0;
    if (hsw_window_poison) {
        hsw_survey.status = HSW_SURVEY_RETAINED;
        return false;
    }
    return ok && hsw_survey_display();
}

static bool hsw_survey_batch(const uint32_t *first, unsigned pages, unsigned words) {
    if (!pages || pages > HSW_SURVEY_PAGES || !words || words > HSW_SURVEY_WORDS) return false;
    uint32_t mmio[HSW_SURVEY_PAGES];
    for (unsigned i = 0; i < pages; i++) {
        uint32_t start = first[i];
        uint32_t last;
        if (start >= 65536 || words > 65536 - start) return false;
        if (!intel_hsw_ggtt_candidate(&hsw_result, start * 4096u, &mmio[i]) ||
            !intel_hsw_ggtt_candidate(&hsw_result, (start + words - 1) * 4096u, &last) ||
            (mmio[i] & 4095u) || last != mmio[i] + (words - 1) * 4u) return false;
    }
    if (hsw_requester_hold.state != HSW_HOLD_ACTIVE)
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
        for (unsigned page = 0; page < pages; page++)
            if (!hsw_survey_page(phase, page, mmio[page], words)) return false;
        pass->end = hsw_survey.end;
    }
    hsw_survey.frames[1] = resource_read(0x70040);
    if (!hsw_survey_display()) return false;
    uint32_t advance = hsw_survey.frames[1] - hsw_survey.frames[0];
    hsw_survey.status = advance && advance <= 8 ? HSW_SURVEY_OBSERVED : HSW_SURVEY_CHANGED;
    return hsw_survey.status == HSW_SURVEY_OBSERVED;
}

static bool hsw_survey_run(void) {
    console_write("hsw-survey-progress: two read-only GGTT passes with inherited Pipe A; no input needed\n");
    return hsw_survey_batch(hsw_survey_first, HSW_SURVEY_PAGES, HSW_SURVEY_WORDS);
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
            for (unsigned j = 0; j < HSW_SURVEY_CHUNK; j++) hda_put(raw, &n, hsw_survey_words[page][i+j], 4);
            hsw_b64(raw, n, data);
            console_printf("hsw-survey-data: first=%u data=%s\n", (uint64_t)hsw_survey_first[page] + i, data);
        }
}

#define HSW_SWEEP_BATCHES 14u
#define HSW_SWEEP_CANDIDATES 16u
struct hsw_sweep_row {
    struct hsw_survey_batch batch;
    uint32_t first[HSW_SURVEY_PAGES];
    unsigned present, nonzero, zero, first_image, stolen, shortlisted;
    uint32_t shortlist[HSW_SWEEP_CANDIDATES];
};
static struct {
    enum hsw_survey_status status;
    unsigned attempted, complete;
    struct hsw_sweep_row row[HSW_SWEEP_BATCHES];
} hsw_sweep;

static void hsw_sweep_tally(struct hsw_sweep_row *row) {
    uint64_t first_image = intel_hsw_ggtt_address(hsw_result.sample[0].pte[0]);
    for (unsigned page = 0; page < HSW_SURVEY_PAGES; page++)
        for (unsigned i = 0; i < HSW_SURVEY_WORDS; i++) {
            uint32_t slot = row->first[page] + i;
            if (hsw_survey_protected(HSW_SURVEY_TABLE + 4 * slot)) continue;
            uint32_t word = hsw_survey_words[page][i];
            if (!word) {
                row->zero++;
                if (row->shortlisted < HSW_SWEEP_CANDIDATES)
                    row->shortlist[row->shortlisted++] = slot;
            } else if (word & 1u) {
                uint64_t address = intel_hsw_ggtt_address(word);
                row->present++;
                row->first_image += address == first_image;
                row->stolen += address >= hsw_result.stolen_base && address < hsw_result.stolen_end;
            } else row->nonzero++;
        }
}

static bool hsw_sweep_run(void) {
    unsigned pages = 0;
    for (uint32_t slot = 4096; slot < 65536; slot += HSW_SURVEY_WORDS) {
        bool surveyed = false;
        for (unsigned i = 0; i < HSW_SURVEY_PAGES; i++)
            surveyed |= slot == hsw_survey_first[i];
        if (surveyed) continue;
        if (pages == HSW_SWEEP_BATCHES * HSW_SURVEY_PAGES) return false;
        hsw_sweep.row[pages / HSW_SURVEY_PAGES].first[pages % HSW_SURVEY_PAGES] = slot;
        pages++;
    }
    if (pages != HSW_SWEEP_BATCHES * HSW_SURVEY_PAGES) return false;

    console_write("hsw-sweep-progress: fourteen read-only GGTT batches; inherited Pipe A stays active\n");
    for (unsigned i = 0; i < HSW_SWEEP_BATCHES; i++) {
        struct hsw_sweep_row *row = &hsw_sweep.row[i];
        hsw_survey = (struct hsw_survey_batch){0};
        bool ok = hsw_survey_batch(row->first, HSW_SURVEY_PAGES, HSW_SURVEY_WORDS);
        row->batch = hsw_survey;
        hsw_sweep.attempted++;
        if (!ok) {
            hsw_sweep.status = hsw_survey.status;
            return false;
        }
        hsw_sweep_tally(row);
        hsw_sweep.complete++;
    }
    hsw_sweep.status = HSW_SURVEY_OBSERVED;
    return true;
}

static void hsw_sweep_report(void) {
    console_printf("hsw-sweep: v=1 status=%u batches=%u complete=%u writes=0 authority=none\n",
                   (uint64_t)hsw_sweep.status, (uint64_t)hsw_sweep.attempted, (uint64_t)hsw_sweep.complete);
    for (unsigned i = 0; i < hsw_sweep.attempted; i++) {
        const struct hsw_sweep_row *row = &hsw_sweep.row[i];
        const struct hsw_survey_batch *b = &row->batch;
        console_printf("hsw-sweep-batch: index=%u status=%u pages=%u,%u,%u,%u begin=%u end=%u frames=%u,%u\n",
                       (uint64_t)i, (uint64_t)b->status, (uint64_t)row->first[0], (uint64_t)row->first[1],
                       (uint64_t)row->first[2], (uint64_t)row->first[3], b->begin, b->end,
                       (uint64_t)b->frames[0], (uint64_t)b->frames[1]);
        for (unsigned phase = 0; phase < 2; phase++) {
            const struct hsw_survey_pass *p = &b->pass[phase];
            console_printf("hsw-sweep-pass: index=%u phase=%u begin=%u end=%u reads=%u maps=%u retired=%u hash=%u\n",
                           (uint64_t)i, (uint64_t)phase, p->begin, p->end, (uint64_t)p->reads,
                           (uint64_t)p->maps, (uint64_t)p->retired, p->hash);
        }
        if (i >= hsw_sweep.complete) continue;
        console_printf("hsw-sweep-count: index=%u present=%u nonzero=%u zero=%u first_image=%u stolen=%u\n",
                       (uint64_t)i, (uint64_t)row->present, (uint64_t)row->nonzero, (uint64_t)row->zero,
                       (uint64_t)row->first_image, (uint64_t)row->stolen);
        for (unsigned j = 0; j < row->shortlisted; j++)
            console_printf("hsw-sweep-zero: index=%u slot=%u\n", (uint64_t)i, (uint64_t)row->shortlist[j]);
    }
}

/* One populated range at GPU address 96 MiB, on the already surveyed page.
 * A complete observation produces a refusal receipt, never a range loan. */
#define HSW_OWNERSHIP_FIRST 24576u
#define HSW_OWNERSHIP_WORDS 16u
static unsigned hsw_ownership_image_aliases;

static bool hsw_ownership_run(void) {
    const uint32_t first = HSW_OWNERSHIP_FIRST;
    console_write("hsw-ownership-progress: checking populated GGTT range without hardware writes; Pipe A stays active\n");
    if (!hsw_survey_batch(&first, 1, HSW_OWNERSHIP_WORDS)) return false;
    for (unsigned i = 0; i < HSW_OWNERSHIP_WORDS; i++)
        if (!(hsw_survey_words[0][i] & 1u)) {
            hsw_survey.status = HSW_SURVEY_NOT_POPULATED;
            return false;
        }
    const struct hwb_sample *image = &hsw_result.sample[0];
    for (unsigned i = 0; i < HSW_OWNERSHIP_WORDS; i++) {
        uint64_t address = intel_hsw_ggtt_address(hsw_survey_words[0][i]);
        for (unsigned j = 0; j < image->count; j++)
            if (address == intel_hsw_ggtt_address(image->pte[j])) {
                hsw_ownership_image_aliases++;
                break;
            }
    }
    return true;
}

static bool hsw_requester_refuse(void) {
    if (hsw_requester_hold.state != HSW_HOLD_ACTIVE || hsw_requester_hold.refusals >= 2 ||
        hsw_requester_hold.igd != gpu || hsw_requester_hold.hda != hsw_display_hda() ||
        pci_read_config32(hsw_requester_hold.hda, 0) != 0x0a0c8086 || !hsw_survey_display())
        return false;

    /* Admission has no trusted firmware handoff provider on this platform.
     * The boot-local guard and read-only endpoints cannot grant a GGTT loan. */
    hsw_requester_hold.denied[hsw_requester_hold.refusals++] = hsw_survey.end;
    return true;
}

static bool hsw_requester_hold_run(void) {
    uint32_t first, last;
    const struct pci_device *hda = hsw_display_hda();
    if (hsw_requester_hold.state != HSW_HOLD_OFF || hsw_window_live || hsw_window_poison ||
        !display_profile_is_air61() || !hda || pci_read_config32(hda, 0) != 0x0a0c8086 ||
        !intel_hsw_ggtt_candidate(&hsw_result, HSW_OWNERSHIP_FIRST * 4096u, &first) ||
        !intel_hsw_ggtt_candidate(&hsw_result, (HSW_OWNERSHIP_FIRST + HSW_OWNERSHIP_WORDS - 1) * 4096u, &last) ||
        last - first != (HSW_OWNERSHIP_WORDS - 1) * 4u) return false;
    uint64_t begin = timer_monotonic_us();
    if (!begin) return false;

    /* The isolated boot has no graphics clients. Keep one scoped read-only
     * owner from before the first admission attempt through alias retirement. */
    hsw_requester_hold.state = HSW_HOLD_ACTIVE;
    hsw_requester_hold.igd = gpu;
    hsw_requester_hold.hda = hda;
    hsw_requester_hold.first = first;
    hsw_requester_hold.end = last + 4;
    hsw_requester_hold.begin = hsw_survey.begin = hsw_survey.end = begin;
    hsw_survey.status = HSW_SURVEY_ACCESS;
    bool observed = hsw_requester_refuse() && hsw_ownership_run();
    bool retired = !hsw_window_live && !hsw_window_poison;
    if (observed && (!retired || !hsw_requester_refuse())) {
        if (hsw_survey.status == HSW_SURVEY_OBSERVED) hsw_survey.status = HSW_SURVEY_ACCESS;
        observed = false;
    }
    hsw_requester_hold.finish = hsw_survey.end;
    hsw_requester_hold.state = retired ? HSW_HOLD_RETIRED : HSW_HOLD_RETAINED;
    return observed;
}

static void hsw_requester_hold_report(void) {
    const char *state = hsw_requester_hold.state == HSW_HOLD_RETIRED ? "retired" :
                        hsw_requester_hold.state == HSW_HOLD_RETAINED ? "retained" : "unavailable";
    console_printf("hsw-requester-hold: v=1 state=%s begin=%u denied=%u,%u end=%u refusals=%u firmware_handoff=unavailable hardware_hold=unverified protected_reads=0 authority=none\n",
                   state, hsw_requester_hold.begin, hsw_requester_hold.denied[0], hsw_requester_hold.denied[1],
                   hsw_requester_hold.finish, (uint64_t)hsw_requester_hold.refusals);
}

static void hsw_ownership_report(void) {
    console_printf("hsw-ownership: v=1 status=%u first=%u count=%u begin=%u end=%u frames=%u,%u writes=0 authority=none\n",
                   (uint64_t)hsw_survey.status, (uint64_t)HSW_OWNERSHIP_FIRST, (uint64_t)HSW_OWNERSHIP_WORDS,
                   hsw_survey.begin, hsw_survey.end, (uint64_t)hsw_survey.frames[0], (uint64_t)hsw_survey.frames[1]);
    for (unsigned i = 0; i < 2; i++) {
        const struct hsw_survey_pass *p = &hsw_survey.pass[i];
        console_printf("hsw-ownership-pass: phase=%u begin=%u end=%u reads=%u maps=%u retired=%u hash=%u\n",
                       (uint64_t)i, p->begin, p->end, (uint64_t)p->reads, (uint64_t)p->maps,
                       (uint64_t)p->retired, p->hash);
    }
    if (hsw_survey.status != HSW_SURVEY_OBSERVED) return;
    /* There is no live grant or maintained requester provider in this
     * profile. Report those gaps independently of the observed aliases. */
    console_printf("hsw-ownership-refusal: admission=refused image_aliases=%u range_grant=unavailable requester_hold=unavailable drain=unverified awake_engines=unavailable firmware_exclusion=unverified\n",
                   (uint64_t)hsw_ownership_image_aliases);
    uint8_t raw[HSW_OWNERSHIP_WORDS * 4];
    char data[sizeof(raw) * 4 / 3 + 5];
    unsigned n = 0;
    for (unsigned i = 0; i < HSW_OWNERSHIP_WORDS; i++) hda_put(raw, &n, hsw_survey_words[0][i], 4);
    hsw_b64(raw, n, data);
    console_printf("hsw-ownership-data: data=%s\n", data);
}
