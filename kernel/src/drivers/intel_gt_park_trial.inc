/* Air7,2 pre-reset diagnostic. The two Gen8 software park registers are
 * writable by the GT owner. All message registers are observation-only. */
#include <srvros/intel_bdw_traffic.h>

enum { PARK_CTL = 0xa090, PARK_STATE = 0xa094, PARK_RC6 = 4u << 16,
       PARK_AUTO = 0x90040000, PARK_POLLS = 8, PARK_WORDS = 24,
       PARK_INTERVAL_US = BDW_TRAFFIC_BLOCK_US / PARK_POLLS,
       PARK_COLD_END_US = 5000, PARK_COLD_FIRST_POLL = 6 };
enum park_op { PARK_OPEN, PARK_POLL, PARK_HELD, PARK_REOPEN };
struct park_frame {
    uint64_t begin, end;
    uint32_t raw[PARK_WORDS];
    unsigned op, count;
};
static struct {
    struct wake_adapter *adapter;
    struct intel_gt_access *owner;
    struct park_frame frames[1 + PARK_POLLS + 2 + 1];
    uint32_t original[2], programmed[2], at_return[2], restored[2];
    uint64_t poll_origin_us, returned_us, prepare_us;
    struct bdw_pci_rc6_sample cold[2];
    uint64_t rewake_us;
    uint32_t smi_last;
    unsigned smi_checks, panel_checks;
    uint32_t plane, surface, live;
    unsigned frames_used, attempts, returns, held, mapped, unmapped, cold_count;
    bool active, sampled, returned, reopened;
} air_park;

static bool park_cold_trial(void)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_park_cold_trial();
#else
    return true;
#endif
}

static bool park_owner(struct wake_adapter *a, struct intel_gt_access *g)
{
    return a == gt_bootstrap_native.a && g == gt_bootstrap_native.g && bcs_stop_owned() &&
           !bcs_stop.writes && !gt_bootstrap_native.issued && !gt_bootstrap_native.prepared &&
           !gt_bootstrap_native.epoch_calls && !gt_bootstrap_native.reset.object &&
           gt_range.completed == 1 && !gt_range.mapped && gt_flush.samples == 1 &&
           !gt_flush.access.mapped && intel_hda_gpu_live(&joint.scope) &&
           completion_mapping(a,0) && completion_mapping(a,8) &&
           wake_pages[0] == 0xa000 && wake_pages[8] == 0x4000;
}

static bool park_alias(bool create)
{
    uint64_t physical = result.mmio_base + 0x8000, actual;
    if (create) {
        unsigned mtrr, pat, effective;
        if (air_park.mapped || gt_range_translation(&actual) || !backing_range_reserved(physical,4096) ||
            !x86_cache_page(&gt_bootstrap_native.cache,physical,3,&mtrr,&pat,&effective) || effective)
            return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        bool installed = intel_display_test_range_map(physical);
#else
        bool installed = vmm_map_page_in_space(gt_bootstrap_native.root,GT_RANGE_VA,physical,
                VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_WRITE_THROUGH);
#endif
        if (!installed) {
            if (gt_range_translation(&actual)) {
                if (actual == physical) air_park.mapped = 1;
                else air_park.adapter->uncertain = true;
            }
            return false;
        }
        air_park.mapped = 1;
        struct vmm_alias_observation alias;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        bool ok = intel_display_test_range_alias(&alias);
#else
        bool ok = vmm_diagnostic_kernel_alias(gt_bootstrap_native.root,GT_RANGE_VA,physical,1,
            gt_bootstrap_native.cache.cpu.physical_bits,&alias,NULL);
#endif
        return ok && gt_range_translation(&actual) && actual == physical && alias.pages == 1 &&
            alias.leaves == 1 && alias.pat_index == 3 && alias.no_execute == 1 && !alias.writable &&
            !alias.user && alias.leaf_sizes == 1;
    }
    if (!air_park.mapped || air_park.unmapped || !gt_range_translation(&actual) || actual != physical)
        return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_range_unmap(&actual)) return false;
#else
    if (!vmm_unmap_page_in_space(gt_bootstrap_native.root,GT_RANGE_VA,&actual)) return false;
#endif
    if (actual != physical || gt_range_translation(&actual)) return false;
    air_park.unmapped = 1;
    return true;
}

static uint32_t park_policy_read(unsigned reg)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_park_read(reg);
#else
    return *(volatile const uint32_t *)(WAKE_VA + (reg & 4095));
#endif
}

static bool park_panel(void)
{
    /* Pipe A is the firmware-selected active panel on this exact Air profile. */
    struct wake_adapter *a = air_park.adapter;
    if (!wake_irq_off() || !display_access_held(&air_park.owner->exclusion) ||
        !completion_mapping(a,10) || wake_pages[10] != 0x70000 ||
        !(air_boot.plane & (1u << 31))) return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    air_park.plane = intel_display_test_park_read(0x70180);
    air_park.surface = intel_display_test_park_read(0x7019c);
    air_park.live = intel_display_test_park_read(0x701ac);
#else
    air_park.plane = *(volatile const uint32_t *)(WAKE_VA + 10*4096 + 0x180);
    air_park.surface = *(volatile const uint32_t *)(WAKE_VA + 10*4096 + 0x19c);
    air_park.live = *(volatile const uint32_t *)(WAKE_VA + 10*4096 + 0x1ac);
#endif
    if (air_park.plane != air_boot.plane || (air_park.surface & ~8u) != air_boot.surface ||
        air_park.live != air_boot.surface) return false;
    ++air_park.panel_checks;
    return true;
}

/* Used only after fresh wake_guard admission with both aliases present. The
 * thread register is powered and must stay outside this pair. */
static bool park_always_on_raw(uint32_t *request, uint32_t *ack)
{
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    return intel_display_test_wake_read(INTEL_GT_REQUEST,request) &&
           intel_display_test_wake_read(INTEL_GT_ACK,ack);
#else
    *request = *(volatile const uint32_t *)(WAKE_VA + (INTEL_GT_REQUEST & 4095));
    *ack = *(volatile const uint32_t *)(WAKE_VA + 4096 + (INTEL_GT_ACK & 4095));
    return true;
#endif
}

static bool park_always_on(void *context, uint32_t *request, uint32_t *ack)
{
    struct wake_adapter *a = context;
    return a->mapped[0] && a->mapped[1] && wake_guard(a) && park_always_on_raw(request,ack);
}

static bool park_cold_guard(void)
{
    struct wake_adapter *a = air_park.adapter;
    const struct intel_gt_access *g = air_park.owner;
    if (!bootstrap_tick() || !wake_guard(a) || !g->pinned || g->state != GT_ACCESS_OPENING ||
        !completion_mapping(a,0) || !completion_mapping(a,1) || !completion_mapping(a,2) ||
        wake_pages[2] != 0x138000 || !park_panel()) return false;
    uint32_t command, request, ack;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    command = intel_display_test_bootstrap_config(4);
#else
    command = pci_read_config32(a->fw.gt.a.common.gpu,4);
#endif
    if ((command & 0xffffu) != 3 || bootstrap_smi_count(BOOT_SMI_PARK) != bootstrap_smi.first) {
        a->uncertain = true;
        return false;
    }
    return park_always_on_raw(&request,&ack) && !(request & 0xffffu) && !ack;
}

static bool park_cold_point(void)
{
    uint64_t begin = timer_monotonic_us();
    if (begin < air_park.poll_origin_us ||
        begin - air_park.poll_origin_us >= BDW_TRAFFIC_BLOCK_US || !park_cold_guard()) return false;
    struct bdw_pci_rc6_sample sample = {.begin_us=begin};
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_park_cold_read(air_park.cold_count,&sample.core_status,&sample.residency);
#else
    sample.core_status = *(volatile const uint32_t *)(WAKE_VA + 8192 + 0x60);
    sample.residency = *(volatile const uint32_t *)(WAKE_VA + 8192 + 0x108);
#endif
    if (sample.core_status == UINT32_MAX || sample.residency == UINT32_MAX || !park_cold_guard()) return false;
    sample.end_us = timer_monotonic_us();
    if (sample.end_us <= begin || sample.end_us - air_park.poll_origin_us > BDW_TRAFFIC_BLOCK_US) return false;
    air_park.cold[air_park.cold_count++] = sample;
    return true;
}

static bool park_wait_offset(uint64_t offset, uint64_t previous)
{
    uint64_t now = timer_monotonic_us();
    if (now < previous) return false;
    uint64_t elapsed = now - air_park.poll_origin_us;
    return elapsed >= offset || timer_delay_us(offset - elapsed);
}

static bool park_observe_cold(void *unused)
{
    (void)unused;
    return park_cold_point() && park_wait_offset(PARK_COLD_END_US,air_park.cold[0].end_us) &&
           park_cold_point();
}

static bool park_store(unsigned i, uint32_t value, uint32_t *readback)
{
    unsigned reg = i ? PARK_STATE : PARK_CTL;
    if (!park_owner(air_park.adapter,air_park.owner) || !bootstrap_guard() || !bootstrap_smi.reads ||
        bootstrap_smi.fault) return false;
    ++air_park.smi_checks;
    air_park.smi_last = bootstrap_smi_count(BOOT_SMI_PARK);
    if (air_park.smi_last != bootstrap_smi.first) {
        air_park.adapter->uncertain = true;
        return false;
    }
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    intel_display_test_park_store(reg,value);
#else
    *(volatile uint32_t *)(WAKE_VA + (reg & 4095)) = value;
#endif
    *readback = park_policy_read(reg);
    return *readback == value;
}

static bool park_read(void *ctx, uint32_t reg, uint32_t *value)
{
    struct park_frame *f = ctx;
    struct wake_adapter *a = air_park.adapter;
    unsigned slot;
    uint64_t address, actual;
    if (f->count >= PARK_WORDS || !air_park.active || !engine_requesters(a) ||
        air_park.owner->state != GT_ACCESS_AWAKE || !air_park.owner->pinned ||
        !display_access_held(&air_park.owner->exclusion)) return false;
    switch (reg & ~4095u) {
    case 0x8000:
        if (!gt_range_translation(&actual) || actual != result.mmio_base+0x8000) return false;
        address = GT_RANGE_VA;
        break;
    case 0x4000: slot=8; goto engine;
    case 0x12000: slot=4; goto engine;
    case 0x1c000: slot=6;
    engine:
        if (!completion_mapping(a,slot)) return false;
        address = WAKE_VA + slot*4096;
        break;
    default: return false;
    }
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    (void)address;
    *value = intel_display_test_park_read(reg);
#else
    *value = *(volatile const uint32_t *)(address + (reg & 4095));
#endif
    f->raw[f->count++] = *value;
    return *value != UINT32_MAX;
}

/* Bracket the L3 pulse itself with WiDi, as the command owner does. */
static bool park_message(void *ctx, uint32_t reg, uint32_t *value)
{
    if (reg != 0x8038) return park_read(ctx,reg,value);
    uint32_t before, after;
    return park_read(ctx,0x8014,&before) && park_read(ctx,reg,value) && park_read(ctx,0x8014,&after) &&
           (!(*value & 1) || !((before | after) & 0x11));
}

static bool park_sample(struct bdw_traffic_sample *s, enum park_op op, bool *drained)
{
    if (air_park.frames_used >= sizeof(air_park.frames)/sizeof(air_park.frames[0])) return false;
    struct park_frame *f = &air_park.frames[air_park.frames_used++];
    s->ctx = f;
    f->op = op;
    f->begin = timer_monotonic_us();
    bool requested = op == PARK_POLL || op == PARK_HELD;
    if (requested) {
        if (f->begin < air_park.poll_origin_us) return false;
        uint64_t elapsed = f->begin - air_park.poll_origin_us;
        if (elapsed >= BDW_TRAFFIC_BLOCK_US) return false;
        /* The open frame precedes poll zero. Refuse a missed slot. */
        unsigned slot = air_park.frames_used - 2 + (park_cold_trial() ? PARK_COLD_FIRST_POLL : 0);
        if (op == PARK_POLL && elapsed / PARK_INTERVAL_US != slot) return false;
    }
    uint32_t ctl = requested ? PARK_RC6 : air_park.original[0];
    struct wake_adapter *a = air_park.adapter;
    bool guarded = requested ? !a->uncertain && completion_mapping(a,0) && completion_mapping(a,1) &&
        completion_mapping(a,2) && engine_requesters(a) && intel_hda_gpu_live(&joint.scope) : park_owner(a,air_park.owner) && bootstrap_guard();
    if (!f->begin || !guarded || !park_panel() || park_policy_read(PARK_CTL) != ctl ||
        park_policy_read(PARK_STATE) != PARK_RC6) return false;
    bool ok;
    switch (op) {
    case PARK_OPEN: case PARK_REOPEN: ok = intel_bdw_traffic_open(s); break;
    case PARK_POLL: ok = intel_bdw_traffic_poll(s,drained); break;
    case PARK_HELD: ok = intel_bdw_traffic_still_blocked(f,park_message); break;
    default: return false;
    }
    if (!ok || !park_panel() || !engine_requesters(air_park.adapter) || park_policy_read(PARK_CTL) != ctl ||
        park_policy_read(PARK_STATE) != PARK_RC6) return false;
    f->end = timer_monotonic_us();
    return f->begin < f->end && f->end - f->begin <= BDW_TRAFFIC_BLOCK_US;
}

static bool park_start(struct wake_adapter *a, struct intel_gt_access *g)
{
    if (air_park.active || air_park.returned || !park_owner(a,g) || !bootstrap_guard()) return false;
    air_park.adapter = a;
    air_park.owner = g;
    air_park.active = true;
    air_park.original[0] = park_policy_read(PARK_CTL);
    air_park.original[1] = park_policy_read(PARK_STATE);
    if (air_park.original[0] != PARK_AUTO || air_park.original[1] != PARK_RC6 ||
        gt_flush.raw[0][13] != PARK_AUTO || gt_flush.raw[0][14] != PARK_RC6 ||
        !park_alias(true)) return false;
    struct bdw_traffic_sample sample = {.read=park_message};
    bool drained = false;
    if (!park_sample(&sample,PARK_OPEN,&drained) || !park_owner(a,g)) return false;
    for (unsigned i=0; i<2; ++i) {
        ++air_park.attempts;
        if (!park_store(i,PARK_RC6,&air_park.programmed[i])) return false;
    }
    bool closed_low = false;
    uint64_t begin = timer_monotonic_us();
    if (!begin) return false;
    air_park.poll_origin_us = begin;
    unsigned first = 0;
    if (park_cold_trial()) {
        if (begin > UINT64_MAX - BDW_TRAFFIC_BLOCK_US) return false;
        a->park_releasing = true;
        bool observed = intel_gt_access_idle_diagnostic(g,&gt_bootstrap_native.borrow,
                            begin+BDW_TRAFFIC_BLOCK_US,park_always_on,park_observe_cold,NULL);
        a->park_releasing = false;
        if (g->state == GT_ACCESS_AWAKE && g->idle_writes == 2) air_park.rewake_us = g->last;
        if (!observed || !park_owner(a,g) || !bootstrap_guard()) return false;
        first = PARK_COLD_FIRST_POLL;
    }
    for (unsigned i=first; i<PARK_POLLS; ++i) {
        if (i && !park_wait_offset(i * PARK_INTERVAL_US,air_park.frames[air_park.frames_used-1].end)) return false;
        if (!park_sample(&sample,PARK_POLL,&drained)) return false;
        struct park_frame *f = &air_park.frames[air_park.frames_used-1];
        if (f->count != 16) return false;
        if (!closed_low) {
            /* Require an observed low after all GAM clients and WiDi close. */
            sample.l3_ack = false;
            drained = false;
            closed_low = !((f->raw[0] | f->raw[1] | f->raw[13] | f->raw[14]) & BDW_GAM_CLIENTS) &&
                         !((f->raw[3] | f->raw[5] | f->raw[11]) & 0x11) && !(f->raw[4] & 3);
        }
        uint64_t end = f->end;
        if (end < begin || end-begin > BDW_TRAFFIC_BLOCK_US) return false;
        if (drained) {
            for (unsigned n=0; n<2; ++n) {
                if (!park_sample(&sample,PARK_HELD,&drained) ||
                    air_park.frames[air_park.frames_used-1].end-begin > BDW_TRAFFIC_BLOCK_US) return false;
                ++air_park.held;
            }
            break;
        }
    }
    air_park.sampled = park_owner(a,g) && bootstrap_guard();
    return air_park.sampled;
}

static bool park_restore(struct wake_adapter *a, struct intel_gt_access *g)
{
    if (!air_park.active) return true;
    air_park.owner = g;
    if (!park_owner(a,g)) return false;
    for (unsigned n=air_park.attempts; n; --n) {
        unsigned i=n-1;
        air_park.at_return[i] = park_policy_read(i ? PARK_STATE : PARK_CTL);
        if (air_park.at_return[i] != PARK_RC6 && air_park.at_return[i] != air_park.original[i]) return false;
        if (!park_store(i,air_park.original[i],&air_park.restored[i])) return false;
        ++air_park.returns;
    }
    if (air_park.attempts) {
        struct bdw_traffic_sample sample = {.read=park_message};
        bool unused = false;
        air_park.reopened = park_sample(&sample,PARK_REOPEN,&unused);
        if (!air_park.reopened) return false;
    }
    if (air_park.mapped && !park_alias(false)) return false;
    if (!park_owner(a,g)) return false;
    air_park.active = false;
    air_park.returned = true;
    return true;
}

/* The existing reset owns its own preparation. A park sample confers no
 * reset or mapping authority. Never pass a live request or alias onward. */
static bool park_before_prepare(struct wake_adapter *a, struct intel_gt_access *g)
{
    (void)park_start(a,g);
    if (!park_restore(a,g) || air_park.active || !air_park.returned ||
        air_park.attempts != air_park.returns || air_park.mapped != air_park.unmapped ||
        (air_park.attempts && !air_park.reopened)) {
        a->uncertain = true;
        return false;
    }
    if (!park_owner(a,g) || !bootstrap_guard()) return false;
    air_park.returned_us = gt_bootstrap_native.last;
    return true;
}

static void park_report(void)
{
    if (park_cold_trial()) {
        const struct intel_gt_access *g = air_park.owner;
        for (unsigned n=0; n<air_park.cold_count; ++n) {
            const struct bdw_pci_rc6_sample *p = &air_park.cold[n];
            console_printf("intel-park-rc6: v=1 n=%u begin=%u end=%u core=%x residency=%x\n",
                (uint64_t)n,p->begin_us,p->end_us,(uint64_t)p->core_status,(uint64_t)p->residency);
        }
        struct intel_gt_release_timing release = g ? g->idle_release : (struct intel_gt_release_timing){0};
        uint64_t store_us = air_park.adapter ? air_park.adapter->park_request_store_us : 0;
        console_printf("intel-park-forcewake: v=2 attempted=%u writes=%u rewake_us=%u write_enter_us=%u store_us=%u write_exit_us=%u ack_clear_us=%u guard_exit_us=%u stop_us=%u\n",
            (uint64_t)(g ? g->idle_attempts : 0),(uint64_t)(g ? g->idle_writes : 0),air_park.rewake_us,
            release.write_enter_us,store_us,release.write_exit_us,release.ack_clear_us,release.guard_exit_us,
            release.stop_us);
    }
    for (unsigned i=0; i<air_park.frames_used; ++i) {
        const struct park_frame *f = &air_park.frames[i];
        char encoded[(PARK_WORDS*4+2)/3*4+1];
        entry_requester_base64((const uint8_t *)f->raw,4*f->count,encoded);
        console_printf("intel-park-frame: v=1 n=%u op=%u begin=%u end=%u words=%u data=%s\n",
            (uint64_t)i,(uint64_t)f->op,f->begin,f->end,(uint64_t)f->count,encoded);
    }
    console_printf("intel-park: v=%u poll_origin_us=%u returned_us=%u prepare_us=%u smi_checks=%u smi_last=%u panel_checks=%u plane=%x surface=%x live=%x attempts=%u returns=%u opened=%u unmapped=%u sampled=%u held=%u reopened=%u returned=%u ctl=%x target=%x set_ctl=%x set_target=%x final_ctl=%x final_target=%x restore_ctl=%x restore_target=%x\n",
        (uint64_t)(park_cold_trial() ? 5 : 3),air_park.poll_origin_us,air_park.returned_us,air_park.prepare_us,(uint64_t)air_park.smi_checks,(uint64_t)air_park.smi_last,
        (uint64_t)air_park.panel_checks,(uint64_t)air_park.plane,(uint64_t)air_park.surface,(uint64_t)air_park.live,
        (uint64_t)air_park.attempts,(uint64_t)air_park.returns,(uint64_t)air_park.mapped,
        (uint64_t)air_park.unmapped,(uint64_t)air_park.sampled,(uint64_t)air_park.held,
        (uint64_t)air_park.reopened,(uint64_t)air_park.returned,(uint64_t)air_park.original[0],
        (uint64_t)air_park.original[1],(uint64_t)air_park.programmed[0],(uint64_t)air_park.programmed[1],
        (uint64_t)air_park.at_return[0],(uint64_t)air_park.at_return[1],(uint64_t)air_park.restored[0],
        (uint64_t)air_park.restored[1]);
}
