/* Captured MacBookPro8,1 Wellspring5. Ubuntu's EHCI queue polls every frame. */
#define EHCI_TRACKPAD_TYPE 2u
#define EHCI_TRACKPAD_REPORT_MAX (30u + 28u * APPLE_TRACKPAD_CONTACTS)
#define EHCI_TRACKPAD_WAIT_US 60000000u
#define EHCI_TRACKPAD_POLL_US 250u
#define EHCI_TRACKPAD_MOVE_UNITS 32

_Static_assert(EHCI_TRACKPAD_REPORT_MAX <= EHCI_DESCRIPTOR_BYTES, "trackpad report fits one DMA page");

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

static bool write_trackpad_mode(struct ehci_control_path *path, const uint8_t data[8]) {
    const struct usb_setup_packet request = { 0x21, 9, 0x0300, 0, 8 };
    unsigned transferred;
    return control_transfer_payload(path, &path->report->trackpad_target, &request, NULL, data, &transferred) &&
        transferred == 8;
}

static const char *activate_trackpad_mode(struct ehci_control_path *path, const uint8_t original[8]) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    uint8_t mode[8];
    for (unsigned i = 0; i < sizeof(mode); i++) mode[i] = original[i];
    mode[0] = 1;
    if (!write_trackpad_mode(path, mode)) return "feature mode write failed";
    if (!read_trackpad_mode(path, mode)) return "feature mode activation readback failed";
    capture->mode_after_read = true;
    capture->mode_after = mode[0];
    /* The endpoint's frame format supplies independent evidence of activation. */
    return NULL;
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

static const char *poll_trackpad(struct ehci_control_path *path) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    const volatile struct ehci_dma *dma = path->dma;
    uint32_t toggle = 0;
    uint64_t start = timer_monotonic_us();
    while (timer_monotonic_us() - start < EHCI_TRACKPAD_WAIT_US) {
        if (!schedules_idle(path)) return "periodic schedule busy";
        prepare_periodic_transfer(path, &path->report->trackpad_target, &trackpad_endpoint, toggle);
        __asm__ volatile ("mfence" : : : "memory");
        bool enabled = enable_periodic(path);
        uint32_t token = dma->setup_td.token;
        uint32_t status = path->op[EHCI_USBSTS];
        const char *failure = enabled ? NULL : "periodic enable timeout";
        while (!failure && (token & EHCI_TD_ACTIVE) && !(token & EHCI_TD_ERROR) &&
            !(status & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) &&
            timer_monotonic_us() - start < EHCI_TRACKPAD_WAIT_US) {
            if (!timer_delay_us(EHCI_TRACKPAD_POLL_US)) failure = "capture timer failure";
            token = dma->setup_td.token;
            status = path->op[EHCI_USBSTS];
        }
        capture->token = token;
        capture->overlay = dma->qh.overlay.token;
        capture->status = status;
        if (!disable_periodic(path)) return "periodic stop timeout";
        if (failure) return failure;
        if ((token & EHCI_TD_ERROR) || (status & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)))
            return "interrupt transfer error";
        if (token & EHCI_TD_ACTIVE) break;
        __asm__ volatile ("mfence" : : : "memory");
        unsigned residual = (token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (residual > trackpad_endpoint.length) return "invalid interrupt residual";
        toggle = dma->qh.overlay.token & EHCI_TD_DATA1;
        record_trackpad_frame(capture, path->dma->data, trackpad_endpoint.length - residual);
        if (trackpad_capture_complete(capture)) return NULL;
    }
    return "gesture capture timeout";
}

static void run_trackpad_trial(struct ehci_control_path *path, uint64_t *frame_list) {
    struct ehci_probe_report *report = path->report;
    struct ehci_trackpad_capture *capture = &report->trackpad;
    capture->result = "captured trackpad interfaces unavailable";
    if (!report->trackpad_target.address) return;
    const struct usb_setup_packet configuration = { 0, USB_REQ_SET_CONFIGURATION, 1, 0, 0 };
    unsigned received;
    capture->result = "SET_CONFIGURATION failed";
    if (!control_transfer(path, &report->trackpad_target, &configuration, NULL, &received)) return;
    uint8_t original[8], mode[8];
    capture->result = "feature mode read failed";
    if (!read_trackpad_mode(path, original)) return;
    capture->mode_before_read = true;
    capture->mode_before = original[0];
    capture->result = activate_trackpad_mode(path, original);
    if (!capture->result) {
        capture->result = configure_periodic_frames(path, &trackpad_endpoint, frame_list);
        if (!capture->result) {
            console_set_framebuffer_muted(false);
            console_write("Trackpad ready. Start the checklist now. You have 60 seconds.\n");
            console_sync();
            capture->result = poll_trackpad(path);
        }
    }
    capture->mode_restored = write_trackpad_mode(path, original) &&
        read_trackpad_mode(path, mode) && mode[0] == original[0];
}
