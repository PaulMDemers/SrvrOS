/* Captured MacBookPro8,1 Wellspring5. Ubuntu's EHCI queue polls every frame. */
#define EHCI_TRACKPAD_TYPE 2u
#define EHCI_TRACKPAD_REPORT_MAX (30u + 28u * APPLE_TRACKPAD_CONTACTS)
#define EHCI_TRACKPAD_WAIT_US 60000000u
#define EHCI_TRACKPAD_MOUSE_WAIT_US 15000000u
#define EHCI_TRACKPAD_MODE_RETRY_US 1000u
#define EHCI_TRACKPAD_POLL_US 250u
#define EHCI_TRACKPAD_MOVE_UNITS 32
#define EHCI_TRACKPAD_FEATURE_READ_OFFSET_US 247973u
#define EHCI_TRACKPAD_NORMAL_MODE 8u
#define EHCI_TRACKPAD_WELLSPRING_MODE 1u

struct ehci_trackpad_slot {
    _Alignas(32) struct ehci_qtd td;
    uint8_t data[EHCI_TRACKPAD_REPORT_MAX];
};

struct ehci_trackpad_dma {
    _Alignas(32) struct ehci_qh qh;
    struct ehci_trackpad_slot slots[2];
};

_Static_assert(sizeof(struct ehci_trackpad_dma) <= EHCI_PAGE_BYTES, "trackpad queue fits one DMA page");
_Static_assert(EHCI_TRACKPAD_REPORT_MAX <= sizeof(((struct ehci_trackpad_capture *)0)->first_payload),
    "trackpad capture retains the whole first payload");

static const struct ehci_periodic_endpoint trackpad_endpoint = { 0x81, 64, 1, EHCI_TRACKPAD_REPORT_MAX };
static const struct ehci_periodic_endpoint trackpad_mouse_endpoint = { 0x84, 8, 8, 8 };

/* Submission offsets from the same Ubuntu capture as the keyboard LED bytes. */
static const struct {
    unsigned offset_us;
    struct usb_setup_packet request;
    const char *failure;
} trackpad_hid_requests[] = {
    {   627, { 0x21, EHCI_HID_SET_IDLE, 0, 0, 0 }, "HID interface 0 SET_IDLE failed" },
    {   904, { 0x81, USB_REQ_GET_DESCRIPTOR, 0x2200, 0, 156 }, "HID interface 0 report descriptor failed" },
    {  7017, { 0x21, 9, 0x0201, 0, 2 }, "first keyboard output report failed" },
    { 61657, { 0x21, 9, 0x0201, 0, 2 }, "second keyboard output report failed" },
    { 82006, { 0x21, EHCI_HID_SET_IDLE, 0, 1, 0 }, "HID interface 1 SET_IDLE failed" },
    { 84321, { 0x81, USB_REQ_GET_DESCRIPTOR, 0x2200, 1, 27 }, "HID interface 1 report descriptor failed" },
};
enum { EHCI_TRACKPAD_HID_REQUEST_COUNT = sizeof(trackpad_hid_requests) / sizeof(trackpad_hid_requests[0]) };

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
                if (mouse || entry[2] != trackpad_mouse_endpoint.address || entry[4] != trackpad_mouse_endpoint.packet ||
                    entry[6] != trackpad_mouse_endpoint.interval) return false;
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

static bool wait_trackpad_hid_offset(uint64_t configured_at, unsigned offset_us) {
    uint64_t elapsed = timer_monotonic_us() - configured_at;
    return elapsed >= offset_us || timer_delay_us(offset_us - elapsed);
}

static const char *replay_trackpad_hid(struct ehci_control_path *path, uint64_t configured_at) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    const uint8_t keyboard_output[] = { 0x01, 0x00 };
    uint8_t descriptor[EHCI_DESCRIPTOR_BYTES];
    capture->hid_replay_started = true;
    for (unsigned i = 0; i < EHCI_TRACKPAD_HID_REQUEST_COUNT; i++) {
        if (!wait_trackpad_hid_offset(configured_at, trackpad_hid_requests[i].offset_us))
            return "HID replay timer failed";
        const struct usb_setup_packet *request = &trackpad_hid_requests[i].request;
        bool input = request->request_type & USB_ENDPOINT_IN;
        unsigned received;
        if (!control_transfer_payload(path, &path->report->trackpad_target, request,
            input ? descriptor : NULL, !input && request->length ? keyboard_output : NULL, &received, NULL) ||
            received != request->length) return trackpad_hid_requests[i].failure;
        capture->hid_replay_completed++;
    }
    return NULL;
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

static uint64_t trackpad_report_word(const uint8_t data[8]) {
    uint64_t sample = 0;
    for (unsigned i = 0; i < 8; i++) sample = (sample << 8) | data[i];
    return sample;
}

static void record_trackpad_payload(struct ehci_trackpad_endpoint_trace *trace,
    const uint8_t *data, unsigned length, uint64_t start) {
    trace->completions++;
    if (length > 8) return;
    trace->short_lengths[length]++;
    if (trace->sample_count == EHCI_TRACKPAD_PAYLOAD_SAMPLES) return;
    uint8_t bytes[8] = {0};
    for (unsigned i = 0; i < length; i++) bytes[i] = data[i];
    uint64_t word = trackpad_report_word(bytes);
    if (trace->sample_count) {
        const struct ehci_trackpad_payload_sample *last = &trace->samples[trace->sample_count - 1];
        if (last->length == length && last->bytes == word) return;
    }
    trace->samples[trace->sample_count++] = (struct ehci_trackpad_payload_sample) {
        .bytes = word, .elapsed_us = timer_monotonic_us() - start,
        .completion = trace->completions, .length = length,
    };
}

static void record_trackpad_frame(struct ehci_trackpad_capture *capture, const uint8_t *data, unsigned length) {
    struct apple_trackpad_frame frame;
    int decoded = apple_trackpad_decode_type(EHCI_TRACKPAD_TYPE, data, length, &frame);
    capture->bytes = length;
    if (length > capture->wire_max) capture->wire_max = length;
    if (length == 8) {
        capture->eight_byte_reports++;
        capture->last_eight_byte_id = data[0];
        capture->last_eight_byte_sample = trackpad_report_word(data);
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

static void park_trackpad_qtd(struct ehci_qtd *td) {
    *td = (struct ehci_qtd) { .next = EHCI_LINK_END, .alternate = EHCI_LINK_END, .token = EHCI_TD_HALTED };
}

static void publish_trackpad_qtd(struct ehci_qtd *td, uint32_t next, uint32_t buffer, unsigned length) {
    /* An inactive dummy with a terminating alternate link is visible to the
     * controller. Publish its buffer and new dummy before setting Active. */
    td->next = next;
    td->buffer[0] = buffer;
    __asm__ volatile ("mfence" : : : "memory");
    *(volatile uint32_t *)&td->token = (length << EHCI_TD_BYTES_SHIFT) |
        EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_IOC | EHCI_TD_ACTIVE;
}

static void prepare_trackpad_queue(struct ehci_trackpad_dma *dma, uint32_t physical,
    const struct ehci_control_target *target, const struct ehci_periodic_endpoint *endpoint) {
    *dma = (struct ehci_trackpad_dma) {0};
    uint32_t first = physical + offsetof(struct ehci_trackpad_dma, slots);
    prepare_periodic_qh(&dma->qh, target, endpoint, first, 0);
    park_trackpad_qtd(&dma->slots[0].td);
    park_trackpad_qtd(&dma->slots[1].td);
    publish_trackpad_qtd(&dma->slots[0].td, first + sizeof(dma->slots[0]),
        first + offsetof(struct ehci_trackpad_slot, data), endpoint->length);
}

static void sample_trackpad_queue(struct ehci_trackpad_queue_sample *sample, const struct ehci_control_path *path,
    const volatile struct ehci_qh *qh, uint32_t td_token, uint32_t overlay_token) {
    unsigned frame = path->op[EHCI_FRINDEX] & EHCI_FRINDEX_MASK;
    if (!sample->polls++) sample->first_frame = frame;
    sample->last_frame = frame;
    sample->td_token = td_token;
    sample->overlay_token = overlay_token;
    sample->current = qh->current & ~31u;
    sample->next = qh->overlay.next;
    sample->c_progress |= qh->overlay.buffer[1] & 0xffu;
    sample->split_seen |= (overlay_token & EHCI_TD_SPLIT_STATE) != 0;
}

static bool trackpad_queue_parked(const volatile struct ehci_trackpad_dma *dma,
    unsigned dummy, uint32_t retired_address, uint32_t dummy_address) {
    return !(dma->qh.overlay.token & (EHCI_TD_ACTIVE | EHCI_TD_ERROR | EHCI_TD_SPLIT_STATE)) &&
        (dma->qh.current & ~31u) == retired_address && dma->qh.overlay.next == dummy_address &&
        dma->slots[dummy].td.token == EHCI_TD_HALTED;
}

static const char *retry_trackpad_mode(struct ehci_control_path *path, uint64_t *retry_completed_us, bool attended) {
    struct ehci_trackpad_recovery *recovery = &path->report->trackpad.recovery;
    recovery->attempted = true;
    if (!disable_periodic(path) || !schedules_idle(path)) return "queue pause for mode retry failed";
    if (attended) {
        console_write("Trackpad mode retry. Repeat the gestures now. You have 60 seconds.\n");
        console_sync();
    }

    /* Match bcm5974's read-modify-write in each direction. The native
     * control helper requires both schedules idle. Keep the periodic QH
     * parked at its inactive dummy until both mode writes have finished. */
    if (!read_trackpad_mode(path, recovery->normal)) return "normal mode feature read failed";
    recovery->mode_at_reset = recovery->normal[0];
    recovery->normal[0] = EHCI_TRACKPAD_NORMAL_MODE;
    uint64_t normal_completed_us;
    if (!write_trackpad_mode(path, recovery->normal, &normal_completed_us)) return "normal mode write failed";
    recovery->normal_written = true;
    if (!timer_delay_us(EHCI_TRACKPAD_MODE_RETRY_US)) return "mode retry wait failed";
    recovery->waited_us = timer_monotonic_us() - normal_completed_us;

    if (!read_trackpad_mode(path, recovery->wellspring)) return "Wellspring retry feature read failed";
    recovery->mode_before_retry = recovery->wellspring[0];
    recovery->wellspring[0] = EHCI_TRACKPAD_WELLSPRING_MODE;
    if (!write_trackpad_mode(path, recovery->wellspring, retry_completed_us)) return "Wellspring retry write failed";
    recovery->wellspring_written = true;
    recovery->switch_us = *retry_completed_us - normal_completed_us;
    return NULL;
}

/* The first request and frame list are ready before the mode write. A successful
 * control transfer has acknowledged async stop, so the next action can set PSE. */
static const char *poll_trackpad(struct ehci_control_path *path, struct ehci_trackpad_dma *dma,
    const struct ehci_periodic_endpoint *endpoint, uint64_t mode_completed_us) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    struct ehci_trackpad_recovery *recovery = &capture->recovery;
    bool gestures = endpoint == &trackpad_endpoint;
    struct ehci_trackpad_endpoint_trace *trace = gestures ? &capture->raw : &capture->mouse_raw;
    unsigned timeout = gestures ? EHCI_TRACKPAD_WAIT_US : EHCI_TRACKPAD_MOUSE_WAIT_US;
    uint32_t first_td = dma->qh.overlay.next;
    const volatile struct ehci_trackpad_dma *progress = dma;
    if (gestures) {
        capture->arm_requested = true;
        capture->submissions = 1;
        capture->mode_to_arm_us = timer_monotonic_us() - mode_completed_us;
    }
    bool enabled = enable_periodic(path);
    uint64_t start = timer_monotonic_us();
    if (gestures) {
        capture->arm_acknowledged = enabled;
        if (enabled) capture->mode_to_active_us = start - mode_completed_us;
    }
    const char *failure = enabled ? NULL : "periodic enable timeout";
    unsigned slot = 0;
    uint64_t retry_completed_us = 0;
    while (!failure) {
        uint32_t token = progress->slots[slot].td.token;
        uint32_t status = path->op[EHCI_USBSTS];
        if (gestures) {
            capture->token = token;
            capture->overlay = progress->qh.overlay.token;
            capture->status = status;
            if (capture->completions < EHCI_TRACKPAD_TRACE_COUNT) sample_trackpad_queue(
                &capture->first_requests[capture->completions], path, &progress->qh, token, capture->overlay);
        }
        if ((token & EHCI_TD_ERROR) || (status & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR))) {
            failure = "interrupt transfer error";
            break;
        }
        if (!(status & EHCI_STS_PERIODIC) || !(path->op[EHCI_USBCMD] & EHCI_CMD_PERIODIC)) {
            failure = "running periodic schedule lost";
            break;
        }
        if (timer_monotonic_us() - start >= timeout) {
            if (gestures) {
                capture->timed_out = true;
                failure = "gesture capture timeout";
            }
            break;
        }
        if (token & EHCI_TD_ACTIVE) {
            if (!timer_delay_us(EHCI_TRACKPAD_POLL_US)) failure = "capture timer failure";
            continue;
        }

        unsigned residual = (token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (residual > endpoint->length) {
            failure = "invalid interrupt residual";
            break;
        }
        __asm__ volatile ("mfence" : : : "memory");
        unsigned length = endpoint->length - residual;
        const uint8_t *data = dma->slots[slot].data;
        if (gestures) {
            if (!capture->completions++) {
                capture->mode_to_first_completion_us = timer_monotonic_us() - mode_completed_us;
                capture->first_payload_length = length;
                for (unsigned byte = 0; byte < length; byte++) capture->first_payload[byte] = data[byte];
            }
            record_trackpad_frame(capture, data, length);
            if (recovery->resumed && capture->reports == 1 && !recovery->first_report_length) {
                recovery->first_report_length = length;
                recovery->first_report_prefix = trackpad_report_word(data);
                recovery->first_report_us = timer_monotonic_us() - retry_completed_us;
            }
        }
        record_trackpad_payload(trace, data, length, start);
        if (gestures && trackpad_capture_complete(capture)) break;

        unsigned dummy = slot ^ 1u;
        uint32_t retired_address = first_td + slot * sizeof(dma->slots[0]);
        uint32_t dummy_address = first_td + dummy * sizeof(dma->slots[0]);
        if (!trackpad_queue_parked(progress, dummy, retired_address, dummy_address)) {
            failure = "queue did not park at the inactive dummy";
            break;
        }
        bool retry = gestures && length == 8 && !capture->reports && !recovery->attempted;
        if (retry) {
            failure = retry_trackpad_mode(path, &retry_completed_us, true);
            if (failure) break;
            if (!trackpad_queue_parked(progress, dummy, retired_address, dummy_address)) {
                failure = "parked queue changed during mode retry";
                break;
            }
        }
        /* The retired request becomes the new dummy. The live overlay and
         * its data toggle remain controller-owned throughout resubmission. */
        park_trackpad_qtd(&dma->slots[slot].td);
        publish_trackpad_qtd(&dma->slots[dummy].td, retired_address,
            dummy_address + offsetof(struct ehci_trackpad_slot, data), endpoint->length);
        slot = dummy;
        if (gestures) capture->submissions++;
        if (retry) {
            if (!enable_periodic(path)) {
                failure = "periodic resume after mode retry failed";
                break;
            }
            start = timer_monotonic_us();
            recovery->resumed = true;
            recovery->mode_to_active_us = start - retry_completed_us;
            trace = &recovery->raw;
        }
    }
    return disable_periodic(path) ? failure : "periodic stop timeout";
}

static const char *configure_trackpad(struct ehci_control_path *path, uint64_t *frame_list,
    uint64_t *transfer_page, uint64_t *configured_at) {
    struct ehci_probe_report *report = path->report;
    struct ehci_trackpad_capture *capture = &report->trackpad;
    if (!report->trackpad_target.address) return "captured trackpad interfaces unavailable";
    if (report->keyboard_target.address != report->trackpad_target.address)
        return "captured keyboard interface unavailable for HID replay";
    const struct usb_setup_packet configuration = { 0, USB_REQ_SET_CONFIGURATION, 1, 0, 0 };
    unsigned received;
    if (!control_transfer_payload(path, &report->trackpad_target, &configuration, NULL, NULL, &received,
        configured_at)) return "SET_CONFIGURATION failed";
    const char *failure = replay_trackpad_hid(path, *configured_at);
    if (failure) return failure;
    if (!wait_trackpad_hid_offset(*configured_at, EHCI_TRACKPAD_FEATURE_READ_OFFSET_US))
        return "HID replay feature-read timer failed";
    capture->configuration_to_feature_read_us = timer_monotonic_us() - *configured_at;
    if (!read_trackpad_mode(path, capture->feature_before)) return "feature mode read failed";
    capture->mode_before_read = true;
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        return "trackpad interrupt DMA unavailable";
    }
    *transfer_page = physical;
    struct ehci_trackpad_dma *dma = pmm_phys_to_virt(physical);
    prepare_trackpad_queue(dma, (uint32_t)physical, &report->trackpad_target, &trackpad_endpoint);
    capture->qh_endpoint = dma->qh.endpoint;
    capture->qh_capabilities = dma->qh.capabilities;
    capture->first_td = dma->qh.overlay.next;
    uint32_t qh = (uint32_t)physical + offsetof(struct ehci_trackpad_dma, qh);
    return configure_periodic_frames(path, &trackpad_endpoint, qh, frame_list);
}

static bool select_trackpad_wellspring(struct ehci_control_path *path, uint64_t *completed_us) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    for (unsigned i = 0; i < sizeof(capture->feature_write); i++) capture->feature_write[i] = capture->feature_before[i];
    capture->feature_write[0] = EHCI_TRACKPAD_WELLSPRING_MODE;
    capture->mode_write_attempted = true;
    return write_trackpad_mode(path, capture->feature_write, completed_us);
}

static bool restore_trackpad_mode(struct ehci_control_path *path) {
    struct ehci_trackpad_capture *capture = &path->report->trackpad;
    capture->mode_restore_read = write_trackpad_mode(path, capture->feature_before, NULL) &&
        read_trackpad_mode(path, capture->feature_restored);
    capture->mode_restored = capture->mode_restore_read &&
        trackpad_report_word(capture->feature_restored) == trackpad_report_word(capture->feature_before);
    return capture->mode_restored;
}

static void run_trackpad_trial(struct ehci_control_path *path, uint64_t *frame_list, uint64_t *transfer_page) {
    struct ehci_probe_report *report = path->report;
    struct ehci_trackpad_capture *capture = &report->trackpad;
    uint64_t configured_at;
    capture->result = configure_trackpad(path, frame_list, transfer_page, &configured_at);
    if (capture->result) return;
    uint32_t physical = (uint32_t)*transfer_page;
    struct ehci_trackpad_dma *dma = pmm_phys_to_virt(physical);
    uint32_t qh = physical + offsetof(struct ehci_trackpad_dma, qh);

    console_set_framebuffer_muted(false);
    console_write("Trackpad capture starting. Start the checklist now. You have 60 seconds.\n");
    console_sync();
    uint64_t mode_completed_us;
    if (select_trackpad_wellspring(path, &mode_completed_us)) {
        capture->result = poll_trackpad(path, dma, &trackpad_endpoint, mode_completed_us);
        capture->configuration_to_mode_us = mode_completed_us - configured_at;
        if (schedules_idle(path) && read_trackpad_mode(path, capture->feature_after)) {
            capture->mode_after_read = true;
        } else if (!capture->result) capture->result = "post-capture feature readback failed";
        if (capture->mode_after_read && capture->feature_after[0] == EHCI_TRACKPAD_WELLSPRING_MODE &&
            (!capture->result || capture->timed_out) && schedules_idle(path)) {
            capture->mouse_requested = true;
            console_write("Mouse endpoint check. Repeat the gestures now. You have 15 seconds.\n");
            console_sync();
            prepare_trackpad_queue(dma, (uint32_t)physical, &report->trackpad_target, &trackpad_mouse_endpoint);
            prepare_periodic_frames(pmm_phys_to_virt(*frame_list), qh, trackpad_mouse_endpoint.interval);
            __asm__ volatile ("mfence" : : : "memory");
            capture->mouse_result = poll_trackpad(path, dma, &trackpad_mouse_endpoint, mode_completed_us);
        }
    } else capture->result = "feature mode write failed";
    restore_trackpad_mode(path);
}
