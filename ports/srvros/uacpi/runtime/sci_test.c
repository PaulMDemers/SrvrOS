/* Explicit QEMU Q35 fixture. No AML, EC access or physical-machine policy. */
#include "work_worker.h"
#include "q35_fixture.h"
#include <srvros/acpi.h>
#include <srvros/acpi_work_test.h>
#include <srvros/console.h>
#include <srvros/irq_handlers.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <uacpi/acpi.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
#include "../../../../kernel/src/arch/x86_64/ioapic.h"

#define POWER_BUTTON (1u << 8)
#define MASKED (1u << 16)
static struct {
    struct acpi_work_worker worker;
    struct acpi_work_item item;
    uint64_t handler, observer, interrupts, observed, events, failures;
    uint32_t gsi, old_low, old_high;
    uint16_t event_port, control_port, smi_port, original_control;
    uint8_t enable_command, disable_command;
    bool engaged, active, stopping, worker_started, route_saved;
} fixture;

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
}
static void irq_restore(uint64_t flags) {
    if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
}
static bool equal(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static bool bytes_equal(const void *a, const char *b, unsigned size) {
    const uint8_t *p = a;
    for (unsigned i = 0; i < size; i++) if (p[i] != (uint8_t)b[i]) return false;
    return true;
}
const struct acpi_fadt *acpi_q35_fixture_fadt(void) {
    bool host = false, lpc = false;
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device->vendor_id != 0x8086) continue;
        host |= device->device_id == 0x29c0 && device->bus == 0 && device->device == 0;
        lpc |= device->device_id == 0x2918 && device->bus == 0 && device->device == 31;
    }
    if (!host || !lpc) return NULL;
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *info = acpi_table_at(i);
        if (!bytes_equal(info->signature, "FACP", 4) || info->length < 116) continue;
        const struct acpi_fadt *fadt = pmm_phys_to_virt(info->physical_address);
        if (!bytes_equal(fadt->hdr.oemid, "BOCHS ", 6) || fadt->sci_int != 9 ||
            fadt->pm1a_evt_blk != 0x600 || fadt->pm1a_cnt_blk != 0x604 ||
            fadt->pm1b_evt_blk || fadt->pm1b_cnt_blk || fadt->pm1_evt_len != 4 ||
            fadt->pm1_cnt_len != 2 || fadt->smi_cmd != 0xb2 ||
            !fadt->acpi_enable || !fadt->acpi_disable ||
            fadt->gpe0_blk != 0x620 || fadt->gpe0_blk_len != 16 || fadt->gpe1_blk)
            return NULL;
        return fadt;
    }
    return NULL;
}
static bool acpi_mode(bool enabled) {
    if (((inw(fixture.control_port) & 1) != 0) == enabled) return true;
    outb(fixture.smi_port, enabled ? fixture.enable_command : fixture.disable_command);
    uint64_t end = timer_monotonic_us() + 1000000;
    do {
        if (((inw(fixture.control_port) & 1) != 0) == enabled) return true;
        timer_delay_us(100);
    } while (timer_monotonic_us() < end);
    return false;
}
static void event_work(void *context) {
    (void)context;
    uint64_t flags = irq_save();
    if (inw(fixture.event_port) & POWER_BUTTON) {
        outw(fixture.event_port, POWER_BUTTON); /* Clear source before re-enable. */
        fixture.events++;
    } else fixture.failures++;
    if (!fixture.stopping) outw(fixture.event_port + 2, POWER_BUTTON);
    irq_restore(flags);
}
static bool sci_handler(void *context) {
    (void)context;
    uint16_t pending = inw(fixture.event_port) & inw(fixture.event_port + 2);
    if (!(pending & POWER_BUTTON)) return false;
    fixture.interrupts++;
    /* On the first delivery leave the level asserted through EOI. A second
     * delivery proves retriggering; then quiesce it for the worker. */
    if (fixture.interrupts & 1) return true;
    outw(fixture.event_port + 2, 0); /* Quiesce level while deferred work runs. */
    if (!acpi_worker_enqueue(&fixture.worker, &fixture.item,
        UACPI_WORK_GPE_EXECUTION, event_work, NULL)) fixture.failures++;
    return true;
}
static bool shared_observer(void *context) {
    (void)context;
    fixture.observed++;
    return false;
}
static bool stop_fixture(void) {
    if (!fixture.engaged) return true;
    uint64_t flags = irq_save();
    fixture.stopping = true;
    fixture.active = false;
    outw(fixture.event_port + 2, 0);
    bool masked = !fixture.route_saved || ioapic_mask_gsi(fixture.gsi, true);
    bool removed = true;
    if (masked && fixture.handler) {
        removed = irq_handler_remove(fixture.handler);
        if (removed) fixture.handler = 0;
    }
    if (masked && fixture.observer) {
        bool done = irq_handler_remove(fixture.observer);
        if (done) fixture.observer = 0;
        removed &= done;
    }
    irq_restore(flags);
    if (!masked || !removed) return false;
    if (fixture.worker_started && (!acpi_worker_drain(&fixture.worker, 1000) ||
        !acpi_worker_stop(&fixture.worker, 1000))) return false;
    fixture.worker_started = false;
    outw(fixture.event_port, POWER_BUTTON);
    bool route = !fixture.route_saved || ioapic_restore_masked_gsi(
        fixture.gsi, fixture.old_low, fixture.old_high);
    bool mode = acpi_mode((fixture.original_control & 1) != 0);
    if (route && mode) fixture.engaged = false;
    return route && mode;
}
static bool arm_fixture(bool fail_after_prepare) {
    if (fixture.engaged || !timer_reference_ready()) return false;
    const struct acpi_fadt *fadt = acpi_q35_fixture_fadt();
    if (!fadt || inw(0x602)) return false; /* No other fixed-event owner. */
    for (uint16_t port = 0x628; port < 0x630; port++) if (inb(port)) return false;
    const struct acpi_ioapic_info *controller = acpi_ioapic();
    if (!controller || !controller->present || controller->gsi_base) return false;
    uint32_t gsi = 9, low, high;
    bool active_low = true, level = true;
    const struct acpi_interrupt_source_override *override = acpi_irq_override(9);
    if (override) {
        unsigned polarity = override->flags & 3, trigger = (override->flags >> 2) & 3;
        if (override->bus || override->source != 9 || (override->flags & ~15u) ||
            polarity == 2 || trigger == 2 || trigger == 1) return false;
        gsi = override->gsi;
        active_low = polarity != 1;
        level = true;
    }
    if (!ioapic_read_gsi(gsi, &low, &high) || !(low & MASKED) ||
        (low & ((1u << 12) | (1u << 14)))) return false;
    fixture = (typeof(fixture)){
        .engaged = true, .gsi = gsi, .old_low = low, .old_high = high,
        .route_saved = true, .event_port = 0x600, .control_port = 0x604,
        .smi_port = 0xb2, .original_control = inw(0x604),
        .enable_command = fadt->acpi_enable, .disable_command = fadt->acpi_disable,
    };
    fixture.worker_started = acpi_worker_start(&fixture.worker);
    if (!fixture.worker_started) goto fail;
    fixture.handler = irq_handler_register(9, sci_handler, NULL);
    fixture.observer = irq_handler_register(9, shared_observer, NULL);
    if (!fixture.handler || !fixture.observer ||
        !ioapic_prepare_gsi(gsi, 41, active_low, level) || !acpi_mode(true)) goto fail;
    if (fail_after_prepare) goto fail;
    uint64_t flags = irq_save();
    outw(fixture.event_port, POWER_BUTTON);
    fixture.active = true;
    outw(fixture.event_port + 2, POWER_BUTTON);
    bool unmasked = ioapic_mask_gsi(gsi, false);
    irq_restore(flags);
    if (!unmasked) goto fail;
    return true;
fail:
    console_printf("sci-test: arm failed cleanup=%s\n", stop_fixture() ? "yes" : "NO");
    return false;
}
void acpi_sci_test_command(const char *args) {
    if (equal(args, "arm") || equal(args, "arm-fail")) {
        bool ok = arm_fixture(equal(args, "arm-fail"));
        console_printf("sci-test: %s gsi=%u vector=41\n", ok ? "ARMED" : "REFUSED", (uint64_t)fixture.gsi);
    } else if (equal(args, "stop")) {
        bool ok = stop_fixture();
        console_printf("sci-test: stopped=%s\n", ok ? "yes" : "NO");
    } else if (!*args || equal(args, "status")) {
        uint64_t flags = irq_save();
        uint64_t interrupts = fixture.interrupts, observed = fixture.observed;
        uint64_t events = fixture.events, failures = fixture.failures;
        bool active = fixture.active;
        irq_restore(flags);
        console_printf("sci-test: active=%u interrupts=%u shared=%u events=%u failures=%u\n",
            (uint64_t)active, interrupts, observed, events, failures);
    } else console_write("scitest arm|arm-fail|status|stop (QEMU Q35 fixture only)\n");
}
