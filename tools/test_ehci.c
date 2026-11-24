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
    if (delay == 1000) {
        handoff_delays++;
        if (!firmware_stuck && handoff_delays == 3) pci_config[LEGACY_OFFSET / 4] &= ~EHCI_LEGACY_BIOS;
        return true;
    }
    assert(delay == EHCI_POLL_US);
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

int main(void) {
    test_completed_probe();
    test_firmware_retains_controller();
    test_malformed_capabilities();
    test_stalled_halt_and_reset();
    puts("EHCI host tests passed");
}
