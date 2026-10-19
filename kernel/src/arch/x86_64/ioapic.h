#ifndef SRVROS_X86_64_IOAPIC_H
#define SRVROS_X86_64_IOAPIC_H

#include <stdbool.h>
#include <stdint.h>

bool ioapic_init(void);
bool ioapic_route_isa_irq(uint8_t irq, uint8_t vector);
bool ioapic_route_pci_irq(uint8_t irq, uint8_t vector);

/* CPU0 route preparation remains masked. Caller owns the source and must
 * install a handler before unmasking. Refuses an already unmasked route and
 * vectors without installed IRQ stubs. No ACPI conforming-flag assumptions. */
bool ioapic_read_gsi(uint32_t gsi,uint32_t *low,uint32_t *high);
bool ioapic_prepare_gsi(uint32_t gsi,uint8_t vector,bool active_low,bool level);
bool ioapic_mask_gsi(uint32_t gsi,bool masked);

/* Restore a previously read masked route only after source quiescence. Both
 * saved and current routes must be masked with no pending delivery/remote IRR. */
bool ioapic_restore_masked_gsi(uint32_t gsi, uint32_t low, uint32_t high);

#endif
