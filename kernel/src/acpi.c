#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/pmm.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct sdt_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct madt {
    struct sdt_header header;
    uint32_t lapic_address;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed));

struct madt_entry_header {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct madt_ioapic {
    struct madt_entry_header header;
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsi_base;
} __attribute__((packed));

struct madt_iso {
    struct madt_entry_header header;
    uint8_t bus;
    uint8_t source;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

#define ACPI_MAX_TABLES 32
#define ACPI_MAX_MCFG_ALLOCATIONS 8
#define ACPI_INPUT_SCAN_MAX_MATCHES 40
#define ACPI_INPUT_DEVICE_SUMMARY_MAX 8
#define ACPI_INPUT_DEVICE_MEMBER_MAX 18

struct mcfg {
    struct sdt_header header;
    uint64_t reserved;
    uint8_t entries[];
} __attribute__((packed));

struct mcfg_allocation_entry {
    uint64_t base_address;
    uint16_t pci_segment;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
} __attribute__((packed));

static struct acpi_ioapic_info ioapic_info;
static struct acpi_interrupt_source_override irq_overrides[16];
static struct acpi_table_info tables[ACPI_MAX_TABLES];
static uint64_t table_count;
static struct acpi_mcfg_allocation mcfg_allocations[ACPI_MAX_MCFG_ALLOCATIONS];
static uint64_t mcfg_allocation_count;
static uint32_t input_summary_offsets[ACPI_INPUT_DEVICE_SUMMARY_MAX];
static uint64_t input_summaries_printed;

static bool signature_matches(const char *a, const char *b, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static uint8_t checksum(const void *ptr, uint32_t length) {
    const uint8_t *bytes = ptr;
    uint8_t sum = 0;

    for (uint32_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }

    return sum;
}

static bool table_is_valid(const struct sdt_header *header, const char *signature) {
    return header != NULL &&
        signature_matches(header->signature, signature, 4) &&
        checksum(header, header->length) == 0;
}

static void remember_table(const struct sdt_header *header, uint64_t physical_address) {
    if (header == NULL || table_count >= ACPI_MAX_TABLES) {
        return;
    }

    struct acpi_table_info *out = &tables[table_count++];
    out->signature[0] = header->signature[0];
    out->signature[1] = header->signature[1];
    out->signature[2] = header->signature[2];
    out->signature[3] = header->signature[3];
    out->signature[4] = '\0';
    out->physical_address = physical_address;
    out->length = header->length;
    out->revision = header->revision;
}

static bool table_was_remembered(uint64_t physical_address) {
    for (uint64_t i = 0; i < table_count; i++) {
        if (tables[i].physical_address == physical_address) {
            return true;
        }
    }
    return false;
}

static void remember_table_if_valid(uint64_t physical_address, const char *signature) {
    if (physical_address == 0 || table_was_remembered(physical_address)) {
        return;
    }

    struct sdt_header *table = pmm_phys_to_virt(physical_address);
    if (table_is_valid(table, signature)) {
        remember_table(table, physical_address);
    }
}

static void remember_child_tables_rsdt(struct sdt_header *rsdt) {
    uint32_t entries = (rsdt->length - sizeof(*rsdt)) / sizeof(uint32_t);
    uint32_t *addresses = (uint32_t *)((uint8_t *)rsdt + sizeof(*rsdt));

    for (uint32_t i = 0; i < entries; i++) {
        struct sdt_header *table = pmm_phys_to_virt(addresses[i]);
        if (table != NULL && checksum(table, table->length) == 0) {
            remember_table(table, addresses[i]);
        }
    }
}

static void remember_child_tables_xsdt(struct sdt_header *xsdt) {
    uint32_t entries = (xsdt->length - sizeof(*xsdt)) / sizeof(uint64_t);
    uint64_t *addresses = (uint64_t *)((uint8_t *)xsdt + sizeof(*xsdt));

    for (uint32_t i = 0; i < entries; i++) {
        struct sdt_header *table = pmm_phys_to_virt(addresses[i]);
        if (table != NULL && checksum(table, table->length) == 0) {
            remember_table(table, addresses[i]);
        }
    }
}

static void parse_madt(struct madt *madt) {
    uint8_t *entry = madt->entries;
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (entry + sizeof(struct madt_entry_header) <= end) {
        struct madt_entry_header *header = (struct madt_entry_header *)entry;
        if (header->length < sizeof(*header) || entry + header->length > end) {
            break;
        }

        if (header->type == 1 && header->length >= sizeof(struct madt_ioapic)) {
            struct madt_ioapic *ioapic = (struct madt_ioapic *)entry;
            ioapic_info.present = true;
            ioapic_info.id = ioapic->id;
            ioapic_info.address = ioapic->address;
            ioapic_info.gsi_base = ioapic->gsi_base;
        } else if (header->type == 2 && header->length >= sizeof(struct madt_iso)) {
            struct madt_iso *iso = (struct madt_iso *)entry;
            if (iso->source < sizeof(irq_overrides) / sizeof(irq_overrides[0])) {
                irq_overrides[iso->source] = (struct acpi_interrupt_source_override) {
                    .present = true,
                    .bus = iso->bus,
                    .source = iso->source,
                    .gsi = iso->gsi,
                    .flags = iso->flags,
                };
            }
        }

        entry += header->length;
    }
}

static void parse_mcfg(struct mcfg *mcfg) {
    if (mcfg == NULL || mcfg->header.length < sizeof(*mcfg)) {
        return;
    }

    uint32_t bytes = mcfg->header.length - sizeof(*mcfg);
    uint32_t entries = bytes / sizeof(struct mcfg_allocation_entry);
    struct mcfg_allocation_entry *entry = (struct mcfg_allocation_entry *)mcfg->entries;

    for (uint32_t i = 0; i < entries && mcfg_allocation_count < ACPI_MAX_MCFG_ALLOCATIONS; i++) {
        if (entry[i].base_address == 0 || entry[i].end_bus < entry[i].start_bus) {
            continue;
        }

        mcfg_allocations[mcfg_allocation_count++] = (struct acpi_mcfg_allocation) {
            .present = true,
            .base_address = entry[i].base_address,
            .pci_segment = entry[i].pci_segment,
            .start_bus = entry[i].start_bus,
            .end_bus = entry[i].end_bus,
        };
    }
}

static void remember_fadt_dsdt(const struct sdt_header *fadt) {
    if (fadt == NULL || fadt->length < 44) {
        return;
    }

    const uint8_t *bytes = (const uint8_t *)fadt;
    uint64_t dsdt_physical = *(const uint32_t *)(bytes + 40);
    if (fadt->length >= 148) {
        uint64_t x_dsdt_physical = *(const uint64_t *)(bytes + 140);
        if (x_dsdt_physical != 0) {
            dsdt_physical = x_dsdt_physical;
        }
    }

    remember_table_if_valid(dsdt_physical, "DSDT");
}

static struct sdt_header *find_table_rsdt(struct sdt_header *rsdt, const char *signature) {
    uint32_t entries = (rsdt->length - sizeof(*rsdt)) / sizeof(uint32_t);
    uint32_t *addresses = (uint32_t *)((uint8_t *)rsdt + sizeof(*rsdt));

    for (uint32_t i = 0; i < entries; i++) {
        struct sdt_header *table = pmm_phys_to_virt(addresses[i]);
        if (table_is_valid(table, signature)) {
            return table;
        }
    }

    return NULL;
}

static struct sdt_header *find_table_xsdt(struct sdt_header *xsdt, const char *signature) {
    uint32_t entries = (xsdt->length - sizeof(*xsdt)) / sizeof(uint64_t);
    uint64_t *addresses = (uint64_t *)((uint8_t *)xsdt + sizeof(*xsdt));

    for (uint32_t i = 0; i < entries; i++) {
        struct sdt_header *table = pmm_phys_to_virt(addresses[i]);
        if (table_is_valid(table, signature)) {
            return table;
        }
    }

    return NULL;
}

void acpi_init(void *rsdp_address) {
    struct rsdp *root = rsdp_address;
    struct sdt_header *fadt_header = NULL;
    struct sdt_header *madt_header = NULL;
    struct sdt_header *mcfg_header = NULL;

    ioapic_info = (struct acpi_ioapic_info) { 0 };
    table_count = 0;
    mcfg_allocation_count = 0;
    for (uint64_t i = 0; i < sizeof(irq_overrides) / sizeof(irq_overrides[0]); i++) {
        irq_overrides[i] = (struct acpi_interrupt_source_override) { 0 };
    }

    if (root == NULL || !signature_matches(root->signature, "RSD PTR ", 8)) {
        console_write("acpi: rsdp unavailable\n");
        return;
    }

    if (checksum(root, 20) != 0) {
        console_write("acpi: rsdp checksum failed\n");
        return;
    }

    if (root->revision >= 2 && root->xsdt_address != 0) {
        struct sdt_header *xsdt = pmm_phys_to_virt(root->xsdt_address);
        if (table_is_valid(xsdt, "XSDT")) {
            remember_table(xsdt, root->xsdt_address);
            remember_child_tables_xsdt(xsdt);
            fadt_header = find_table_xsdt(xsdt, "FACP");
            madt_header = find_table_xsdt(xsdt, "APIC");
            mcfg_header = find_table_xsdt(xsdt, "MCFG");
        }
    }

    if (madt_header == NULL && root->rsdt_address != 0) {
        struct sdt_header *rsdt = pmm_phys_to_virt(root->rsdt_address);
        if (table_is_valid(rsdt, "RSDT")) {
            remember_table(rsdt, root->rsdt_address);
            remember_child_tables_rsdt(rsdt);
            fadt_header = find_table_rsdt(rsdt, "FACP");
            madt_header = find_table_rsdt(rsdt, "APIC");
            mcfg_header = find_table_rsdt(rsdt, "MCFG");
        }
    }

    remember_fadt_dsdt(fadt_header);

    if (mcfg_header != NULL) {
        parse_mcfg((struct mcfg *)mcfg_header);
        console_printf("acpi: mcfg allocations=%u\n", mcfg_allocation_count);
    }

    if (madt_header == NULL) {
        console_write("acpi: madt unavailable\n");
        return;
    }

    parse_madt((struct madt *)madt_header);
    if (ioapic_info.present) {
        console_printf("acpi: ioapic id=%u addr=%x gsi_base=%u\n",
            (uint64_t)ioapic_info.id,
            (uint64_t)ioapic_info.address,
            (uint64_t)ioapic_info.gsi_base);
    } else {
        console_write("acpi: ioapic not found\n");
    }
}

static bool table_signature_is(const struct acpi_table_info *table, const char *signature) {
    return table != NULL && signature_matches(table->signature, signature, 4);
}

static bool bytes_match(const uint8_t *bytes, uint32_t offset, uint32_t length, const char *pattern) {
    uint32_t pattern_length = 0;
    while (pattern[pattern_length] != '\0') {
        pattern_length++;
    }

    if (pattern_length == 0 || offset + pattern_length > length) {
        return false;
    }

    for (uint32_t i = 0; i < pattern_length; i++) {
        if (bytes[offset + i] != (uint8_t)pattern[i]) {
            return false;
        }
    }
    return true;
}

static char printable(uint8_t value) {
    if (value >= 32 && value <= 126) {
        return (char)value;
    }
    return '.';
}

static void print_aml_context(const uint8_t *bytes, uint32_t length, uint32_t offset) {
    uint32_t start = offset > 24 ? offset - 24 : 0;
    uint32_t end = offset + 56;
    if (end > length) {
        end = length;
    }

    console_write(" ascii=\"");
    for (uint32_t i = start; i < end; i++) {
        console_putc(printable(bytes[i]));
    }
    console_write("\"\n");
}

static bool aml_is_nameseg_char(uint8_t value) {
    return (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9') ||
        value == '_';
}

static bool aml_read_nameseg(const uint8_t *bytes, uint32_t length, uint32_t offset, char name[5]) {
    if (offset + 4 > length) {
        return false;
    }

    for (uint32_t i = 0; i < 4; i++) {
        if (!aml_is_nameseg_char(bytes[offset + i])) {
            return false;
        }
        name[i] = (char)bytes[offset + i];
    }
    name[4] = '\0';
    return true;
}

static bool aml_read_namestring_simple(const uint8_t *bytes,
                                       uint32_t length,
                                       uint32_t offset,
                                       char name[5],
                                       uint32_t *name_bytes) {
    uint32_t current = offset;
    while (current < length && (bytes[current] == '\\' || bytes[current] == '^')) {
        current++;
    }

    if (current >= length) {
        return false;
    }

    if (bytes[current] == 0x2e) {
        current++;
        if (!aml_read_nameseg(bytes, length, current, name)) {
            return false;
        }
        if (name_bytes != NULL) {
            *name_bytes = current + 8 - offset;
        }
        return true;
    }

    if (bytes[current] == 0x2f) {
        if (current + 2 >= length) {
            return false;
        }
        uint32_t count = bytes[current + 1];
        current += 2;
        if (count == 0 || current + count * 4 > length ||
            !aml_read_nameseg(bytes, length, current, name)) {
            return false;
        }
        if (name_bytes != NULL) {
            *name_bytes = current + count * 4 - offset;
        }
        return true;
    }

    if (bytes[current] == 0x00) {
        if (name_bytes != NULL) {
            *name_bytes = current + 1 - offset;
        }
        name[0] = '\0';
        return true;
    }

    if (!aml_read_nameseg(bytes, length, current, name)) {
        return false;
    }

    if (name_bytes != NULL) {
        *name_bytes = current + 4 - offset;
    }
    return true;
}

static bool aml_parse_pkg_length(const uint8_t *bytes,
                                 uint32_t length,
                                 uint32_t offset,
                                 uint32_t *pkg_length,
                                 uint32_t *pkg_bytes) {
    if (offset >= length) {
        return false;
    }

    uint8_t lead = bytes[offset];
    uint32_t byte_count = lead >> 6;
    if (offset + 1 + byte_count > length) {
        return false;
    }

    uint32_t value = 0;
    if (byte_count == 0) {
        value = lead & 0x3f;
    } else {
        value = lead & 0x0f;
        for (uint32_t i = 1; i <= byte_count; i++) {
            value |= (uint32_t)bytes[offset + i] << (4 + (i - 1) * 8);
        }
    }

    if (value == 0 || offset + value > length) {
        return false;
    }

    if (pkg_length != NULL) {
        *pkg_length = value;
    }
    if (pkg_bytes != NULL) {
        *pkg_bytes = 1 + byte_count;
    }
    return true;
}

static bool aml_find_enclosing_device(const uint8_t *bytes,
                                      uint32_t length,
                                      uint32_t target,
                                      uint32_t *device_offset,
                                      uint32_t *device_end,
                                      char name[5]) {
    bool found = false;
    uint32_t best_offset = 0;
    uint32_t best_end = 0;
    char best_name[5] = { 0 };

    for (uint32_t offset = sizeof(struct sdt_header); offset + 8 < length && offset < target; offset++) {
        if (bytes[offset] != 0x5b || bytes[offset + 1] != 0x82) {
            continue;
        }

        uint32_t pkg_length = 0;
        uint32_t pkg_bytes = 0;
        if (!aml_parse_pkg_length(bytes, length, offset + 2, &pkg_length, &pkg_bytes)) {
            continue;
        }

        uint32_t name_offset = offset + 2 + pkg_bytes;
        char current_name[5] = { 0 };
        if (!aml_read_namestring_simple(bytes, length, name_offset, current_name, NULL)) {
            continue;
        }

        uint32_t end = offset + 2 + pkg_length;
        if (end > length || target >= end) {
            continue;
        }

        found = true;
        best_offset = offset;
        best_end = end;
        for (uint32_t i = 0; i < sizeof(best_name); i++) {
            best_name[i] = current_name[i];
        }
    }

    if (!found) {
        return false;
    }

    if (device_offset != NULL) {
        *device_offset = best_offset;
    }
    if (device_end != NULL) {
        *device_end = best_end;
    }
    if (name != NULL) {
        for (uint32_t i = 0; i < sizeof(best_name); i++) {
            name[i] = best_name[i];
        }
    }
    return true;
}

static void aml_print_device_members(const uint8_t *bytes,
                                     uint32_t length,
                                     uint32_t device_offset,
                                     uint32_t device_end) {
    uint64_t printed = 0;
    for (uint32_t offset = device_offset + 1;
         offset + 6 < device_end && offset + 6 < length && printed < ACPI_INPUT_DEVICE_MEMBER_MAX;
         offset++) {
        char name[5] = { 0 };
        if (bytes[offset] == 0x14) {
            uint32_t pkg_length = 0;
            uint32_t pkg_bytes = 0;
            if (!aml_parse_pkg_length(bytes, length, offset + 1, &pkg_length, &pkg_bytes)) {
                continue;
            }
            uint32_t name_offset = offset + 1 + pkg_bytes;
            uint32_t name_bytes = 0;
            if (aml_read_namestring_simple(bytes, length, name_offset, name, &name_bytes) &&
                name[0] != '\0' && name_offset + name_bytes < length) {
                console_printf("acpi-input-member: method off=%u name=%s flags=%x\n",
                    (uint64_t)offset,
                    name,
                    (uint64_t)bytes[name_offset + name_bytes]);
                printed++;
            }
        } else if (bytes[offset] == 0x08) {
            if (aml_read_namestring_simple(bytes, length, offset + 1, name, NULL) &&
                name[0] != '\0') {
                console_printf("acpi-input-member: name off=%u name=%s\n",
                    (uint64_t)offset,
                    name);
                printed++;
            }
        } else if (bytes[offset] == 0x5b && bytes[offset + 1] == 0x82) {
            uint32_t pkg_length = 0;
            uint32_t pkg_bytes = 0;
            if (!aml_parse_pkg_length(bytes, length, offset + 2, &pkg_length, &pkg_bytes)) {
                continue;
            }
            if (aml_read_namestring_simple(bytes, length, offset + 2 + pkg_bytes, name, NULL) &&
                name[0] != '\0') {
                console_printf("acpi-input-member: child-device off=%u name=%s end=%u\n",
                    (uint64_t)offset,
                    name,
                    (uint64_t)(offset + 2 + pkg_length));
                printed++;
            }
        }
    }
}

static void print_aml_device_summary(const uint8_t *bytes, uint32_t length, uint32_t offset) {
    if (input_summaries_printed >= ACPI_INPUT_DEVICE_SUMMARY_MAX) {
        return;
    }

    uint32_t device_offset = 0;
    uint32_t device_end = 0;
    char device_name[5] = { 0 };
    if (!aml_find_enclosing_device(bytes, length, offset, &device_offset, &device_end, device_name)) {
        return;
    }

    for (uint64_t i = 0; i < input_summaries_printed; i++) {
        if (input_summary_offsets[i] == device_offset) {
            return;
        }
    }
    input_summary_offsets[input_summaries_printed++] = device_offset;

    console_printf("acpi-input-device: name=%s off=%u end=%u span=%u\n",
        device_name,
        (uint64_t)device_offset,
        (uint64_t)device_end,
        (uint64_t)(device_end - device_offset));
    aml_print_device_members(bytes, length, device_offset, device_end);
}

static bool find_pattern_offset(const uint8_t *bytes,
                                uint32_t length,
                                const char *pattern,
                                uint32_t *offset_out) {
    for (uint32_t offset = sizeof(struct sdt_header); offset < length; offset++) {
        if (bytes_match(bytes, offset, length, pattern)) {
            if (offset_out != NULL) {
                *offset_out = offset;
            }
            return true;
        }
    }
    return false;
}

static void print_optional_pattern_offset(const char *name, bool present, uint32_t offset) {
    if (present) {
        console_printf(" %s=%u", name, (uint64_t)offset);
    } else {
        console_printf(" %s=-", name);
    }
}

static void print_topcase_summary_for_table(const struct acpi_table_info *table,
                                            const uint8_t *bytes,
                                            uint32_t length) {
    uint32_t spi1_offset = 0;
    uint32_t spit_offset = 0;
    uint32_t hid_offset = 0;
    uint32_t sien_offset = 0;
    uint32_t sist_offset = 0;
    uint32_t uien_offset = 0;
    uint32_t uist_offset = 0;
    bool has_spi1 = find_pattern_offset(bytes, length, "SPI1", &spi1_offset);
    bool has_spit = find_pattern_offset(bytes, length, "SPIT", &spit_offset);
    bool has_hid = find_pattern_offset(bytes, length, "APPLE-SPI-TOPCASE", &hid_offset) ||
        find_pattern_offset(bytes, length, "APPLESPITOPCASE", &hid_offset) ||
        find_pattern_offset(bytes, length, "TOPCASE", &hid_offset) ||
        find_pattern_offset(bytes, length, "APP000D", &hid_offset);
    bool has_sien = find_pattern_offset(bytes, length, "SIEN", &sien_offset);
    bool has_sist = find_pattern_offset(bytes, length, "SIST", &sist_offset);
    bool has_uien = find_pattern_offset(bytes, length, "UIEN", &uien_offset);
    bool has_uist = find_pattern_offset(bytes, length, "UIST", &uist_offset);

    if (!has_spi1 && !has_spit && !has_hid && !has_sien && !has_sist) {
        return;
    }

    char spi1_name[5] = { 0 };
    char spit_name[5] = { 0 };
    uint32_t spi1_device = 0;
    uint32_t spi1_end = 0;
    uint32_t spit_device = 0;
    uint32_t spit_end = 0;
    bool has_spi1_device = has_spi1 &&
        aml_find_enclosing_device(bytes, length, spi1_offset, &spi1_device, &spi1_end, spi1_name);
    bool has_spit_device = has_spit &&
        aml_find_enclosing_device(bytes, length, spit_offset, &spit_device, &spit_end, spit_name);

    console_printf("acpi-input-topcase: %s topcase=%s", table->signature, has_spit ? "spit" : "unknown");
    print_optional_pattern_offset("spi1", has_spi1, spi1_offset);
    if (has_spi1_device) {
        console_printf(" spi1_device=%s@%u", spi1_name, (uint64_t)spi1_device);
    }
    print_optional_pattern_offset("spit", has_spit, spit_offset);
    if (has_spit_device) {
        console_printf(" spit_device=%s@%u", spit_name, (uint64_t)spit_device);
    }
    print_optional_pattern_offset("hid_marker", has_hid, hid_offset);
    print_optional_pattern_offset("sien", has_sien, sien_offset);
    print_optional_pattern_offset("sist", has_sist, sist_offset);
    print_optional_pattern_offset("uien", has_uien, uien_offset);
    print_optional_pattern_offset("uist", has_uist, uist_offset);
    console_write("\n");
}

static bool pattern_wants_device_summary(const char *pattern) {
    return bytes_match((const uint8_t *)pattern, 0, 4, "SPI1") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "SPIT") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "APP0") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "APPL") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "HSSP") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "TPD0") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "KBD0") ||
        bytes_match((const uint8_t *)pattern, 0, 4, "MOU0");
}

void acpi_print_input_diagnostics(void) {
    static const char *patterns[] = {
        "SPI1",
        "SDMA",
        "HSSP",
        "APP0",
        "APP000D",
        "APPLE-SPI-TOPCASE",
        "APPLESPITOPCASE",
        "TPD0",
        "TPAD",
        "KBD0",
        "MOU0",
        "SPIT",
        "SIEN",
        "SIST",
        "UIEN",
        "UIST",
        "ETPD",
        "ATKD",
        "spiCSDelay",
        "spiSclkPeriod",
    };

    console_write("acpi-input: scanning DSDT/SSDT for SPI/topcase markers\n");
    uint64_t matches = 0;
    input_summaries_printed = 0;
    for (uint64_t i = 0; i < ACPI_INPUT_DEVICE_SUMMARY_MAX; i++) {
        input_summary_offsets[i] = 0;
    }

    for (uint64_t table_index = 0; table_index < table_count; table_index++) {
        const struct acpi_table_info *table = &tables[table_index];
        if (!table_signature_is(table, "DSDT") && !table_signature_is(table, "SSDT")) {
            continue;
        }

        const struct sdt_header *header = pmm_phys_to_virt(table->physical_address);
        if (header == NULL || header->length < sizeof(*header)) {
            continue;
        }

        const uint8_t *bytes = (const uint8_t *)header;
        print_topcase_summary_for_table(table, bytes, header->length);
        for (uint32_t offset = sizeof(*header); offset < header->length; offset++) {
            for (uint64_t pattern_index = 0;
                 pattern_index < sizeof(patterns) / sizeof(patterns[0]);
                 pattern_index++) {
                const char *pattern = patterns[pattern_index];
                if (!bytes_match(bytes, offset, header->length, pattern)) {
                    continue;
                }

                console_printf("acpi-input: %s phys=%x off=%u key=%s",
                    table->signature,
                    table->physical_address,
                    (uint64_t)offset,
                    pattern);
                print_aml_context(bytes, header->length, offset);
                if (pattern_wants_device_summary(pattern)) {
                    print_aml_device_summary(bytes, header->length, offset);
                }
                matches++;
                if (matches >= ACPI_INPUT_SCAN_MAX_MATCHES) {
                    console_write("acpi-input: match limit reached\n");
                    return;
                }
            }
        }
    }

    if (matches == 0) {
        console_write("acpi-input: no topcase markers found\n");
    }
}

const struct acpi_ioapic_info *acpi_ioapic(void) {
    return &ioapic_info;
}

const struct acpi_interrupt_source_override *acpi_irq_override(uint8_t source_irq) {
    if (source_irq >= sizeof(irq_overrides) / sizeof(irq_overrides[0]) ||
        !irq_overrides[source_irq].present) {
        return NULL;
    }

    return &irq_overrides[source_irq];
}

uint64_t acpi_table_count(void) {
    return table_count;
}

const struct acpi_table_info *acpi_table_at(uint64_t index) {
    if (index >= table_count) {
        return NULL;
    }
    return &tables[index];
}

uint64_t acpi_mcfg_allocation_count(void) {
    return mcfg_allocation_count;
}

const struct acpi_mcfg_allocation *acpi_mcfg_allocation_at(uint64_t index) {
    if (index >= mcfg_allocation_count) {
        return NULL;
    }
    return &mcfg_allocations[index];
}

void acpi_print_status(void) {
    console_printf("acpi: tables=%u mcfg=%u\n", table_count, mcfg_allocation_count);
    for (uint64_t i = 0; i < table_count; i++) {
        const struct acpi_table_info *table = &tables[i];
        console_printf("  %s phys=%x length=%u rev=%u\n",
            table->signature,
            table->physical_address,
            (uint64_t)table->length,
            (uint64_t)table->revision);
    }

    for (uint64_t i = 0; i < mcfg_allocation_count; i++) {
        const struct acpi_mcfg_allocation *entry = &mcfg_allocations[i];
        console_printf("  mcfg[%u] base=%x segment=%u buses=%u-%u\n",
            i,
            entry->base_address,
            (uint64_t)entry->pci_segment,
            (uint64_t)entry->start_bus,
            (uint64_t)entry->end_bus);
    }

    if (ioapic_info.present) {
        console_printf("  ioapic id=%u addr=%x gsi_base=%u\n",
            (uint64_t)ioapic_info.id,
            (uint64_t)ioapic_info.address,
            (uint64_t)ioapic_info.gsi_base);
    }
}
