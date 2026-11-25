#include <srvros/ehci.h>
#include <srvros/apple_trackpad.h>
#include <srvros/console.h>
#include <srvros/ehci_enumeration.h>
#include <srvros/keyboard.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/scheduler.h>
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
#define EHCI_FRINDEX (0x0cu / 4)
#define EHCI_CTRLDSSEGMENT (0x10u / 4)
#define EHCI_PERIODICLISTBASE (0x14u / 4)
#define EHCI_ASYNCLISTADDR (0x18u / 4)
#define EHCI_CONFIGFLAG (0x40u / 4)
#define EHCI_PORTSC (0x44u / 4)
#define EHCI_CMD_RUN (1u << 0)
#define EHCI_CMD_RESET (1u << 1)
#define EHCI_CMD_FRAME_SIZE (3u << 2)
#define EHCI_CMD_PERIODIC (1u << 4)
#define EHCI_CMD_ASYNC (1u << 5)
#define EHCI_CMD_SCHEDULES ((1u << 4) | (1u << 5))
#define EHCI_CMD_ASYNC_ADVANCE (1u << 6)
#define EHCI_CMD_LIGHT_RESET (1u << 7)
#define EHCI_STS_HALTED (1u << 12)
#define EHCI_STS_SCHEDULES ((1u << 14) | (1u << 15))
#define EHCI_STS_PERIODIC (1u << 14)
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
/* USB 2.0 section 9.2.6.4 allows 500 ms for the first data packet and
 * another 50 ms for status. This one-packet diagnostic allows host margin. */
#define EHCI_FLASH_DESCRIPTOR_US 600000u

struct ehci_transfer_failure {
    const char *reason;
    struct usb_setup_packet request;
    uint32_t usbsts;
    uint32_t setup_token;
    uint32_t data_token;
    uint32_t status_token;
    uint32_t overlay_token;
    uint8_t address;
    bool qtd_valid;
};

struct ehci_keyboard_schedule {
    unsigned first_frame_index;
    unsigned last_frame_index;
    unsigned frame_index_changes;
    uint32_t usbsts;
    uint32_t overlay_token;
    uint32_t c_progress_seen;
    bool sampled;
    bool qtd_loaded;
};

struct ehci_trackpad_capture {
    unsigned reports, ignored, invalid, bytes, largest, contacts;
    uint32_t token, overlay, status;
    int16_t first_x, first_y;
    bool have_position, moved, pressed, released, mode_restored;
    const char *result;
};

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
    uint8_t flash_address;
    bool flash_attempted;
    unsigned flash_received;
    uint64_t flash_wait_us;
    struct ehci_control_target keyboard_target;
    struct ehci_control_target trackpad_target;
    struct ehci_trackpad_capture trackpad;
    const char *keyboard_result;
    bool keyboard_passed;
    unsigned keyboard_reports;
    unsigned keyboard_bytes;
    uint32_t keyboard_token;
    const char *keyboard_idle;
    uint64_t keyboard_first_report_us;
    struct ehci_keyboard_schedule keyboard_schedule;
    struct ehci_scan_step stopped;
    uint32_t stopped_usbsts;
    struct ehci_transfer_failure transfer_failure;
};

struct ehci_trial_report {
    unsigned found;
    unsigned scans;
    unsigned released;
    struct ehci_probe_report controllers[2];
    struct ehci_probe_report other_failure;
    const char *keyboard_release;
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
    const struct ehci_scan_step *scan;
    int trial_slot;
};

enum ehci_monitor_state {
    EHCI_MONITOR_UNUSED, EHCI_MONITOR_RUNNING, EHCI_MONITOR_STOPPING,
    EHCI_MONITOR_RELEASED, EHCI_MONITOR_UNSAFE,
};

static struct ehci_monitor_session {
    struct ehci_probe_report report;
    struct ehci_control_path path;
    const struct pci_device *device;
    uint64_t frame_list, root, page;
    uint16_t memory_command;
    enum ehci_monitor_state state;
} monitor_session;

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

static const char *wait_control(const struct ehci_control_path *path, bool data_phase, unsigned limit_us, uint64_t start) {
    const volatile struct ehci_dma *dma = path->dma;
    for (unsigned poll = 0; poll <= limit_us / EHCI_POLL_US; poll++) {
        uint32_t setup = dma->setup_td.token;
        uint32_t data = data_phase ? dma->data_td.token : 0;
        uint32_t status = dma->status_td.token;
        if (path->op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) return "controller error";
        if ((setup | data | status) & EHCI_TD_ERROR) return "qTD error";
        if (!((setup | data | status) & EHCI_TD_ACTIVE)) {
            return (setup | status) & (EHCI_TD_BYTES_MASK << EHCI_TD_BYTES_SHIFT) ? "setup/status residual" : 0;
        }
        if (poll == limit_us / EHCI_POLL_US || timer_monotonic_us() - start >= limit_us) break;
        if (!timer_delay_us(EHCI_POLL_US)) return "completion timer failure";
    }
    return "completion timeout";
}

static void record_transfer_failure(struct ehci_control_path *path, uint8_t address,
    const struct usb_setup_packet *setup, const char *reason, bool prepared) {
    if (!path->report || path->report->transfer_failure.reason) return;
    struct ehci_transfer_failure *failure = &path->report->transfer_failure;
    *failure = (struct ehci_transfer_failure) {
        .reason = reason, .request = *setup, .address = address, .usbsts = path->op[EHCI_USBSTS],
        .qtd_valid = prepared,
    };
    if (prepared) {
        const volatile struct ehci_dma *dma = path->dma;
        failure->setup_token = dma->setup_td.token;
        failure->data_token = dma->data_td.token;
        failure->status_token = dma->status_td.token;
        failure->overlay_token = dma->qh.overlay.token;
    }
}

static void prepare_control(struct ehci_control_path *path, const struct ehci_control_target *target,
    const struct usb_setup_packet *setup, const uint8_t *out_data) {
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
        dma->data_td.alternate = out_data ? EHCI_LINK_END : status_td;
        dma->data_td.token = ((uint32_t)setup->length << EHCI_TD_BYTES_SHIFT) | EHCI_TD_DATA1 |
            (out_data ? EHCI_TD_PID_OUT : EHCI_TD_PID_IN) | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
        dma->data_td.buffer[0] = path->physical + offsetof(struct ehci_dma, data);
        if (out_data) {
            for (unsigned i = 0; i < setup->length; i++) dma->data[i] = out_data[i];
        }
    }
    dma->status_td.next = EHCI_LINK_END;
    dma->status_td.alternate = EHCI_LINK_END;
    dma->status_td.token = (setup->length && !out_data ? EHCI_TD_PID_OUT : EHCI_TD_PID_IN) |
        EHCI_TD_DATA1 | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
}

static bool flash_descriptor_request(const struct ehci_control_path *path, const struct ehci_control_target *target,
    const struct usb_setup_packet *setup) {
    return path->trial_slot == 1 && path->report && path->scan && path->report->trial_hub &&
        path->scan->hub == path->report->trial_hub &&
        path->scan->port == 2 && !target->tt_hub && target->packet_size == 64 && !target->address &&
        setup->request_type == 0x80 && setup->request == USB_REQ_GET_DESCRIPTOR &&
        setup->value == (USB_DESC_DEVICE << 8) && !setup->index && setup->length == 64;
}

/* Both success and failure must acknowledge schedule disable before the next
 * request can overwrite the shared descriptor page. */
static bool control_transfer_payload(struct ehci_control_path *path, const struct ehci_control_target *target,
    const struct usb_setup_packet *setup, uint8_t *in_data, const uint8_t *out_data, unsigned *received) {
    volatile uint32_t *op = path->op;
    *received = 0;
    bool in = (setup->request_type & USB_ENDPOINT_IN) != 0;
    if (target->address > 127 || setup->length > EHCI_DESCRIPTOR_BYTES ||
        (setup->length && (in ? !in_data || out_data : !out_data || in_data))) return false;
    if (target->tt_hub) {
        if (target->tt_hub > 127 || target->tt_hub == target->address || !target->tt_port || target->tt_port > 127 ||
            !ehci_full_speed_packet_size(target->packet_size)) return false;
    } else if (target->tt_port || target->packet_size != 64) return false;
    uint32_t command = op[EHCI_USBCMD];
    if ((command & (EHCI_CMD_RUN | EHCI_CMD_SCHEDULES)) != EHCI_CMD_RUN ||
        (op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_SCHEDULES))) {
        record_transfer_failure(path, target->address, setup, "schedule busy or halted", false);
        return false;
    }

    prepare_control(path, target, setup, out_data);
    __asm__ volatile ("mfence" : : : "memory");
    op[EHCI_USBCMD] = command | EHCI_CMD_ASYNC;
    bool flash_probe = flash_descriptor_request(path, target, setup);
    bool enabled = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, EHCI_STS_ASYNC, EHCI_HALT_US);
    uint64_t start = timer_monotonic_us();
    const char *failure = enabled ? wait_control(path, setup->length != 0,
        flash_probe ? EHCI_FLASH_DESCRIPTOR_US : EHCI_CONTROL_US, start) : "schedule enable timeout";
    if (flash_probe) {
        path->report->flash_attempted = true;
        path->report->flash_wait_us = timer_monotonic_us() - start;
    }
    if (failure) record_transfer_failure(path, target->address, setup, failure, true);
    op[EHCI_USBCMD] = command;
    if (!wait_mmio(&op[EHCI_USBSTS], EHCI_STS_ASYNC, 0, EHCI_HALT_US)) {
        record_transfer_failure(path, target->address, setup, "schedule disable timeout", true);
        return false;
    }
    if (failure) return false;

    __asm__ volatile ("mfence" : : : "memory");
    if (setup->length) {
        unsigned residual = (path->dma->data_td.token >> EHCI_TD_BYTES_SHIFT) & EHCI_TD_BYTES_MASK;
        if (residual > setup->length || (out_data && residual)) {
            record_transfer_failure(path, target->address, setup, "invalid data residual", true);
            return false;
        }
        *received = setup->length - residual;
        if (flash_probe) path->report->flash_received = *received;
        if (in_data) {
            for (unsigned i = 0; i < *received; i++) in_data[i] = path->dma->data[i];
        }
    }
    return true;
}

static bool control_transfer(void *context, const struct ehci_control_target *target, const struct usb_setup_packet *setup,
    uint8_t *data, unsigned *received) {
    return control_transfer_payload(context, target, setup, data, NULL, received);
}

struct ehci_periodic_endpoint {
    unsigned address, packet, interval, length;
};

#define EHCI_FRAME_ENTRIES (EHCI_PAGE_BYTES / sizeof(uint32_t))

static void prepare_periodic_frames(uint32_t *frames, uint32_t qh, unsigned interval) {
    for (unsigned i = 0; i < EHCI_FRAME_ENTRIES; i++)
        frames[i] = i % interval ? EHCI_LINK_END : qh | EHCI_LINK_QH;
}

static const char *configure_periodic_frames(struct ehci_control_path *path,
    const struct ehci_periodic_endpoint *endpoint, uint64_t *frame_list) {
    if (path->op[EHCI_USBCMD] & EHCI_CMD_FRAME_SIZE) return "unsupported frame-list size";
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        return "periodic DMA unavailable";
    }
    *frame_list = physical;
    prepare_periodic_frames(pmm_phys_to_virt(physical), path->physical + offsetof(struct ehci_dma, qh),
        endpoint->interval);
    __asm__ volatile ("mfence" : : : "memory");
    path->op[EHCI_PERIODICLISTBASE] = (uint32_t)physical;
    return path->op[EHCI_PERIODICLISTBASE] == physical ? NULL : "periodic frame-list readback failed";
}

static void prepare_periodic_transfer(struct ehci_control_path *path, const struct ehci_control_target *target,
    const struct ehci_periodic_endpoint *endpoint, uint32_t toggle) {
    struct ehci_dma *dma = path->dma;
    *dma = (struct ehci_dma) {0};
    dma->qh.link = EHCI_LINK_END;
    dma->qh.endpoint = target->address | ((endpoint->address & 15u) << 8) |
        (endpoint->packet << EHCI_QH_PACKET_SHIFT);
    /* Full speed, one start-split at uframe 0 and complete-splits at 2–4. */
    dma->qh.capabilities = EHCI_QH_MULT_ONE | ((uint32_t)target->tt_hub << EHCI_QH_TT_HUB_SHIFT) |
        ((uint32_t)target->tt_port << EHCI_QH_TT_PORT_SHIFT) | 0x1c01u;
    dma->qh.overlay.next = path->physical + offsetof(struct ehci_dma, setup_td);
    dma->qh.overlay.alternate = EHCI_LINK_END;
    dma->qh.overlay.token = toggle;
    dma->setup_td.next = dma->setup_td.alternate = EHCI_LINK_END;
    dma->setup_td.token = (endpoint->length << EHCI_TD_BYTES_SHIFT) | EHCI_TD_PID_IN | EHCI_TD_RETRIES | EHCI_TD_ACTIVE;
    dma->setup_td.buffer[0] = path->physical + offsetof(struct ehci_dma, data);
}

static bool periodic_idle(const struct ehci_control_path *path) {
    return (path->op[EHCI_USBCMD] & (EHCI_CMD_RUN | EHCI_CMD_SCHEDULES)) == EHCI_CMD_RUN &&
        !(path->op[EHCI_USBSTS] & (EHCI_STS_HALTED | EHCI_STS_SCHEDULES | EHCI_STS_HOST_ERROR));
}

static bool enable_periodic(const struct ehci_control_path *path) {
    path->op[EHCI_USBCMD] |= EHCI_CMD_PERIODIC;
    return wait_mmio(&path->op[EHCI_USBSTS], EHCI_STS_PERIODIC, EHCI_STS_PERIODIC, EHCI_HALT_US);
}

static bool disable_periodic(const struct ehci_control_path *path) {
    path->op[EHCI_USBCMD] &= ~EHCI_CMD_PERIODIC;
    return wait_mmio(&path->op[EHCI_USBSTS], EHCI_STS_PERIODIC, 0, EHCI_HALT_US);
}

#include "ehci_keyboard.inc"
#include "ehci_trackpad.inc"

static uint32_t read_root_port(struct ehci_enumeration *bus, volatile uint32_t *port) {
    uint32_t status = *port;
    bus->step.port_status = status;
    bus->step.port_status_valid = true;
    return status;
}

static bool enumerate_root_ports(const struct ehci_probe_report *report, struct ehci_control_path *path,
    struct ehci_enumeration *bus) {
    volatile uint32_t *ports = path->op + EHCI_PORTSC;
    for (unsigned i = 0; i < report->ports; i++) {
        bus->step = (struct ehci_scan_step) { .operation = "root port preflight", .port = (uint8_t)(i + 1) };
        uint32_t status = read_root_port(bus, &ports[i]);
        if (status == UINT32_MAX || (status & (EHCI_PORT_ENABLE | EHCI_PORT_RESET))) return false;
        if ((report->structural & EHCI_HCS_PORT_POWER) && !(status & EHCI_PORT_POWER)) {
            ports[i] = (status & ~EHCI_PORT_CHANGE) | EHCI_PORT_POWER;
        }
    }
    bus->step = (struct ehci_scan_step) { .operation = "root port power and debounce" };
    if (!timer_delay_us(100000)) return false;
    for (unsigned i = 0; i < report->ports; i++) {
        bus->step = (struct ehci_scan_step) { .operation = "root port status", .port = (uint8_t)(i + 1) };
        uint32_t status = read_root_port(bus, &ports[i]);
        if (status == UINT32_MAX || (status & (EHCI_PORT_ENABLE | EHCI_PORT_RESET | EHCI_PORT_OVERCURRENT))) return false;
        if (!(status & EHCI_PORT_CONNECTED) || (status & EHCI_PORT_OWNER)) continue;
        bus->step.operation = "hold root port reset";
        ports[i] = (status & ~EHCI_PORT_CHANGE) | EHCI_PORT_RESET;
        if (!timer_delay_us(50000)) return false;
        status = read_root_port(bus, &ports[i]);
        if (status == UINT32_MAX) return false;
        ports[i] = status & ~(EHCI_PORT_CHANGE | EHCI_PORT_RESET);
        bus->step.operation = "release root port reset";
        if (!wait_mmio(&ports[i], EHCI_PORT_RESET, 0, EHCI_HALT_US)) {
            read_root_port(bus, &ports[i]);
            return false;
        }
        bus->step.operation = "root port reset recovery";
        if (!timer_delay_us(10000)) return false;
        status = read_root_port(bus, &ports[i]);
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

static bool retire_control_path(const struct pci_device *device, volatile uint32_t *op, bool periodic) {
    uint32_t command = op[EHCI_USBCMD];
    if (command != UINT32_MAX) {
        op[EHCI_USBCMD] = command & ~(EHCI_CMD_RUN | EHCI_CMD_SCHEDULES | EHCI_CMD_ASYNC_ADVANCE);
    }
    bool halted = wait_mmio(&op[EHCI_USBSTS], EHCI_STS_HALTED | EHCI_STS_SCHEDULES,
        EHCI_STS_HALTED, EHCI_HALT_US);
    bool master_off = set_command_bits(device, PCI_COMMAND_MASTER, 0);
    if (!halted || !master_off) return false;
    op[EHCI_ASYNCLISTADDR] = 0;
    if (periodic) op[EHCI_PERIODICLISTBASE] = 0;
    op[EHCI_CONFIGFLAG] = 0;
    return op[EHCI_ASYNCLISTADDR] == 0 && op[EHCI_CONFIGFLAG] == 0 &&
        (!periodic || op[EHCI_PERIODICLISTBASE] == 0);
}

static void track_trial_device(void *context, const struct ehci_control_target *target,
    uint16_t vendor, uint16_t product, uint8_t parent, uint8_t port,
    const uint8_t *configuration, uint16_t length) {
    struct ehci_control_path *path = context;
    struct ehci_probe_report *report = path->report;
    if (vendor == 0x0424 && product == 0x2513 && !parent && port == 1 && !target->tt_hub) {
        report->trial_hub = target->address;
    }
    if (!report->trial_hub || parent != report->trial_hub || port != 2) return;
    if (path->trial_slot == 0 && vendor == 0x05ac && product == 0x0245 &&
        target->tt_hub == parent && target->tt_port == port && target->packet_size == 8) {
        report->topcase_address = target->address;
        if (captured_keyboard_interface(configuration, length)) report->keyboard_target = *target;
        if (captured_trackpad_interfaces(configuration, length)) report->trackpad_target = *target;
    }
    if (path->trial_slot == 1 && vendor == 0x13fe && product == 0x6700 &&
        !target->tt_hub && target->packet_size == 64) report->flash_address = target->address;
}

static void enumerate_controller(const struct pci_device *device, volatile uint32_t *op,
    struct ehci_probe_report *report, enum ehci_boot_mode mode, int trial_slot, const char *keyboard_release) {
    bool keep_keyboard = mode == EHCI_BOOT_KEYBOARD_MONITOR;
    bool split = mode != EHCI_BOOT_ENUMERATE;
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!physical || physical > UINT32_MAX) {
        if (physical) pmm_free_frame(physical);
        report->stopped.operation = "allocate control DMA";
        report->stopped_usbsts = op[EHCI_USBSTS];
        console_write("ehci: enumeration unavailable; no 32-bit DMA page\n");
        return;
    }
    struct ehci_control_path path = { .op = op, .dma = pmm_phys_to_virt(physical), .physical = (uint32_t)physical,
        .report = report, .trial_slot = trial_slot };
    struct ehci_enumeration bus = { .control = control_transfer, .context = &path, .split = split,
        .device_found = trial_slot >= 0 ? track_trial_device : 0, .step = { .operation = "configure DMA and bus mastering" } };
    path.scan = &bus.step;
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
        bus.step.operation = "start controller";
        op[EHCI_USBCMD] = op[EHCI_USBCMD] | EHCI_CMD_RUN;
        if (wait_mmio(&op[EHCI_USBSTS], EHCI_STS_HALTED, 0, EHCI_HALT_US)) {
            bus.step.operation = "route root ports to EHCI";
            op[EHCI_CONFIGFLAG] = 1;
            complete = op[EHCI_CONFIGFLAG] == 1 && enumerate_root_ports(report, &path, &bus);
        }
    }
    if (!complete) {
        report->stopped = bus.step;
        report->stopped_usbsts = report->transfer_failure.reason ? report->transfer_failure.usbsts : op[EHCI_USBSTS];
    }
    uint64_t frame_list = 0;
    if (complete && keep_keyboard) {
        report->keyboard_result = configure_keyboard(&path, &frame_list);
        if (!report->keyboard_result) {
            report->enumerated = true;
            path.scan = NULL;
            monitor_session.path = path;
            monitor_session.frame_list = frame_list;
            return;
        }
    } else if (complete && mode == EHCI_BOOT_TRACKPAD_TRIAL) {
        run_trackpad_trial(&path, &frame_list);
    } else if (complete && keyboard_release && trial_slot == 0) {
        report->keyboard_result = run_keyboard_trial(&path, keyboard_release, &frame_list);
        console_set_framebuffer_muted(true);
    }
    bool retired = retire_control_path(device, op, frame_list != 0);
    if (complete && !retired) {
        report->stopped.operation = "retire controller DMA";
        report->stopped_usbsts = op[EHCI_USBSTS];
    }
    report->enumerated = complete;
    report->dma_released = retired;
    if (keep_keyboard && !retired) monitor_session.state = EHCI_MONITOR_UNSAFE;
    if (retired) {
        __asm__ volatile ("mfence" : : : "memory");
        pmm_free_frame(physical);
        if (frame_list) pmm_free_frame(frame_list);
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
    struct ehci_probe_report *report, int trial_slot, const char *keyboard_release) {
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
            bool keep_keyboard = mode == EHCI_BOOT_KEYBOARD_MONITOR;
            enumerate_controller(device, caps + report->cap_length / 4, report, mode, trial_slot, keyboard_release);
            if (keep_keyboard && monitor_session.frame_list) {
                monitor_session.device = device;
                monitor_session.root = root;
                monitor_session.page = page;
                monitor_session.memory_command = command & PCI_COMMAND_MEMORY;
                return false; /* The worker now owns the window. */
            }
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

static bool ehci_usb_controller(const struct pci_device *device) {
    return device && device->class_code == EHCI_PCI_USB_CLASS &&
        device->subclass == EHCI_PCI_USB_SUBCLASS && device->prog_if == EHCI_PCI_PROG_IF;
}

static const struct pci_device *captured_topcase_controller(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (ehci_usb_controller(device) && trial_controller_slot(device) == 0) return device;
    }
    return NULL;
}

static void log_transfer_failure(const struct ehci_transfer_failure *failure) {
    if (!failure->reason) return;
    const struct usb_setup_packet *setup = &failure->request;
    console_printf("ehci: control failure=%s address=%u type=%x request=%x value=%x index=%u length=%u\n",
        failure->reason, (uint64_t)failure->address, (uint64_t)setup->request_type, (uint64_t)setup->request,
        (uint64_t)setup->value, (uint64_t)setup->index, (uint64_t)setup->length);
    console_printf("ehci: control USBSTS=%x setup_qtd=%x data_qtd=%x status_qtd=%x\n",
        (uint64_t)failure->usbsts, (uint64_t)failure->setup_token, (uint64_t)failure->data_token,
        (uint64_t)failure->status_token);
}

static enum ehci_monitor_state monitor_state(void) {
    return __atomic_load_n(&monitor_session.state, __ATOMIC_ACQUIRE);
}

static bool release_monitor_keyboard(void) {
    struct ehci_monitor_session *session = &monitor_session;
    if (!retire_control_path(session->device, session->path.op, true)) return false;
    __asm__ volatile ("mfence" : : : "memory");
    pmm_free_frame(session->path.physical);
    pmm_free_frame(session->frame_list);
    bool restored = set_command_bits(session->device, PCI_COMMAND_MEMORY, session->memory_command);
    uint64_t removed;
    bool unmapped = vmm_unmap_page_in_space(session->root, EHCI_MMIO_WINDOW, &removed) && removed == session->page;
    return restored && unmapped;
}

static bool monitor_poll_due(void *arg) {
    return monitor_state() != EHCI_MONITOR_RUNNING || timer_ticks() >= *(uint64_t *)arg;
}

static bool monitor_worker_eligible(void *arg) {
    (void)arg;
    return monitor_state() == EHCI_MONITOR_RUNNING || monitor_state() == EHCI_MONITOR_STOPPING;
}

static void monitor_keyboard_worker(void *arg) {
    struct ehci_control_path *path = arg;
    struct keyboard_hid_state state = {0};
    struct scheduler_wait_queue wake = {0};
    uint64_t start = timer_monotonic_us();
    uint32_t toggle = 0;
    const char *failure = monitor_state() == EHCI_MONITOR_RUNNING ? arm_keyboard_poll(path, toggle) : NULL;
    while (!failure && monitor_state() == EHCI_MONITOR_RUNNING) {
        sample_keyboard_schedule(path);
        uint32_t token = path->report->keyboard_token;
        if ((path->report->keyboard_schedule.usbsts & (EHCI_STS_HALTED | EHCI_STS_HOST_ERROR)) ||
            (token & EHCI_TD_ERROR)) {
            failure = "interrupt transfer error";
            break;
        }
        uint8_t bytes[EHCI_KEYBOARD_REPORT];
        bool completed = !(token & EHCI_TD_ACTIVE);
        if (completed) {
            failure = finish_keyboard_poll(path, start, &toggle, bytes, NULL);
            if (failure) break;
        }
        uint64_t flags;
        __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
        if (completed) keyboard_inject_hid_report(bytes, &state);
        keyboard_repeat_hid(&state, timer_monotonic_us());
        if (flags & (1u << 9)) __asm__ volatile ("sti" : : : "memory");
        if (completed && monitor_state() == EHCI_MONITOR_RUNNING) failure = arm_keyboard_poll(path, toggle);
        if (!failure) {
            uint64_t due = timer_ticks() + 1;
            if (!scheduler_wait_timeout(&wake, monitor_poll_due, &due, due)) scheduler_yield();
        }
    }
    bool released = release_monitor_keyboard();
    if (failure) console_printf("\nehci-monitor: keyboard stopped (%s), resources=%s\n",
        failure, released ? "released" : "unverified");
    __atomic_store_n(&monitor_session.state, released ? EHCI_MONITOR_RELEASED : EHCI_MONITOR_UNSAFE,
        __ATOMIC_RELEASE);
}

void ehci_monitor_start(void) {
    uint64_t root = vmm_kernel_address_space();
    if (!timer_reference_ready() || !vmm_diagnostic_sole_kernel_root(root)) {
        console_write("ehci-monitor: keyboard unavailable (timer or address space)\n");
        return;
    }
    const struct pci_device *device = captured_topcase_controller();
    if (device) {
        struct ehci_probe_report *report = &monitor_session.report;
        report->mapping = "none";
        bool reusable = probe_controller(device, root, EHCI_BOOT_KEYBOARD_MONITOR, report, 0, NULL);
        if (!monitor_session.frame_list) {
            if (!reusable || (report->reset_halted && !report->access_retired)) monitor_session.state = EHCI_MONITOR_UNSAFE;
            const char *reason = report->keyboard_result ? report->keyboard_result :
                (report->stopped.operation ? report->stopped.operation : report->result);
            console_printf("ehci-monitor: keyboard unavailable (%s)\n", reason);
            log_transfer_failure(&report->transfer_failure);
            return;
        }
        monitor_session.state = EHCI_MONITOR_RUNNING;
        if (!scheduler_spawn_io_service("ehci-keyboard", monitor_keyboard_worker, &monitor_session.path,
                monitor_worker_eligible)) {
            monitor_session.state = release_monitor_keyboard() ? EHCI_MONITOR_RELEASED : EHCI_MONITOR_UNSAFE;
            console_write("ehci-monitor: keyboard service could not start\n");
            return;
        }
        console_printf("ehci-monitor: built-in keyboard at address %u, idle=%s\n",
            (uint64_t)report->keyboard_target.address, report->keyboard_idle);
        return;
    }
    console_write("ehci-monitor: captured controller absent, serial input available\n");
}

bool ehci_monitor_stop(void) {
    if (monitor_state() == EHCI_MONITOR_UNUSED) return true;
    enum ehci_monitor_state state = EHCI_MONITOR_RUNNING;
    __atomic_compare_exchange_n(&monitor_session.state, &state, EHCI_MONITOR_STOPPING,
        false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    uint64_t start = timer_monotonic_us();
    while ((state = monitor_state()) == EHCI_MONITOR_STOPPING && timer_monotonic_us() - start < 1000000u)
        scheduler_yield();
    if (state != EHCI_MONITOR_RELEASED) {
        console_write("ehci-monitor: release unverified. Photograph this screen and restart manually.\n");
        return false;
    }
    console_printf("ehci-monitor: reports=%u dma=released mapping=released\n",
        (uint64_t)monitor_session.report.keyboard_reports);
    console_sync();
    for (unsigned second = 0; second < 8; second++)
        if (!timer_delay_us(1000000u)) break;
    return true;
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
        if (!ehci_usb_controller(device)) continue;
        found++;
        struct ehci_probe_report report = { .handoff = "not-attempted", .mapping = "none" };
        int slot = trial ? trial_controller_slot(device) : -1;
        bool reusable = probe_controller(device, root, mode, &report, slot, trial ? trial->keyboard_release : 0);
        if (trial) {
            trial->found++;
            trial->scans += report.enumerated;
            trial->released += report.dma_released;
            if (slot >= 0) trial->controllers[slot] = report;
            else if (report.stopped.operation && !trial->other_failure.stopped.operation) trial->other_failure = report;
        }
        console_printf("ehci: bus=%u dev=%u fn=%u vendor=%x device=%x version=%x ports=%u "
                       "handoff=%s result=%s was_running=%u mapping=%s\n",
            (uint64_t)device->bus, (uint64_t)device->device, (uint64_t)device->function,
            (uint64_t)device->vendor_id, (uint64_t)device->device_id, (uint64_t)report.version,
            (uint64_t)report.ports, report.handoff, report.result, (uint64_t)report.was_running, report.mapping);
        log_transfer_failure(&report.transfer_failure);
        if (!reusable) break;
    }
    if (!found) console_write("ehci: no controller\n");
}

void ehci_boot_probe(enum ehci_boot_mode mode) {
    probe_all(mode, 0);
}

void ehci_trial_screen(const char *release, bool keyboard) {
    console_set_framebuffer_muted(false);
    console_clear();
    console_printf("SrvrOS MacBookPro8,1 EHCI %s trial\nRelease %s\n", keyboard ? "keyboard" : "split", release);
    if (keyboard) console_write("1. At srv>, type echo Ehci-42 and press Return within 90 seconds.\n"
                                "   Use the built-in keyboard. Delete corrects a typo.\n");
    else console_write("1. Wait for the result. If the screen stops changing for 90 seconds,\n"
                       "   treat the trial as incomplete. No keyboard input is needed.\n");
    console_write(
                  "2. Photograph the entire screen, including the release and result.\n"
                  "3. Hold the power button until the Mac turns off. Press it once\n"
                  "   to start Ubuntu. Share the photo or exact result lines in Codex.\n\n");
}

static bool trial_passed(const struct ehci_trial_report *trial) {
    const struct ehci_probe_report *storage = &trial->controllers[1];
    if (trial->found != 2 || !trial->controllers[0].topcase_address || !storage->flash_address ||
        !storage->flash_attempted || storage->flash_received != 18) return false;
    for (unsigned i = 0; i < 2; i++) {
        const struct ehci_probe_report *report = &trial->controllers[i];
        if (!report->reset_halted || !report->enumerated || !report->dma_released || !report->access_retired) {
            return false;
        }
    }
    return true;
}

static void print_scan_stop(const struct ehci_probe_report *report) {
    const struct ehci_scan_step *step = &report->stopped;
    if (!step->operation) return;
    if (step->hub) {
        console_printf("  Stop at hub %u port %u during %s\n",
            (uint64_t)step->hub, (uint64_t)step->port, step->operation);
    } else if (step->port) {
        console_printf("  Stop at root port %u during %s\n", (uint64_t)step->port, step->operation);
    } else {
        console_printf("  Stop during %s\n", step->operation);
    }
    if (step->port_status_valid) {
        console_printf("  USBSTS=%x  last port status=%x\n", (uint64_t)report->stopped_usbsts, (uint64_t)step->port_status);
    } else {
        console_printf("  USBSTS=%x  last port status unavailable\n", (uint64_t)report->stopped_usbsts);
    }
    const struct ehci_transfer_failure *failure = &report->transfer_failure;
    if (failure->reason) {
        console_printf("  Control %s at USB address %u, request %u\n", failure->reason,
            (uint64_t)failure->address, (uint64_t)failure->request.request);
        if (failure->qtd_valid) {
            console_printf("  qTD status bytes (decimal) setup=%u data=%u status=%u\n",
                (uint64_t)(uint8_t)failure->setup_token, (uint64_t)(uint8_t)failure->data_token,
                (uint64_t)(uint8_t)failure->status_token);
            if (report->flash_attempted) console_printf("  QH overlay token=%x\n", (uint64_t)failure->overlay_token);
        }
    }
}

void ehci_boot_trackpad_trial(const char *release) {
    console_set_framebuffer_muted(false);
    console_clear();
    console_printf("SrvrOS MacBookPro8,1 trackpad trial\nRelease %s\n\n", release);
    console_write("Wait for Trackpad ready. Then move one finger across the pad,\n"
                  "place two fingers on it, and press and release the clickpad.\n"
                  "Photograph the result. Power off and start again to return to Ubuntu.\n\n"
                  "Scanning for the built-in trackpad...\n");
    console_sync();
    uint64_t root = vmm_kernel_address_space();
    const struct pci_device *device = captured_topcase_controller();
    struct ehci_probe_report report = { .handoff = "not-attempted", .mapping = "none" };
    if (device && timer_reference_ready() && vmm_diagnostic_sole_kernel_root(root)) {
        console_set_framebuffer_muted(true);
        probe_controller(device, root, EHCI_BOOT_TRACKPAD_TRIAL, &report, 0, NULL);
        console_set_framebuffer_muted(false);
    }
    struct ehci_trackpad_capture *capture = &report.trackpad;
    if (!device) console_write("Captured topcase controller absent.\n");
    else if (!report.result) console_write("Trackpad probe unavailable before register access.\n");
    else console_printf("00:1a.7 %s. Scan %s. DMA %s. Mapping %s.\n", report.result,
        report.enumerated ? "complete" : "incomplete", report.dma_released ? "released" : "unverified", report.mapping);
    print_scan_stop(&report);
    console_printf("Trackpad %s. Reports %u. Last %u bytes. Largest %u bytes.\n",
        capture->result ? capture->result : capture->reports ? "captured" : "unavailable",
        (uint64_t)capture->reports, (uint64_t)capture->bytes, (uint64_t)capture->largest);
    console_printf("Motion %s. Max contacts %u. Click %s. Mode %s.\n",
        capture->moved ? "yes" : "unverified", (uint64_t)capture->contacts,
        capture->released ? "pressed and released" : capture->pressed ? "pressed" : "unverified",
        capture->mode_restored ? "restored" : "unverified");
    console_printf("Invalid %u. Ignored %u.\n", (uint64_t)capture->invalid, (uint64_t)capture->ignored);
    bool passed = !capture->result && !capture->invalid && trackpad_capture_complete(capture) &&
        capture->mode_restored && report.dma_released && report.access_retired;
    if (!passed && capture->token) console_printf("USBSTS=%x qTD=%x QH=%x\n",
        (uint64_t)capture->status, (uint64_t)capture->token, (uint64_t)capture->overlay);
    if (report.transfer_failure.reason) log_transfer_failure(&report.transfer_failure);
    console_printf("ehci-trackpad: release=%s result=%s\n", release, passed ? "PASS" : "INCOMPLETE");
    console_write("Screen held for photo. Use the power button to return to Ubuntu.\n");
}

void ehci_boot_trial(const char *release, bool keyboard) {
    struct ehci_trial_report trial = { .keyboard_release = keyboard ? release : 0 };
    console_set_framebuffer_muted(false);
    console_write("Scanning EHCI controllers and the internal hub...\n");
    console_sync();
    console_set_framebuffer_muted(true);
    probe_all(EHCI_BOOT_SPLIT, &trial);
    ehci_trial_screen(release, keyboard);
    console_printf("Controllers found %u. Scans complete %u. DMA released %u.\n",
        (uint64_t)trial.found, (uint64_t)trial.scans, (uint64_t)trial.released);
    const char *names[] = { "00:1a.7 8086:1c2d", "00:1d.7 8086:1c26" };
    for (unsigned i = 0; i < 2; i++) {
        const struct ehci_probe_report *report = &trial.controllers[i];
        console_printf("%s  %s  scan=%s  DMA=%s\n", names[i],
            !report->result ? "absent" : report->reset_halted && !report->access_retired ? "access-unverified" : report->result,
            report->enumerated ? "complete" : "incomplete", report->dma_released ? "released" : "unverified");
        print_scan_stop(report);
    }
    if (trial.other_failure.stopped.operation) print_scan_stop(&trial.other_failure);
    const struct ehci_probe_report *input = &trial.controllers[0];
    console_printf("Topcase 05ac:0245 via hub 0424:2513 port 2, EP0 8: %s\n",
        input->topcase_address ? "descriptors read" : "unverified");
    if (input->topcase_address) console_printf("Assigned USB address %u through hub address %u.\n",
        (uint64_t)input->topcase_address, (uint64_t)input->trial_hub);
    const struct ehci_probe_report *storage = &trial.controllers[1];
    if (storage->flash_attempted) console_printf("Flash EP0 wait %u / %u us. Descriptor bytes %u.\n",
        storage->flash_wait_us, (uint64_t)EHCI_FLASH_DESCRIPTOR_US, (uint64_t)storage->flash_received);
    console_printf("Flash 13fe:6700 USB address %u (0 means unverified).\n", (uint64_t)storage->flash_address);
    bool keyboard_passed = input->keyboard_passed;
    if (keyboard) {
        console_printf("Keyboard %s. Reports %u. Last bytes %u.\n",
            keyboard_passed ? "PASS" : input->keyboard_result ? input->keyboard_result : "unavailable",
            (uint64_t)input->keyboard_reports, (uint64_t)input->keyboard_bytes);
        if (input->keyboard_idle) {
            if (input->keyboard_reports) console_printf("Keyboard idle 32 ms %s. First report %u us.\n",
                input->keyboard_idle, input->keyboard_first_report_us);
            else console_printf("Keyboard idle 32 ms %s.\n", input->keyboard_idle);
        }
        const struct ehci_keyboard_schedule *schedule = &input->keyboard_schedule;
        if (schedule->sampled && !keyboard_passed) {
            console_printf("FRINDEX %u to %u changes=%u loaded=%u C-progress=%u\n",
                (uint64_t)schedule->first_frame_index, (uint64_t)schedule->last_frame_index,
                (uint64_t)schedule->frame_index_changes, (uint64_t)schedule->qtd_loaded,
                (uint64_t)schedule->c_progress_seen);
            console_printf("USBSTS=%x qTD=%x QH=%x\n",
                (uint64_t)schedule->usbsts, (uint64_t)input->keyboard_token, (uint64_t)schedule->overlay_token);
        }
        if (keyboard_passed) console_write("srv> echo Ehci-42\nEhci-42\n");
    }
    console_printf("ehci-trial: release=%s result=%s\n", release,
        trial_passed(&trial) && (!keyboard || keyboard_passed) ? "PASS" : "INCOMPLETE");
    console_write("Screen held for photo. Use the power button to return to Ubuntu.\n");
}
