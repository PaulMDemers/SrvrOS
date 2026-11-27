#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../kernel/src/drivers/ehci.c"

#define LEGACY_OFFSET 0x68
#define CAP_LENGTH 0x20
#define LEGACY_EVENT (1u << 29)

static struct pci_device device;
static uint32_t pci_config[64];
static uint32_t mmio[64];
static volatile uint32_t *const op = mmio + CAP_LENGTH / 4;
static unsigned handoff_delays, halt_delays, reset_delays, os_writes, smi_writes, command_writes;
static bool firmware_stuck, halt_stuck, reset_stuck;
static struct ehci_dma *control_dma;
static unsigned control_polls, control_completes_at;
static bool mock_flash;
static bool mock_feature_control;
static bool mock_short_hid_descriptor;
static uint8_t mock_feature_readback;
static unsigned trackpad_control_requests, trackpad_dma_allocations, trackpad_prompt_syncs;
static uint64_t trackpad_configured_at;
static uint32_t trackpad_frames[EHCI_FRAME_ENTRIES];
static const uint8_t feature_bytes[] = {1, 7, 6, 5, 4, 3, 2, 1};
static struct ehci_dma *keyboard_dma;
static struct ehci_trackpad_dma *trackpad_dma;
static unsigned trackpad_reports, trackpad_offset, trackpad_packets;
static unsigned trackpad_queue_starts, trackpad_queue_stops;
static unsigned trackpad_mouse_reports;
static bool mock_trackpad_dummy_overlay;
static bool mock_trackpad_recovery, mock_trackpad_retry_wait_fails;
static bool mock_desktop_mode_switch, mock_desktop_first_write_fails;
static uint64_t trackpad_normal_at;
static unsigned keyboard_packet_bytes;
static bool keyboard_stop_stuck;
static bool keyboard_fetch;
static uint64_t now;
static unsigned freed_monitor_frames, unmapped_monitor_windows;
static struct ehci_flash_io *mock_bulk;
static bool mock_bulk_timeout;
static unsigned injected_trackpad_events;
static int32_t trackpad_dx, trackpad_dy;
static uint8_t trackpad_buttons;

void console_write(const char *message) { assert(message); }

void mouse_trackpad_get_settings(unsigned *speed, unsigned *options) {
    *speed = 3;
    *options = 0;
}

void mouse_inject_trackpad(int32_t dx, int32_t dy, uint8_t buttons) {
    injected_trackpad_events++;
    trackpad_dx = dx;
    trackpad_dy = dy;
    trackpad_buttons = buttons;
}

void mouse_cancel_trackpad(void) {
    trackpad_buttons = 0;
}

void console_set_framebuffer_muted(bool muted) { assert(!muted); }

bool console_sync(void) {
    unsigned expected = 8;
    if (trackpad_prompt_syncs) {
        expected = mock_trackpad_recovery ? (trackpad_prompt_syncs == 1 ? 9u : 14u) : 10u;
    }
    assert(trackpad_control_requests == expected && op[EHCI_PERIODICLISTBASE] == 0xb000);
    trackpad_prompt_syncs++;
    return true;
}

uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA && trackpad_dma && trackpad_control_requests == 8 && trackpad_dma_allocations < 2);
    return 0xa000 + trackpad_dma_allocations++ * EHCI_PAGE_BYTES;
}

void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical == 0xa000 || physical == 0xb000);
    return physical == 0xa000 ? (void *)trackpad_dma : (void *)trackpad_frames;
}

void pmm_free_frame(uint64_t physical) {
    assert(op[EHCI_USBSTS] == EHCI_STS_HALTED);
    assert(!op[EHCI_ASYNCLISTADDR] && !op[EHCI_PERIODICLISTBASE] && !op[EHCI_CONFIGFLAG]);
    assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));
    assert(physical == (freed_monitor_frames ? 0xa000u : 0x9000u));
    freed_monitor_frames++;
}

bool vmm_unmap_page_in_space(uint64_t root, uint64_t address, uint64_t *physical) {
    assert(root == 0x1000 && address == EHCI_MMIO_WINDOW);
    assert(!pci_config[PCI_COMMAND / 4] && freed_monitor_frames == 2);
    *physical = 0xfed00000;
    unmapped_monitor_windows++;
    return true;
}

uint16_t pci_read_config16(const struct pci_device *p, uint8_t offset) {
    assert(p == &device && offset == PCI_COMMAND);
    return pci_config[offset / 4];
}

void pci_write_config16(const struct pci_device *p, uint8_t offset, uint16_t value) {
    assert(p == &device && offset == PCI_COMMAND && !(value & PCI_COMMAND_MASTER));
    command_writes++;
    pci_config[offset / 4] = value;
}

uint32_t pci_read_config32(const struct pci_device *p, uint8_t offset) {
    assert(p == &device && !(offset & 3));
    return pci_config[offset / 4];
}

void pci_write_config8(const struct pci_device *p, uint8_t offset, uint8_t value) {
    assert(p == &device && offset == LEGACY_OFFSET + 3 && value <= 1);
    os_writes++;
    uint32_t *legacy = &pci_config[LEGACY_OFFSET / 4];
    *legacy = (*legacy & ~EHCI_LEGACY_OS) | ((uint32_t)value << 24);
}

void pci_write_config32(const struct pci_device *p, uint8_t offset, uint32_t value) {
    assert(p == &device && offset == LEGACY_OFFSET + 4 && value == 0);
    assert((pci_config[LEGACY_OFFSET / 4] & (EHCI_LEGACY_BIOS | EHCI_LEGACY_OS)) == EHCI_LEGACY_OS);
    smi_writes++;
    pci_config[offset / 4] &= ~EHCI_LEGACY_SMI_ENABLES;
}

bool timer_delay_us(uint64_t delay) {
    now += delay;
    if (mock_desktop_mode_switch) {
        uint32_t cmd = op[EHCI_USBCMD];
        op[EHCI_USBSTS] = (cmd & EHCI_CMD_PERIODIC ? EHCI_STS_PERIODIC : 0) |
            (cmd & EHCI_CMD_ASYNC ? EHCI_STS_ASYNC : 0);
        if (!(cmd & EHCI_CMD_ASYNC)) return true;
        assert(!(cmd & EHCI_CMD_PERIODIC) && control_dma->qh.endpoint == 0x0808c003u);
        assert(control_dma->setup.value == 0x0300 && !control_dma->setup.index && control_dma->setup.length == 8);
        unsigned step = trackpad_control_requests++;
        bool fail = step == 3 && mock_desktop_first_write_fails;
        if (!(step & 1u)) {
            assert(control_dma->setup.request_type == 0xa1 && control_dma->setup.request == 1);
            memcpy(control_dma->data, feature_bytes, sizeof(feature_bytes));
            control_dma->data[0] = mock_feature_readback;
        } else {
            assert(control_dma->setup.request_type == 0x21 && control_dma->setup.request == 9);
            assert(control_dma->data[0] == ((step & 2u) ? 1 : 8));
            assert(!memcmp(control_dma->data + 1, feature_bytes + 1, sizeof(feature_bytes) - 1));
            if (!fail) mock_feature_readback = control_dma->data[0];
        }
        control_dma->setup_td.token = control_dma->data_td.token = 0;
        control_dma->status_td.token = fail ? EHCI_TD_HALTED : 0;
        return true;
    }
    if (mock_bulk) {
        if (!(op[EHCI_USBCMD] & EHCI_CMD_ASYNC)) {
            op[EHCI_USBSTS] &= ~EHCI_STS_ASYNC;
            return true;
        }
        op[EHCI_USBSTS] |= EHCI_STS_ASYNC;
        if (mock_bulk_timeout) return true;
        struct ehci_dma *dma = mock_bulk->path->dma;
        bool in = (dma->setup_td.token & EHCI_TD_PID_IN) != 0;
        assert(dma->qh.endpoint == (in ? 0x0200a102u : 0x0200a202u));
        assert(dma->qh.overlay.token == mock_bulk->endpoint_state[in]);
        assert(dma->setup_td.buffer[0] == mock_bulk->physical);
        assert(dma->setup_td.alternate == EHCI_LINK_END);
        uint32_t length = (dma->setup_td.token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (in) {
            assert(length == 32);
            memset(mock_bulk->buffer, 0x5a, 13);
            dma->setup_td.token = 19 << EHCI_TD_BYTES_SHIFT;
        } else {
            assert(length == 31 && mock_bulk->buffer[0] == 0x55 && mock_bulk->buffer[30] == 0xaa);
            dma->setup_td.token = 0;
        }
        dma->qh.overlay.token ^= EHCI_TD_DATA1;
        return true;
    }
    if (trackpad_dma && ((op[EHCI_USBCMD] & EHCI_CMD_PERIODIC) || (op[EHCI_USBSTS] & EHCI_STS_PERIODIC))) {
        if (!(op[EHCI_USBCMD] & EHCI_CMD_PERIODIC)) {
            trackpad_queue_stops++;
            op[EHCI_USBSTS] &= ~EHCI_STS_PERIODIC;
            return true;
        }
        bool mouse = trackpad_dma->qh.endpoint == 0x00080403;
        if (!(op[EHCI_USBSTS] & EHCI_STS_PERIODIC)) {
            unsigned expected = mouse ? 10u : 9u;
            unsigned prompts = mouse ? 2u : 1u;
            if (mock_trackpad_recovery && trackpad_queue_starts) {
                expected = mouse ? 14u : 13u;
                prompts = mouse ? 3u : 2u;
            }
            assert(trackpad_control_requests == expected && trackpad_prompt_syncs == prompts);
            assert(!(op[EHCI_USBSTS] & EHCI_STS_ASYNC));
            if (!trackpad_queue_starts || mouse) assert(!trackpad_dma->qh.overlay.token);
            trackpad_queue_starts++;
        }
        op[EHCI_USBSTS] |= EHCI_STS_PERIODIC;
        if (delay != EHCI_TRACKPAD_POLL_US) return true;
        if (mouse && trackpad_mouse_reports == 2) {
            now += EHCI_TRACKPAD_MOUSE_WAIT_US;
            return true;
        }
        assert(mouse || trackpad_reports < (mock_trackpad_recovery ? 7u : 11u));
        unsigned index = (mouse ? trackpad_mouse_reports : trackpad_reports) & 1u;
        struct ehci_trackpad_slot *slot = &trackpad_dma->slots[index];
        struct ehci_trackpad_slot *dummy = &trackpad_dma->slots[index ^ 1u];
        uint32_t first = 0xa000u + offsetof(struct ehci_trackpad_dma, slots);
        uint32_t address = first + index * sizeof(*slot);
        uint32_t dummy_address = first + (index ^ 1u) * sizeof(*slot);
        assert(slot->td.token & EHCI_TD_ACTIVE);
        assert(slot->td.next == dummy_address && slot->td.alternate == EHCI_LINK_END);
        assert(slot->td.buffer[0] == address + offsetof(struct ehci_trackpad_slot, data));
        assert(dummy->td.token == EHCI_TD_HALTED && dummy->td.next == EHCI_LINK_END);
        if (mouse) {
            assert((slot->td.token >> EHCI_TD_BYTES_SHIFT) == 8);
            const uint8_t sample[8] = {2, 1, 2, 3, 4, 5, 6, 7};
            unsigned length = trackpad_mouse_reports++ ? 8 : 4;
            memcpy(slot->data, sample, length);
            slot->td.token = ((8 - length) << EHCI_TD_BYTES_SHIFT) | EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_IOC;
            trackpad_dma->qh.current = address;
            trackpad_dma->qh.overlay.next = dummy_address;
            trackpad_dma->qh.overlay.token = slot->td.token;
            return true;
        }
        if (!trackpad_offset) {
            assert(trackpad_dma->qh.overlay.next == address);
            assert((trackpad_dma->qh.overlay.token & EHCI_TD_DATA1) == (trackpad_packets % 2 ? EHCI_TD_DATA1 : 0));
            if (trackpad_reports) assert(trackpad_dma->qh.current == dummy_address);
        }
        op[EHCI_FRINDEX] += 2;
        trackpad_dma->qh.current = address;
        trackpad_dma->qh.overlay.next = slot->td.next;
        trackpad_dma->qh.overlay.buffer[1] = 4;
        uint8_t report[86] = {0x74, [85] = 0xb7};
        /* First six requests use seven packets, then finish the gesture checklist. */
        int gesture = (int)trackpad_reports - (mock_trackpad_recovery ? 2 : 6);
        unsigned length = gesture == -6 ? 86 : gesture < 0 ? 2 : gesture == 2 ? 86 : 58;
        if (mock_trackpad_recovery && gesture < 0) {
            report[0] = gesture == -2 ? 0x60 : 2;
            report[1] = gesture == -2 ? 2 : 0;
            length = gesture == -2 ? 2 : 8;
        }
        report[30 + 2] = gesture >= 1 ? 64 : 0;
        report[30 + 16] = 5;
        if (gesture == 2) report[58 + 16] = 5;
        if (gesture == 3) report[15] = 1;
        unsigned packet = length - trackpad_offset;
        if (packet > trackpad_endpoint.packet) packet = trackpad_endpoint.packet;
        memcpy(slot->data + trackpad_offset, report + trackpad_offset, packet);
        trackpad_offset += packet;
        trackpad_packets++;
        trackpad_dma->qh.overlay.token ^= EHCI_TD_DATA1;
        if (trackpad_offset == length) {
            slot->td.token = ((trackpad_endpoint.length - length) << EHCI_TD_BYTES_SHIFT) |
                EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_IOC | (trackpad_dma->qh.overlay.token & EHCI_TD_DATA1);
            trackpad_dma->qh.overlay.token = slot->td.token;
            trackpad_offset = 0;
            trackpad_reports++;
            if (mock_trackpad_dummy_overlay) {
                trackpad_dma->qh.overlay.next = EHCI_LINK_END;
                trackpad_dma->qh.overlay.token |= EHCI_TD_HALTED;
            }
        } else trackpad_dma->qh.overlay.token |= EHCI_TD_SPLIT_STATE;
        return true;
    }
    if (keyboard_dma) {
        if (op[EHCI_USBCMD] & EHCI_CMD_PERIODIC) {
            op[EHCI_USBSTS] |= EHCI_STS_PERIODIC;
            op[EHCI_FRINDEX] = (op[EHCI_FRINDEX] + 1) & EHCI_FRINDEX_MASK;
            if (delay == EHCI_KEYBOARD_POLL_US && keyboard_fetch) {
                keyboard_dma->qh.current = 0x9000 + offsetof(struct ehci_dma, setup_td);
                keyboard_dma->qh.overlay.buffer[1] = 0x04;
            }
            if (delay == EHCI_KEYBOARD_POLL_US && keyboard_packet_bytes) {
                keyboard_dma->setup_td.token = (EHCI_KEYBOARD_PACKET - keyboard_packet_bytes) << EHCI_TD_BYTES_SHIFT;
                keyboard_dma->qh.overlay.token ^= EHCI_TD_DATA1;
                keyboard_dma->data[2] = 4; /* HID usage A. */
            }
        } else if (!keyboard_stop_stuck) {
            op[EHCI_USBSTS] &= ~EHCI_STS_PERIODIC;
        }
        return true;
    }
    if (mock_feature_control && trackpad_control_requests >= 1 && trackpad_control_requests <= 7 &&
        !(op[EHCI_USBCMD] & EHCI_CMD_SCHEDULES) && !(op[EHCI_USBSTS] & EHCI_STS_SCHEDULES)) return true;
    if (mock_trackpad_recovery && delay == EHCI_TRACKPAD_MODE_RETRY_US) {
        assert(trackpad_control_requests == 11 && schedules_idle(&(struct ehci_control_path) { .op = op }));
        assert(now - trackpad_normal_at == delay + EHCI_POLL_US);
        return !mock_trackpad_retry_wait_fails;
    }
    if (delay == 1000) {
        handoff_delays++;
        if (!firmware_stuck && handoff_delays == 3) pci_config[LEGACY_OFFSET / 4] &= ~EHCI_LEGACY_BIOS;
        return true;
    }
    assert(delay == EHCI_POLL_US);
    if (control_dma) {
        if (op[EHCI_USBCMD] & EHCI_CMD_ASYNC) {
            op[EHCI_USBSTS] |= EHCI_STS_ASYNC;
            if (mock_feature_control) {
                assert(control_dma->qh.endpoint == 0x0808c003u);
                assert(control_dma->qh.capabilities == 0x41010000u);
                unsigned step = trackpad_control_requests++;
                unsigned residual = 0;
                unsigned after_capture = mock_trackpad_recovery ? (mock_trackpad_retry_wait_fails ? 11u : 13u) : 9u;
                bool mid_retry = mock_trackpad_recovery && step >= 9 && step < after_capture;
                if (!step) {
                    assert(!control_dma->setup.request_type && control_dma->setup.request == USB_REQ_SET_CONFIGURATION);
                    assert(control_dma->setup.value == 1 && !control_dma->setup.index && !control_dma->setup.length);
                    trackpad_configured_at = now;
                } else if (step <= 6) {
                    const struct usb_setup_packet expected[] = {
                        {0x21, 10, 0, 0, 0}, {0x81, 6, 0x2200, 0, 156},
                        {0x21, 9, 0x0201, 0, 2}, {0x21, 9, 0x0201, 0, 2},
                        {0x21, 10, 0, 1, 0}, {0x81, 6, 0x2200, 1, 27},
                    };
                    const unsigned offsets[] = {627, 904, 7017, 61657, 82006, 84321};
                    assert(now - trackpad_configured_at >= offsets[step - 1]);
                    assert(!memcmp(&control_dma->setup, &expected[step - 1], sizeof(control_dma->setup)));
                    if (step == 3 || step == 4) assert(control_dma->data[0] == 1 && control_dma->data[1] == 0);
                    if (step == 2 && mock_short_hid_descriptor) residual = 1;
                } else if (step == 8 || step == after_capture + 1 || (mid_retry && !(step & 1u))) {
                    assert(control_dma->setup.request_type == 0x21 && control_dma->setup.request == 9);
                    assert(control_dma->setup.value == 0x0300 && !control_dma->setup.index && control_dma->setup.length == 8);
                    assert(!memcmp(control_dma->data + 1, feature_bytes + 1, sizeof(feature_bytes) - 1));
                    assert(control_dma->data[0] == (step == 8 || (mid_retry && step == 12) ? 1 : 8));
                    mock_feature_readback = control_dma->data[0];
                    unsigned prompts = step == 8 || mock_trackpad_dummy_overlay ? 1 : 2;
                    if (mock_trackpad_recovery && step == after_capture + 1 && !mock_trackpad_retry_wait_fails) prompts = 3;
                    assert(trackpad_prompt_syncs == prompts && op[EHCI_PERIODICLISTBASE] == 0xb000);
                    if (step == 8) assert(trackpad_dma->slots[0].td.token & EHCI_TD_ACTIVE);
                    assert(control_dma->data_td.token == (EHCI_TD_DATA1 | (8u << EHCI_TD_BYTES_SHIFT) |
                        EHCI_TD_PID_OUT | EHCI_TD_RETRIES | EHCI_TD_ACTIVE));
                    assert((control_dma->status_td.token & EHCI_TD_PID_IN) &&
                        control_dma->data_td.alternate == EHCI_LINK_END);
                } else {
                    assert(step == 7 || step == after_capture || step == after_capture + 2 || mid_retry);
                    if (step == 7) assert(now - trackpad_configured_at >= 247973);
                    if (step == after_capture) {
                        unsigned reports = mock_trackpad_dummy_overlay ? 1u : 11u;
                        if (mock_trackpad_recovery) reports = mock_trackpad_retry_wait_fails ? 2u : 7u;
                        assert(trackpad_reports == reports && !(op[EHCI_USBSTS] & EHCI_STS_PERIODIC));
                    }
                    assert(control_dma->setup.request_type == 0xa1 && control_dma->setup.request == 1);
                    assert(control_dma->setup.value == 0x0300 && !control_dma->setup.index && control_dma->setup.length == 8);
                    memcpy(control_dma->data, feature_bytes, sizeof(feature_bytes));
                    control_dma->data[0] = mock_feature_readback;
                }
                if (mid_retry) {
                    uint32_t first = 0xa000u + offsetof(struct ehci_trackpad_dma, slots);
                    assert(!(op[EHCI_USBSTS] & EHCI_STS_PERIODIC) && trackpad_queue_stops == 1);
                    assert(trackpad_queue_parked(trackpad_dma, 0, first + sizeof(trackpad_dma->slots[0]), first));
                    if (step == 10) trackpad_normal_at = now;
                    if (step == 11) assert(now - trackpad_normal_at >= EHCI_TRACKPAD_MODE_RETRY_US);
                }
                control_dma->setup_td.token = control_dma->status_td.token = 0;
                control_dma->data_td.token = residual << EHCI_TD_BYTES_SHIFT;
                return true;
            }
            if (mock_flash) {
                assert(control_dma->qh.endpoint == 0x0040e000u);
                assert(control_dma->qh.capabilities == 0x40000000u);
                control_dma->setup_td.token = 0;
                control_dma->qh.overlay.token = control_dma->data_td.token;
            } else {
                assert(control_dma->qh.endpoint == 0x0810c005u);
                assert(control_dma->qh.capabilities == 0x41820000u);
                assert(!control_dma->qh.overlay.token);
            }
            if (++control_polls == control_completes_at) {
                unsigned bytes = mock_flash ? 18 : 17;
                control_dma->setup_td.token = 0;
                control_dma->data_td.token = (control_dma->setup.length - bytes) << EHCI_TD_BYTES_SHIFT;
                control_dma->status_td.token = 0;
                memset(control_dma->data, 0x5a, bytes);
            }
        } else {
            op[EHCI_USBSTS] &= ~EHCI_STS_ASYNC;
        }
        return true;
    }
    if (op[EHCI_USBCMD] & EHCI_CMD_RESET) {
        assert((op[EHCI_USBSTS] & EHCI_STS_HALTED) && op[EHCI_USBINTR] == 0);
        assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));
        reset_delays++;
        if (!reset_stuck && reset_delays == 3) {
            op[EHCI_USBCMD] = 0;
            op[EHCI_USBSTS] = EHCI_STS_HALTED;
            op[EHCI_CONFIGFLAG] = 0;
        }
    } else {
        assert(!(op[EHCI_USBCMD] & EHCI_CMD_RUN));
        halt_delays++;
        if (!halt_stuck && halt_delays == 2) op[EHCI_USBSTS] |= EHCI_STS_HALTED;
    }
    return true;
}

uint64_t timer_monotonic_us(void) { return now; }

static void fixture_reset(void) {
    memset(pci_config, 0, sizeof(pci_config));
    memset(mmio, 0, sizeof(mmio));
    pci_config[PCI_COMMAND / 4] = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_config[LEGACY_OFFSET / 4] = EHCI_LEGACY_CAP | EHCI_LEGACY_BIOS;
    pci_config[(LEGACY_OFFSET + 4) / 4] = EHCI_LEGACY_SMI_ENABLES | LEGACY_EVENT;
    mmio[0] = (EHCI_VERSION_1_0 << 16) | CAP_LENGTH;
    mmio[1] = 6;
    mmio[2] = LEGACY_OFFSET << 8;
    op[EHCI_USBCMD] = EHCI_CMD_RUN | EHCI_CMD_SCHEDULES;
    op[EHCI_USBSTS] = EHCI_STS_SCHEDULES;
    op[EHCI_USBINTR] = 0x3f;
    op[EHCI_CONFIGFLAG] = 1;
    handoff_delays = halt_delays = reset_delays = os_writes = smi_writes = command_writes = 0;
    firmware_stuck = halt_stuck = reset_stuck = false;
    control_dma = 0;
    control_polls = 0;
    control_completes_at = 2;
    mock_flash = false;
    mock_bulk = NULL;
    mock_bulk_timeout = false;
    mock_feature_control = false;
    mock_short_hid_descriptor = false;
    trackpad_dma = NULL;
    trackpad_reports = trackpad_offset = trackpad_packets = 0;
    trackpad_queue_starts = trackpad_queue_stops = 0;
    trackpad_mouse_reports = 0;
    mock_trackpad_dummy_overlay = false;
    mock_trackpad_recovery = mock_trackpad_retry_wait_fails = false;
    mock_desktop_mode_switch = mock_desktop_first_write_fails = false;
    trackpad_normal_at = 0;
    trackpad_control_requests = trackpad_dma_allocations = trackpad_prompt_syncs = 0;
    trackpad_configured_at = 0;
    keyboard_dma = 0;
    keyboard_packet_bytes = 0;
    keyboard_stop_stuck = false;
    keyboard_fetch = true;
    now = 0;
}

static void test_completed_probe(void) {
    fixture_reset();
    struct ehci_probe_report report = {0};
    assert(!strcmp(probe_registers(&device, mmio, sizeof(mmio), &report), "reset-halted"));
    assert(report.was_running && report.version == EHCI_VERSION_1_0 && report.ports == 6);
    assert(!strcmp(report.handoff, "os-owned"));
    assert(handoff_delays == 3 && halt_delays == 2 && reset_delays == 3);
    assert(os_writes == 1 && smi_writes == 1 && command_writes == 1);
    assert(pci_config[(LEGACY_OFFSET + 4) / 4] == LEGACY_EVENT);
}

static void test_firmware_retains_controller(void) {
    fixture_reset();
    firmware_stuck = true;
    struct ehci_probe_report report = {0};
    uint32_t original[sizeof(mmio) / sizeof(mmio[0])];
    memcpy(original, mmio, sizeof(mmio));
    assert(!strcmp(probe_registers(&device, mmio, sizeof(mmio), &report), "handoff-refused"));
    assert(!strcmp(report.handoff, "bios-timeout"));
    assert(handoff_delays == EHCI_HANDOFF_POLLS && os_writes == 2 && !smi_writes && !command_writes);
    assert(pci_config[LEGACY_OFFSET / 4] == (EHCI_LEGACY_CAP | EHCI_LEGACY_BIOS));
    assert(!memcmp(mmio, original, sizeof(mmio)));
}

static void test_malformed_capabilities(void) {
    fixture_reset();
    struct ehci_probe_report report = {0};
    pci_config[LEGACY_OFFSET / 4] |= (LEGACY_OFFSET + 4) << 8;
    assert(!strcmp(probe_registers(&device, mmio, sizeof(mmio), &report), "handoff-refused"));
    assert(!strcmp(report.handoff, "bad-legacy-chain"));
    assert(!os_writes && !smi_writes && !command_writes && !halt_delays);
    assert(!strcmp(probe_registers(&device, mmio, CAP_LENGTH, &report), "bad-capabilities"));
    assert(!os_writes && !smi_writes && !command_writes && !halt_delays);
}

static void test_stalled_halt_and_reset(void) {
    struct ehci_probe_report report = {0};
    fixture_reset();
    mmio[2] = 0;
    halt_stuck = true;
    assert(!strcmp(probe_registers(&device, mmio, sizeof(mmio), &report), "halt-timeout"));
    assert(!strcmp(report.handoff, "no-legacy"));
    assert(halt_delays == EHCI_HALT_US / EHCI_POLL_US && !reset_delays);
    assert(!(op[EHCI_USBCMD] & EHCI_CMD_RESET));
    assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));

    fixture_reset();
    reset_stuck = true;
    assert(!strcmp(probe_registers(&device, mmio, sizeof(mmio), &report), "reset-timeout"));
    assert(reset_delays == EHCI_RESET_US / EHCI_POLL_US);
    assert(!(op[EHCI_USBCMD] & EHCI_CMD_RUN) && op[EHCI_USBINTR] == 0);
    assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));
}

static void test_control_schedule(void) {
    fixture_reset();
    struct ehci_dma dma;
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000 };
    struct ehci_control_target target = { .address = 3, .packet_size = 64 };
    struct usb_setup_packet get_descriptor = { 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, 64 };
    prepare_control(&path, &target, &get_descriptor, NULL);
    assert(dma.qh.endpoint == 0x0040e003u && dma.qh.capabilities == 0x40000000u);
    assert(dma.qh.link == (path.physical | EHCI_LINK_QH));
    assert(dma.qh.overlay.next == path.physical + offsetof(struct ehci_dma, setup_td));
    assert(dma.setup_td.next == path.physical + offsetof(struct ehci_dma, data_td));
    assert(dma.data_td.next == dma.data_td.alternate);
    assert(dma.data_td.alternate == path.physical + offsetof(struct ehci_dma, status_td));
    assert((dma.data_td.token & (EHCI_TD_PID_IN | EHCI_TD_DATA1)) == (EHCI_TD_PID_IN | EHCI_TD_DATA1));
    assert(!(dma.status_td.token & EHCI_TD_PID_IN));

    struct usb_setup_packet set_address = { 0, USB_REQ_SET_ADDRESS, 1, 0, 0 };
    target.address = 0;
    prepare_control(&path, &target, &set_address, NULL);
    assert(dma.setup_td.next == path.physical + offsetof(struct ehci_dma, status_td));
    assert(dma.status_td.token & EHCI_TD_PID_IN);

    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = EHCI_STS_ASYNC; /* ASE is off, but the async owner still has the queue. */
    unsigned received = 99;
    assert(!control_transfer(&path, &target, &set_address, 0, &received));
    assert(!received && dma.setup.request == USB_REQ_SET_ADDRESS && dma.status_td.token & EHCI_TD_ACTIVE);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN);
}

static void test_split_control_schedule(void) {
    fixture_reset();
    struct ehci_dma dma;
    struct ehci_probe_report report = {0};
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
    struct ehci_control_target target = { .packet_size = 8, .tt_hub = 2, .tt_port = 3 };
    struct usb_setup_packet descriptor = { 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, 8 };
    prepare_control(&path, &target, &descriptor, NULL);
    assert(dma.qh.endpoint == 0x0808c000u && dma.qh.capabilities == 0x41820000u);

    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    unsigned received = 99;
    uint8_t data[18] = {0};
    target.tt_port = 0;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && !control_polls && dma.qh.endpoint == 0x0808c000u);

    target = (struct ehci_control_target) { .address = 5, .packet_size = 16, .tt_hub = 2, .tt_port = 3 };
    descriptor.length = sizeof(data);
    assert(control_transfer(&path, &target, &descriptor, data, &received));
    assert(received == 17 && data[0] == 0x5a && data[16] == 0x5a && !data[17]);
    assert(control_polls == 2 && op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    assert(!report.transfer_failure.reason);

    control_polls = 0;
    control_completes_at = 0;
    uint64_t start = now;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && now - start == EHCI_CONTROL_US + 2 * EHCI_POLL_US);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    const struct ehci_transfer_failure *failure = &report.transfer_failure;
    assert(!strcmp(failure->reason, "completion timeout") && failure->usbsts == EHCI_STS_ASYNC);
    assert(failure->address == 5 && failure->request.request == USB_REQ_GET_DESCRIPTOR && failure->request.length == 18);
    assert(failure->setup_token & EHCI_TD_ACTIVE);
    assert(failure->data_token & EHCI_TD_ACTIVE);
    assert(failure->status_token & EHCI_TD_ACTIVE);
    control_dma = 0;
}

static void test_flash_descriptor_wait(void) {
    fixture_reset();
    struct ehci_dma dma;
    struct ehci_probe_report report = { .trial_hub = 1 };
    struct ehci_scan_step scan = { .hub = 1, .port = 2 };
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000,
        .report = &report, .trial_slot = 1, .scan = &scan };
    struct ehci_control_target target = { .packet_size = 64 };
    struct usb_setup_packet descriptor = { 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, 64 };
    uint8_t data[64];
    unsigned received;
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    mock_flash = true;
    control_completes_at = 1 + 120000 / EHCI_POLL_US;

    scan.port = 1;
    uint64_t start = now;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && !report.flash_attempted && now - start == EHCI_CONTROL_US + 2 * EHCI_POLL_US);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);

    report = (struct ehci_probe_report) { .trial_hub = 1 };
    control_polls = 0;
    scan.port = 2;
    assert(control_transfer(&path, &target, &descriptor, data, &received));
    assert(received == 18 && report.flash_received == 18 && data[17] == 0x5a);
    assert(report.flash_attempted && report.flash_wait_us == 120000 && !report.transfer_failure.reason);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);

    report = (struct ehci_probe_report) { .trial_hub = 1 };
    control_polls = control_completes_at = 0;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && report.flash_attempted && report.flash_wait_us == EHCI_FLASH_DESCRIPTOR_US);
    assert(report.transfer_failure.overlay_token == dma.data_td.token);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    control_dma = 0;
}

static void test_dma_retirement(void) {
    fixture_reset();
    op[EHCI_ASYNCLISTADDR] = 0x9000;
    op[EHCI_PERIODICLISTBASE] = 0xa000;
    op[EHCI_USBCMD] = EHCI_CMD_RUN | EHCI_CMD_ASYNC;
    op[EHCI_USBSTS] = EHCI_STS_ASYNC;
    halt_stuck = true;
    assert(!retire_control_path(&device, op, true));
    assert(op[EHCI_ASYNCLISTADDR] == 0x9000 && op[EHCI_CONFIGFLAG] == 1);
    assert(op[EHCI_PERIODICLISTBASE] == 0xa000);
    assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));

    op[EHCI_USBSTS] = EHCI_STS_HALTED;
    assert(retire_control_path(&device, op, true));
    assert(!op[EHCI_ASYNCLISTADDR] && !op[EHCI_CONFIGFLAG]);
    assert(!op[EHCI_PERIODICLISTBASE]);
}

static void test_monitor_resource_release(void) {
    fixture_reset();
    monitor_session = (struct ehci_monitor_session) {
        .device = &device, .path = {.op = op, .physical = 0x9000},
        .frame_list = 0xa000, .root = 0x1000, .page = 0xfed00000,
    };
    op[EHCI_ASYNCLISTADDR] = 0x9000;
    op[EHCI_PERIODICLISTBASE] = 0xa000;
    halt_stuck = true;
    assert(!release_monitor_keyboard());
    assert(!freed_monitor_frames && !unmapped_monitor_windows);

    op[EHCI_USBSTS] = EHCI_STS_HALTED;
    assert(release_monitor_keyboard());
    assert(freed_monitor_frames == 2 && unmapped_monitor_windows == 1);
}

static void test_keyboard_report_schedule(void) {
    fixture_reset();
    struct ehci_dma dma;
    struct ehci_probe_report report = { .keyboard_target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 } };
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
    uint32_t frames[EHCI_FRAME_ENTRIES];
    prepare_periodic_frames(frames, path.physical, keyboard_endpoint.interval);
    for (unsigned i = 0; i < EHCI_FRAME_ENTRIES; i++) {
        assert(frames[i] == (i % 8 ? EHCI_LINK_END : (0x9000 | EHCI_LINK_QH)));
    }
    prepare_periodic_transfer(&path, &report.keyboard_target, &keyboard_endpoint, EHCI_TD_DATA1);
    assert(dma.qh.link == EHCI_LINK_END && dma.qh.endpoint == 0x000a0303u);
    assert(dma.qh.capabilities == 0x41011c01u && dma.qh.overlay.token == EHCI_TD_DATA1);
    assert(dma.qh.overlay.next == path.physical + offsetof(struct ehci_dma, setup_td));
    assert(dma.setup_td.next == EHCI_LINK_END && dma.setup_td.alternate == EHCI_LINK_END);
    assert(dma.setup_td.buffer[0] == path.physical + offsetof(struct ehci_dma, data));
    assert(dma.setup_td.token == 0x000a0d80u);

    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    keyboard_dma = &dma;
    keyboard_packet_bytes = 8;
    uint32_t toggle = 0;
    uint8_t bytes[8];
    assert(!keyboard_poll_once(&path, now, &toggle, bytes));
    assert(toggle == EHCI_TD_DATA1 && bytes[2] == 4 && report.keyboard_reports == 1);
    assert(report.keyboard_first_report_us == EHCI_KEYBOARD_POLL_US + 2 * EHCI_POLL_US);
    const struct ehci_keyboard_schedule *schedule = &report.keyboard_schedule;
    assert(schedule->sampled && schedule->qtd_loaded && schedule->frame_index_changes == 2);
    assert(schedule->c_progress_seen == 0x04 && (schedule->usbsts & EHCI_STS_PERIODIC));
    assert(!keyboard_poll_once(&path, now, &toggle, bytes));
    assert(!toggle && report.keyboard_reports == 2);
    assert(!(op[EHCI_USBSTS] & EHCI_STS_PERIODIC));

    keyboard_packet_bytes = 10;
    assert(!strcmp(keyboard_poll_once(&path, now, &toggle, bytes), "unexpected boot report length"));
    assert(report.keyboard_bytes == 10 && report.keyboard_reports == 2);
    keyboard_packet_bytes = 0;
    op[EHCI_FRINDEX] = EHCI_FRINDEX_MASK;
    uint64_t start = now - (EHCI_KEYBOARD_WAIT_US - 2 * EHCI_KEYBOARD_POLL_US);
    assert(!strcmp(keyboard_poll_once(&path, start, &toggle, bytes), "typing timeout"));
    assert(schedule->first_frame_index == EHCI_FRINDEX_MASK && schedule->last_frame_index == 2);
    assert(schedule->qtd_loaded && schedule->c_progress_seen == 0x04 && (report.keyboard_token & EHCI_TD_ACTIVE));
    assert(!(op[EHCI_USBSTS] & EHCI_STS_PERIODIC));
    keyboard_fetch = false;
    start = now - (EHCI_KEYBOARD_WAIT_US - 2 * EHCI_KEYBOARD_POLL_US);
    assert(!strcmp(keyboard_poll_once(&path, start, &toggle, bytes), "typing timeout"));
    assert(schedule->frame_index_changes && !schedule->qtd_loaded && !schedule->c_progress_seen);
    assert(!(op[EHCI_USBSTS] & EHCI_STS_PERIODIC));

    keyboard_packet_bytes = 8;
    keyboard_stop_stuck = true;
    assert(!strcmp(keyboard_poll_once(&path, now, &toggle, bytes), "periodic stop timeout"));
    uint32_t token = dma.setup_td.token;
    assert(!strcmp(keyboard_poll_once(&path, now, &toggle, bytes), "periodic schedule busy"));
    assert(dma.setup_td.token == token && report.keyboard_reports == 2);
    keyboard_dma = 0;
}

static void test_captured_keyboard_interface(void) {
    uint8_t config[] = {
        9, 2, 34, 0, 1, 1, 0, 0xa0, 0x14,
        9, 4, 0, 0, 1, 3, 1, 1, 3,
        9, 0x21, 0x11, 1, 0x21, 1, 0x22, 0x9c, 0,
        7, 5, 0x83, 3, 10, 0, 8,
    };
    assert(captured_keyboard_interface(config, sizeof(config)));
    assert(!captured_keyboard_interface(config, sizeof(config) - 1));
    config[29] = 0x81;
    assert(!captured_keyboard_interface(config, sizeof(config)));
    config[29] = 0x83;
    config[12] = 1;
    assert(!captured_keyboard_interface(config, sizeof(config)));
}

static void test_trackpad_capture(void) {
    fixture_reset();
    uint8_t configuration[] = {
        9, 2, 59, 0, 2, 1, 0, 0xa0, 0x14,
        9, 4, 1, 0, 1, 3, 0, 0, 4, 9, 0x21, 0x11, 1, 0, 1, 0x22, 0x1b, 0,
        7, 5, 0x81, 3, 64, 0, 2,
        9, 4, 2, 0, 1, 3, 1, 2, 4, 9, 0x21, 0x11, 1, 0, 1, 0x22, 0x34, 0,
        7, 5, 0x84, 3, 8, 0, 8,
    };
    assert(captured_trackpad_interfaces(configuration, sizeof(configuration)));
    assert(!captured_trackpad_interfaces(configuration, sizeof(configuration) - 1));
    configuration[32] = 8;
    assert(!captured_trackpad_interfaces(configuration, sizeof(configuration)));

    struct ehci_dma dma;
    struct ehci_probe_report report = { .trackpad_target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 } };
    report.keyboard_target = report.trackpad_target;
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    mock_feature_control = true;
    mock_feature_readback = 8;

    struct ehci_trackpad_dma queue;
    uint32_t physical = 0xa000;
    prepare_trackpad_queue(&queue, physical, &report.trackpad_target, &trackpad_endpoint);
    assert(queue.qh.endpoint == 0x00400103u && queue.qh.capabilities == 0x41011c01u);
    uint32_t address = physical + offsetof(struct ehci_trackpad_dma, slots);
    assert(queue.qh.overlay.next == address && !(address & 31u));
    assert(queue.slots[0].td.next == address + sizeof(queue.slots[0]));
    assert(queue.slots[0].td.alternate == EHCI_LINK_END && queue.slots[0].td.token == 0x01de8d80u);
    assert(queue.slots[0].td.buffer[0] == address + offsetof(struct ehci_trackpad_slot, data));
    assert(queue.slots[1].td.next == EHCI_LINK_END && queue.slots[1].td.alternate == EHCI_LINK_END);
    assert(queue.slots[1].td.token == EHCI_TD_HALTED);
    trackpad_dma = &queue;
    uint64_t frames = 0, transfer_page = 0;
    run_trackpad_trial(&path, &frames, &transfer_page);
    assert(!report.trackpad.result && transfer_page == physical && frames == 0xb000);
    for (unsigned i = 0; i < EHCI_FRAME_ENTRIES; i++) {
        assert(trackpad_frames[i] == (i % 8 ? EHCI_LINK_END : physical | EHCI_LINK_QH));
    }
    assert(trackpad_control_requests == 12 && trackpad_dma_allocations == 2 && trackpad_prompt_syncs == 2);
    assert(report.trackpad.hid_replay_started && report.trackpad.hid_replay_completed == 6);
    assert(report.trackpad.configuration_to_feature_read_us == 247973);
    assert(report.trackpad.configuration_to_mode_us > report.trackpad.configuration_to_feature_read_us);
    assert(report.trackpad.mode_before_read && report.trackpad.mode_write_attempted);
    assert(report.trackpad.mode_after_read && report.trackpad.mode_restore_read && report.trackpad.mode_restored);
    assert(trackpad_report_word(report.trackpad.feature_before) == 0x0807060504030201ull);
    assert(!memcmp(report.trackpad.feature_write, feature_bytes, sizeof(feature_bytes)));
    assert(!memcmp(report.trackpad.feature_after, feature_bytes, sizeof(feature_bytes)));
    assert(!memcmp(report.trackpad.feature_restored, report.trackpad.feature_before, sizeof(feature_bytes)));
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    assert(trackpad_packets == 13 && report.trackpad.reports == 6 && report.trackpad.ignored == 5);
    assert(report.trackpad.submissions == 11 && report.trackpad.completions == 11);
    assert(trackpad_queue_starts == 2 && trackpad_queue_stops == 2);
    assert(!report.trackpad.recovery.attempted);
    assert(report.trackpad.raw.completions == 11 && report.trackpad.raw.short_lengths[2] == 5);
    assert(report.trackpad.raw.sample_count == 1 && report.trackpad.raw.samples[0].completion == 2);
    assert(report.trackpad.raw.samples[0].bytes == 0x7400000000000000ull);
    assert(report.trackpad.mouse_requested && !report.trackpad.mouse_result);
    assert(report.trackpad.mouse_raw.completions == 2 && report.trackpad.mouse_raw.short_lengths[4] == 1 &&
        report.trackpad.mouse_raw.short_lengths[8] == 1 && report.trackpad.mouse_raw.sample_count == 2);
    assert(report.trackpad.mouse_raw.samples[0].bytes == 0x0201020300000000ull &&
        report.trackpad.mouse_raw.samples[1].bytes == 0x0201020304050607ull);
    assert(report.trackpad.arm_requested && report.trackpad.arm_acknowledged);
    assert(report.trackpad.mode_to_arm_us == EHCI_POLL_US && report.trackpad.mode_to_active_us == 2 * EHCI_POLL_US);
    assert(report.trackpad.mode_to_first_completion_us == 2 * EHCI_POLL_US + 2 * EHCI_TRACKPAD_POLL_US);
    const uint8_t first_payload[86] = {0x74, [46] = 5, [85] = 0xb7};
    assert(report.trackpad.first_payload_length == sizeof(first_payload));
    assert(!memcmp(report.trackpad.first_payload, first_payload, sizeof(first_payload)));
    assert(report.trackpad.qh_endpoint == 0x00400103u && report.trackpad.qh_capabilities == 0x41011c01u);
    assert(report.trackpad.first_td == physical + offsetof(struct ehci_trackpad_dma, slots));
    const struct ehci_trackpad_queue_sample *first = &report.trackpad.first_requests[0];
    assert(first->polls == 3 && first->first_frame == 0 && first->last_frame == 4);
    assert(first->td_token == ((478u - 86u) << EHCI_TD_BYTES_SHIFT | EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_IOC));
    assert(first->current == report.trackpad.first_td && first->next == first->current + sizeof(queue.slots[0]));
    assert(first->c_progress == 4 && first->split_seen);
    const struct ehci_trackpad_queue_sample *last = &report.trackpad.first_requests[EHCI_TRACKPAD_TRACE_COUNT - 1];
    assert(last->current == report.trackpad.first_td + sizeof(queue.slots[0]) && last->next == report.trackpad.first_td);
    assert(report.trackpad.largest == 86 && report.trackpad.bytes == 58 &&
        report.trackpad.contacts == 2 && !report.trackpad.invalid);
    assert(trackpad_capture_complete(&report.trackpad) && !op[EHCI_USBSTS]);
    const uint8_t short_report[8] = {0x44, 1, 2, 3, 4, 5, 6, 7};
    record_trackpad_frame(&report.trackpad, short_report, sizeof(short_report));
    assert(report.trackpad.invalid == 1 && report.trackpad.eight_byte_reports == 1 &&
        report.trackpad.eight_byte_id44 == 1 && report.trackpad.last_eight_byte_id == 0x44 &&
        report.trackpad.last_eight_byte_sample == 0x4401020304050607ull && report.trackpad.wire_max == 86);

    fixture_reset();
    report.trackpad = (struct ehci_trackpad_capture) {0};
    frames = transfer_page = 0;
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    mock_feature_control = mock_short_hid_descriptor = true;
    run_trackpad_trial(&path, &frames, &transfer_page);
    assert(!strcmp(report.trackpad.result, "HID interface 0 report descriptor failed"));
    assert(report.trackpad.hid_replay_started && report.trackpad.hid_replay_completed == 1);
    assert(trackpad_control_requests == 3 && !frames && !transfer_page && !report.trackpad.arm_requested);
    assert(!report.trackpad.mode_before_read && !report.trackpad.mode_restored && !op[EHCI_USBSTS]);

    fixture_reset();
    report.trackpad = (struct ehci_trackpad_capture) {0};
    frames = transfer_page = 0;
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    mock_feature_control = mock_trackpad_dummy_overlay = true;
    mock_feature_readback = 8;
    trackpad_dma = &queue;
    run_trackpad_trial(&path, &frames, &transfer_page);
    assert(!strcmp(report.trackpad.result, "queue did not park at the inactive dummy"));
    assert(report.trackpad.completions == 1 && report.trackpad.submissions == 1);
    assert(!report.trackpad.mouse_requested);
    assert(queue.slots[1].td.token == EHCI_TD_HALTED && queue.slots[1].td.next == EHCI_LINK_END);
    assert(trackpad_queue_starts == 1 && trackpad_queue_stops == 1 && report.trackpad.mode_restored);
}

static void test_trackpad_mode_retry(void) {
    for (unsigned fail_wait = 0; fail_wait < 2; fail_wait++) {
        fixture_reset();
        struct ehci_dma dma;
        const struct ehci_control_target target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 };
        struct ehci_probe_report report = { .trackpad_target = target, .keyboard_target = target };
        struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
        struct ehci_trackpad_dma queue;
        uint64_t frames = 0, transfer_page = 0;
        op[EHCI_USBCMD] = EHCI_CMD_RUN;
        op[EHCI_USBSTS] = 0;
        control_dma = &dma;
        trackpad_dma = &queue;
        mock_feature_control = mock_trackpad_recovery = true;
        mock_trackpad_retry_wait_fails = fail_wait != 0;
        mock_feature_readback = 8;

        run_trackpad_trial(&path, &frames, &transfer_page);
        const struct ehci_trackpad_capture *capture = &report.trackpad;
        const struct ehci_trackpad_recovery *recovery = &capture->recovery;
        assert(schedules_idle(&path) && capture->mode_restored);
        assert(frames == 0xb000 && transfer_page == 0xa000 && trackpad_dma_allocations == 2);
        assert(capture->raw.completions == 2 && capture->invalid == 1 && capture->ignored == 1);
        assert(capture->raw.short_lengths[2] == 1 && capture->raw.short_lengths[8] == 1);
        assert(capture->raw.samples[0].bytes == 0x6002000000000000ull);
        assert(capture->raw.samples[1].bytes == 0x0200000000000000ull);
        assert(recovery->attempted && recovery->normal_written && recovery->mode_at_reset == 1);
        assert(!memcmp(recovery->normal, capture->feature_before, sizeof(recovery->normal)));
        if (fail_wait) {
            assert(!strcmp(capture->result, "mode retry wait failed"));
            assert(!recovery->wellspring_written && !recovery->resumed && !recovery->raw.completions);
            assert(!capture->mouse_requested && trackpad_control_requests == 14);
            assert(trackpad_queue_starts == 1 && trackpad_queue_stops == 1);
            assert(queue.slots[0].td.token == EHCI_TD_HALTED);
        } else {
            assert(!capture->result && trackpad_capture_complete(capture));
            assert(recovery->wellspring_written && recovery->resumed && recovery->mode_before_retry == 8);
            assert(!memcmp(recovery->wellspring, feature_bytes, sizeof(recovery->wellspring)));
            assert(recovery->waited_us >= EHCI_TRACKPAD_MODE_RETRY_US && recovery->switch_us > recovery->waited_us);
            assert(recovery->mode_to_active_us == 2 * EHCI_POLL_US);
            assert(recovery->raw.completions == 5 && recovery->first_report_length == 58);
            assert(recovery->first_report_prefix == 0x7400000000000000ull);
            assert(recovery->first_report_us == recovery->mode_to_active_us + EHCI_TRACKPAD_POLL_US);
            assert(capture->submissions == 7 && capture->completions == 7);
            assert(capture->mouse_requested && trackpad_control_requests == 16);
            assert(trackpad_queue_starts == 3 && trackpad_queue_stops == 3);
        }
    }
}

static void retire_desktop_test_report(struct ehci_trackpad_dma *queue, unsigned slot,
    const uint8_t *data, unsigned length) {
    struct ehci_trackpad_slot *active = &queue->slots[slot];
    assert(active->td.token & EHCI_TD_ACTIVE);
    if (length) memcpy(active->data, data, length);
    active->td.token = ((trackpad_endpoint.length - length) << EHCI_TD_BYTES_SHIFT) |
        EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_IOC;
    queue->qh.current = queue->qh.overlay.next;
    queue->qh.overlay.next = active->td.next;
    queue->qh.overlay.token = active->td.token;
}

static void finish_desktop_test_normal(struct ehci_control_path *path,
    struct ehci_trackpad_dma *queue, struct trackpad_pointer *pointer,
    unsigned *slot, uint64_t *last_report, bool have_status) {
    const uint8_t status[] = {0x60, 0x02};
    struct ehci_desktop_mode_attempt *attempt = &desktop_trackpad.attempts[desktop_trackpad.mode_attempts - 1];
    assert(attempt->polling_normal && mock_feature_readback == 8 && op[EHCI_USBSTS] == EHCI_STS_PERIODIC);
    assert(trackpad_control_requests == 4 * desktop_trackpad.mode_attempts - 2);
    if (have_status) {
        now = attempt->normal_completed_us + EHCI_TRACKPAD_MODE_RETRY_US - 1;
        retire_desktop_test_report(queue, *slot, status, sizeof(status));
        assert(!desktop_trackpad_poll(path, queue, pointer, slot, last_report));
        assert(attempt->polling_normal && attempt->normal_reports == 1 && attempt->normal_status);
    } else {
        now = attempt->normal_completed_us + EHCI_DESKTOP_NORMAL_POLL_US - 1;
        assert(!desktop_trackpad_poll(path, queue, pointer, slot, last_report));
        assert(attempt->polling_normal && !attempt->normal_reports && !attempt->normal_status);
    }
    now++;
    struct ehci_qtd active = queue->slots[*slot].td;
    assert(!desktop_trackpad_poll(path, queue, pointer, slot, last_report));
    assert(!attempt->polling_normal && attempt->resumed_at_us);
    assert(!memcmp(&active, &queue->slots[*slot].td, sizeof(active)));
}

static void test_desktop_persistent_mode_recovery(void) {
    const uint8_t legacy[] = {0x02, 0, 7, 0, 0, 0, 9, 0};
    const uint8_t native[58] = {0x74, [46] = 5};
    for (unsigned scenario = 0; scenario < 3; scenario++) {
        fixture_reset();
        desktop_trackpad = (struct ehci_desktop_trackpad){0};
        struct ehci_dma control;
        struct ehci_trackpad_dma queues[2];
        struct ehci_probe_report *report = &desktop_trackpad.report;
        const struct ehci_control_target target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 };
        report->trackpad_target = target;
        struct ehci_control_path path = { .op = op, .dma = &control, .physical = 0x9000, .report = report };
        struct trackpad_pointer pointer = {0};
        unsigned slot = 0;
        uint64_t last_report = 0;
        prepare_trackpad_queue(&queues[0], 0xa000, &target, &trackpad_endpoint);
        prepare_trackpad_queue(&queues[1], 0xa000 + sizeof(queues[0]), &target, &keyboard_endpoint);
        queues[1].qh.overlay.token |= EHCI_TD_DATA1;
        uint8_t keyboard_before[sizeof(queues[1])];
        memcpy(keyboard_before, &queues[1], sizeof(keyboard_before));
        report->trackpad.first_td = queues[0].qh.overlay.next;
        control_dma = &control;
        mock_desktop_mode_switch = true;
        mock_desktop_first_write_fails = scenario == 1;
        mock_feature_readback = 1;
        op[EHCI_USBCMD] = EHCI_CMD_RUN | EHCI_CMD_PERIODIC;
        op[EHCI_USBSTS] = EHCI_STS_PERIODIC;
        op[EHCI_PORTSC] = EHCI_PORT_CONNECTED;

        retire_desktop_test_report(&queues[0], slot, legacy, sizeof(legacy));
        queues[0].qh.overlay.token |= EHCI_TD_DATA1;
        assert(!desktop_trackpad_poll(&path, &queues[0], &pointer, &slot, &last_report));
        assert(desktop_trackpad.mode_attempts == 1);
        assert(queues[0].qh.overlay.token & EHCI_TD_DATA1);
        finish_desktop_test_normal(&path, &queues[0], &pointer, &slot, &last_report, scenario != 2);
        assert(trackpad_control_requests == 4);
        assert((desktop_trackpad.attempts[0].control_error != NULL) == mock_desktop_first_write_fails);
        assert(op[EHCI_USBSTS] == EHCI_STS_PERIODIC && (queues[1].slots[0].td.token & EHCI_TD_ACTIVE));

        now = desktop_trackpad.retry_activated_us + EHCI_DESKTOP_RECOVERY_INTERVAL_US - 1;
        retire_desktop_test_report(&queues[0], slot, legacy, sizeof(legacy));
        assert(!desktop_trackpad_poll(&path, &queues[0], &pointer, &slot, &last_report));
        assert(desktop_trackpad.mode_attempts == 1 && desktop_trackpad.legacy_reports == 2);
        assert(desktop_trackpad.attempts[0].first_reply_length == 8 && desktop_trackpad.attempts[0].first_reply_id == 2);

        unsigned target_attempts = scenario == 0 ? 1 : scenario == 1 ? 2 : EHCI_DESKTOP_RECOVERY_ATTEMPTS;
        while (desktop_trackpad.mode_attempts < target_attempts) {
            now = desktop_trackpad.retry_activated_us + EHCI_DESKTOP_RECOVERY_INTERVAL_US;
            retire_desktop_test_report(&queues[0], slot, legacy, sizeof(legacy));
            assert(!desktop_trackpad_poll(&path, &queues[0], &pointer, &slot, &last_report));
            finish_desktop_test_normal(&path, &queues[0], &pointer, &slot, &last_report, scenario == 1);
            assert(mock_feature_readback == 1);
        }
        assert(trackpad_control_requests == 4 * target_attempts && mock_feature_readback == 1);
        assert(!desktop_trackpad.malformed && !desktop_trackpad.reports);
        assert(desktop_trackpad.attempts[target_attempts - 1].waited_us >= EHCI_TRACKPAD_MODE_RETRY_US);

        now = desktop_trackpad.retry_activated_us + 2 * EHCI_DESKTOP_RECOVERY_INTERVAL_US;
        if (scenario == 2) {
            retire_desktop_test_report(&queues[0], slot, legacy, sizeof(legacy));
            assert(!desktop_trackpad_poll(&path, &queues[0], &pointer, &slot, &last_report));
            assert(!desktop_trackpad.reports && !ehci_desktop_trackpad_verified());
        } else {
            retire_desktop_test_report(&queues[0], slot, native, sizeof(native));
            assert(!desktop_trackpad_poll(&path, &queues[0], &pointer, &slot, &last_report));
            assert(desktop_trackpad.reports == 1 && !desktop_trackpad.malformed);
            assert(desktop_trackpad.attempts[target_attempts - 1].first_valid_us != 0);
        }
        assert(desktop_trackpad.mode_attempts == target_attempts && trackpad_control_requests == 4 * target_attempts);
        assert(!memcmp(keyboard_before, &queues[1], sizeof(keyboard_before)));
    }
}

static void test_desktop_shared_tt_schedule(void) {
    fixture_reset();
    struct ehci_trackpad_dma queues[2];
    trackpad_dma = queues;
    const struct ehci_control_target target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 };
    struct ehci_probe_report report = { .keyboard_target = target };
    struct ehci_control_path path = { .op = op, .report = &report };
    prepare_trackpad_queue(&queues[0], 0xa000, &target, &trackpad_endpoint);
    prepare_periodic_frames(trackpad_frames, 0xa000, trackpad_endpoint.interval);
    prepare_desktop_keyboard_queue(&path, 0xb000, 0xa000);

    uint32_t keyboard_qh = 0xa000 + sizeof(queues[0]);
    for (unsigned i = 0; i < EHCI_FRAME_ENTRIES; i++) {
        uint32_t expected = i % keyboard_endpoint.interval ? 0xa000u : keyboard_qh;
        assert(trackpad_frames[i] == (expected | EHCI_LINK_QH));
    }
    assert(queues[0].qh.link == EHCI_LINK_END && queues[1].qh.link == EHCI_LINK_END);
    assert(queues[0].qh.capabilities == queues[1].qh.capabilities);
    assert(((queues[0].qh.endpoint >> 8) & 15) == 1 && ((queues[1].qh.endpoint >> 8) & 15) == 3);
    assert(queues[1].qh.overlay.next == keyboard_qh + offsetof(struct ehci_trackpad_dma, slots));
    assert(queues[1].slots[0].td.token & EHCI_TD_ACTIVE);
    assert((queues[1].slots[0].td.token >> EHCI_TD_BYTES_SHIFT) == EHCI_KEYBOARD_PACKET);
}

static void test_desktop_trackpad_input(void) {
    fixture_reset();
    desktop_trackpad = (struct ehci_desktop_trackpad){0};
    desktop_trackpad.trace = true;
    desktop_trackpad.frame_list = 0xb000;
    struct ehci_probe_report *report = &desktop_trackpad.report;
    struct ehci_control_path path = { .op = op, .report = report };
    const struct ehci_control_target target = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 };
    struct ehci_trackpad_dma queue;
    struct trackpad_pointer pointer = {0};
    unsigned slot = 0;
    uint64_t last_report = 0;
    prepare_trackpad_queue(&queue, 0xa000, &target, &trackpad_endpoint);
    report->trackpad.first_td = queue.qh.overlay.next;
    op[EHCI_USBCMD] = EHCI_CMD_RUN | EHCI_CMD_PERIODIC;
    op[EHCI_USBSTS] = EHCI_STS_PERIODIC;
    op[EHCI_PORTSC] = EHCI_PORT_CONNECTED;
    op[EHCI_PERIODICLISTBASE] = 0xb000;
    trackpad_frames[0] = 0xa000 | EHCI_LINK_QH;
    assert(!desktop_trackpad_poll(&path, &queue, &pointer, &slot, &last_report));
    assert(slot == 0 && !desktop_trackpad.reports);

    for (unsigned request = 0; request < 4; request++) {
        const uint8_t report_bytes[58] = {0x74, [15] = request == 2, [32] = 64 * request,
            [34] = 48 * request, [46] = 5};
        retire_desktop_test_report(&queue, slot, report_bytes, sizeof(report_bytes));
        now += 8000;
        assert(!desktop_trackpad_poll(&path, &queue, &pointer, &slot, &last_report));
        assert(trackpad_buttons == (request == 2 ? 1 : 0));
        if (request == 1) assert(trackpad_dx > 0 && trackpad_dy < 0);
    }
    assert(desktop_trackpad.reports == 4 && desktop_trackpad.motion && injected_trackpad_events >= 3);
    assert(desktop_trackpad.presses == 1 && desktop_trackpad.releases == 1);
    const struct ehci_desktop_queue_trace *trace = &desktop_trackpad.trackpad_trace;
    assert(trace->completions == 4 && trace->last.polls == 5);
    assert(trace->frame_link == (0xa000 | EHCI_LINK_QH) && trace->schedule_base == 0xb000);
    assert(trace->first[0].completed && trace->first[0].length == 58 && trace->first[0].report_id == 0x74);
    assert(trace->first[0].queue.polls == 2 && trace->first[0].last_us - trace->first[0].first_us == 8000);
    observe_desktop_queue(&desktop_trackpad.keyboard_trace, &path, &queue, EHCI_TD_ACTIVE);
    complete_desktop_queue(&desktop_trackpad.keyboard_trace, 8, 0);
    assert(desktop_trackpad.keyboard_trace.first[0].completed && desktop_trackpad.keyboard_trace.first[0].length == 8);
    assert(desktop_trackpad.keyboard_trace.first[0].report_id == 0 && trace->completions == 4);

    /* Full-size Type 2 transfer with a negative pressure field. */
    uint8_t malformed[EHCI_TRACKPAD_REPORT_MAX] = {0x74, [47] = 0x80,
        [EHCI_TRACKPAD_REPORT_MAX - 1] = 0xff};
    now += 9000;
    op[EHCI_FRINDEX] = 0x4321;
    retire_desktop_test_report(&queue, slot, malformed, sizeof(malformed));
    assert(!desktop_trackpad_poll(&path, &queue, &pointer, &slot, &last_report));
    const struct ehci_desktop_frame_sample *sample = &desktop_trackpad.malformed_samples[0];
    assert(sample->length == sizeof(malformed) && !memcmp(sample->bytes, malformed, sizeof(malformed)));
    assert(sample->completion == 5 && sample->previous_valid_us == last_report && sample->observed_us - last_report == 9000);
    assert(sample->frame_index == 0x321 && sample->qtd_token == sample->overlay_token);

    for (unsigned extra = 0; extra < EHCI_DESKTOP_MALFORMED_SAMPLES; extra++) {
        now += 8000;
        retire_desktop_test_report(&queue, slot, NULL, 0);
        assert(!desktop_trackpad_poll(&path, &queue, &pointer, &slot, &last_report));
    }
    assert(desktop_trackpad.malformed == EHCI_DESKTOP_MALFORMED_SAMPLES + 1);
    assert(desktop_trackpad.malformed_samples[1].length == 0);
    assert(desktop_trackpad.malformed_samples[EHCI_DESKTOP_MALFORMED_SAMPLES - 1].completion ==
        4 + EHCI_DESKTOP_MALFORMED_SAMPLES);
    assert(!memcmp(sample->bytes, malformed, sizeof(malformed)));
    assert(trace->completions == 9 && trace->first[4].length == sizeof(malformed) && trace->first[5].length == 0);
}

static void test_flash_bulk(void) {
    fixture_reset();
    uint8_t config[] = {9, 2, 32, 0, 1, 1, 0, 0x80, 50,
        9, 4, 0, 0, 2, 8, 6, 0x50, 0, 7, 5, 0x81, 2, 0, 2, 0, 7, 5, 2, 2, 0, 2, 0};
    assert(captured_flash_interface(config, sizeof(config)));
    assert(!captured_flash_interface(config, sizeof(config) - 1));
    config[27] = 0x82;
    assert(!captured_flash_interface(config, sizeof(config)));
    struct ehci_dma dma;
    struct ehci_probe_report report = { .flash_address = 2 };
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
    uint8_t bounce[USB_STORAGE_BULK_BYTES], data[32] = {0x55, [30] = 0xaa};
    struct ehci_flash_io flash = { .path = &path, .physical = 0xa000, .buffer = bounce };
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    mock_bulk = &flash;
    uint32_t actual;
    assert(flash_bulk(&flash, false, data, 31, &actual) && actual == 31);
    assert(flash.endpoint_state[0] == EHCI_TD_DATA1 && flash.endpoint_state[1] == 0);
    assert(flash_bulk(&flash, true, data, sizeof(data), &actual) && actual == 13);
    assert(flash.endpoint_state[1] == EHCI_TD_DATA1 && data[12] == 0x5a && data[13] == 0);
    assert(flash_bulk(&flash, true, data, sizeof(data), &actual) && !flash.endpoint_state[1]);
    mock_bulk_timeout = true;
    assert(!flash_bulk(&flash, true, data, sizeof(data), &actual) && !actual && !op[EHCI_USBSTS]);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN);
}

static void test_native_trial_result(void) {
    struct pci_device controller = { .vendor_id = 0x8086, .device_id = 0x1c2d, .device = 0x1a, .function = 7 };
    assert(trial_controller_slot(&controller) == 0);
    controller.function = 0;
    assert(trial_controller_slot(&controller) == -1);
    controller = (struct pci_device) { .vendor_id = 0x8086, .device_id = 0x1c26, .device = 0x1d, .function = 7 };
    assert(trial_controller_slot(&controller) == 1);
    controller.device_id = 0x293c;
    assert(trial_controller_slot(&controller) == -1);

    struct ehci_probe_report clean = {
        .reset_halted = true, .enumerated = true, .dma_released = true, .access_retired = true,
    };
    struct ehci_trial_report trial = { .found = 2, .controllers = { clean, clean } };
    struct ehci_control_path path = { .report = &trial.controllers[0] };
    struct ehci_control_target hub = { .address = 1, .packet_size = 64 };
    struct ehci_control_target topcase = { .address = 3, .packet_size = 8, .tt_hub = 1, .tt_port = 2 };
    track_trial_device(&path, &topcase, 0x05ac, 0x0245, 1, 2, 0, 0);
    assert(!trial_passed(&trial));
    track_trial_device(&path, &hub, 0x0424, 0x2513, 0, 1, 0, 0);
    track_trial_device(&path, &topcase, 0x05ac, 0x0245, 1, 3, 0, 0);
    assert(!trial_passed(&trial));
    topcase.packet_size = 16;
    track_trial_device(&path, &topcase, 0x05ac, 0x0245, 1, 2, 0, 0);
    assert(!trial_passed(&trial));
    topcase.packet_size = 8;
    track_trial_device(&path, &topcase, 0x05ac, 0x0245, 1, 2, 0, 0);
    assert(!trial_passed(&trial));
    path.report = &trial.controllers[1];
    path.trial_slot = 1;
    track_trial_device(&path, &hub, 0x0424, 0x2513, 0, 1, 0, 0);
    struct ehci_control_target flash = { .address = 2, .packet_size = 64 };
    track_trial_device(&path, &flash, 0x13fe, 0x6700, 1, 3, 0, 0);
    assert(!path.report->flash_address);
    track_trial_device(&path, &flash, 0x13fe, 0x6700, 1, 2, 0, 0);
    assert(!trial_passed(&trial));
    path.report->flash_attempted = true;
    path.report->flash_received = 18;
    assert(trial_passed(&trial));
    trial.controllers[1].dma_released = false;
    assert(!trial_passed(&trial));
    trial.controllers[1].dma_released = true;
    trial.controllers[0].access_retired = false;
    assert(!trial_passed(&trial));
}

int main(void) {
    test_completed_probe();
    test_firmware_retains_controller();
    test_malformed_capabilities();
    test_stalled_halt_and_reset();
    test_control_schedule();
    test_split_control_schedule();
    test_flash_descriptor_wait();
    test_dma_retirement();
    test_monitor_resource_release();
    test_keyboard_report_schedule();
    test_captured_keyboard_interface();
    test_trackpad_capture();
    test_trackpad_mode_retry();
    test_desktop_persistent_mode_recovery();
    test_desktop_shared_tt_schedule();
    test_desktop_trackpad_input();
    test_flash_bulk();
    test_native_trial_result();
    puts("EHCI host tests passed");
}
