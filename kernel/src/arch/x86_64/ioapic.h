#ifndef SRVROS_X86_64_IOAPIC_H
#define SRVROS_X86_64_IOAPIC_H

#include <stdbool.h>
#include <stdint.h>

bool ioapic_init(void);
bool ioapic_route_isa_irq(uint8_t irq, uint8_t vector);
bool ioapic_route_pci_irq(uint8_t irq, uint8_t vector);

#endif
