/* Broadwell PRM volume 2c, pp. 403-405, 587-588, 805-814, 859, 994 and 1034.
 * The two GAM observations describe engine memory access, not completion. */
enum { GT_FLUSH_ENGINES = 5, GT_FLUSH_WORDS = 25, GT_FLUSH_SAMPLES = 4 };
static const uint32_t gt_flush_registers[GT_FLUSH_WORDS] = {
    0x40d0, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8038, 0x8040,
    0x12870, 0x12878, 0x1c870, 0x1c878, 0xa090, 0xa094, 0xa0b8,
    0x8038, 0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8040, 0x40d0
};

/* Verify both wireless producers while the display HDA owner is held and
 * immediately before sealing their programming pages in the sole CPU root. */
static struct {
    struct gt_display_access access;
    uint32_t strap, function, well_before, codec[3], well_after;
    struct gt_display_access messages;
    uint32_t device[GT_FLUSH_SAMPLES][2];
    unsigned samples;
    bool reading, sampling, disabled;
} gt_widi;
static bool slot_widi_preflight_live(void) {
    return gt_widi.reading && !gt_range.mapped && gt_range_live(0) &&
        independent_matches(0) && intel_hda_gpu_live(&joint.scope);
}
static bool slot_widi_register(uint32_t offset) {
    return offset==0x42014 || offset==0x6e400 || offset==0x45404 || offset==0x65020 || offset==0x65800;
}
static bool slot_widi_observe(const uint32_t *offsets,uint32_t *out,unsigned count) {
    return gt_held_read_alias(&gt_widi.access,offsets,out,count,slot_widi_preflight_live,slot_widi_register);
}
static bool slot_widi_preflight(void) {
    const uint32_t strap=0x42014, function=0x6e400, well=0x45404;
    const uint32_t codec[]={0x65020,0x65800,0x65020};
    if(gt_widi.reading || gt_widi.access.maps || gt_widi.disabled)return false;
    gt_widi.reading=true;
    bool ok=slot_widi_observe(&strap,&gt_widi.strap,1) && intel_bdw_air_wireless_video_disabled(gt_widi.strap) &&
        slot_widi_observe(&function,&gt_widi.function,1) && !(gt_widi.function&(1u<<31)) &&
        slot_widi_observe(&well,&gt_widi.well_before,1) && (gt_widi.well_before&(1u<<30)) &&
        slot_widi_observe(codec,gt_widi.codec,3) && gt_widi.codec[0]==0x80862808 &&
        !(gt_widi.codec[1]&(1u<<30)) && gt_widi.codec[2]==gt_widi.codec[0] &&
        slot_widi_observe(&well,&gt_widi.well_after,1) && (gt_widi.well_after&(1u<<30));
    gt_widi.reading=false;
    gt_widi.disabled=ok && !gt_widi.access.mapped && gt_widi.access.retired==5 && gt_widi.access.reads==7;
    return gt_widi.disabled;
}
/* Read the actual PTE once bus mastering is back, while the original slot,
 * its private backing and the WiDi programming exclusion are all still held. */
static struct {
    struct gt_display_access access;
    uint32_t raw[3];
    bool reading, matched;
} gt_reopened;
#define GT_SECOND_PTE_OFFSET (0x800000u + 8u * GT_SECOND_SLOT)
static bool slot_reopened_live(void) {
    return gt_reopened.reading && independent_reopened && gt_range.completed==2 && !gt_range.mapped &&
        gt_bootstrap_native.command_writes==2 && gt_bootstrap_native.pci_samples==4 &&
        bootstrap_parent() && gt_slot_live();
}
static bool slot_pte_register(uint32_t reg) {
    return reg==GT_SECOND_PTE_OFFSET || reg==GT_SECOND_PTE_OFFSET+4;
}
static bool slot_verify_reopened(void) {
    const uint32_t regs[]={GT_SECOND_PTE_OFFSET+4,GT_SECOND_PTE_OFFSET,GT_SECOND_PTE_OFFSET+4};
    if(gt_reopened.reading || gt_reopened.access.maps)return false;
    gt_reopened.reading=true;
    bool ok=gt_held_read_alias(&gt_reopened.access,regs,gt_reopened.raw,3,slot_reopened_live,slot_pte_register);
    gt_reopened.reading=false;
    uint64_t entry=((uint64_t)gt_reopened.raw[0]<<32)|gt_reopened.raw[1];
    gt_reopened.matched=ok && gt_reopened.raw[2]==gt_reopened.raw[0] &&
        entry==gt_range.words[1][GT_RANGE_TAIL+2] && entry==slot_backing.plan.original;
    return gt_reopened.matched;
}
/* Compare against the post-reset epoch again at the two PCI boundaries.
 * Both the slot and WiDi programming exclusion remain held. Queue identity
 * and parser state alone cannot acknowledge the L3 or pending device DMA. */
struct slot_queue_sample {
    uint32_t words[INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS];
    uint32_t context[INTEL_COMPLETION_CONTEXT];
};
static struct slot_queue_sample slot_queues[2];
static unsigned slot_queue_samples, slot_queue_matches;

/* After the reset, the queue census needs only reads. Keep the sole UC
 * mapping of each engine page immutable through both queue samples and the
 * final PTE readback. The parent stays on the boot CPU with IRQs excluded. */
static unsigned slot_ports_pinned, slot_ports_restored;
static bool slot_submission_hold(void) {
    if (gt_bootstrap_native.engine_ro_mask || slot_ports_pinned || !context_wp()) return false;
    for (unsigned first = 0; first < INTEL_ENGINE_COUNT; first += VMM_ALIAS_SPANS) {
        struct vmm_alias_span spans[VMM_ALIAS_SPANS];
        unsigned count = INTEL_ENGINE_COUNT - first;
        if (count > VMM_ALIAS_SPANS) count = VMM_ALIAS_SPANS;
        for (unsigned i = 0; i < count; ++i) {
            unsigned engine = first + i, slot = 3 + engine;
            if (!bootstrap_guard() || !bootstrap_tick() || !engine_page_protect(gt_bootstrap_native.root,slot, false)) return false;
            gt_bootstrap_native.engine_ro_mask |= 1u << engine;
            spans[i] = (struct vmm_alias_span){WAKE_VA + 4096 * slot, result.mmio_base + wake_pages[slot], 1};
        }
        if (!vmm_reserved_guard_hold_readonly_registers(&slot_widi.guard, gt_bootstrap_native.root, spans, count))
            return false;
        slot_ports_pinned += count;
    }
    return context_wp() && bootstrap_guard() && bootstrap_tick();
}

static bool slot_submission_restore(void) {
    if (slot_ports_pinned != INTEL_ENGINE_COUNT || gt_bootstrap_native.engine_ro_mask != 31 ||
        !slot_widi.closed || !independent_returned || !gt_slot_returned) return false;
    for (unsigned n = INTEL_ENGINE_COUNT; n; --n) {
        if (!context_wp() || !bootstrap_guard() || !bootstrap_tick() || !engine_page_protect(gt_bootstrap_native.root, n+2, true))
            return false;
        gt_bootstrap_native.engine_ro_mask &= ~(1u << (n-1));
        ++slot_ports_restored;
    }
    return bootstrap_guard() && bootstrap_tick();
}

static bool slot_queue_boundary(unsigned phase) {
    const struct intel_completion_result *post = &gt_bootstrap_native.post;
    if (phase > 1 || slot_queue_matches != phase || slot_queue_samples != phase ||
        gt_bootstrap_native.command_writes != phase + 1 || gt_bootstrap_native.pci_samples != phase + 3 ||
        !gt_bootstrap_native.post_bound || !post->complete || !post->stable ||
        post->engine_epoch != gt_bootstrap_native.g->engine_epoch ||
        post->enabled || post->pending || post->flip_wait || post->tlb_pending ||
        !gt_slot_live() || !bootstrap_guard() || !bootstrap_tick()) return false;

    if ((!phase && !slot_submission_hold()) || slot_ports_pinned != INTEL_ENGINE_COUNT ||
        gt_bootstrap_native.engine_ro_mask != 31 || !context_wp()) return false;
    struct slot_queue_sample *sample = &slot_queues[phase];
    if (!completion_snapshot(gt_bootstrap_native.a, sample->words, sample->context)) return false;
    ++slot_queue_samples;
    for (unsigned e = 0; e < INTEL_ENGINE_COUNT; ++e)
        for (unsigned i = 0; i < INTEL_COMPLETION_WORDS; ++i)
            if (sample->words[e][i] != post->words[1][e][i]) return false;
    for (unsigned i = 0; i < INTEL_COMPLETION_CONTEXT; ++i)
        if (sample->context[i] != post->context[1][i]) return false;
    if (!gt_slot_live() || !bootstrap_guard() || !bootstrap_tick()) return false;
    ++slot_queue_matches;
    return true;
}

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

static bool slot_widi_message(uint32_t offset) {
    return offset==0x8014 || offset==0x8108;
}
static bool slot_widi_sample(unsigned n) {
    static const uint32_t offsets[]={0x8014,0x8108}; /* MSG_IDLE_WIN, RSTFCTLMSG */
    if(n!=gt_widi.samples || n>=GT_FLUSH_SAMPLES || gt_widi.sampling || !gt_widi.disabled)return false;
    gt_widi.sampling=true;
    bool ok=gt_held_read_alias(&gt_widi.messages,offsets,gt_widi.device[n],2,gt_flush_live,slot_widi_message);
    gt_widi.sampling=false;
    if(!ok || gt_widi.messages.mapped || gt_widi.messages.retired!=n+1)return false;
    ++gt_widi.samples;
    return true;
}
static bool gt_flush_sample(unsigned n) {
    if (n >= GT_FLUSH_SAMPLES || n != gt_flush.samples || !gt_flush_live() || !bootstrap_tick())
        return false;
    struct gt_display_access *access = &gt_flush.access;
    gt_flush.begin[n] = gt_bootstrap_native.last;
    uint32_t *slice = gt_flush.slice[n];
    if (!slot_widi_sample(n) || !gt_flush_slice_read(9, 0x9120, &slice[0]) ||
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
    for (unsigned n = 0; n < slot_queue_samples; ++n) {
        const struct slot_queue_sample *sample = &slot_queues[n];
        char encoded[(sizeof(*sample)+2)/3*4+1];
        _Static_assert(sizeof(*sample) == 46 * sizeof(uint32_t), "queue receipt word layout");
        entry_requester_base64((const uint8_t *)sample,sizeof(*sample),encoded);
        console_printf("intel-slot-queues: v=1 phase=%u matched=%u data=%s\n",
            (uint64_t)n,(uint64_t)(n<slot_queue_matches),encoded);
    }
    console_printf("intel-gam-trace: v=1 reads=%u spans=%u omitted=%u max_gap_us=%u\n",
        (uint64_t)gt_request_trace.reads, (uint64_t)gt_request_trace.used,
        (uint64_t)gt_request_trace.omitted, gt_request_trace.max_gap);
    for (unsigned i = 0; i < gt_request_trace.used; ++i) {
        const struct gt_request_span *s = &gt_request_trace.spans[i];
        console_printf("intel-gam-span: v=1 n=%u pulse=%u word=%x count=%u first_begin=%u first_end=%u last_begin=%u last_end=%u\n",
            (uint64_t)i, (uint64_t)s->pulse, (uint64_t)s->word, (uint64_t)s->count,
            s->first_begin, s->first_end, s->last_begin, s->last_end);
    }
    console_printf("intel-widi-producers: v=1 disabled=%u strap=%x function=%x well_before=%x codec_before=%x control=%x codec_after=%x well_after=%x maps=%u retired=%u live=%x reads=%u\n",
        (uint64_t)gt_widi.disabled,(uint64_t)gt_widi.strap,(uint64_t)gt_widi.function,(uint64_t)gt_widi.well_before,
        (uint64_t)gt_widi.codec[0],(uint64_t)gt_widi.codec[1],(uint64_t)gt_widi.codec[2],(uint64_t)gt_widi.well_after,
        (uint64_t)gt_widi.access.maps,(uint64_t)gt_widi.access.retired,gt_widi.access.mapped,(uint64_t)gt_widi.access.reads);
    for(unsigned n=0;n<gt_widi.samples;n++)
        console_printf("intel-widi-device: v=1 n=%u win=%x reset_flow=%x\n",(uint64_t)n,
            (uint64_t)gt_widi.device[n][0],(uint64_t)gt_widi.device[n][1]);
    console_printf("intel-slot-reopened: v=3 pinned=%u restored=%u ro_mask=%u matched=%u high0=%x low=%x high1=%x reads=%u maps=%u retired=%u live=%x\n",
        (uint64_t)slot_ports_pinned,(uint64_t)slot_ports_restored,(uint64_t)gt_bootstrap_native.engine_ro_mask,
        (uint64_t)gt_reopened.matched,(uint64_t)gt_reopened.raw[0],(uint64_t)gt_reopened.raw[1],
        (uint64_t)gt_reopened.raw[2],(uint64_t)gt_reopened.access.reads,(uint64_t)gt_reopened.access.maps,
        (uint64_t)gt_reopened.access.retired,gt_reopened.access.mapped);
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
