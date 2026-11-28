#include <limine.h>
#include <stddef.h>
#include <srvros/acpi.h>
#include <srvros/arch.h>
#include <srvros/console.h>
#include <srvros/display_profile.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>
#include "io.h"
#include "ioapic.h"

extern const uint8_t s3_wake_start[], s3_wake_end[], s3_wake_segment[];
extern const uint8_t s3_wake_cr3[], s3_wake_efer[], s3_wake_cr0[];
extern const uint8_t s3_wake_gdt[], s3_wake_gdt_base[], s3_wake_flat[], s3_wake_long[], s3_wake_return[];
extern bool s3_fixture_enter(uint16_t port, uint16_t value);
extern void s3_fixture_resume(void);

static uint64_t load_le(const uint8_t *ptr, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; i++) value |= (uint64_t)ptr[i] << (8 * i);
    return value;
}

static void wake_patch(uint8_t *page, const uint8_t *field, uint64_t value, unsigned bytes) {
    uintptr_t offset = (uintptr_t)field - (uintptr_t)s3_wake_start;
    for (unsigned i = 0; i < bytes; i++) page[offset + i] = (uint8_t)(value >> (8 * i));
}

static bool pch_is(uint16_t id) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device->bus == 0 && device->device == 31 && device->function == 0 &&
            device->vendor_id == 0x8086 && device->device_id == id) return true;
    }
    return false;
}

static bool q35_tcg(void) {
    uint32_t eax = 0x40000000, ebx, ecx = 0, edx;
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    return eax >= 0x40000000 && ebx == 0x54474354 && ecx == 0x43544743 && edx == 0x47435447 &&
        pch_is(0x2918);
}

static const struct pci_device *q35_xhci(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (!device->bus && device->device == 2 && !device->function &&
            device->vendor_id == 0x1b36 && device->device_id == 0x000d &&
            device->class_code == 0x0c && device->subclass == 3 && device->prog_if == 0x30)
            return device;
    }
    return NULL;
}

static bool restore_q35_xhci_bar(const struct pci_device *device) {
    if (pci_read_config32(device, 0) != 0x000d1b36) return false;
    uint16_t command = pci_read_config16(device, 4);
    if (command == UINT16_MAX) return false;
    /* Firmware wake leaves this endpoint's BAR unassigned. Restore the
     * enumerated window with memory decode and bus mastering fenced. */
    uint16_t fenced = command & ~6u;
    pci_write_config16(device, 4, fenced);
    if (pci_read_config16(device, 4) != fenced) return false;
    pci_write_config32(device, 0x10, device->bar[0]);
    pci_write_config32(device, 0x14, device->bar[1]);
    return pci_read_config32(device, 0x10) == device->bar[0] &&
        pci_read_config32(device, 0x14) == device->bar[1];
}

static bool contains_nvs(const struct limine_memmap_response *map, uint64_t address) {
    for (uint64_t i = 0; map && i < map->entry_count; i++) {
        const struct limine_memmap_entry *entry = map->entries[i];
        if (entry->type == LIMINE_MEMMAP_ACPI_NVS && address >= entry->base && entry->length >= 64 &&
            address - entry->base <= entry->length - 64) return true;
    }
    return false;
}

static const uint8_t *find_fadt(uint32_t *length) {
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (table->signature[0] != 'F' || table->signature[1] != 'A' ||
            table->signature[2] != 'C' || table->signature[3] != 'P') continue;
        *length = table->length;
        return pmm_phys_to_virt(table->physical_address);
    }
    return NULL;
}

static uint64_t fadt_facs(const uint8_t *data, uint32_t length, uint64_t oem, uint16_t port) {
    if (!data || length < 116 || load_le(data + 10, 6) != oem ||
        load_le(data + 64, 4) != port || data[89] != 2 || (load_le(data + 112, 4) & (1u << 20))) return 0;
    uint64_t legacy = load_le(data + 36, 4);
    uint64_t extended = length >= 140 ? load_le(data + 132, 8) : 0;
    if (legacy && extended && legacy != extended) return 0;
    return legacy ? legacy : extended;
}

static bool hhdm_matches(uint64_t physical) {
    uint64_t mapped;
    return vmm_virt_to_phys((uintptr_t)pmm_phys_to_virt(physical), &mapped) && mapped == physical;
}

static bool apple_s3_table(void) {
    /* Exact 14-byte AML object from this unit's 50-byte APPLE/SsdtS3. */
    static const uint8_t s3[] = {0x08, '_', 'S', '3', '_', 0x12, 8, 3, 0x0a, 5, 0x0a, 5, 0x0a, 0};
    for (uint64_t i = 0; i < acpi_table_count(); i++) {
        const struct acpi_table_info *table = acpi_table_at(i);
        if (table->length != 36 + sizeof s3) continue;
        const uint8_t *data = pmm_phys_to_virt(table->physical_address);
        if (load_le(data, 4) != 0x54445353 || load_le(data + 10, 6) != 0x20454c505041ull ||
            load_le(data + 16, 8) != 0x0000335374647353ull) continue;
        for (unsigned j = 0; j < sizeof s3; j++) if (data[36 + j] != s3[j]) return false;
        return true;
    }
    return false;
}

static uint64_t apple_s3_fadt(void) {
    uint32_t length = 0;
    const uint8_t *data = find_fadt(&length);
    uint64_t facs = fadt_facs(data, length, 0x20454c505041ull, 0x404); /* APPLE */
    if (!facs || length < 232 || load_le(data + 46, 2) != 9 ||
        load_le(data + 48, 4) != 0xb2 || data[52] != 0xf0 || data[53] != 0xf1 ||
        load_le(data + 56, 4) != 0x400 || load_le(data + 60, 4) || load_le(data + 68, 4) ||
        load_le(data + 80, 4) != 0x420 || load_le(data + 84, 4) || data[88] != 4 || data[92] != 16 ||
        data[172] != 1 || load_le(data + 176, 8) != 0x404 ||
        data[220] != 1 || load_le(data + 224, 8) != 0x420) return 0;
    return facs;
}

bool s3_pro81_preflight(const struct limine_memmap_response *map) {
    if (!display_profile_is_pro81() || !pch_is(0x1c49)) return false;
    uint64_t root = vmm_kernel_address_space();
    uint64_t facs = apple_s3_fadt();
    bool nvs = facs && !(facs & 63) && contains_nvs(map, facs) && hhdm_matches(facs);
    const uint8_t *table = nvs ? pmm_phys_to_virt(facs) : NULL;
    bool valid = table && load_le(table, 4) == 0x53434146 && load_le(table + 4, 4) == 64;
    uint64_t low = 0, alias;
    for (uint64_t page = PMM_FRAME_SIZE; page < 0xa0000; page += PMM_FRAME_SIZE) {
        if (pmm_frame_has_tag(page, PMM_FRAME_FREE) && hhdm_matches(page) && !vmm_virt_to_phys(page, &alias)) {
            low = page;
            break;
        }
    }
    console_printf("pro81-s3: fadt=%u ssdt_s3=%u facs=%x facs_nvs=%u facs_valid=%u low_page=%x cr3=%x sole_root=%u\n",
        (uint64_t)(facs != 0), (uint64_t)apple_s3_table(), facs, (uint64_t)nvs, (uint64_t)valid, low,
        root, (uint64_t)vmm_diagnostic_sole_kernel_root(root));
    if (valid) console_printf("pro81-s3: legacy_vector=%x extended_vector=%x global_lock=%x\n",
        load_le(table + 12, 4), load_le(table + 24, 8), load_le(table + 16, 4));
    if (facs) console_printf("pro81-s3: pm1_control=%x lid_gpe_enable_byte=%x lid_gpe_status_byte=%x\n",
        (uint64_t)inw(0x404), (uint64_t)inb(0x42c), (uint64_t)inb(0x424));
    return true;
}

static uint64_t apple_gpe_enables(void) {
    uint64_t enables = 0;
    for (unsigned index = 0; index < 8; index++) enables |= (uint64_t)inb(0x428 + index) << (8 * index);
    return enables;
}

static bool wait_for_sci(bool enabled) {
    for (unsigned attempt = 0; attempt < 1000; attempt++) {
        uint16_t control = inw(0x404);
        if (control == UINT16_MAX || (control & (1u << 13))) return false;
        if ((bool)(control & 1) == enabled) return true;
        if (!timer_delay_us(100)) return false;
    }
    return false;
}

/* Awake firmware transition with delivery masked and no EC transactions. */
bool s3_pro81_mode_roundtrip(void) {
    if (!display_profile_is_pro81() || !pch_is(0x1c49) || !apple_s3_fadt() ||
        !apple_s3_table() || !timer_reference_ready() ||
        !vmm_diagnostic_sole_kernel_root(vmm_kernel_address_space())) return false;
    const struct acpi_interrupt_source_override *override = acpi_irq_override(9);
    unsigned gsi = override ? override->gsi : 9;
    uint32_t route_low, route_high;
    uint16_t control = inw(0x404), fixed = inw(0x402);
    uint64_t gpe = apple_gpe_enables();
    /* The MacBook firmware can leave GPE 6 enabled in legacy mode. Leave
     * its enable and status alone and keep SCI delivery masked throughout. */
    if (control || fixed || (gpe & ~(1ull << 6)) || !ioapic_read_gsi(gsi, &route_low, &route_high) ||
        !(route_low & (1u << 16)) || (route_low & ((1u << 12) | (1u << 14)))) {
        console_printf("pro81-s3-mode: refused control=%x fixed_enable=%x gpe_enable=%x gsi=%u\n",
            (uint64_t)control, (uint64_t)fixed, gpe, (uint64_t)gsi);
        return false;
    }
    outb(0xb2, 0xf0);
    bool entered = wait_for_sci(true);
    uint16_t active_control = inw(0x404), active_fixed = inw(0x402);
    uint64_t active_gpe = apple_gpe_enables();
    outb(0xb2, 0xf1);
    bool exited = wait_for_sci(false);
    uint16_t final_control = inw(0x404), final_fixed = inw(0x402);
    uint64_t final_gpe = apple_gpe_enables();
    uint32_t final_low, final_high;
    bool route_restored = ioapic_read_gsi(gsi, &final_low, &final_high) &&
        route_low == final_low && route_high == final_high;
    bool fixed_repaired = false;
    /* The firmware sets PWRBTN_EN on entry and leaves it set on exit.
     * Restore the captured enable halfword without writing PM1 status. */
    if (entered && exited && final_control == control && final_gpe == gpe && route_restored &&
        !fixed && final_fixed == (1u << 8)) {
        outw(0x402, fixed);
        final_fixed = inw(0x402);
        fixed_repaired = final_fixed == fixed;
    }
    bool restored = exited && final_control == control && final_fixed == fixed && final_gpe == gpe && route_restored;
    console_printf("pro81-s3-mode: entered=%u restored=%u active_control=%x active_fixed=%x active_gpe=%x initial_gpe=%x\n",
        (uint64_t)entered, (uint64_t)restored, (uint64_t)active_control, (uint64_t)active_fixed, active_gpe, gpe);
    console_printf("pro81-s3-mode: final_control=%x final_fixed=%x final_gpe=%x route_restored=%u fixed_repaired=%u\n",
        (uint64_t)final_control, (uint64_t)final_fixed, final_gpe, (uint64_t)route_restored, (uint64_t)fixed_repaired);
    return entered && restored;
}

/* A pre-driver CPU0 experiment. The physical target cannot enter this path. */
bool s3_q35_fixture(const struct limine_memmap_response *map) {
    uint64_t root = vmm_kernel_address_space(), cr0, cr4;
    __asm__ volatile ("mov %%cr0,%0; mov %%cr4,%1" : "=r"(cr0), "=r"(cr4));
    if (!q35_tcg() || !vmm_diagnostic_sole_kernel_root(root) || root > UINT32_MAX ||
        (cr4 & ((1u << 12) | (1u << 17)))) {
        console_write("s3-fixture: refused platform or processor context\n");
        return false;
    }
    const struct pci_device *xhci = q35_xhci();
    if (!xhci || (xhci->bar[0] & 15u) != 4 || (!xhci->bar[1] && xhci->bar[0] == 4) ||
        pci_read_config32(xhci, 0x10) != xhci->bar[0] || pci_read_config32(xhci, 0x14) != xhci->bar[1]) {
        console_write("s3-fixture: refused xHCI PCI resource\n");
        return false;
    }
    uint32_t fadt_length = 0;
    const uint8_t *fadt = find_fadt(&fadt_length);
    uint64_t facs = fadt_facs(fadt, fadt_length, 0x205348434f42ull, 0x604); /* BOCHS */
    uint64_t mapped;
    if (!facs || (facs & 63) || !contains_nvs(map, facs) || !hhdm_matches(facs)) {
        console_printf("s3-fixture: refused FACS range address=%x\n", facs);
        return false;
    }
    uint8_t *facs_data = pmm_phys_to_virt(facs);
    if (load_le(facs_data, 4) != 0x53434146 || load_le(facs_data + 4, 4) != 64 ||
        load_le(facs_data + 12, 4) || (load_le(facs_data + 16, 4) & 3) ||
        load_le(facs_data + 24, 8) || load_le(facs_data + 36, 4)) {
        console_write("s3-fixture: refused FACS layout or ownership\n");
        return false;
    }
    uint64_t low = pmm_alloc_frames_above(1, PMM_FRAME_SIZE);
    uintptr_t size = (uintptr_t)s3_wake_end - (uintptr_t)s3_wake_start;
    if (!low || low >= 0xa0000 || size > PMM_FRAME_SIZE ||
        !hhdm_matches(low) ||
        vmm_virt_to_phys(low, &mapped) || !vmm_map_page(low, low, 0)) {
        if (low) pmm_free_frame(low);
        console_write("s3-fixture: refused low-memory wake page\n");
        return false;
    }
    uint8_t *page = pmm_phys_to_virt(low);
    for (uintptr_t i = 0; i < size; i++) page[i] = s3_wake_start[i];
    uint32_t efer, efer_hi;
    __asm__ volatile ("rdmsr" : "=a"(efer), "=d"(efer_hi) : "c"(0xc0000080u));
    wake_patch(page, s3_wake_segment, low >> 4, 2);
    wake_patch(page, s3_wake_cr3, root, 4);
    wake_patch(page, s3_wake_cr0, cr0, 4);
    wake_patch(page, s3_wake_efer, efer & 0x901, 4); /* LMA is read-only. */
    wake_patch(page, s3_wake_gdt_base, low + ((uintptr_t)s3_wake_gdt - (uintptr_t)s3_wake_start), 4);
    wake_patch(page, s3_wake_flat, low + ((uintptr_t)s3_wake_long - (uintptr_t)s3_wake_start), 4);
    wake_patch(page, s3_wake_return, (uintptr_t)s3_fixture_resume, 8);

    volatile uint32_t *vector = (volatile uint32_t *)(facs_data + 12);
    uint16_t control = inw(0x604);
    bool returned = false;
    if (control != UINT16_MAX && !(control & (1u << 13))) {
        *vector = (uint32_t)low;
        if (*vector == low) {
            console_printf("s3-fixture: enter vector=%x root=%x\n", low, root);
            uint16_t sleep = (uint16_t)((control & ~(7u << 10)) | (1u << 10) | (1u << 13));
            returned = s3_fixture_enter(0x604, sleep);
        }
        *vector = 0;
    }
    bool retired = vmm_unmap_page(low, &mapped) && mapped == low;
    if (retired) pmm_free_frame(low);
    bool pass = returned && *vector == 0 && retired && vmm_diagnostic_sole_kernel_root(root);
    console_printf("s3-fixture: resumed=%u vector_restored=%u page_retired=%u result=%s\n",
        (uint64_t)returned, (uint64_t)(*vector == 0), (uint64_t)retired, pass ? "PASS" : "INCOMPLETE");
    if (!pass) return false;
    bool bar_restored = restore_q35_xhci_bar(xhci);
    console_printf("q35-s3-usb: bar_restored=%u\n", (uint64_t)bar_restored);
    return bar_restored;
}
