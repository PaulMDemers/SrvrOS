#include <srvros/ehci.h>
#include <srvros/console.h>
#include <srvros/ehci_enumeration.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>
#include <stddef.h>

/* EHCI 1.0, sections 2.1, 2.3 and 5.1. */
#define EHCI_PCI_USB_CLASS 0x0c
#define EHCI_PCI_USB_SUBCLASS 0x03
#define EHCI_PCI_PROG_IF 0x20
#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY 0x0002
#define PCI_COMMAND_MASTER 0x0004

#define EHCI_MMIO_WINDOW 0xffffc001f6000000ull
#define EHCI_PAGE_BYTES 4096u
#define EHCI_CAP_BYTES 0x0cu
#define EHCI_VERSION_1_0 0x0100u

/* Operational register indices for aligned DWORD accesses. */
#define EHCI_USBCMD (0x00u / 4)
#define EHCI_USBSTS (0x04u / 4)
#define EHCI_USBINTR (0x08u / 4)
#define EHCI_CTRLDSSEGMENT (0x10u / 4)
#define EHCI_ASYNCLISTADDR (0x18u / 4)
#define EHCI_CONFIGFLAG (0x40u / 4)
#define EHCI_PORTSC (0x44u / 4)
#define EHCI_CMD_RUN (1u << 0)
#define EHCI_CMD_RESET (1u << 1)
#define EHCI_CMD_ASYNC (1u << 5)
#define EHCI_CMD_SCHEDULES ((1u << 4) | (1u << 5))
#define EHCI_CMD_ASYNC_ADVANCE (1u << 6)
#define EHCI_CMD_LIGHT_RESET (1u << 7)
#define EHCI_STS_HALTED (1u << 12)
#define EHCI_STS_SCHEDULES ((1u << 14) | (1u << 15))
#define EHCI_STS_ASYNC (1u << 15)
#define EHCI_STS_HOST_ERROR (1u << 4)

#define EHCI_HCS_PORT_POWER (1u << 4)
#define EHCI_HCC_64BIT (1u << 0)
#define EHCI_PORT_CONNECTED (1u << 0)
#define EHCI_PORT_ENABLE (1u << 2)
#define EHCI_PORT_OVERCURRENT (1u << 4)
#define EHCI_PORT_RESET (1u << 8)
#define EHCI_PORT_POWER (1u << 12)
#define EHCI_PORT_OWNER (1u << 13)
#define EHCI_PORT_CHANGE ((1u << 1) | (1u << 3) | (1u << 5))

#define EHCI_LINK_END 1u
#define EHCI_LINK_QH 2u
#define EHCI_QH_SPEED_HIGH (2u << 12)
#define EHCI_QH_TOGGLE_FROM_TD (1u << 14)
#define EHCI_QH_HEAD (1u << 15)
#define EHCI_QH_PACKET_SHIFT 16
#define EHCI_QH_CONTROL (1u << 27)
#define EHCI_QH_TT_HUB_SHIFT 16
#define EHCI_QH_TT_PORT_SHIFT 23
#define EHCI_QH_MULT_ONE (1u << 30)
#define EHCI_TD_ACTIVE (1u << 7)
#define EHCI_TD_ERROR 0x7cu
#define EHCI_TD_PID_OUT (0u << 8)
#define EHCI_TD_PID_IN (1u << 8)
#define EHCI_TD_PID_SETUP (2u << 8)
#define EHCI_TD_RETRIES (3u << 10)
#define EHCI_TD_DATA1 (1u << 31)
#define EHCI_TD_BYTES_SHIFT 16
#define EHCI_TD_BYTES_MASK 0x7fffu

#define EHCI_LEGACY_CAP 1u
#define EHCI_LEGACY_BIOS (1u << 16)
#define EHCI_LEGACY_OS (1u << 24)
#define EHCI_LEGACY_SMI_ENABLES 0xe03fu
#define EHCI_HANDOFF_POLLS 1000u
#define EHCI_HALT_US 20000u
#define EHCI_RESET_US 250000u
#define EHCI_POLL_US 10u
#define EHCI_CONTROL_US 100000u

struct ehci_probe_report {
    const char *result;
    const char *handoff;
    const char *mapping;
    unsigned version;
    unsigned ports;
    unsigned cap_length;
    uint32_t structural;
    uint32_t capabilities;
    bool was_running;
    bool reset_halted;
    bool enumerated;
    bool dma_released;
    bool access_retired;
    uint8_t trial_hub;
    uint8_t topcase_address;
};

struct ehci_trial_report {
    unsigned found;
    unsigned scans;
    unsigned released;
    struct ehci_probe_report controllers[2];
};

struct ehci_handoff_result {
    const char *status;
    bool complete;
};

/* Include the optional high buffer words. All DMA in this diagnostic uses
 * the low 32-bit segment, including on controllers with 64-bit capability. */
struct ehci_qtd {
    uint32_t next, alternate, token;
    uint32_t buffer[5], buffer_high[5];
};

struct ehci_qh {
    uint32_t link, endpoint, capabilities, current;
    struct ehci_qtd overlay;
};

struct ehci_dma {
    _Alignas(32) struct ehci_qh qh;
    _Alignas(32) struct ehci_qtd setup_td;
    _Alignas(32) struct ehci_qtd data_td;
    _Alignas(32) struct ehci_qtd status_td;
    struct usb_setup_packet setup;
    uint8_t data[EHCI_DESCRIPTOR_BYTES];
};

_Static_assert(sizeof(struct usb_setup_packet) == 8 && sizeof(struct ehci_dma) <= EHCI_PAGE_BYTES,
    "EHCI control descriptors and their buffers must fit one DMA page");
_Static_assert(offsetof(struct ehci_qh, overlay) == 16 && offsetof(struct ehci_qtd, buffer_high) == 32,
    "EHCI queue head and transfer descriptor layout");
_Static_assert(!(offsetof(struct ehci_dma, setup_td) % 32) && !(offsetof(struct ehci_dma, data_td) % 32) &&
    !(offsetof(struct ehci_dma, status_td) % 32), "EHCI transfer descriptors require 32-byte alignment");

struct ehci_control_path {
    volatile uint32_t *op;
    struct ehci_dma *dma;
    uint32_t physical;
    struct ehci_probe_report *report;
};

static bool set_command_bits(const struct pci_device *device, uint16_t mask, uint16_t bits) {
    uint16_t command = pci_read_config16(device, PCI_COMMAND);
    if (command == UINT16_MAX) return false;
    if ((command & mask) != bits) {
        pci_write_config16(device, PCI_COMMAND, (command & ~mask) | bits);
        command = pci_read_config16(device, PCI_COMMAND);
    }
    return command != UINT16_MAX && (command & mask) == bits;
}

static struct ehci_handoff_result firmware_handoff(const struct pci_device *device, uint8_t offset) {
    const struct ehci_handoff_result malformed = { "bad-legacy-chain", false };
    uint64_t visited = 0;
    uint8_t legacy = 0;
    while (offset) {
        uint64_t bit = 1ull << (offset / 4);
        if (offset < 0x40 || (offset & 3) || (visited & bit)) return malformed;
        visited |= bit;
        uint32_t capability = pci_read_config32(device, offset);
        if (capability == UINT32_MAX || !(capability & 0xff)) return malformed;
        if ((capability & 0xff) == EHCI_LEGACY_CAP) {
            if (legacy || offset > 0xf8 || (visited & (bit << 1))) return malformed;
            legacy = offset;
            visited |= bit << 1;
        }
        offset = (uint8_t)(capability >> 8);
    }
    if (!legacy) return (struct ehci_handoff_result) { "no-legacy", true };

    uint32_t capability = pci_read_config32(device, legacy);
    if (capability == UINT32_MAX) return (struct ehci_handoff_result) { "legacy-unreadable", false };
    /* Request ownership through the OS byte. The firmware owns the BIOS byte. */
    bool requested = !(capability & EHCI_LEGACY_OS);
    if (requested) pci_write_config8(device, legacy + 3, 1);
    for (unsigned poll = 0; poll <= EHCI_HANDOFF_POLLS; poll++) {
        capability = pci_read_config32(device, legacy);
        if (capability == UINT32_MAX) return (struct ehci_handoff_result) { "legacy-unreadable", false };
        if (!(capability & EHCI_LEGACY_OS)) return (struct ehci_handoff_result) { "legacy-request-refused", false };
        if (!(capability & EHCI_LEGACY_BIOS)) {
            /* Zero disables the SMI enables and preserves write-one-to-clear events. */
            pci_write_config32(device, legacy + 4, 0);
            uint32_t control = pci_read_config32(device, legacy + 4);
            if (control == UINT32_MAX || (control & EHCI_LEGACY_SMI_ENABLES)) {
                return (struct ehci_handoff_result) { "smi-disable-failed", false };
            }
            return (struct ehci_handoff_result) { "os-owned", true };
        }
        if (poll == EHCI_HANDOFF_POLLS || !timer_delay_us(1000)) break;
    }
    if (requested) pci_write_config8(device, legacy + 3, 0);
    return (struct ehci_handoff_result) { "bios-timeout", false };
}

static bool wait_mmio(volatile uint32_t *reg, uint32_t mask, uint32_t expected, unsigned timeout_us) {
    for (unsigned elapsed = 0; elapsed <= timeout_us; elapsed += EHCI_POLL_US) {
        uint32_t value = *reg;
        if (value == UINT32_MAX) return false;
        if ((value & mask) == expected) return true;
        if (elapsed == timeout_us || !timer_delay_us(EHCI_POLL_US)) return false;
    }
    return false;
}

static bool wait_control(const struct ehci_control_path *path, bool data_phase) {
    const volatile struct ehci_dma *dma = path->dma;
    uint64_t start = timer_monotonic_us();
    for (unsigned poll = 0; poll <= EHCI_CONTROL_US / EHCI_POLL_US; poll++) {
        uint32_t setup = dma->setup_td.token;
        uint32_t data = data_phase ? dma->data_td.token : 0;
        uint32_t status = dma->status_td.token;
        if (((setup | data | status) & EHCI_TD_ERROR) ||
            (path->op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR))) return false;
        if (!((setup | data | status) & EHCI_TD_ACTIVE)) {
            return !((setup | status) & (EHCI_TD_BYTES_MASK << EHCI_TD_BYTES_SHIFT));
        }
        if (timer_monotonic_us() - start >= EHCI_CONTROL_US || !timer_delay_us(EHCI_POLL_US)) break;
    }
    return false;
}

static void prepare_control(struct ehci_control_path *path, const struct ehci_control_target *target,
    const struct usb_setup_packet *setup) {
    struct ehci_dma *dma = path->dma;
    uint32_t setup_td = path->physical + offsetof(struct ehci_dma, setup_td);
    uint32_t data_td = path->physical + offsetof(struct ehci_dma, data_td);
    uint32_t status_td = path->physical + offsetof(struct ehci_dma, status_td);
    *dma = (struct ehci_dma) {0};
    dma->setup = *setup;
    dma->qh.link = (path->physical + offsetof(struct ehci_dma, qh)) | EHCI_LINK_QH;
    dma->qh.endpoint = target->address | EHCI_QH_TOGGLE_FROM_TD | EHCI_QH_HEAD |
        ((uint32_t)target->packet_size << EHCI_QH_PACKET_SHIFT) |
        (target->tt_hub ? EHCI_QH_CONTROL : EHCI_QH_SPEED_HIGH);
    /* Asynchronous control splits use zero start and complete masks. */
    dma->qh.capabilities = EHCI_QH_MULT_ONE | ((uint32_t)target->tt_hub << EHCI_QH_TT_HUB_SHIFT) |
        ((uint32_t)target->tt_port << EHCI_QH_TT_PORT_SHIFT);
    dma->qh.overlay.next = setup_td;
    dma->qh.overlay.alternate = EHCI_LINK_END;
    dma->setup_td.next = setup->length ? data_td : status_td;
    dma->setup_td.alternate = EHCI_LINK_END;
    dma->setup_td.token = (sizeof(*setup) << EHCI_TD_BYTES_SHIFT) | EHCI_TD_PID_SETUP | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
    dma->setup_td.buffer[0] = path->physical + offsetof(struct ehci_dma, setup);
    if (setup->length) {
        dma->data_td.next = status_td;
        dma->data_td.alternate = status_td;
        dma->data_td.token = ((uint32_t)setup->length << EHCI_TD_BYTES_SHIFT) | EHCI_TD_DATA1 |
            EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
        dma->data_td.buffer[0] = path->physical + offsetof(struct ehci_dma, data);
    }
    dma->status_td.next = EHCI_LINK_END;
    dma->status_td.alternate = EHCI_LINK_END;
    dma->status_td.token = (setup->length ? EHCI_TD_PID_OUT : EHCI_TD_PID_IN) |
        EHCI_TD_DATA1 | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
}

/* Both success and failure must acknowledge schedule disable before the next
 * request can overwrite the shared descriptor page. */
static bool control_transfer(void *context, const struct ehci_control_target *target, const struct usb_setup_packet *setup,
    uint8_t *data, unsigned *received) {
    struct ehci_control_path *path = context;
    volatile uint32_t *op = path->op;
    *received = 0;
    if (target->address > 127 || setup->length > EHCI_DESCRIPTOR_BYTES ||
        (setup->length && (!data || !(setup->request_type & USB_ENDPOINT_IN)))) return false;
    if (target->tt_hub) {
        if (target->tt_hub > 127 || target->tt_hub == target->address || !target->tt_port || target->tt_port > 127 ||
            !ehci_full_speed_packet_size(target->packet_size)) return false;
    } else if (target->tt_port || target->packet_size != 64) return false;
    uint32_t command = op[EHCI_USBCMD];
    if ((command & (EHCI_CMD_RUN | EHCI_CMD_SCHEDULES)) != EHCI_CMD_RUN ||
        (op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_SCHEDULES))) return false;

    prepare_control(path, target, setup);
    __asm__ volatile ("mfence" : : : "memory");
    op[EHCI_USBCMD] = command | EHCI_CMD_ASYNC;
    bool complete = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, EHCI_STS_ASYNC, EHCI_HALT_US) &&
        wait_control(path, setup->length != 0);
    op[EHCI_USBCMD] = command;
    if (!wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, 0, EHCI_HALT_US) || !complete) return false;

    __asm__ volatile ("mfence" : : : "memory");
    if (setup->length) {
        unsigned residual = (path->dma->data_td.token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (residual > setup->length) return false;
        *received = setup->length - residual;
        for (unsigned i = 0; i < *received; i++) data[i] = path->dma->data[i];
    }
    return true;
}

static bool enumerate_root_ports(const struct ehci_probe_report *report, struct ehci_control_path *path,
    struct ehci_enumeration *bus) {
    volatile uint32_t *ports = path->op + EHCI_PORTSC;
    for (unsigned i = 0; i < report->ports; i++) {
        uint32_t status = ports[i];
        if (status == UINT32_MAX || (status & (EHCI_PORT_ENABLE | EHCI_PORT_RESET))) return false;
        if ((report->structural & EHCI_HCS_PORT_POWER) && !(status & EHCI_PORT_POWER)) {
            ports[i] = (status & ~EHCI_PORT_CHANGE) | EHCI_PORT_POWER;
        }
    }
    if (!timer_delay_us(100000)) return false;
    for (unsigned i = 0; i < report->ports; i++) {
        uint32_t status = ports[i];
        if (status == UINT32_MAX || (status & (EHCI_PORT_ENABLE | EHCI_PORT_RESET | EHCI_PORT_OVERCURRENT))) return false;
        if (!(status & EHCI_PORT_CONNECTED) || (status & EHCI_PORT_OWNER)) continue;
        ports[i] = (status & ~EHCI_PORT_CHANGE) | EHCI_PORT_RESET;
        if (!timer_delay_us(50000)) return false;
        status = ports[i];
        if (status == UINT32_MAX) return false;
        ports[i] = status & ~(EHCI_PORT_CHANGE | EHCI_PORT_RESET);
        if (!wait_mmio(&ports[i], EHCI_PORT_RESET, 0, EHCI_HALT_US) || !timer_delay_us(10000)) return false;
        status = ports[i];
        if (status == UINT32_MAX || !(status & EHCI_PORT_CONNECTED) || (status & EHCI_PORT_OVERCURRENT)) return false;
        if (!(status & EHCI_PORT_ENABLE) || (status & EHCI_PORT_OWNER)) {
            bus->deferred++;
            console_printf("ehci: root port=%u deferred non-high-speed device\n", (uint64_t)i + 1);
            continue;
        }
        if (!ehci_enumerate_device(bus, 0, (uint8_t)(i + 1), 0, false)) return false;
    }
    return true;
}

static bool retire_control_path(const struct pci_device *device, volatile uint32_t *op) {
    uint32_t command = op[EHCI_USBCMD];
    if (command != UINT32_MAX) {
        op[EHCI_USBCMD] = command & ~(EHCI_CMD_RUN | EHCI_CMD_SCHEDULES | EHCI_CMD_ASYNC_ADVANCE);
    }
    bool halted = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_HALTED | EHCI_STS_SCHEDULES,
        EHCI_STS_HALTED, EHCI_HALT_US);
    bool master_off = set_command_bits(device, PCI_COMMAND_MASTER, 0);
    if (!halted || !master_off) return false;
    op[EHCI_ASYNCLISTADDR] = 0;
    op[EHCI_CONFIGFLAG] = 0;
    return op[EHCI_ASYNCLISTADDR] == 0 && op[EHCI_CONFIGFLAG] == 0;
}

static void track_topcase(void *context, const struct ehci_control_target *target,
    uint16_t vendor, uint16_t product, uint8_t parent, uint8_t port) {
    struct ehci_control_path *path = context;
    struct ehci_probe_report *report = path->report;
    if (vendor == 0x0424 && product == 0x2513 && !parent && port == 1 && !target->tt_hub) {
        report->trial_hub = target->address;
    }
    if (vendor == 0x05ac && product == 0x0245 && report->trial_hub && parent == report->trial_hub &&
        target->tt_hub == parent && port == 2 && target->tt_port == port && target->packet_size == 8) {
        report->topcase_address = target->address;
    }
}

static void enumerate_controller(const struct pci_device *device, volatile uint32_t *op,
    struct ehci_probe_report *report, bool split, bool track_trial) {
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        console_write("ehci: enumeration unavailable; no 32-bit DMA page\n");
        return;
    }
    struct ehci_control_path path = { op, pmm_phys_to_virt(physical), (uint32_t)physical, report };
    struct ehci_enumeration bus = { .control = control_transfer, .context = &path, .split = split,
        .device_found = track_trial ? track_topcase : 0 };
    *path.dma = (struct ehci_dma) { .qh = { .link = EHCI_LINK_END,
        .overlay = { .next = EHCI_LINK_END, .alternate = EHCI_LINK_END } } };
    __asm__ volatile ("mfence" : : : "memory");
    if (report->capabilities & EHCI_HCC_64BIT) op[EHCI_CTRLDSSEGMENT] = 0;
    uint32_t list = path.physical + offsetof(struct ehci_dma, qh);
    op[EHCI_ASYNCLISTADDR] = list;
    bool complete = false;
    if (op[EHCI_ASYNCLISTADDR] == list &&
        (!(report->capabilities & EHCI_HCC_64BIT) || !op[EHCI_CTRLDSSEGMENT]) &&
        set_command_bits(device, PCI_COMMAND_MASTER, PCI_COMMAND_MASTER)) {
        op[EHCI_USBCMD] = op[EHCI_USBCMD] | EHCI_CMD_RUN;
        if (wait_mmio(&op[EHCI_USBSTS], EHCI_STS_HALTED, 0, EHCI_HALT_US)) {
            op[EHCI_CONFIGFLAG] = 1;
            complete = op[EHCI_CONFIGFLAG] == 1 && enumerate_root_ports(report, &path, &bus);
        }
    }
    bool retired = retire_control_path(device, op);
    report->enumerated = complete;
    report->dma_released = retired;
    if (retired) {
        __asm__ volatile ("mfence" : : : "memory");
        pmm_free_frame(physical);
    }
    console_printf("ehci: enumeration complete=%u devices=%u hubs=%u deferred=%u dma=%s\n",
        (uint64_t)(complete && retired), (uint64_t)bus.devices, (uint64_t)bus.hubs, (uint64_t)bus.deferred,
        retired ? "released" : "retained");
    if (split) console_printf("ehci: split devices=%u\n", (uint64_t)bus.split_devices);
}

static const char *probe_registers(const struct pci_device *device, volatile uint32_t *caps,
    unsigned mapped_bytes, struct ehci_probe_report *report) {
    uint32_t cap_header = caps[0];
    unsigned cap_length = cap_header & 0xff;
    uint32_t params = caps[1];
    uint32_t caps_params = caps[2];
    report->version = cap_header >> 16;
    report->ports = params & 0xf;
    if (report->version != EHCI_VERSION_1_0 || params == UINT32_MAX ||
        caps_params == UINT32_MAX || !report->ports || cap_length < EHCI_CAP_BYTES ||
        (cap_length & 3) || cap_length + (EHCI_PORTSC + report->ports) * 4 > mapped_bytes) {
        return "bad-capabilities";
    }

    struct ehci_handoff_result handoff = firmware_handoff(device, (uint8_t)(caps_params >> 8));
    report->handoff = handoff.status;
    if (!handoff.complete) return "handoff-refused";

    volatile uint32_t *op = caps + cap_length / 4;
    uint32_t command = op[EHCI_USBCMD];
    if (command == UINT32_MAX || (command & (EHCI_CMD_RESET | EHCI_CMD_LIGHT_RESET))) {
        return "command-unavailable";
    }
    report->was_running = (command & EHCI_CMD_RUN) != 0;
    op[EHCI_USBINTR] = 0;
    if (report->was_running) op[EHCI_USBCMD] = command & ~(EHCI_CMD_RUN | EHCI_CMD_ASYNC_ADVANCE);
    bool halted = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_HALTED, EHCI_STS_HALTED, EHCI_HALT_US);
    if (!set_command_bits(device, PCI_COMMAND_MASTER, 0)) return "master-disable-failed";
    if (!halted) return "halt-timeout";
    command = op[EHCI_USBCMD];
    if (command == UINT32_MAX || (command & (EHCI_CMD_RUN | EHCI_CMD_RESET | EHCI_CMD_LIGHT_RESET))) {
        return "halt-state-invalid";
    }

    op[EHCI_USBCMD] = EHCI_CMD_RESET;
    if (!wait_mmio(&op[EHCI_USBCMD], EHCI_CMD_RESET, 0, EHCI_RESET_US)) return "reset-timeout";
    uint32_t status = op[EHCI_USBSTS];
    if ((op[EHCI_USBCMD] & (EHCI_CMD_RUN | EHCI_CMD_RESET | EHCI_CMD_LIGHT_RESET | EHCI_CMD_SCHEDULES)) ||
        (status & (EHCI_STS_HALTED | EHCI_STS_SCHEDULES)) != EHCI_STS_HALTED ||
        op[EHCI_USBINTR] || (op[EHCI_CONFIGFLAG] & 1) ||
        (pci_read_config16(device, PCI_COMMAND) & PCI_COMMAND_MASTER)) {
        return "reset-state-invalid";
    }
    report->cap_length = cap_length;
    report->structural = params;
    report->capabilities = caps_params;
    report->reset_halted = true;
    return "reset-halted";
}

/* Return whether the temporary window is safe to reuse for another controller. */
static bool probe_controller(const struct pci_device *device, uint64_t root, enum ehci_boot_mode mode,
    struct ehci_probe_report *report, bool track_trial) {
    uint32_t bar = device->bar[0];
    uint64_t base = bar & 0xffffff00u;
    unsigned bar_type = bar & 0xff;
    if (bar_type == 4) base |= (uint64_t)device->bar[1] << 32;
    unsigned page_offset = base & (EHCI_PAGE_BYTES - 1);
    unsigned mapped_bytes = EHCI_PAGE_BYTES - page_offset;
    if ((device->header_type & 0x7f) || !base || (base >> 52) ||
        (bar_type != 0 && bar_type != 4) || mapped_bytes < EHCI_CAP_BYTES) {
        report->result = "bad-bar";
        return true;
    }
    uint16_t command = pci_read_config16(device, PCI_COMMAND);
    if (command == UINT16_MAX) {
        report->result = "pci-unreadable";
        return true;
    }
    uint64_t page = base - page_offset;
    if (!vmm_map_page_in_space(root, EHCI_MMIO_WINDOW, page,
            VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
        report->result = "map-failed";
        return false;
    }

    if (set_command_bits(device, PCI_COMMAND_MEMORY, PCI_COMMAND_MEMORY)) {
        volatile uint32_t *caps = (volatile uint32_t *)(uintptr_t)(EHCI_MMIO_WINDOW + page_offset);
        report->result = probe_registers(device, caps, mapped_bytes, report);
        if (mode != EHCI_BOOT_PROBE && report->reset_halted) {
            enumerate_controller(device, caps + report->cap_length / 4, report, mode == EHCI_BOOT_SPLIT, track_trial);
        }
    } else {
        report->result = "memory-decode-failed";
    }
    bool memory_restored = set_command_bits(device, PCI_COMMAND_MEMORY, command & PCI_COMMAND_MEMORY);
    if (!memory_restored) {
        report->result = "memory-restore-failed";
    }
    uint64_t removed;
    if (!vmm_unmap_page_in_space(root, EHCI_MMIO_WINDOW, &removed) || removed != page) {
        report->mapping = "retirement-failed";
        return false;
    }
    report->mapping = "released";
    report->access_retired = memory_restored;
    return true;
}

static int trial_controller_slot(const struct pci_device *device) {
    if (device->vendor_id != 0x8086 || device->bus || device->function != 7) return -1;
    if (device->device_id == 0x1c2d && device->device == 0x1a) return 0;
    if (device->device_id == 0x1c26 && device->device == 0x1d) return 1;
    return -1;
}

static void probe_all(enum ehci_boot_mode mode, struct ehci_trial_report *trial) {
    uint64_t root = vmm_kernel_address_space();
    if (!timer_reference_ready() || !vmm_diagnostic_sole_kernel_root(root)) {
        console_write("ehci: probe unavailable before controller access\n");
        return;
    }
    unsigned found = 0;
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (!device || device->class_code != EHCI_PCI_USB_CLASS ||
            device->subclass != EHCI_PCI_USB_SUBCLASS || device->prog_if != EHCI_PCI_PROG_IF) continue;
        found++;
        struct ehci_probe_report report = { .handoff = "not-attempted", .mapping = "none" };
        int slot = trial ? trial_controller_slot(device) : -1;
        bool reusable = probe_controller(device, root, mode, &report, slot == 0);
        if (trial) {
            trial->found++;
            trial->scans += report.enumerated;
            trial->released += report.dma_released;
            if (slot >= 0) trial->controllers[slot] = report;
        }
        console_printf("ehci: bus=%u dev=%u fn=%u vendor=%x device=%x version=%x ports=%u "
                       "handoff=%s result=%s was_running=%u mapping=%s\n",
            (uint64_t)device->bus, (uint64_t)device->device, (uint64_t)device->function,
            (uint64_t)device->vendor_id, (uint64_t)device->device_id, (uint64_t)report.version,
            (uint64_t)report.ports, report.handoff, report.result, (uint64_t)report.was_running, report.mapping);
        if (!reusable) break;
    }
    if (!found) console_write("ehci: no controller\n");
}

void ehci_boot_probe(enum ehci_boot_mode mode) {
    probe_all(mode, 0);
}

void ehci_trial_screen(const char *release) {
    console_set_framebuffer_muted(false);
    console_clear();
    console_printf("SrvrOS MacBookPro8,1 EHCI split trial\nRelease %s\n\n", release);
    console_write("1. Wait for the result. If the screen stops changing for 90 seconds,\n"
                  "   treat the trial as incomplete. No keyboard input is needed.\n"
                  "2. Photograph the entire screen, including the release and result.\n"
                  "3. Hold the power button until the Mac turns off. Press it once\n"
                  "   to start Ubuntu. Share the photo or exact result lines in Codex.\n\n");
}

static bool trial_passed(const struct ehci_trial_report *trial) {
    if (trial->found != 2 || !trial->controllers[0].topcase_address) return false;
    for (unsigned i = 0; i < 2; i++) {
        const struct ehci_probe_report *report = &trial->controllers[i];
        if (!report->reset_halted || !report->enumerated || !report->dma_released || !report->access_retired) {
            return false;
        }
    }
    return true;
}

void ehci_boot_trial(const char *release) {
    struct ehci_trial_report trial = {0};
    console_set_framebuffer_muted(false);
    console_write("Scanning EHCI controllers and the internal hub...\n");
    console_sync();
    console_set_framebuffer_muted(true);
    probe_all(EHCI_BOOT_SPLIT, &trial);
    ehci_trial_screen(release);
    console_printf("Controllers found %u. Scans complete %u. DMA released %u.\n",
        (uint64_t)trial.found, (uint64_t)trial.scans, (uint64_t)trial.released);
    const char *names[] = { "00:1a.7 8086:1c2d", "00:1d.7 8086:1c26" };
    for (unsigned i = 0; i < 2; i++) {
        const struct ehci_probe_report *report = &trial.controllers[i];
        console_printf("%s  %s  scan=%s  DMA=%s\n", names[i],
            !report->result ? "absent" : report->reset_halted && !report->access_retired ? "access-unverified" : report->result,
            report->enumerated ? "complete" : "incomplete", report->dma_released ? "released" : "unverified");
    }
    const struct ehci_probe_report *input = &trial.controllers[0];
    console_printf("Topcase 05ac:0245 via hub 0424:2513 port 2, EP0 8: %s\n",
        input->topcase_address ? "descriptors read" : "unverified");
    if (input->topcase_address) console_printf("Assigned USB address %u through hub address %u.\n",
        (uint64_t)input->topcase_address, (uint64_t)input->trial_hub);
    console_printf("ehci-trial: release=%s result=%s\n", release,
        trial_passed(&trial) ? "PASS" : "INCOMPLETE");
    console_write("Screen held for photo. Use the power button to return to Ubuntu.\n");
}
