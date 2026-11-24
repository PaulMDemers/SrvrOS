#include <srvros/ehci.h>
#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

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
#define EHCI_CONFIGFLAG (0x40u / 4)
#define EHCI_CMD_RUN (1u << 0)
#define EHCI_CMD_RESET (1u << 1)
#define EHCI_CMD_SCHEDULES ((1u << 4) | (1u << 5))
#define EHCI_CMD_ASYNC_ADVANCE (1u << 6)
#define EHCI_CMD_LIGHT_RESET (1u << 7)
#define EHCI_STS_HALTED (1u << 12)
#define EHCI_STS_SCHEDULES ((1u << 14) | (1u << 15))

#define EHCI_LEGACY_CAP 1u
#define EHCI_LEGACY_BIOS (1u << 16)
#define EHCI_LEGACY_OS (1u << 24)
#define EHCI_LEGACY_SMI_ENABLES 0xe03fu
#define EHCI_HANDOFF_POLLS 1000u
#define EHCI_HALT_US 20000u
#define EHCI_RESET_US 250000u
#define EHCI_POLL_US 10u

struct ehci_probe_report {
    const char *result;
    const char *handoff;
    const char *mapping;
    unsigned version;
    unsigned ports;
    bool was_running;
};

struct ehci_handoff_result {
    const char *status;
    bool complete;
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
        (cap_length & 3) || cap_length + (EHCI_CONFIGFLAG + 1) * 4 > mapped_bytes) {
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
    return "reset-halted";
}

/* Return whether the temporary window is safe to reuse for another controller. */
static bool probe_controller(const struct pci_device *device, uint64_t root, struct ehci_probe_report *report) {
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
    } else {
        report->result = "memory-decode-failed";
    }
    if (!set_command_bits(device, PCI_COMMAND_MEMORY, command & PCI_COMMAND_MEMORY)) {
        report->result = "memory-restore-failed";
    }
    uint64_t removed;
    if (!vmm_unmap_page_in_space(root, EHCI_MMIO_WINDOW, &removed) || removed != page) {
        report->mapping = "retirement-failed";
        return false;
    }
    report->mapping = "released";
    return true;
}

void ehci_boot_probe(void) {
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
        bool reusable = probe_controller(device, root, &report);
        console_printf("ehci: bus=%u dev=%u fn=%u vendor=%x device=%x version=%x ports=%u "
                       "handoff=%s result=%s was_running=%u mapping=%s\n",
            (uint64_t)device->bus, (uint64_t)device->device, (uint64_t)device->function,
            (uint64_t)device->vendor_id, (uint64_t)device->device_id, (uint64_t)report.version,
            (uint64_t)report.ports, report.handoff, report.result, (uint64_t)report.was_running, report.mapping);
        if (!reusable) break;
    }
    if (!found) console_write("ehci: no controller\n");
}
