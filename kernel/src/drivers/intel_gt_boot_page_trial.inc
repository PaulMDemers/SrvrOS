/* Read-only qualification at the point where all five submission ports are
 * sealed, under the original reset owner and with PCI bus mastering off. */
enum { BOOT_PAGE_INDEX = GT_SECOND_SLOT + 1, BOOT_PAGE_PTE = 0x800000 + 8 * BOOT_PAGE_INDEX,
       BOOT_DRIVER_SLOT = 16385, BOOT_DRIVER_PTE = 0x800000 + 8 * BOOT_DRIVER_SLOT,
       BOOT_PAGE_MISC_CTRL0 = 0xa180, BOOT_PAGE_POWER_SLOT = 0 };
static const uint32_t boot_page_widi_regs[] = {0x8104, 0x802c, 0x8014, 0x8108, 0x8028};
static const uint32_t boot_page_tlb_regs[] = {0x40d0, 0x4278, 0x4dc0, 0x4dc4, 0x4278, 0x40d0};
static const uint32_t boot_page_pte_regs[] = {BOOT_PAGE_PTE + 4, BOOT_PAGE_PTE, BOOT_PAGE_PTE + 4};
static const uint32_t boot_page_panel_regs[] = {0x70040, 0x70180, 0x7019c, 0x701ac};
static const uint32_t boot_driver_messages[] = {0x8028, 0x8000, 0x8004, 0x8010, 0x8008, 0x800c, 0x8014, 0x8038};

static struct {
    struct gt_display_access access;
    uint32_t widi[2][5], tlb[6], pte[3], panel[2][4], smi[2];
    uint32_t policy[2];
    uint64_t begin, end, epoch;
    unsigned stage, policy_samples;
    bool sampling;
} boot_page_preflight;

static struct {
    struct gt_display_access access, traffic;
    uint32_t words[2][3];
    uint32_t messages[2][9];
    uint64_t begin, end;
    unsigned samples, packets;
    bool active, stable;
} boot_driver_page;

static bool boot_page_preflight_live(void)
{
    return boot_page_preflight.sampling && wake_irq_off() && bootstrap_parent() && context_wp() &&
        vmm_current_address_space() == gt_bootstrap_native.root && gt_bootstrap_native.command_writes == 1 &&
        gt_bootstrap_native.pci_samples == 3 && gt_bootstrap_native.pci[2][1] == gt_bootstrap_native.pci[1][1] &&
        gt_bootstrap_native.g->engine_epoch == boot_page_preflight.epoch &&
        gt_memory.d.state == 2 && !gt_bootstrap_native.memory_child && gt_memory.d.epoch == boot_page_preflight.epoch &&
        slot_queue_matches == 1 && gt_bootstrap_native.engine_ro_mask == 31 &&
        intel_bdw_peer_ports_held(&command_slots.routes) && gt_slot_live() &&
        gt_primary.complete && gt_aux.samples == 2 && !(gt_aux.blockers[0] | gt_aux.blockers[1]);
}

static bool boot_page_preflight_register(uint32_t offset)
{
    switch (offset) {
    case 0x8104: case 0x802c: case 0x8014: case 0x8108: case 0x8028:
    case 0x40d0: case 0x4278: case 0x4dc0: case 0x4dc4:
    case BOOT_PAGE_PTE: case BOOT_PAGE_PTE + 4:
        return true;
    default:
        return false;
    }
}

static bool boot_page_read(const uint32_t *registers, uint32_t *words, unsigned count)
{
    return gt_held_read_alias(&boot_page_preflight.access, registers, words, count,
                              boot_page_preflight_live, boot_page_preflight_register);
}

static bool boot_page_panel(unsigned n)
{
    if (!boot_page_preflight_live() || !completion_mapping(gt_bootstrap_native.a, PIPE_WRITER_SLOT) ||
        !bootstrap_tick()) return false;
    uint32_t panel[4];
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t offset = boot_page_panel_regs[i];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
        if (!intel_display_test_primary_read(offset, &panel[i])) return false;
#else
        panel[i] = *(volatile const uint32_t *)(WAKE_VA + 4096 * PIPE_WRITER_SLOT + (offset & 4095u));
#endif
    }
    const uint32_t *original = gt_primary.input.sample[1].reader[0];
    if (panel[1] != original[0] || panel[2] != original[1] || panel[3] != original[2] ||
        !completion_mapping(gt_bootstrap_native.a, PIPE_WRITER_SLOT) || !boot_page_preflight_live() || !bootstrap_tick())
        return false;
    for (unsigned i = 0; i < 4; ++i) boot_page_preflight.panel[n][i] = panel[i];
    return true;
}

/* MISC_CTRL0 is a firmware policy register in the bus reset domain.
 * CPD GO-message enablement is independent of the live GO/ACK packet. */
static bool boot_page_power_policy(void)
{
    const unsigned slot = BOOT_PAGE_POWER_SLOT;
    struct wake_adapter *a = gt_bootstrap_native.a;
    if (boot_page_preflight.policy_samples >= 2 || wake_pages[slot] != (BOOT_PAGE_MISC_CTRL0 & ~4095u) ||
        !boot_page_preflight_live() || !completion_mapping(a, slot) || !bootstrap_tick()) return false;
    uint32_t word;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    if (!intel_display_test_primary_read(BOOT_PAGE_MISC_CTRL0, &word)) return false;
#else
    word = *(volatile const uint32_t *)(WAKE_VA + 4096 * slot + (BOOT_PAGE_MISC_CTRL0 & 4095u));
#endif
    if (word == UINT32_MAX || !completion_mapping(a, slot) || !boot_page_preflight_live() || !bootstrap_tick())
        return false;
    boot_page_preflight.policy[boot_page_preflight.policy_samples++] = word;
    return true;
}

static bool boot_driver_register(uint32_t offset)
{
    return offset == BOOT_DRIVER_PTE || offset == BOOT_DRIVER_PTE + 4;
}

static bool boot_driver_message_register(uint32_t offset)
{
    if (offset == 0x40d0) return true;
    for (unsigned i = 0; i < sizeof(boot_driver_messages) / sizeof(boot_driver_messages[0]); ++i)
        if (offset == boot_driver_messages[i]) return true;
    return false;
}

static bool boot_driver_powered(void)
{
    struct wake_adapter *a = gt_bootstrap_native.a;
    uint32_t request, ack, thread;
    return boot_page_preflight_live() &&
        wake_read(a,INTEL_GT_REQUEST,&request) && (request & 0xffffu) == 1 &&
        wake_read(a,INTEL_GT_ACK,&ack) && ack == 1 &&
        wake_read(a,INTEL_GT_THREAD,&thread) && !(thread & 7u) && bootstrap_tick();
}

/* Observe powered messages on each side of the exact PTE read pair. This
 * does not request traffic control or grant any rights, regardless of bits. */
static bool boot_driver_traffic(void)
{
    const uint32_t req = 0x40d0;
    unsigned n = boot_driver_page.packets;
    if (n >= 2 || !boot_driver_powered()) return false;
    struct gt_display_access *a = &boot_driver_page.traffic;
    uint32_t *words = boot_driver_page.messages[n];
    if (!gt_held_read_alias(a,&req,words,1,boot_page_preflight_live,boot_driver_message_register) ||
        !gt_held_read_alias(a,boot_driver_messages,words + 1,8,boot_page_preflight_live,boot_driver_message_register) ||
        !boot_driver_powered()) return false;
    ++boot_driver_page.packets;
    return true;
}

static bool boot_driver_sample(void)
{
    uint64_t address = BOOT_DRIVER_SLOT * 4096ull;
    if (address < window_result.endpoint.fence_end || address >= window_result.aperture_bytes ||
        BOOT_DRIVER_SLOT <= BOOT_PAGE_INDEX || BOOT_DRIVER_SLOT < GT_COMMAND_FIRST + GT_COMMAND_PAGES ||
        !boot_page_preflight_live() || !bootstrap_tick()) return false;
    static const uint32_t regs[] = {BOOT_DRIVER_PTE + 4, BOOT_DRIVER_PTE, BOOT_DRIVER_PTE + 4};
    boot_driver_page.begin = gt_bootstrap_native.last;
    boot_driver_page.active = true;
    (void)boot_driver_traffic();
    for (unsigned i = 0; i < 2 && !boot_driver_page.traffic.mapped; ++i) {
        uint32_t *words = boot_driver_page.words[i];
        if (!gt_held_read_alias(&boot_driver_page.access, regs, words, 3,
                                boot_page_preflight_live, boot_driver_register))
            break;
        ++boot_driver_page.samples;
    }
    if (boot_driver_page.samples == 2 && boot_driver_page.packets == 1)
        (void)boot_driver_traffic();
    boot_driver_page.active = false;
    bool closed = boot_driver_page.samples == 2 && !boot_driver_page.traffic.mapped && boot_page_panel(1) &&
        (uint32_t)(boot_page_preflight.panel[1][0] - boot_page_preflight.panel[0][0]) <= 8 && bootstrap_guard();
    if (closed) {
        boot_page_preflight.smi[1] = bootstrap_smi_count(BOOT_SMI_PIPE);
        closed = !bootstrap_smi.fault && boot_page_preflight.smi[1] == boot_page_preflight.smi[0] && bootstrap_tick();
    }
    boot_driver_page.end = gt_bootstrap_native.last;
    const uint32_t *first = boot_driver_page.words[0], *second = boot_driver_page.words[1];
    boot_driver_page.stable = closed && first[0] == first[2] && second[0] == second[2] &&
        first[0] == second[0] && first[1] == second[1] && !boot_driver_page.access.mapped &&
        boot_driver_page.end > boot_driver_page.begin && boot_driver_page.end - boot_page_preflight.begin <= BDW_BOOT_PHASE_US;
    return boot_driver_page.stable;
}

static bool slot_boot_page_preflight(void)
{
    if (gt_aux.blockers[0] || gt_aux.blockers[1]) return true;
    if (boot_page_preflight.stage || !bootstrap_tick()) return false;
    boot_page_preflight.begin = gt_bootstrap_native.last;
    boot_page_preflight.epoch = gt_bootstrap_native.g->engine_epoch;
    boot_page_preflight.stage = 1;
    boot_page_preflight.sampling = true;
    boot_page_preflight.smi[0] = bootstrap_smi_count(BOOT_SMI_PIPE);
    if (!bootstrap_smi.reads || bootstrap_smi.fault || boot_page_preflight.smi[0] != bootstrap_smi.first ||
        !boot_page_power_policy() || !boot_page_panel(0) ||
        !boot_page_read(boot_page_widi_regs, boot_page_preflight.widi[0], 5)) goto finished;
    boot_page_preflight.stage = 2;
    if (!boot_page_read(boot_page_tlb_regs, boot_page_preflight.tlb, 6) ||
        !boot_page_read(boot_page_pte_regs, boot_page_preflight.pte, 3)) goto finished;
    boot_page_preflight.stage = 3;
    const uint32_t *pte = boot_page_preflight.pte;
    if (pte[0] != pte[2] || ((uint64_t)pte[0] << 32 | pte[1]) != gt_range.words[1][GT_RANGE_TAIL+3] ||
        !boot_page_read(boot_page_widi_regs, boot_page_preflight.widi[1], 5) || !boot_page_panel(1) ||
        !boot_page_power_policy()) goto finished;
    boot_page_preflight.stage = 4;
    boot_page_preflight.smi[1] = bootstrap_smi_count(BOOT_SMI_PIPE);
    if (boot_page_preflight.smi[0] != boot_page_preflight.smi[1] || !boot_page_preflight_live() ||
        boot_page_preflight.policy[0] != boot_page_preflight.policy[1] ||
        (uint32_t)(boot_page_preflight.panel[1][0] - boot_page_preflight.panel[0][0]) > 8 ||
        !bootstrap_guard() || !bootstrap_tick() || gt_bootstrap_native.last <= boot_page_preflight.begin ||
        gt_bootstrap_native.last - boot_page_preflight.begin > BDW_BOOT_PHASE_US ||
        boot_page_preflight.access.mapped || boot_page_preflight.access.maps != 4 ||
        boot_page_preflight.access.retired != 4 || boot_page_preflight.access.reads != 19) goto finished;
    boot_page_preflight.stage = 5;
    if (!boot_driver_sample()) boot_page_preflight.stage = 4;
finished:
    boot_page_preflight.end = gt_bootstrap_native.last;
    boot_page_preflight.sampling = false;
    return boot_page_preflight.stage == 5;
}

static void slot_boot_page_report(void)
{
    if (!boot_page_preflight.stage) return;
    const struct gt_display_access *a = &boot_page_preflight.access;
    console_printf("intel-boot-page-preflight: v=3 slot=%u stage=%u epoch=%u begin=%u end=%u smi0=%u smi1=%u maps=%u retired=%u reads=%u live=%x policy_samples=%u policy0=%x policy1=%x authority=none\n",
        (uint64_t)BOOT_PAGE_INDEX, (uint64_t)boot_page_preflight.stage, boot_page_preflight.epoch,
        boot_page_preflight.begin, boot_page_preflight.end, (uint64_t)boot_page_preflight.smi[0],
        (uint64_t)boot_page_preflight.smi[1], (uint64_t)a->maps, (uint64_t)a->retired, (uint64_t)a->reads, a->mapped,
        (uint64_t)boot_page_preflight.policy_samples, (uint64_t)boot_page_preflight.policy[0],
        (uint64_t)boot_page_preflight.policy[1]);
    if (boot_page_preflight.stage >= 3) {
        const uint32_t *t = boot_page_preflight.tlb, *p = boot_page_preflight.pte;
        console_printf("intel-boot-page-translation: v=1 gam0=%x wtcr0=%x valid0=%x valid1=%x wtcr1=%x gam1=%x high0=%x low=%x high1=%x\n",
            (uint64_t)t[0], (uint64_t)t[1], (uint64_t)t[2], (uint64_t)t[3], (uint64_t)t[4],
            (uint64_t)t[5], (uint64_t)p[0], (uint64_t)p[1], (uint64_t)p[2]);
    }
    for (unsigned n = 0; n < 2 && boot_page_preflight.stage >= 2 + n * 2; ++n) {
        const uint32_t *w = boot_page_preflight.widi[n], *p = boot_page_preflight.panel[n];
        console_printf("intel-boot-page-widi: v=1 n=%u req=%x ack=%x win=%x reset=%x gam=%x frame=%x plane=%x programmed=%x live=%x\n",
            (uint64_t)n, (uint64_t)w[0], (uint64_t)w[1], (uint64_t)w[2], (uint64_t)w[3], (uint64_t)w[4],
            (uint64_t)p[0], (uint64_t)p[1], (uint64_t)p[2], (uint64_t)p[3]);
    }
    if (boot_driver_page.begin) {
        const struct gt_display_access *driver = &boot_driver_page.access;
        const uint32_t *first = boot_driver_page.words[0], *second = boot_driver_page.words[1];
        const struct gt_display_access *traffic = &boot_driver_page.traffic;
        console_printf("intel-driver-range-preflight: v=2 slot=%u begin=%u end=%u maps=%u retired=%u reads=%u live=%x samples=%u stable=%u a0=%x a1=%x a2=%x b0=%x b1=%x b2=%x packets=%u traffic_maps=%u traffic_retired=%u traffic_reads=%u traffic_live=%x authority=none\n",
            (uint64_t)BOOT_DRIVER_SLOT, boot_driver_page.begin, boot_driver_page.end, (uint64_t)driver->maps,
            (uint64_t)driver->retired, (uint64_t)driver->reads, driver->mapped, (uint64_t)boot_driver_page.samples,
            (uint64_t)boot_driver_page.stable, (uint64_t)first[0], (uint64_t)first[1], (uint64_t)first[2],
            (uint64_t)second[0], (uint64_t)second[1], (uint64_t)second[2], (uint64_t)boot_driver_page.packets,
            (uint64_t)traffic->maps, (uint64_t)traffic->retired, (uint64_t)traffic->reads, traffic->mapped);
        for (unsigned n = 0; n < boot_driver_page.packets; ++n) {
            const uint32_t *words = boot_driver_page.messages[n];
            console_printf("intel-driver-range-traffic: v=1 n=%u gam=%x reply=%x rcs=%x vcs0=%x vecs=%x vcs1=%x bcs=%x widi=%x l3=%x\n",
                (uint64_t)n,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3],
                (uint64_t)words[4],(uint64_t)words[5],(uint64_t)words[6],(uint64_t)words[7],(uint64_t)words[8]);
        }
    }
}
