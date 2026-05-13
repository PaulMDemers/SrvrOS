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

static struct acpi_ioapic_info ioapic_info;
static struct acpi_interrupt_source_override irq_overrides[16];

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
    struct sdt_header *madt_header = NULL;

    ioapic_info = (struct acpi_ioapic_info) { 0 };
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
            madt_header = find_table_xsdt(xsdt, "APIC");
        }
    }

    if (madt_header == NULL && root->rsdt_address != 0) {
        struct sdt_header *rsdt = pmm_phys_to_virt(root->rsdt_address);
        if (table_is_valid(rsdt, "RSDT")) {
            madt_header = find_table_rsdt(rsdt, "APIC");
        }
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
