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
static struct ehci_dma *split_dma;
static unsigned split_polls;
static bool split_stuck;
static uint64_t now;

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
    if (delay == 1000) {
        handoff_delays++;
        if (!firmware_stuck && handoff_delays == 3) pci_config[LEGACY_OFFSET / 4] &= ~EHCI_LEGACY_BIOS;
        return true;
    }
    assert(delay == EHCI_POLL_US);
    if (split_dma) {
        if (op[EHCI_USBCMD] & EHCI_CMD_ASYNC) {
            op[EHCI_USBSTS] |= EHCI_STS_ASYNC;
            assert(split_dma->qh.endpoint == 0x0810c005u);
            assert(split_dma->qh.capabilities == 0x41820000u);
            assert(!split_dma->qh.overlay.token);
            if (++split_polls == 2 && !split_stuck) {
                split_dma->setup_td.token = 0;
                split_dma->data_td.token = 1u << EHCI_TD_BYTES_SHIFT;
                split_dma->status_td.token = 0;
                memset(split_dma->data, 0x5a, 17);
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
    split_dma = 0;
    split_polls = 0;
    split_stuck = false;
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
    struct ehci_control_path path = { op, &dma, 0x9000 };
    struct ehci_control_target target = { .address = 3, .packet_size = 64 };
    struct usb_setup_packet get_descriptor = { 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, 64 };
    prepare_control(&path, &target, &get_descriptor);
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
    prepare_control(&path, &target, &set_address);
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
    struct ehci_control_path path = { op, &dma, 0x9000 };
    struct ehci_control_target target = { .packet_size = 8, .tt_hub = 2, .tt_port = 3 };
    struct usb_setup_packet descriptor = { 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0, 8 };
    prepare_control(&path, &target, &descriptor);
    assert(dma.qh.endpoint == 0x0808c000u && dma.qh.capabilities == 0x41820000u);

    op[EHCI_USBCMD] = EHCI_CMD_RUN;
    op[EHCI_USBSTS] = 0;
    split_dma = &dma;
    unsigned received = 99;
    uint8_t data[18] = {0};
    target.tt_port = 0;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && !split_polls && dma.qh.endpoint == 0x0808c000u);

    target = (struct ehci_control_target) { .address = 5, .packet_size = 16, .tt_hub = 2, .tt_port = 3 };
    descriptor.length = sizeof(data);
    assert(control_transfer(&path, &target, &descriptor, data, &received));
    assert(received == 17 && data[0] == 0x5a && data[16] == 0x5a && !data[17]);
    assert(split_polls == 2 && op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);

    split_polls = 0;
    split_stuck = true;
    uint64_t start = now;
    assert(!control_transfer(&path, &target, &descriptor, data, &received));
    assert(!received && now - start == EHCI_CONTROL_US + 2 * EHCI_POLL_US);
    assert(op[EHCI_USBCMD] == EHCI_CMD_RUN && !op[EHCI_USBSTS]);
    split_dma = 0;
}

static void test_dma_retirement(void) {
    fixture_reset();
    op[EHCI_ASYNCLISTADDR] = 0x9000;
    op[EHCI_USBCMD] = EHCI_CMD_RUN | EHCI_CMD_ASYNC;
    op[EHCI_USBSTS] = EHCI_STS_ASYNC;
    halt_stuck = true;
    assert(!retire_control_path(&device, op));
    assert(op[EHCI_ASYNCLISTADDR] == 0x9000 && op[EHCI_CONFIGFLAG] == 1);
    assert(!(pci_config[PCI_COMMAND / 4] & PCI_COMMAND_MASTER));

    op[EHCI_USBSTS] = EHCI_STS_HALTED;
    assert(retire_control_path(&device, op));
    assert(!op[EHCI_ASYNCLISTADDR] && !op[EHCI_CONFIGFLAG]);
}

int main(void) {
    test_completed_probe();
    test_firmware_retains_controller();
    test_malformed_capabilities();
    test_stalled_halt_and_reset();
    test_control_schedule();
    test_split_control_schedule();
    test_dma_retirement();
    puts("EHCI host tests passed");
}
