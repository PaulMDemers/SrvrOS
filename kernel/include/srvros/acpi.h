#ifndef SRVROS_ACPI_H
#define SRVROS_ACPI_H

#include <stdbool.h>
#include <stdint.h>

struct acpi_ioapic_info {
    bool present;
    uint8_t id;
    uint32_t address;
    uint32_t gsi_base;
};

struct acpi_interrupt_source_override {
    bool present;
    uint8_t bus;
    uint8_t source;
    uint32_t gsi;
    uint16_t flags;
};

struct acpi_table_info {
    char signature[5];
    uint64_t physical_address;
    uint32_t length;
    uint8_t revision;
};

struct acpi_mcfg_allocation {
    bool present;
    uint64_t base_address;
    uint16_t pci_segment;
    uint8_t start_bus;
    uint8_t end_bus;
};

void acpi_init(void *rsdp);
const struct acpi_ioapic_info *acpi_ioapic(void);
const struct acpi_interrupt_source_override *acpi_irq_override(uint8_t source_irq);
uint64_t acpi_table_count(void);
const struct acpi_table_info *acpi_table_at(uint64_t index);
uint64_t acpi_mcfg_allocation_count(void);
const struct acpi_mcfg_allocation *acpi_mcfg_allocation_at(uint64_t index);
void acpi_print_status(void);
void acpi_print_input_diagnostics(void);

#endif
