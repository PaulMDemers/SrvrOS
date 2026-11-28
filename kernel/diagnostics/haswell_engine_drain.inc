/* The Haswell 0a26 render, video decode, blitter and video enhancement
 * rings. Only HEAD, TAIL, CTL, MI_MODE and ACTHD are admitted. */
#define HSW_DRAIN_VA (HWB_VA + 8192u)
#define HSW_DRAIN_GAP_US 250u
#define HSW_DRAIN_LIMIT_US 4000u
static const uint32_t hsw_drain_pages[] = {0x02000, 0x12000, 0x22000, 0x1a000};
static const uint32_t hsw_drain_offsets[] = {0x34, 0x30, 0x3c, 0x9c, 0x74};
enum { HSW_DRAIN_RINGS = 4, HSW_DRAIN_WORDS = 5 };
#define HSW_OPREGION_BYTES 8192u
enum hsw_opregion_carrier {
    HSW_OPREGION_UNOBSERVED, HSW_OPREGION_NVS, HSW_OPREGION_RESERVED,
    HSW_OPREGION_OTHER, HSW_OPREGION_AMBIGUOUS, HSW_OPREGION_UNDESCRIBED,
};

struct hsw_smi_route {
    uint32_t pmbase, acpi, pmcon, en, status;
    bool read, valid;
};

struct hsw_handoff_sample {
    uint32_t asls;
    uint16_t swsci;
    enum hsw_opregion_carrier carrier;
    bool stable;
};

struct hsw_drain_sample {
    uint64_t begin, end;
    uint32_t ack[2], thread[2], smi;
    uint32_t ring[HSW_DRAIN_RINGS][HSW_DRAIN_WORDS];
    uint32_t pci[2][4];
    struct package_pci_af af[2];
    struct hsw_smi_route smi_route;
    struct hsw_handoff_sample handoff;
};

static bool hsw_exclusion_audit, hsw_handoff_audit;

static struct {
    unsigned maps, retired, reads, samples;
    bool poisoned;
    uint64_t begin, end;
    struct {
        bool entered, irq_before, smi_valid;
        uint64_t ticks[2], preemptions[2], max_gap;
        uint32_t smi[2];
        const char *stage, *gap_stage, *stop;
    } timing;
    const struct pci_device *hda;
    struct hsw_drain_sample sample[2];
} hsw_engine_drain;

static void hsw_sample_smi_route(struct hsw_smi_route *route) {
    uint32_t pch;
    if (!dependency_pch_valid(&pch)) return;
    route->pmbase = pci_read_config32(pch_device, 0x40);
    route->acpi = pci_read_config16(pch_device, 0x44) & 0xff;
    route->pmcon = pci_read_config16(pch_device, 0xa0);
    /* Only the captured Air6,1 PM I/O region. Check PCI decode and bind the
     * base to the independently validated FADT timer port. */
    if (route->pmbase != 0x1801 || route->acpi == 0xff || !(route->acpi & 0x80) ||
        route->pmcon == UINT16_MAX || timer_reference_port() != 0x1808) return;
#ifdef SRVROS_PLATFORM_HOST_TEST
    extern uint32_t platform_test_hsw_smi_io(uint16_t);
    route->en = platform_test_hsw_smi_io(0x1830);
    route->status = platform_test_hsw_smi_io(0x1834);
#else
    route->en = inl(0x1830);
    route->status = inl(0x1834); /* Sticky status. Leave it intact. */
#endif
    route->read = true;
    route->valid = route->en != UINT32_MAX && route->status != UINT32_MAX &&
                   dependency_pch_valid(&pch) &&
                   pci_read_config32(pch_device, 0x40) == route->pmbase &&
                   (pci_read_config16(pch_device, 0x44) & 0xff) == route->acpi &&
                   pci_read_config16(pch_device, 0xa0) == route->pmcon;
}

static bool hsw_drain_prepare(void) {
    uint64_t kernel = vmm_kernel_address_space(), pa;
    if (!kernel || vmm_current_address_space() != kernel || !hsw_config_valid() ||
        !hsw_safe_range(gpu_base, 0x400000, false)) return false;
    hsw_engine_drain.hda = hsw_display_hda();
    if (!hsw_engine_drain.hda) return false;
    for (unsigned i = 0; i < HSW_DRAIN_RINGS; i++) {
        uint64_t va = HSW_DRAIN_VA + 4096ull * i;
        if (vmm_virt_to_phys_in_space(kernel, va, &pa)) return false;
        if (!vmm_map_page_in_space(kernel, va, gpu_base + hsw_drain_pages[i],
                                  VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE)) {
            hsw_engine_drain.poisoned = true;
            return false;
        }
        hsw_engine_drain.maps++;
    }
    return true;
}

static bool hsw_drain_retire(void) {
    uint64_t kernel = vmm_kernel_address_space(), pa;
    for (unsigned i = 0; i < hsw_engine_drain.maps; i++) {
        uint64_t va = HSW_DRAIN_VA + 4096ull * i;
        if (vmm_current_address_space() != kernel ||
            !vmm_unmap_page_in_space(kernel, va, &pa) ||
            pa != gpu_base + hsw_drain_pages[i] ||
            vmm_virt_to_phys_in_space(kernel, va, &pa)) {
            hsw_engine_drain.poisoned = true;
        } else {
            hsw_engine_drain.retired++;
        }
    }
    return !hsw_engine_drain.poisoned;
}

static bool hsw_drain_tick(void) {
    uint64_t now = timer_monotonic_us(), previous = hsw_engine_drain.end;
    if (!now || !hsw_forcewake.request_us || now < previous || now < hsw_forcewake.request_us) {
        hsw_engine_drain.timing.stop = "clock";
        return false;
    }
    uint64_t gap = now - previous;
    if (gap > hsw_engine_drain.timing.max_gap) {
        hsw_engine_drain.timing.max_gap = gap;
        hsw_engine_drain.timing.gap_stage = hsw_engine_drain.timing.stage;
    }
    hsw_engine_drain.end = now;
    if (now - hsw_forcewake.request_us > HSW_WAKE_HOLD_US) {
        hsw_engine_drain.timing.stop = "hold-deadline";
        return false;
    }
    if (hsw_engine_drain.begin && now - hsw_engine_drain.begin > HSW_DRAIN_LIMIT_US) {
        hsw_engine_drain.timing.stop = "sample-deadline";
        return false;
    }
    return true;
}

static bool hsw_drain_wake(uint32_t *ack, uint32_t *thread) {
    if (!hsw_drain_tick() || !hsw_read(NULL, HSW_WAKE_ACK, ack) || *ack != HSW_WAKE_KERNEL)
        return false;
    *thread = graphics_read(0x5c);
    return !(*thread & 7u) && hsw_drain_tick();
}

static bool hsw_classify_opregion(struct hsw_handoff_sample *sample) {
    const struct limine_memmap_response *map = memmap_request.response;
    uint64_t base = sample->asls, end = base + HSW_OPREGION_BYTES;
    if (!base || (base & 3) || end > (1ull << 32) ||
        !map || !map->entries || !map->entry_count || map->entry_count > 4096)
        return true;
    /* Classify the nominal extent from the boot memory map only. ASLS
     * never authorizes a mapping, a memory read or a firmware message. */
    enum hsw_opregion_carrier carrier = HSW_OPREGION_UNDESCRIBED;
    for (unsigned i = 0; i < map->entry_count; i++) {
        if (!(i % 32) && !hsw_drain_tick()) return false;
        const struct limine_memmap_entry *entry = map->entries[i];
        if (!entry || !entry->length || entry->base > UINT64_MAX - entry->length) return true;
        uint64_t entry_end = entry->base + entry->length;
        if (entry->base >= end || entry_end <= base) continue;
        if (carrier != HSW_OPREGION_UNDESCRIBED || entry->base > base || entry_end < end) {
            carrier = HSW_OPREGION_AMBIGUOUS;
        } else if (entry->type == LIMINE_MEMMAP_ACPI_NVS) {
            carrier = HSW_OPREGION_NVS;
        } else if (entry->type == LIMINE_MEMMAP_RESERVED || entry->type == LIMINE_MEMMAP_RESERVED_MAPPED) {
            carrier = HSW_OPREGION_RESERVED;
        } else {
            carrier = HSW_OPREGION_OTHER;
        }
    }
    sample->carrier = carrier;
    return true;
}

static bool hsw_sample_handoff(struct hsw_handoff_sample *sample) {
    if (!hsw_drain_tick()) return false;
    sample->asls = pci_read_config32(gpu, 0xfc);
    sample->swsci = pci_read_config16(gpu, 0xe8);
    if (!hsw_classify_opregion(sample)) return false;
    sample->stable = sample->asls != UINT32_MAX && graphics_access_valid() &&
                     pci_read_config32(gpu, 0xfc) == sample->asls &&
                     pci_read_config16(gpu, 0xe8) == sample->swsci;
    if (!sample->stable) sample->carrier = HSW_OPREGION_UNOBSERVED;
    return hsw_drain_tick();
}

static bool hsw_drain_sample(unsigned phase) {
    struct hsw_drain_sample *sample = &hsw_engine_drain.sample[phase];
    if (!hsw_drain_tick()) return false;
    sample->begin = hsw_engine_drain.end;
    if (!hsw_drain_wake(&sample->ack[0], &sample->thread[0]) || !graphics_access_valid() ||
        !hsw_candidate_pci(gpu, 0x0a268086, sample->pci[0], &sample->af[0]) ||
        !(sample->pci[0][0] & 2u) || (sample->pci[0][1] & 3u) ||
        !hsw_candidate_pci(hsw_engine_drain.hda, 0x0a0c8086, sample->pci[1], &sample->af[1]) ||
        !hsw_smi_count(&sample->smi)) return false;
    if (hsw_exclusion_audit) {
        if (!hsw_drain_tick()) return false;
        hsw_sample_smi_route(&sample->smi_route);
    }
    if (hsw_handoff_audit && !hsw_sample_handoff(&sample->handoff)) return false;
    uint64_t kernel = vmm_kernel_address_space();
    if (vmm_current_address_space() != kernel) return false;
    for (unsigned i = 0; i < HSW_DRAIN_RINGS; i++) {
        uint64_t va = HSW_DRAIN_VA + 4096ull * i, pa;
        if (!vmm_virt_to_phys_in_space(kernel, va, &pa) || pa != gpu_base + hsw_drain_pages[i])
            return false;
        for (unsigned word = 0; word < HSW_DRAIN_WORDS; word++) {
            if (!hsw_drain_tick()) return false;
            uint32_t offset = hsw_drain_offsets[word];
#ifdef SRVROS_PLATFORM_HOST_TEST
            extern uint32_t platform_test_hsw_drain_read(unsigned);
            sample->ring[i][word] = platform_test_hsw_drain_read(hsw_drain_pages[i] + offset);
#else
            sample->ring[i][word] = *(volatile const uint32_t *)(va + offset);
#endif
            hsw_engine_drain.reads++;
        }
    }
    if (!hsw_drain_wake(&sample->ack[1], &sample->thread[1])) return false;
    sample->end = hsw_engine_drain.end;
    hsw_engine_drain.samples++;
    return true;
}

static bool hsw_drain_sample_pair(void) {
    hsw_engine_drain.timing.stage = "entry";
    hsw_engine_drain.end = hsw_forcewake.c0_us;
    if (!hsw_drain_tick()) return false;
    hsw_engine_drain.begin = hsw_engine_drain.end;
    hsw_engine_drain.timing.stage = "pass-one";
    if (!hsw_drain_sample(0)) return false;
    uint64_t first_end = hsw_engine_drain.end;
    hsw_engine_drain.timing.stage = "gap";
    for (unsigned spin = 0; spin < HSW_WAKE_POLL_LIMIT; spin++) {
        if (!hsw_drain_tick()) return false;
        if (hsw_engine_drain.end - first_end >= HSW_DRAIN_GAP_US) {
            hsw_engine_drain.timing.stage = "pass-two";
            return hsw_drain_sample(1);
        }
        __asm__ volatile("pause");
    }
    hsw_engine_drain.timing.stop = "spin-limit";
    return false;
}

static bool hsw_drain_run(void) {
    /* Exclude CPU0 timer/worker dispatch only across the two MMIO/PCI
     * samples and their bounded gap. Release, unmap and I/O are outside. */
    uint64_t flags;
#ifdef SRVROS_PLATFORM_HOST_TEST
    extern uint64_t platform_test_hsw_drain_irq_save(void);
    extern void platform_test_hsw_drain_irq_restore(uint64_t);
    flags = platform_test_hsw_drain_irq_save();
#else
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
#endif
    hsw_engine_drain.timing.entered = true;
    hsw_engine_drain.timing.irq_before = (flags & (1ull << 9)) != 0;
    hsw_engine_drain.timing.stop = "sample";
    hsw_engine_drain.timing.ticks[0] = timer_ticks();
    hsw_engine_drain.timing.preemptions[0] = scheduler_preemptions();
    bool smi_begin = hsw_smi_count(&hsw_engine_drain.timing.smi[0]);
    bool sampled = hsw_drain_sample_pair();
    bool smi_end = hsw_smi_count(&hsw_engine_drain.timing.smi[1]);
    hsw_engine_drain.timing.smi_valid = smi_begin && smi_end;
    hsw_engine_drain.timing.ticks[1] = timer_ticks();
    hsw_engine_drain.timing.preemptions[1] = scheduler_preemptions();
    if (hsw_engine_drain.timing.ticks[0] != hsw_engine_drain.timing.ticks[1] ||
        hsw_engine_drain.timing.preemptions[0] != hsw_engine_drain.timing.preemptions[1]) {
        sampled = false;
        hsw_engine_drain.timing.stop = "irq-progress";
    } else if (sampled) {
        hsw_engine_drain.timing.stop = "complete";
    }
#ifdef SRVROS_PLATFORM_HOST_TEST
    platform_test_hsw_drain_irq_restore(flags);
#else
    if (flags & (1ull << 9)) __asm__ volatile("sti" : : : "memory");
#endif
    return sampled;
}

static void hsw_drain_report(void) {
    console_printf("hsw-engine-drain: v=1 samples=%u maps=%u retired=%u reads=%u begin=%u end=%u authority=none\n",
                   (uint64_t)hsw_engine_drain.samples, (uint64_t)hsw_engine_drain.maps, (uint64_t)hsw_engine_drain.retired,
                   (uint64_t)hsw_engine_drain.reads, hsw_engine_drain.begin, hsw_engine_drain.end);
    console_printf("hsw-engine-drain-timing: v=1 entered=%u irq_before=%u ticks=%u,%u preempt=%u,%u smi=%u,%u,%u max_gap_us=%u gap_stage=%s stop=%s\n",
                   (uint64_t)hsw_engine_drain.timing.entered, (uint64_t)hsw_engine_drain.timing.irq_before,
                   hsw_engine_drain.timing.ticks[0], hsw_engine_drain.timing.ticks[1],
                   hsw_engine_drain.timing.preemptions[0], hsw_engine_drain.timing.preemptions[1],
                   (uint64_t)hsw_engine_drain.timing.smi_valid,
                   (uint64_t)hsw_engine_drain.timing.smi[0], (uint64_t)hsw_engine_drain.timing.smi[1],
                   hsw_engine_drain.timing.max_gap,
                   hsw_engine_drain.timing.gap_stage ? hsw_engine_drain.timing.gap_stage : "none",
                   hsw_engine_drain.timing.stop ? hsw_engine_drain.timing.stop : "none");
    if (hsw_exclusion_audit)
        console_printf("hsw-exclusion-audit: v=1 rows=%u firmware_exclusion=unverified requester_drain=unverified\n",
                       (uint64_t)hsw_engine_drain.samples);
    if (hsw_handoff_audit)
        console_printf("hsw-handoff-audit: v=1 rows=%u requester_hold=unavailable firmware_handoff=unavailable authority=none\n",
                       (uint64_t)hsw_engine_drain.samples);
    for (unsigned phase = 0; phase < hsw_engine_drain.samples; phase++) {
        const struct hsw_drain_sample *s = &hsw_engine_drain.sample[phase];
        if (hsw_exclusion_audit) {
            const struct hsw_smi_route *r = &s->smi_route;
            console_printf("hsw-exclusion-smi: phase=%u read=%u valid=%u pmbase=%u acpi=%u pmcon=%u en=%u sts=%u\n",
                           (uint64_t)phase, (uint64_t)r->read, (uint64_t)r->valid, (uint64_t)r->pmbase,
                           (uint64_t)r->acpi, (uint64_t)r->pmcon, (uint64_t)r->en, (uint64_t)r->status);
        }
        if (hsw_handoff_audit) {
            const struct hsw_handoff_sample *h = &s->handoff;
            console_printf("hsw-handoff-opregion: phase=%u stable=%u asls=%u swsci=%u carrier=%u\n",
                           (uint64_t)phase, (uint64_t)h->stable, (uint64_t)h->asls,
                           (uint64_t)h->swsci, (uint64_t)h->carrier);
        }
        console_printf("hsw-engine-drain-pass: phase=%u begin=%u end=%u ack=%u,%u thread=%u,%u smi=%u\n",
                       (uint64_t)phase, s->begin, s->end, (uint64_t)s->ack[0], (uint64_t)s->ack[1],
                       (uint64_t)s->thread[0], (uint64_t)s->thread[1], (uint64_t)s->smi);
        for (unsigned i = 0; i < HSW_DRAIN_RINGS; i++) {
            const uint32_t *r = s->ring[i];
            console_printf("hsw-engine-drain-ring: phase=%u engine=%u head=%u tail=%u ctl=%u mode=%u acthd=%u\n",
                           (uint64_t)phase, (uint64_t)i, (uint64_t)r[0], (uint64_t)r[1],
                           (uint64_t)r[2], (uint64_t)r[3], (uint64_t)r[4]);
        }
        for (unsigned i = 0; i < 2; i++) {
            const uint32_t *p = s->pci[i];
            const struct package_pci_af *af = &s->af[i];
            console_printf("hsw-engine-drain-pci: phase=%u requester=%u cmd=%u pm=%u pcie=%u status=%u af=%u,%u,%u\n",
                           (uint64_t)phase, (uint64_t)i, (uint64_t)p[0], (uint64_t)p[1], (uint64_t)p[2], (uint64_t)p[3],
                           (uint64_t)af->offset, (uint64_t)af->header, (uint64_t)af->control_status);
        }
    }
}
