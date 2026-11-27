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
static uint8_t mock_feature_readback;
static const uint8_t feature_bytes[] = {1, 7, 6, 5, 4, 3, 2, 1};
static struct ehci_dma *keyboard_dma;
static struct ehci_dma *trackpad_dma;
static unsigned trackpad_reports, trackpad_offset, trackpad_packets;
static unsigned keyboard_packet_bytes;
static bool keyboard_stop_stuck;
static bool keyboard_fetch;
static uint64_t now;
static unsigned freed_monitor_frames, unmapped_monitor_windows;

void console_write(const char *message) { assert(message); }

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
    if (trackpad_dma) {
        if (!(op[EHCI_USBCMD] & EHCI_CMD_PERIODIC)) {
            op[EHCI_USBSTS] &= ~EHCI_STS_PERIODIC;
            return true;
        }
        op[EHCI_USBSTS] |= EHCI_STS_PERIODIC;
        if (delay != EHCI_TRACKPAD_POLL_US) return true;
        assert(trackpad_reports < 5 && (trackpad_dma->setup_td.token & EHCI_TD_ACTIVE));
        uint8_t report[86] = {0};
        unsigned length = trackpad_reports == 2 ? 86 : 58;
        report[30 + 2] = trackpad_reports >= 1 ? 64 : 0;
        report[30 + 16] = 5;
        if (trackpad_reports == 2) report[58 + 16] = 5;
        if (trackpad_reports == 3) report[15] = 1;
        unsigned packet = length - trackpad_offset;
        if (packet > trackpad_endpoint.packet) packet = trackpad_endpoint.packet;
        memcpy(trackpad_dma->data + trackpad_offset, report + trackpad_offset, packet);
        trackpad_offset += packet;
        trackpad_packets++;
        trackpad_dma->qh.overlay.token ^= EHCI_TD_DATA1;
        if (trackpad_offset == length) {
            trackpad_dma->setup_td.token = (trackpad_endpoint.length - length) << EHCI_TD_BYTES_SHIFT;
            trackpad_offset = 0;
            trackpad_reports++;
        }
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
                assert(control_dma->setup.value == 0x0300 && !control_dma->setup.index);
                if (control_dma->setup.request == 9) {
                    assert(control_dma->setup.request_type == 0x21);
                    assert(!memcmp(control_dma->data, feature_bytes, sizeof(feature_bytes)));
                    assert(control_dma->data_td.token == (EHCI_TD_DATA1 | (8u << EHCI_TD_BYTES_SHIFT) |
                        EHCI_TD_PID_OUT | EHCI_TD_RETRIES | EHCI_TD_ACTIVE));
                    assert((control_dma->status_td.token & EHCI_TD_PID_IN) &&
                        control_dma->data_td.alternate == EHCI_LINK_END);
                } else {
                    assert(control_dma->setup.request_type == 0xa1 && control_dma->setup.request == 1);
                    memcpy(control_dma->data, feature_bytes, sizeof(feature_bytes));
                    control_dma->data[0] = mock_feature_readback;
                }
                control_dma->setup_td.token = control_dma->data_td.token = control_dma->status_td.token = 0;
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
    mock_feature_control = false;
    trackpad_dma = NULL;
    trackpad_reports = trackpad_offset = trackpad_packets = 0;
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
    struct ehci_control_path path = { .op = op, .dma = &dma, .physical = 0x9000, .report = &report };
    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    control_dma = &dma;
    mock_feature_control = true;
    uint8_t original_mode[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    mock_feature_readback = 8;
    assert(!activate_trackpad_mode(&path, original_mode));
    assert(report.trackpad.mode_after_read && report.trackpad.mode_after == 8);
    mock_feature_readback = 1;
    assert(!activate_trackpad_mode(&path, original_mode));
    assert(report.trackpad.mode_after == 1 && original_mode[0] == 8);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    control_dma = NULL;

    prepare_periodic_transfer(&path, &report.trackpad_target, &trackpad_endpoint, 0);
    assert(dma.qh.endpoint == 0x00400103u && dma.qh.capabilities == 0x41011c01u);
    assert(dma.setup_td.token == 0x01de0d80u);
    uint32_t frames[EHCI_FRAME_ENTRIES];
    prepare_periodic_frames(frames, path.physical, trackpad_endpoint.interval);
    for (unsigned i = 0; i < EHCI_FRAME_ENTRIES; i++) assert(frames[i] == (path.physical | EHCI_LINK_QH));
    trackpad_dma = &dma;
    assert(!poll_trackpad(&path));
    assert(trackpad_packets == 6 && report.trackpad.reports == 5);
    assert(report.trackpad.largest == 86 && report.trackpad.bytes == 58 &&
        report.trackpad.contacts == 2 && !report.trackpad.invalid);
    assert(trackpad_capture_complete(&report.trackpad) && !op[EHCI_USBSTS]);
    const uint8_t short_report[8] = {0x44};
    record_trackpad_frame(&report.trackpad, short_report, sizeof(short_report));
    assert(report.trackpad.invalid == 1 && report.trackpad.eight_byte_reports == 1 &&
        report.trackpad.eight_byte_id44 == 1 && report.trackpad.last_eight_byte_id == 0x44 &&
        report.trackpad.wire_max == 86);
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
    test_native_trial_result();
    puts("EHCI host tests passed");
}
