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
            madt_header = find_table_xsdt(xsdt, "APIC");
            mcfg_header = find_table_xsdt(xsdt, "MCFG");
        }
    }

    if (madt_header == NULL && root->rsdt_address != 0) {
        struct sdt_header *rsdt = pmm_phys_to_virt(root->rsdt_address);
        if (table_is_valid(rsdt, "RSDT")) {
            remember_table(rsdt, root->rsdt_address);
            remember_child_tables_rsdt(rsdt);
            madt_header = find_table_rsdt(rsdt, "APIC");
            mcfg_header = find_table_rsdt(rsdt, "MCFG");
        }
    }

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
