/* Isolated Air6,1 trial. The two masked stores affect only the kernel
 * request bit. Keep both aliases through the release observation. */
#define HSW_WAKE_REQUEST 0xa188u
#define HSW_WAKE_REQUEST_PAGE (HSW_WAKE_REQUEST & ~4095u)
#define HSW_WAKE_REQUEST_VA (HWB_VA + 4096u)
#define HSW_WAKE_KERNEL 1u
#define HSW_WAKE_MASK (HSW_WAKE_KERNEL << 16)
#define HSW_WAKE_ACK_US 10000u
#define HSW_WAKE_C0_US 5000u
#define HSW_WAKE_HOLD_US 20000u
#define HSW_WAKE_POLL_LIMIT 65536u

static struct {
    unsigned maps, retired, stores;
    bool request_mapped, request_poison, released;
    uint64_t request_us, ack_us, c0_us, clear_us, release_us;
    uint32_t baseline, ack, thread, held, final;
    const char *failure;
} hsw_forcewake;

static bool hsw_forcewake_map_request(void) {
    uint64_t kernel = vmm_kernel_address_space(), pa;
    if (!hsw_wake_ack_window || !hsw_window_live || hsw_window_poison || !kernel ||
        vmm_current_address_space() != kernel ||
        !hsw_config_valid() || !hsw_safe_range(gpu_base, 0x400000, false) ||
        vmm_virt_to_phys_in_space(kernel, HSW_WAKE_REQUEST_VA, &pa)) return false;
    if (!vmm_map_page_in_space(kernel, HSW_WAKE_REQUEST_VA, gpu_base + HSW_WAKE_REQUEST_PAGE,
                              VMM_PAGE_NO_EXECUTE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_WRITABLE)) {
        hsw_forcewake.request_poison = true;
        return false;
    }
    hsw_forcewake.request_mapped = true;
    hsw_forcewake.maps++;
    return true;
}

static bool hsw_forcewake_store(bool acquire) {
    uint64_t kernel = vmm_kernel_address_space(), pa;
    if (!kernel || vmm_current_address_space() != kernel || !hsw_forcewake.request_mapped ||
        !vmm_virt_to_phys_in_space(kernel, HSW_WAKE_REQUEST_VA, &pa) ||
        pa != gpu_base + HSW_WAKE_REQUEST_PAGE || hsw_forcewake.stores != (acquire ? 0u : 1u) ||
        !graphics_access_valid()) return false;
    uint32_t value = HSW_WAKE_MASK | (acquire ? HSW_WAKE_KERNEL : 0);
#ifdef SRVROS_PLATFORM_HOST_TEST
    extern void platform_test_hsw_forcewake_store(unsigned, uint32_t);
    platform_test_hsw_forcewake_store(HSW_WAKE_REQUEST, value);
#else
    *(volatile uint32_t *)(HSW_WAKE_REQUEST_VA + (HSW_WAKE_REQUEST & 4095u)) = value;
#endif
    hsw_forcewake.stores++;
    return true;
}

static bool hsw_forcewake_clock(uint64_t start, unsigned limit, uint64_t *last) {
    uint64_t now = timer_monotonic_us();
    if (!start || now < *last || now < start || now - start > limit) return false;
    *last = now;
    return true;
}

static bool hsw_forcewake_wait_ack(uint64_t start, uint32_t wanted, uint32_t *value,
                                  uint64_t *last) {
    *last = start;
    for (unsigned i = 0; i < HSW_WAKE_POLL_LIMIT; i++) {
        /* The same-device ACK read also drains the posted request store. */
        if (!hsw_read(NULL, HSW_WAKE_ACK, value) || *value > HSW_WAKE_KERNEL ||
            !hsw_forcewake_clock(start, HSW_WAKE_ACK_US, last)) return false;
        if (*value == wanted) return true;
        __asm__ volatile("pause");
    }
    return false;
}

static bool hsw_forcewake_wait_c0(void) {
    uint64_t start = hsw_forcewake.ack_us, now = start;
    for (unsigned i = 0; i < HSW_WAKE_POLL_LIMIT; i++) {
        hsw_forcewake.thread = graphics_read(0x5c);
        if (hsw_forcewake.thread == UINT32_MAX ||
            !hsw_forcewake_clock(start, HSW_WAKE_C0_US, &now)) return false;
        if (!(hsw_forcewake.thread & 7u)) {
            hsw_forcewake.c0_us = now;
            return true;
        }
        __asm__ volatile("pause");
    }
    return false;
}

static bool hsw_forcewake_acquire(void) {
    uint32_t pci[4];
    hsw_forcewake.failure = "preflight";
    if (!hsw_candidate_pci(gpu, 0x0a268086, pci, NULL) ||
        !(pci[0] & 2u) || (pci[1] & 3u) || !hsw_survey_display()) return false;
    hsw_survey.frames[0] = resource_read(0x70040);
    hsw_forcewake.failure = "baseline-ack";
    if (!hsw_read(NULL, HSW_WAKE_ACK, &hsw_forcewake.baseline) || hsw_forcewake.baseline)
        return false;
    hsw_forcewake.failure = "request";
    hsw_forcewake.request_us = timer_monotonic_us();
    /* Leave room for both bounded ACK polls, C0 and a post-release frame. */
    if (hsw_forcewake.request_us < hsw_survey.end ||
        hsw_forcewake.request_us - hsw_survey.begin > HSW_CANDIDATE_LIMIT_US / 2 ||
        !hsw_forcewake_store(true)) return false;
    hsw_forcewake.failure = "acquire-ack";
    if (!hsw_forcewake_wait_ack(hsw_forcewake.request_us, HSW_WAKE_KERNEL,
                               &hsw_forcewake.ack, &hsw_forcewake.ack_us)) return false;
    hsw_forcewake.failure = "thread-c0";
    if (!hsw_forcewake_wait_c0()) return false;
    hsw_forcewake.failure = "held-display";
    if (!hsw_survey_display()) return false;
    hsw_forcewake.failure = "held-ack";
    return hsw_read(NULL, HSW_WAKE_ACK, &hsw_forcewake.held) &&
           hsw_forcewake.held == HSW_WAKE_KERNEL;
}

static bool hsw_forcewake_release(void) {
    /* Never skip the clear store because acquisition, display or clock checks
     * failed. A separate budget bounds confirmation of the cleanup. */
    hsw_forcewake.clear_us = timer_monotonic_us();
    bool stored = hsw_forcewake_store(false);
    bool clear = stored && hsw_forcewake_wait_ack(hsw_forcewake.clear_us, 0,
                                                 &hsw_forcewake.final, &hsw_forcewake.release_us);
    hsw_forcewake.released = clear && hsw_forcewake.clear_us >= hsw_forcewake.ack_us &&
                             hsw_forcewake.clear_us >= hsw_forcewake.c0_us;
    return hsw_forcewake.released;
}

static bool hsw_forcewake_retire(void) {
    if (hsw_forcewake.request_mapped) {
        uint64_t kernel = vmm_kernel_address_space(), pa;
        if (vmm_current_address_space() == kernel &&
            vmm_unmap_page_in_space(kernel, HSW_WAKE_REQUEST_VA, &pa) &&
            pa == gpu_base + HSW_WAKE_REQUEST_PAGE &&
            !vmm_virt_to_phys_in_space(kernel, HSW_WAKE_REQUEST_VA, &pa)) {
            hsw_forcewake.request_mapped = false;
            hsw_forcewake.retired++;
        } else {
            hsw_forcewake.request_poison = true;
        }
    }
    if (hsw_window_live && hsw_unmap(NULL)) hsw_forcewake.retired++;
    hsw_wake_ack_window = false;
    return !hsw_forcewake.request_poison && !hsw_window_poison;
}

static bool hsw_forcewake_run(void) {
    uint32_t ignored;
    if (!display_profile_is_air61() ||
        !intel_hsw_ggtt_candidate(&hsw_result, HSW_OWNERSHIP_FIRST * 4096u, &ignored))
        return false;
    console_write("hsw-forcewake-progress: testing kernel ACK and GT thread C0 while Pipe A scans\n");
    hsw_survey.begin = hsw_survey.end = timer_monotonic_us();
    hsw_survey.status = HSW_SURVEY_ACCESS;
    hsw_forcewake.failure = "map";
    hsw_wake_ack_window = true;
    bool ack_mapped = hsw_survey.begin && hsw_map(NULL, HSW_WAKE_ACK_PAGE);
    if (ack_mapped) hsw_forcewake.maps++;
    bool acquired = ack_mapped && hsw_forcewake_map_request() && hsw_forcewake_acquire();
    if (hsw_forcewake.stores && !hsw_forcewake_release()) {
        hsw_forcewake.failure = "release-unverified";
        hsw_survey.status = HSW_SURVEY_RETAINED;
        return false;
    }
    if (!hsw_forcewake_retire()) {
        hsw_forcewake.failure = "alias-retirement";
        hsw_survey.status = HSW_SURVEY_RETAINED;
        return false;
    }
    if (!acquired) return false;
    if (hsw_forcewake.clear_us - hsw_forcewake.request_us > HSW_WAKE_HOLD_US) {
        hsw_forcewake.failure = "hold-deadline";
        hsw_survey.status = HSW_SURVEY_DEADLINE;
        return false;
    }
    hsw_forcewake.failure = "display";
    if (!hsw_survey_display() || hsw_survey.end < hsw_forcewake.release_us) return false;
    uint64_t before_gap = hsw_survey.end;
    if (!timer_delay_us(HSW_CANDIDATE_GAP_US) || !hsw_survey_tick() ||
        hsw_survey.end - before_gap < HSW_CANDIDATE_GAP_US) {
        hsw_survey.status = HSW_SURVEY_DEADLINE;
        return false;
    }
    hsw_survey.frames[1] = resource_read(0x70040);
    if (!hsw_survey_display()) return false;
    uint32_t frames = hsw_survey.frames[1] - hsw_survey.frames[0];
    if (!frames || frames > 8) {
        hsw_survey.status = HSW_SURVEY_CHANGED;
        return false;
    }
    hsw_survey.status = HSW_SURVEY_OBSERVED;
    hsw_forcewake.failure = NULL;
    return true;
}

static void hsw_forcewake_report(void) {
    console_printf("hsw-forcewake: v=1 status=%u reason=%s maps=%u retired=%u stores=%u released=%u frames=%u,%u ring_reads=0 protected_reads=0 requester_hold=unverified\n",
                   (uint64_t)hsw_survey.status, hsw_forcewake.failure ? hsw_forcewake.failure : "none",
                   (uint64_t)hsw_forcewake.maps, (uint64_t)hsw_forcewake.retired,
                   (uint64_t)hsw_forcewake.stores, (uint64_t)hsw_forcewake.released,
                   (uint64_t)hsw_survey.frames[0], (uint64_t)hsw_survey.frames[1]);
    console_printf("hsw-forcewake-proof: begin=%u request=%u ack=%u c0=%u clear=%u release=%u end=%u baseline=%u acquired=%u thread=%u held=%u final=%u\n",
                   hsw_survey.begin, hsw_forcewake.request_us, hsw_forcewake.ack_us, hsw_forcewake.c0_us,
                   hsw_forcewake.clear_us, hsw_forcewake.release_us, hsw_survey.end,
                   (uint64_t)hsw_forcewake.baseline, (uint64_t)hsw_forcewake.ack,
                   (uint64_t)hsw_forcewake.thread, (uint64_t)hsw_forcewake.held,
                   (uint64_t)hsw_forcewake.final);
}
