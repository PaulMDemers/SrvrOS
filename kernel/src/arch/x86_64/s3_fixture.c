#include <limine.h>
#include <stddef.h>
#include <srvros/acpi.h>
#include <srvros/arch.h>
#include <srvros/console.h>
#include <srvros/display_profile.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include "io.h"

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

bool s3_pro81_preflight(const struct limine_memmap_response *map) {
    if (!display_profile_is_pro81() || !pch_is(0x1c49)) return false;
    uint64_t root = vmm_kernel_address_space();
    uint32_t length = 0;
    const uint8_t *data = find_fadt(&length);
    uint64_t facs = fadt_facs(data, length, 0x20454c505041ull, 0x404); /* APPLE */
    bool fadt = facs && length >= 232 && load_le(data + 56, 4) == 0x400 &&
        load_le(data + 80, 4) == 0x420 && data[92] == 16 && data[172] == 1 &&
        load_le(data + 176, 8) == 0x404 && data[220] == 1 && load_le(data + 224, 8) == 0x420;
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
        (uint64_t)fadt, (uint64_t)apple_s3_table(), facs, (uint64_t)nvs, (uint64_t)valid, low,
        root, (uint64_t)vmm_diagnostic_sole_kernel_root(root));
    if (valid) console_printf("pro81-s3: legacy_vector=%x extended_vector=%x global_lock=%x\n",
        load_le(table + 12, 4), load_le(table + 24, 8), load_le(table + 16, 4));
    if (fadt) console_printf("pro81-s3: pm1_control=%x lid_gpe_enable_byte=%x lid_gpe_status_byte=%x\n",
        (uint64_t)inw(0x404), (uint64_t)inb(0x42c), (uint64_t)inb(0x424));
    return true;
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
    return pass;
}
