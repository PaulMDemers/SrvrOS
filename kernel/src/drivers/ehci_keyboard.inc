/* Attended boot-keyboard trial for the captured 05ac:0245 interface. */
#define EHCI_KEYBOARD_ENDPOINT 0x83u
#define EHCI_KEYBOARD_PACKET 10u
#define EHCI_KEYBOARD_INTERVAL 8u
#define EHCI_KEYBOARD_REPORT 8u
#define EHCI_KEYBOARD_WAIT_US 90000000u
#define EHCI_KEYBOARD_POLL_US 1000u
#define EHCI_FRINDEX_MASK 0x3fffu
#define EHCI_HID_SET_IDLE 10u
#define EHCI_KEYBOARD_IDLE_4MS 8u

static bool captured_keyboard_interface(const uint8_t *configuration, uint16_t length) {
    if (!configuration || length < 9 || configuration[5] != 1) return false;
    bool keyboard = false;
    for (unsigned offset = 9; offset + 2 <= length;) {
        const uint8_t *entry = configuration + offset;
        unsigned size = entry[0];
        if (size < 2 || size > length - offset) return false;
        if (entry[1] == USB_DESC_INTERFACE) {
            keyboard = size == 9 && !entry[2] && !entry[3] && entry[4] == 1 &&
                entry[5] == 3 && entry[6] == 1 && entry[7] == 1;
        } else if (keyboard && entry[1] == USB_DESC_ENDPOINT) {
            return size == 7 && entry[2] == EHCI_KEYBOARD_ENDPOINT && entry[3] == 3 &&
                entry[4] == EHCI_KEYBOARD_PACKET && !entry[5] && entry[6] == EHCI_KEYBOARD_INTERVAL;
        }
        offset += size;
    }
    return false;
}

static const struct ehci_periodic_endpoint keyboard_endpoint = {
    EHCI_KEYBOARD_ENDPOINT, EHCI_KEYBOARD_PACKET, EHCI_KEYBOARD_INTERVAL, EHCI_KEYBOARD_PACKET,
};

static void sample_keyboard_schedule(const struct ehci_control_path *path) {
    struct ehci_probe_report *report = path->report;
    struct ehci_keyboard_schedule *schedule = &report->keyboard_schedule;
    const volatile struct ehci_dma *dma = path->dma;
    unsigned index = path->op[EHCI_FRINDEX] & EHCI_FRINDEX_MASK;
    if (!schedule->sampled) schedule->first_frame_index = index;
    else if (index != schedule->last_frame_index) schedule->frame_index_changes++;
    schedule->sampled = true;
    schedule->last_frame_index = index;
    schedule->usbsts = path->op[EHCI_USBSTS];
    schedule->overlay_token = dma->qh.overlay.token;
    schedule->c_progress_seen |= dma->qh.overlay.buffer[1] & 0xffu;
    uint32_t qtd = path->physical + offsetof(struct ehci_dma, setup_td);
    schedule->qtd_loaded |= (dma->qh.current & ~31u) == qtd;
    report->keyboard_token = dma->setup_td.token;
}

static const char *arm_keyboard_poll(struct ehci_control_path *path, uint32_t toggle) {
    struct ehci_probe_report *report = path->report;
    if (!schedules_idle(path)) return "periodic schedule busy";
    prepare_periodic_transfer(path, &report->keyboard_target, &keyboard_endpoint, toggle);
    __asm__ volatile ("mfence" : : : "memory");
    report->keyboard_schedule = (struct ehci_keyboard_schedule) {0};
    sample_keyboard_schedule(path);
    return enable_periodic(path) ? 0 : "periodic enable timeout";
}

static const char *finish_keyboard_poll(struct ehci_control_path *path, uint64_t start, uint32_t *toggle,
    uint8_t received[EHCI_KEYBOARD_REPORT], const char *failure) {
    const volatile struct ehci_dma *dma = path->dma;
    struct ehci_probe_report *report = path->report;
    sample_keyboard_schedule(path);
    /* Reuse the qTD and preserve its toggle only after PSS acknowledges stop. */
    if (!disable_periodic(path)) return "periodic stop timeout";
    if (failure) return failure;

    __asm__ volatile ("mfence" : : : "memory");
    *toggle = dma->qh.overlay.token & EHCI_TD_DATA1;
    unsigned residual = (report->keyboard_token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
    if (residual > EHCI_KEYBOARD_PACKET) return "invalid interrupt residual";
    report->keyboard_bytes = EHCI_KEYBOARD_PACKET - residual;
    if (report->keyboard_bytes != EHCI_KEYBOARD_REPORT) return "unexpected boot report length";
    for (unsigned i = 0; i < EHCI_KEYBOARD_REPORT; i++) received[i] = dma->data[i];
    if (!report->keyboard_reports) report->keyboard_first_report_us = timer_monotonic_us() - start;
    report->keyboard_reports++;
    return 0;
}

static const char *keyboard_poll_once(struct ehci_control_path *path, uint64_t start, uint32_t *toggle,
    uint8_t received[EHCI_KEYBOARD_REPORT]) {
    struct ehci_probe_report *report = path->report;
    const char *failure = arm_keyboard_poll(path, *toggle);
    if (failure) return failure;
    failure = "typing timeout";
    for (unsigned poll = 0; poll < EHCI_KEYBOARD_WAIT_US / EHCI_KEYBOARD_POLL_US; poll++) {
        sample_keyboard_schedule(path);
        uint32_t token = report->keyboard_token;
        if ((report->keyboard_schedule.usbsts & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) || (token & EHCI_TD_ERROR)) {
            failure = "interrupt transfer error";
            break;
        }
        if (!(token & EHCI_TD_ACTIVE)) {
            failure = 0;
            break;
        }
        if (timer_monotonic_us() - start >= EHCI_KEYBOARD_WAIT_US) break;
        if (!timer_delay_us(EHCI_KEYBOARD_POLL_US)) {
            failure = "keyboard timer failure";
            break;
        }
    }
    return finish_keyboard_poll(path, start, toggle, received, failure);
}

static bool keyboard_trial_line(char text[32], unsigned *length) {
    const char expected[] = "echo Ehci-42";
    char c;
    while (keyboard_try_read_char(&c)) {
        if (c == '\n') {
            console_write("\n");
            bool matches = *length == sizeof(expected) - 1;
            for (unsigned i = 0; matches && i < *length; i++) matches = text[i] == expected[i];
            if (matches) return true;
            *length = 0;
            console_write("Try again. Type echo Ehci-42 and press Return.\nsrv> ");
        } else if (c == '\b' && *length) {
            (*length)--;
            console_write("\b \b");
        } else if (c >= ' ' && c <= '~' && *length < 32) {
            text[(*length)++] = c;
            console_putc(c);
        }
    }
    return false;
}

static const char *configure_keyboard(struct ehci_control_path *path, uint64_t *frame_list) {
    struct ehci_probe_report *report = path->report;
    const struct ehci_control_target *target = &report->keyboard_target;
    if (!target->address) return "captured interface unavailable";
    volatile uint32_t *op = path->op;
    if (op[EHCI_USBCMD] & EHCI_CMD_FRAME_SIZE) return "unsupported frame-list size";
    const struct usb_setup_packet configuration = { 0, USB_REQ_SET_CONFIGURATION, 1, 0, 0 };
    const struct usb_setup_packet boot_protocol = { 0x21, 11, 0, 0, 0 };
    unsigned received;
    if (!control_transfer(path, target, &configuration, 0, &received)) return "SET_CONFIGURATION failed";
    if (!control_transfer(path, target, &boot_protocol, 0, &received)) return "SET_PROTOCOL failed";
    const struct usb_setup_packet idle = { 0x21, EHCI_HID_SET_IDLE, EHCI_KEYBOARD_IDLE_4MS << 8, 0, 0 };
    report->keyboard_idle = control_transfer(path, target, &idle, 0, &received) ? "accepted" : "refused";
    if ((op[EHCI_USBCMD] & EHCI_CMD_SCHEDULES) ||
        (op[EHCI_USBSTS] & (EHCI_STS_SCHEDULES | EHCI_STS_HALTED | EHCI_STS_HOST_ERROR))) {
        return "idle request schedule unverified";
    }

    return configure_periodic_frames(path, &keyboard_endpoint, path->physical + offsetof(struct ehci_dma, qh), frame_list);
}

static const char *run_keyboard_trial(struct ehci_control_path *path, const char *release, uint64_t *frame_list) {
    const char *configuration_error = configure_keyboard(path, frame_list);
    if (configuration_error) return configuration_error;
    struct ehci_probe_report *report = path->report;

    ehci_trial_screen(release, true);
    console_write("srv> ");
    console_sync();
    uint64_t start = timer_monotonic_us();
    uint64_t dropped = keyboard_dropped_count();
    struct keyboard_hid_state state = {0};
    uint32_t toggle = 0;
    unsigned length = 0;
    char text[32];
    for (unsigned poll = 0; poll < EHCI_KEYBOARD_WAIT_US / EHCI_KEYBOARD_POLL_US; poll++) {
        if (timer_monotonic_us() - start >= EHCI_KEYBOARD_WAIT_US) break;
        uint8_t bytes[EHCI_KEYBOARD_REPORT];
        const char *failure = keyboard_poll_once(path, start, &toggle, bytes);
        if (failure) return failure;
        keyboard_inject_hid_report(bytes, &state);
        if (keyboard_dropped_count() != dropped) return "keyboard input dropped";
        if (keyboard_trial_line(text, &length)) {
            report->keyboard_passed = true;
            console_write("Ehci-42\n");
            return "PASS";
        }
    }
    return "typing timeout";
}
