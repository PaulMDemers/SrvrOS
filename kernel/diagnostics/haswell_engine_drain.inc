/* The Haswell 0a26 render, video decode, blitter and video enhancement
 * rings. Only HEAD, TAIL, CTL, MI_MODE and ACTHD are admitted. */
#define HSW_DRAIN_VA (HWB_VA + 8192u)
#define HSW_DRAIN_GAP_US 250u
#define HSW_DRAIN_LIMIT_US 4000u
static const uint32_t hsw_drain_pages[] = {0x02000, 0x12000, 0x22000, 0x1a000};
static const uint32_t hsw_drain_offsets[] = {0x34, 0x30, 0x3c, 0x9c, 0x74};
enum { HSW_DRAIN_RINGS = 4, HSW_DRAIN_WORDS = 5 };

struct hsw_drain_sample {
    uint64_t begin, end;
    uint32_t ack[2], thread[2], smi;
    uint32_t ring[HSW_DRAIN_RINGS][HSW_DRAIN_WORDS];
    uint32_t pci[2][4];
    struct package_pci_af af[2];
};

static struct {
    unsigned maps, retired, reads, samples;
    bool poisoned;
    uint64_t begin, end;
    const struct pci_device *hda;
    struct hsw_drain_sample sample[2];
} hsw_engine_drain;

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
    return hsw_forcewake_clock(hsw_forcewake.request_us, HSW_WAKE_HOLD_US, &hsw_engine_drain.end) &&
           hsw_engine_drain.end >= hsw_engine_drain.begin &&
           hsw_engine_drain.end - hsw_engine_drain.begin <= HSW_DRAIN_LIMIT_US;
}

static bool hsw_drain_wake(uint32_t *ack, uint32_t *thread) {
    if (!hsw_drain_tick() || !hsw_read(NULL, HSW_WAKE_ACK, ack) || *ack != HSW_WAKE_KERNEL)
        return false;
    *thread = graphics_read(0x5c);
    return !(*thread & 7u) && hsw_drain_tick();
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

static bool hsw_drain_run(void) {
    hsw_engine_drain.end = hsw_forcewake.c0_us;
    if (!hsw_forcewake_clock(hsw_forcewake.request_us, HSW_WAKE_HOLD_US, &hsw_engine_drain.end))
        return false;
    hsw_engine_drain.begin = hsw_engine_drain.end;
    if (!hsw_drain_sample(0)) return false;
    uint64_t first_end = hsw_engine_drain.end;
    for (unsigned spin = 0; spin < HSW_WAKE_POLL_LIMIT; spin++) {
        if (!hsw_drain_tick()) return false;
        if (hsw_engine_drain.end - first_end >= HSW_DRAIN_GAP_US) return hsw_drain_sample(1);
        __asm__ volatile("pause");
    }
    return false;
}

static void hsw_drain_report(void) {
    console_printf("hsw-engine-drain: v=1 samples=%u maps=%u retired=%u reads=%u begin=%u end=%u authority=none\n",
                   (uint64_t)hsw_engine_drain.samples, (uint64_t)hsw_engine_drain.maps, (uint64_t)hsw_engine_drain.retired,
                   (uint64_t)hsw_engine_drain.reads, hsw_engine_drain.begin, hsw_engine_drain.end);
    for (unsigned phase = 0; phase < hsw_engine_drain.samples; phase++) {
        const struct hsw_drain_sample *s = &hsw_engine_drain.sample[phase];
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
