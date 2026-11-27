/* One kernel I/O worker owns both captured topcase endpoints. */
static enum ehci_monitor_state desktop_trackpad_state(void *arg) {
    (void)arg;
    return __atomic_load_n(&desktop_trackpad.state, __ATOMIC_ACQUIRE);
}

static bool desktop_trackpad_eligible(void *arg) {
    enum ehci_monitor_state state = desktop_trackpad_state(arg);
    return state == EHCI_MONITOR_RUNNING || state == EHCI_MONITOR_STOPPING;
}

static bool desktop_trackpad_due(void *arg) {
    return desktop_trackpad_state(NULL) != EHCI_MONITOR_RUNNING || timer_ticks() >= *(const uint64_t *)arg;
}

static bool release_desktop_trackpad(void) {
    struct ehci_desktop_trackpad *session = &desktop_trackpad;
    struct ehci_control_path *path = &session->path;
    if (!disable_periodic(path) || !schedules_idle(path) || !restore_trackpad_mode(path)) {
        if (!session->failure) session->failure = "feature mode restoration failed";
    }
    if (!retire_control_path(session->device, path->op, true)) return false;
    __asm__ volatile ("mfence" : : : "memory");
    pmm_free_frame(path->physical);
    pmm_free_frame(session->frame_list);
    pmm_free_frame(session->transfer_page);
    bool restored = set_command_bits(session->device, PCI_COMMAND_MEMORY, session->memory_command);

    /* These are non-global kernel pages. The I/O worker runs on the kernel
     * root, and user roots share the high-half tables. Unpublish with local
     * interrupts disabled after the controller has stopped touching DMA. */
    uint64_t flags, removed;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    bool unmapped = vmm_unmap_page_in_space(session->root, EHCI_MMIO_WINDOW, &removed) && removed == session->page;
    if (flags & (1u << 9)) __asm__ volatile ("sti" : : : "memory");
    return restored && unmapped;
}

static void desktop_trackpad_pointer(struct trackpad_pointer *pointer, const struct apple_trackpad_frame *frame) {
    struct trackpad_motion motion;
    unsigned speed, options;
    mouse_trackpad_get_settings(&speed, &options);
    uint8_t previous = pointer->buttons;
    if (!trackpad_pointer_configured(pointer, frame, false, false, timer_monotonic_us(), speed, options, &motion))
        return;
    if (previous && motion.palm_suppressed) {
        mouse_cancel_trackpad();
        return;
    }
    mouse_inject_trackpad(motion.dx, motion.dy, motion.buttons);
    desktop_trackpad.motion += motion.dx != 0 || motion.dy != 0;
    if (motion.buttons != previous) {
        if (motion.buttons & 1u) desktop_trackpad.presses++;
        else if (previous & 1u) desktop_trackpad.releases++;
    }
}

static void cancel_desktop_pointer(struct trackpad_pointer *pointer) {
    struct trackpad_motion motion;
    if (trackpad_pointer_cancel(pointer, &motion)) mouse_cancel_trackpad();
}

static const char *desktop_trackpad_poll(struct ehci_control_path *path, struct ehci_trackpad_dma *dma,
    struct trackpad_pointer *pointer, unsigned *slot, uint64_t *last_report) {
    const volatile struct ehci_trackpad_dma *progress = dma;
    uint32_t token = progress->slots[*slot].td.token;
    uint32_t status = path->op[EHCI_USBSTS];
    if ((token & EHCI_TD_ERROR) || (status & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) ||
        !(status & EHCI_STS_PERIODIC) || !(path->op[EHCI_PORTSC] & EHCI_PORT_CONNECTED))
        return "interrupt queue stopped";
    if (token & EHCI_TD_ACTIVE) {
        if (pointer->buttons && timer_monotonic_us() - *last_report >= 1000000u) cancel_desktop_pointer(pointer);
        return NULL;
    }

    unsigned residual = (token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
    if (residual > trackpad_endpoint.length) return "invalid interrupt residual";
    __asm__ volatile ("mfence" : : : "memory");
    unsigned length = trackpad_endpoint.length - residual;
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    unsigned next = *slot ^ 1u;
    uint32_t retired = capture->first_td + *slot * sizeof(dma->slots[0]);
    uint32_t dummy = capture->first_td + next * sizeof(dma->slots[0]);
    if (!trackpad_queue_parked(progress, next, retired, dummy)) return "queue did not park at the inactive dummy";
    const uint8_t *data = dma->slots[*slot].data;
    struct apple_trackpad_frame frame;
    desktop_trackpad.completions++;
    int decoded = apple_trackpad_decode_type(EHCI_TRACKPAD_TYPE, data, length, &frame);
    bool retry = decoded < 0 && length == 8 && !desktop_trackpad.reports && !capture->recovery.attempted;
    uint64_t retry_completed = 0;
    if (retry) {
        capture->last_eight_byte_sample = trackpad_report_word(data);
        const char *failure = retry_trackpad_mode(path, &retry_completed, false);
        if (failure) return failure;
        if (!trackpad_queue_parked(progress, next, retired, dummy)) return "parked queue changed during mode retry";
    } else if (decoded > 0) {
        desktop_trackpad.reports++;
        *last_report = timer_monotonic_us();
        desktop_trackpad_pointer(pointer, &frame);
    } else if (decoded < 0) {
        /* Sample only retired requests, before the buffer can be reused. */
        unsigned sample = desktop_trackpad.malformed++;
        if (sample < EHCI_DESKTOP_MALFORMED_SAMPLES) {
            struct ehci_desktop_frame_sample *trace = &desktop_trackpad.malformed_samples[sample];
            trace->observed_us = timer_monotonic_us();
            trace->previous_valid_us = *last_report;
            trace->qtd_token = token;
            trace->overlay_token = progress->qh.overlay.token;
            trace->frame_index = path->op[EHCI_FRINDEX] & EHCI_FRINDEX_MASK;
            trace->completion = desktop_trackpad.completions;
            trace->length = length;
            for (unsigned i = 0; i < length; i++) trace->bytes[i] = data[i];
        }
        cancel_desktop_pointer(pointer);
    } else desktop_trackpad.ignored++;

    park_trackpad_qtd(&dma->slots[*slot].td);
    publish_trackpad_qtd(&dma->slots[next].td, retired,
        dummy + offsetof(struct ehci_trackpad_slot, data), trackpad_endpoint.length);
    *slot = next;
    if (retry) {
        if (!enable_periodic(path)) return "periodic resume after mode retry failed";
        capture->recovery.resumed = true;
        desktop_trackpad.retry_activated_us = timer_monotonic_us();
        capture->recovery.mode_to_active_us = desktop_trackpad.retry_activated_us - retry_completed;
    }
    return NULL;
}

static bool desktop_keyboard_has_key(const uint8_t report[EHCI_KEYBOARD_REPORT]) {
    for (unsigned i = 2; i < EHCI_KEYBOARD_REPORT; i++) {
        if (report[i] > 3) return true;
    }
    return false;
}

static void desktop_keyboard_report(const uint8_t *data, unsigned length, struct keyboard_hid_state *state) {
    if (length != EHCI_KEYBOARD_REPORT || data[1]) {
        desktop_trackpad.keyboard_invalid++;
        return;
    }
    for (unsigned i = 2; i < EHCI_KEYBOARD_REPORT; i++) {
        if (data[i] && data[i] <= 3) {
            desktop_trackpad.keyboard_invalid++;
            break; /* The HID decoder cancels repeat without generating false key releases. */
        }
    }
    bool was_down = desktop_keyboard_has_key(state->previous_report);
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    keyboard_inject_hid_report(data, state);
    if (flags & (1u << 9)) __asm__ volatile ("sti" : : : "memory");
    bool down = desktop_keyboard_has_key(state->previous_report);
    desktop_trackpad.keyboard_reports++;
    desktop_trackpad.keyboard_presses += !was_down && down;
    desktop_trackpad.keyboard_releases += was_down && !down;
}

static const char *desktop_keyboard_poll(struct ehci_trackpad_dma *dma, uint32_t physical,
    struct keyboard_hid_state *state, unsigned *slot) {
    const volatile struct ehci_trackpad_dma *progress = dma;
    uint32_t token = progress->slots[*slot].td.token;
    if (token & EHCI_TD_ERROR) return "keyboard interrupt transfer error";
    if (!(token & EHCI_TD_ACTIVE)) {
        unsigned residual = (token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (residual > keyboard_endpoint.length) return "invalid keyboard interrupt residual";
        __asm__ volatile ("mfence" : : : "memory");
        unsigned next = *slot ^ 1u;
        uint32_t first = physical + offsetof(struct ehci_trackpad_dma, slots);
        uint32_t retired = first + *slot * sizeof(dma->slots[0]);
        uint32_t dummy = first + next * sizeof(dma->slots[0]);
        if (!trackpad_queue_parked(progress, next, retired, dummy)) return "keyboard queue did not park";
        desktop_keyboard_report(dma->slots[*slot].data, keyboard_endpoint.length - residual, state);
        park_trackpad_qtd(&dma->slots[*slot].td);
        publish_trackpad_qtd(&dma->slots[next].td, retired,
            dummy + offsetof(struct ehci_trackpad_slot, data), keyboard_endpoint.length);
        *slot = next;
    }
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    keyboard_repeat_hid(state, timer_monotonic_us());
    if (flags & (1u << 9)) __asm__ volatile ("sti" : : : "memory");
    return NULL;
}

static void desktop_trackpad_worker(void *arg) {
    struct ehci_control_path *path = arg;
    struct ehci_trackpad_dma *dma = pmm_phys_to_virt(desktop_trackpad.transfer_page);
    uint32_t keyboard_queue = (uint32_t)desktop_trackpad.transfer_page + sizeof(*dma);
    struct trackpad_pointer pointer = {0};
    struct keyboard_hid_state keyboard = {0};
    struct scheduler_wait_queue wake = {0};
    unsigned slot = 0, keyboard_slot = 0;
    uint64_t last_report = 0;
    while (desktop_trackpad_state(NULL) == EHCI_MONITOR_RUNNING) {
        desktop_trackpad.failure = desktop_trackpad_poll(path, dma, &pointer, &slot, &last_report);
        if (!desktop_trackpad.failure)
            desktop_trackpad.failure = desktop_keyboard_poll(dma + 1, keyboard_queue, &keyboard, &keyboard_slot);
        if (desktop_trackpad.failure) break;
        uint64_t due = timer_ticks() + 1;
        if (!scheduler_wait_timeout(&wake, desktop_trackpad_due, &due, due)) scheduler_yield();
    }
    cancel_desktop_pointer(&pointer);
    uint64_t flags;
    const uint8_t released_keys[EHCI_KEYBOARD_REPORT] = {0};
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    keyboard_inject_hid_report(released_keys, &keyboard);
    if (flags & (1u << 9)) __asm__ volatile ("sti" : : : "memory");
    bool released = release_desktop_trackpad();
    __atomic_store_n(&desktop_trackpad.state, released ? EHCI_MONITOR_RELEASED : EHCI_MONITOR_UNSAFE,
        __ATOMIC_RELEASE);
}

bool ehci_desktop_trackpad_start(void) {
    uint64_t root = vmm_kernel_address_space();
    if (desktop_trackpad_state(NULL) != EHCI_MONITOR_UNUSED || !timer_reference_ready() ||
        !vmm_diagnostic_sole_kernel_root(root)) return false;
    const struct pci_device *device = captured_topcase_controller();
    if (!device) {
        console_write("ehci-desktop: captured trackpad controller absent\n");
        return false;
    }
    struct ehci_probe_report *report = &desktop_trackpad.report;
    bool reusable = probe_controller(device, root, EHCI_BOOT_DESKTOP_TRACKPAD, report, 0, NULL);
    if (!desktop_trackpad.frame_list) {
        if (!reusable || (report->reset_halted && !report->access_retired))
            desktop_trackpad.state = EHCI_MONITOR_UNSAFE;
        const char *reason = desktop_trackpad.failure ? desktop_trackpad.failure :
            report->stopped.operation ? report->stopped.operation : report->result;
        console_printf("ehci-desktop: trackpad unavailable (%s)\n", reason);
        log_transfer_failure(&report->transfer_failure);
        return false;
    }
    desktop_trackpad.state = EHCI_MONITOR_RUNNING;
    if (!scheduler_spawn_io_service("ehci-topcase", desktop_trackpad_worker, &desktop_trackpad.path,
            desktop_trackpad_eligible)) {
        desktop_trackpad.failure = "topcase worker could not start";
        desktop_trackpad.state = release_desktop_trackpad() ? EHCI_MONITOR_RELEASED : EHCI_MONITOR_UNSAFE;
        return false;
    }
    console_printf("ehci-desktop: trackpad ready endpoint=0x81 frames-per-8=7 address=%u active-us=%u\n",
        (uint64_t)report->trackpad_target.address, report->trackpad.mode_to_active_us);
    console_printf("ehci-desktop-keyboard: ready endpoint=0x83 interval-ms=%u shared-tt=1\n",
        (uint64_t)keyboard_endpoint.interval);
    return true;
}

static void print_desktop_frame_samples(void) {
    unsigned count = desktop_trackpad.malformed;
    if (count > EHCI_DESKTOP_MALFORMED_SAMPLES) count = EHCI_DESKTOP_MALFORMED_SAMPLES;
    console_printf("ehci-desktop-malformed: endpoint=0x81 samples=%u omitted=%u\n",
        (uint64_t)count, (uint64_t)(desktop_trackpad.malformed - count));
    for (unsigned i = 0; i < count; i++) {
        const struct ehci_desktop_frame_sample *sample = &desktop_trackpad.malformed_samples[i];
        uint64_t retry_us = desktop_trackpad.retry_activated_us;
        bool after_retry = retry_us && sample->observed_us >= retry_us;
        uint64_t phase_start = after_retry ? retry_us : desktop_trackpad.activated_us;
        console_printf("ehci-desktop-malformed: completion=%u length=%u phase=%s observed-us=%u phase-us=%u\n",
            (uint64_t)sample->completion, (uint64_t)sample->length, after_retry ? "retry" : "initial",
            sample->observed_us, sample->observed_us - phase_start);
        console_printf("ehci-desktop-malformed: completion=%u previous-valid-us=%u since-valid-us=%u frindex=%x qtd=%x overlay=%x\n",
            (uint64_t)sample->completion, sample->previous_valid_us,
            sample->previous_valid_us ? sample->observed_us - sample->previous_valid_us : 0,
            (uint64_t)sample->frame_index, (uint64_t)sample->qtd_token, (uint64_t)sample->overlay_token);
        for (unsigned offset = 0; offset < sample->length; offset += EHCI_TRACKPAD_HEX_LINE_BYTES) {
            char hex[2 * EHCI_TRACKPAD_HEX_LINE_BYTES + 1];
            unsigned bytes = sample->length - offset;
            if (bytes > EHCI_TRACKPAD_HEX_LINE_BYTES) bytes = EHCI_TRACKPAD_HEX_LINE_BYTES;
            format_trackpad_hex(hex, sample->bytes + offset, bytes);
            console_printf("ehci-desktop-malformed: completion=%u offset=%u bytes=%s\n",
                (uint64_t)sample->completion, (uint64_t)offset, hex);
        }
    }
}

bool ehci_desktop_trackpad_stop(void) {
    enum ehci_monitor_state state = desktop_trackpad_state(NULL);
    if (state == EHCI_MONITOR_UNUSED) return true;
    state = EHCI_MONITOR_RUNNING;
    __atomic_compare_exchange_n(&desktop_trackpad.state, &state, EHCI_MONITOR_STOPPING,
        false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    uint64_t start = timer_monotonic_us();
    while ((state = desktop_trackpad_state(NULL)) == EHCI_MONITOR_STOPPING && timer_monotonic_us() - start < 1000000u)
        scheduler_yield();
    if (state == EHCI_MONITOR_STOPPING) {
        console_write("ehci-desktop: trackpad retirement timed out\n");
        return false;
    }

    const struct ehci_trackpad_capture *capture = &desktop_trackpad.report.trackpad;
    const struct ehci_trackpad_recovery *recovery = &capture->recovery;
    if (recovery->attempted) console_printf(
        "ehci-desktop: trigger=%x normal=%x retry=%x waited-us=%u switch-us=%u resumed=%u\n",
        capture->last_eight_byte_sample, trackpad_report_word(recovery->normal), trackpad_report_word(recovery->wellspring),
        recovery->waited_us, recovery->switch_us, (uint64_t)recovery->resumed);
    console_printf("ehci-desktop: reports=%u ignored=%u malformed=%u motion=%u presses=%u releases=%u mode=%s cleanup=%s error=%s\n",
        (uint64_t)desktop_trackpad.reports, (uint64_t)desktop_trackpad.ignored, (uint64_t)desktop_trackpad.malformed,
        (uint64_t)desktop_trackpad.motion, (uint64_t)desktop_trackpad.presses, (uint64_t)desktop_trackpad.releases,
        capture->mode_restored ? "restored" : "unverified", state == EHCI_MONITOR_RELEASED ? "released" : "unverified",
        desktop_trackpad.failure ? desktop_trackpad.failure : "none");
    console_printf("ehci-desktop-keyboard: reports=%u invalid=%u presses=%u releases=%u\n",
        (uint64_t)desktop_trackpad.keyboard_reports, (uint64_t)desktop_trackpad.keyboard_invalid,
        (uint64_t)desktop_trackpad.keyboard_presses, (uint64_t)desktop_trackpad.keyboard_releases);
    print_desktop_frame_samples();
    return state == EHCI_MONITOR_RELEASED;
}

bool ehci_desktop_trackpad_verified(void) {
    return desktop_trackpad_state(NULL) == EHCI_MONITOR_RELEASED && !desktop_trackpad.failure &&
        !desktop_trackpad.malformed && desktop_trackpad.motion && desktop_trackpad.presses && desktop_trackpad.releases &&
        !desktop_trackpad.keyboard_invalid && desktop_trackpad.keyboard_presses && desktop_trackpad.keyboard_releases;
}
