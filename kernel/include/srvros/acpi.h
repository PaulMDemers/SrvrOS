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

void acpi_init(void *rsdp);
const struct acpi_ioapic_info *acpi_ioapic(void);
const struct acpi_interrupt_source_override *acpi_irq_override(uint8_t source_irq);

#endif

