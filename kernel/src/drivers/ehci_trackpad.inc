/* Captured MacBookPro8,1 Wellspring5. Ubuntu's EHCI queue polls every frame. */
#define EHCI_TRACKPAD_TYPE 2u
#define EHCI_TRACKPAD_REPORT_MAX (30u + 28u * APPLE_TRACKPAD_CONTACTS)
#define EHCI_TRACKPAD_WAIT_US 60000000u
#define EHCI_TRACKPAD_POLL_US 250u
#define EHCI_TRACKPAD_MOVE_UNITS 32
#define EHCI_TRACKPAD_BATCH_SIZE 6u

struct ehci_trackpad_slot {
    _Alignas(32) struct ehci_qtd td;
    uint8_t data[EHCI_TRACKPAD_REPORT_MAX];
};

struct ehci_trackpad_dma {
    _Alignas(32) struct ehci_qh qh;
    struct ehci_trackpad_slot slots[EHCI_TRACKPAD_BATCH_SIZE];
};

_Static_assert(sizeof(struct ehci_trackpad_dma) <= EHCI_PAGE_BYTES, "trackpad batch fits one DMA page");
_Static_assert(EHCI_TRACKPAD_REPORT_MAX <= sizeof(((struct ehci_trackpad_capture *)0)->first_payload),
    "trackpad capture retains the whole first payload");

static const struct ehci_periodic_endpoint trackpad_endpoint = { 0x81, 64, 1, EHCI_TRACKPAD_REPORT_MAX };

static bool captured_trackpad_interfaces(const uint8_t *configuration, uint16_t length) {
    if (!configuration || length < 9 || configuration[5] != 1) return false;
    bool data = false, mouse = false;
    unsigned interface = 0;
    for (unsigned offset = 9; offset + 2 <= length;) {
        const uint8_t *entry = configuration + offset;
        unsigned size = entry[0];
        if (size < 2 || size > length - offset) return false;
        if (entry[1] == USB_DESC_INTERFACE) {
            interface = 0;
            if (size == 9 && !entry[3] && entry[4] == 1 && entry[5] == 3) {
                if (entry[2] == 1 && !entry[6] && !entry[7]) interface = 1;
                if (entry[2] == 2 && entry[6] == 1 && entry[7] == 2) interface = 2;
            }
        } else if (interface && entry[1] == USB_DESC_ENDPOINT) {
            if (size != 7 || entry[3] != 3 || entry[5]) return false;
            if (interface == 1) {
                if (data || entry[2] != trackpad_endpoint.address ||
                    entry[4] != trackpad_endpoint.packet || entry[6] != 2) return false;
                data = true;
            } else {
                if (mouse || entry[2] != 0x84 || entry[4] != 8 || entry[6] != 8) return false;
                mouse = true;
            }
        }
        offset += size;
    }
    return data && mouse;
}

static bool read_trackpad_mode(struct ehci_control_path *path, uint8_t data[8]) {
    const struct usb_setup_packet request = { 0xa1, 1, 0x0300, 0, 8 };
    unsigned received;
    return control_transfer(path, &path->report->trackpad_target, &request, data, &received) && received == 8;
}

static bool write_trackpad_mode(struct ehci_control_path *path, const uint8_t data[8], uint64_t *completed_us) {
    const struct usb_setup_packet request = { 0x21, 9, 0x0300, 0, 8 };
    unsigned transferred;
    return control_transfer_payload(path, &path->report->trackpad_target, &request, NULL, data, &transferred,
        completed_us) &&
        transferred == 8;
}

static bool trackpad_capture_complete(const struct ehci_trackpad_capture *capture) {
    return capture->moved && capture->contacts >= 2 && capture->released;
}

static void record_trackpad_frame(struct ehci_trackpad_capture *capture, const uint8_t *data, unsigned length) {
    struct apple_trackpad_frame frame;
    int decoded = apple_trackpad_decode_type(EHCI_TRACKPAD_TYPE, data, length, &frame);
    capture->bytes = length;
    if (length > capture->wire_max) capture->wire_max = length;
    if (length == 8) {
        capture->eight_byte_reports++;
        capture->last_eight_byte_id = data[0];
        uint64_t sample = 0;
        for (unsigned i = 0; i < length; i++) sample = (sample << 8) | data[i];
        capture->last_eight_byte_sample = sample;
        /* Interface 1 advertises this HID report ID on the captured topcase. */
        if (data[0] == 0x44) capture->eight_byte_id44++;
    }
    if (decoded <= 0) {
        if (decoded == 0) capture->ignored++;
        else capture->invalid++;
        return;
    }
    capture->reports++;
    if (length > capture->largest) capture->largest = length;
    if (frame.count > capture->contacts) {
        capture->contacts = frame.count;
        if (capture->contacts == 1) console_write("One contact detected. Move it across the pad.\n");
        else if (capture->contacts == 2) console_write("Two contacts detected. Press and release the clickpad.\n");
    }
    if (!capture->moved && frame.count == 1) {
        const struct apple_trackpad_contact *contact = &frame.contacts[0];
        if (capture->have_position) {
            int dx = contact->x - capture->first_x, dy = contact->y - capture->first_y;
            if (dx >= EHCI_TRACKPAD_MOVE_UNITS || dx <= -EHCI_TRACKPAD_MOVE_UNITS ||
                dy >= EHCI_TRACKPAD_MOVE_UNITS || dy <= -EHCI_TRACKPAD_MOVE_UNITS) {
                capture->moved = true;
                console_write("One-finger motion detected. Place two fingers on the pad.\n");
            }
        } else {
            capture->first_x = contact->x;
            capture->first_y = contact->y;
            capture->have_position = true;
        }
    } else if (frame.count != 1) {
        capture->have_position = false;
    }
    if (!capture->pressed && frame.button) {
        capture->pressed = true;
        console_write("Click down detected. Release the clickpad.\n");
    } else if (capture->pressed && !capture->released && !frame.button) {
        capture->released = true;
        console_write("Click release detected.\n");
    }
}

static void prepare_trackpad_batch(struct ehci_trackpad_dma *dma, uint32_t physical,
    const struct ehci_control_target *target, uint32_t toggle) {
    *dma = (struct ehci_trackpad_dma) {0};
    uint32_t first = physical + offsetof(struct ehci_trackpad_dma, slots);
    prepare_periodic_qh(&dma->qh, target, &trackpad_endpoint, first, toggle);
    for (unsigned i = 0; i < EHCI_TRACKPAD_BATCH_SIZE; i++) {
        struct ehci_qtd *td = &dma->slots[i].td;
        uint32_t next = i + 1 < EHCI_TRACKPAD_BATCH_SIZE ? first + (i + 1) * sizeof(dma->slots[0]) : EHCI_LINK_END;
        /* Each short IN ends only its report. Both links advance to the next buffer. */
        td->next = td->alternate = next;
        td->token = (EHCI_TRACKPAD_REPORT_MAX << EHCI_TD_BYTES_SHIFT) |
            EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
        td->buffer[0] = first + i * sizeof(dma->slots[0]) + offsetof(struct ehci_trackpad_slot, data);
    }
}

/* The first batch and frame list are ready before the mode write. A successful
 * control transfer has acknowledged async stop, so the next action can set PSE. */
static const char *poll_trackpad(struct ehci_control_path *path, struct ehci_trackpad_dma *dma, uint32_t physical,
    uint64_t mode_completed_us) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    const volatile struct ehci_trackpad_dma *progress = dma;
    capture->arm_requested = true;
    capture->mode_to_arm_us = timer_monotonic_us() - mode_completed_us;
    bool enabled = enable_periodic(path);
    uint64_t start = timer_monotonic_us();
    capture->arm_acknowledged = enabled;
    if (enabled) capture->mode_to_active_us = start - mode_completed_us;
    for (;;) {
        capture->batches++;
        const char *failure = enabled ? NULL : "periodic enable timeout";
        for (unsigned i = 0; i < EHCI_TRACKPAD_BATCH_SIZE && !failure; i++) {
            for (;;) {
                capture->token = progress->slots[i].td.token;
                capture->overlay = progress->qh.overlay.token;
                capture->status = path->op[EHCI_USBSTS];
                if ((capture->token & EHCI_TD_ERROR) || (capture->status & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR))) {
                    failure = "interrupt transfer error";
                } else if (!(capture->token & EHCI_TD_ACTIVE)) {
                    break;
                } else if (timer_monotonic_us() - start >= EHCI_TRACKPAD_WAIT_US) {
                    failure = "gesture capture timeout";
                } else if (!timer_delay_us(EHCI_TRACKPAD_POLL_US)) {
                    failure = "capture timer failure";
                }
                if (failure) break;
            }
            if (failure) break;
            unsigned residual = (capture->token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
            if (residual > EHCI_TRACKPAD_REPORT_MAX) {
                failure = "invalid interrupt residual";
                break;
            }
            __asm__ volatile ("mfence" : : : "memory");
            unsigned length = EHCI_TRACKPAD_REPORT_MAX - residual;
            if (!capture->completions) {
                capture->mode_to_first_completion_us = timer_monotonic_us() - mode_completed_us;
                capture->first_payload_length = length;
                for (unsigned byte = 0; byte < length; byte++) capture->first_payload[byte] = dma->slots[i].data[byte];
            }
            capture->completions++;
            record_trackpad_frame(capture, dma->slots[i].data, length);
            if (trackpad_capture_complete(capture)) break;
        }
        /* Keep the frame linked across reports. Reuse the batch only after PSS clears. */
        if (!disable_periodic(path)) return "periodic stop timeout";
        if (failure || trackpad_capture_complete(capture)) return failure;
        if (timer_monotonic_us() - start >= EHCI_TRACKPAD_WAIT_US) return "gesture capture timeout";
        if (!schedules_idle(path)) return "periodic schedule busy";
        __asm__ volatile ("mfence" : : : "memory");
        uint32_t toggle = progress->qh.overlay.token & EHCI_TD_DATA1;
        prepare_trackpad_batch(dma, physical, &path->report->trackpad_target, toggle);
        __asm__ volatile ("mfence" : : : "memory");
        enabled = enable_periodic(path);
    }
}

static void run_trackpad_trial(struct ehci_control_path *path, uint64_t *frame_list, uint64_t *transfer_page) {
    struct ehci_probe_report *report = path->report;
    struct ehci_trackpad_capture *capture = &report->trackpad;
    capture->result = "captured trackpad interfaces unavailable";
    if (!report->trackpad_target.address) return;
    const struct usb_setup_packet configuration = { 0, USB_REQ_SET_CONFIGURATION, 1, 0, 0 };
    unsigned received;
    capture->result = "SET_CONFIGURATION failed";
    if (!control_transfer(path, &report->trackpad_target, &configuration, NULL, &received)) return;
    uint8_t original[8];
    capture->result = "feature mode read failed";
    if (!read_trackpad_mode(path, original)) return;
    capture->mode_before_read = true;
    capture->mode_before = original[0];
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        capture->result = "trackpad interrupt DMA unavailable";
        return;
    }
    *transfer_page = physical;
    struct ehci_trackpad_dma *dma = pmm_phys_to_virt(physical);
    prepare_trackpad_batch(dma, (uint32_t)physical, &report->trackpad_target, 0);
    uint32_t qh = (uint32_t)physical + offsetof(struct ehci_trackpad_dma, qh);
    capture->result = configure_periodic_frames(path, &trackpad_endpoint, qh, frame_list);
    if (capture->result) return;

    console_set_framebuffer_muted(false);
    console_write("Trackpad capture starting. Start the checklist now. You have 60 seconds.\n");
    console_sync();
    uint8_t mode[8];
    for (unsigned i = 0; i < sizeof(mode); i++) mode[i] = original[i];
    mode[0] = 1;
    uint64_t mode_completed_us;
    if (write_trackpad_mode(path, mode, &mode_completed_us)) {
        capture->result = poll_trackpad(path, dma, (uint32_t)physical, mode_completed_us);
        if (schedules_idle(path) && read_trackpad_mode(path, mode)) {
            capture->mode_after_read = true;
            capture->mode_after = mode[0];
        } else if (!capture->result) capture->result = "post-capture feature readback failed";
    } else capture->result = "feature mode write failed";
    capture->mode_restored = write_trackpad_mode(path, original, NULL) &&
        read_trackpad_mode(path, mode) && mode[0] == original[0];
}
