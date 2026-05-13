#ifndef SRVROS_X86_64_IRQ_H
#define SRVROS_X86_64_IRQ_H

#include <stdint.h>

#define IRQ_VECTOR_BASE 32
#define IRQ_COUNT 16

void irq_init(void);
void irq_dispatch(uint64_t vector);
void irq0_legacy_dispatch(void);
void irq1_legacy_dispatch(void);
void irq_enable_line(uint8_t irq);
void irq_disable_line(uint8_t irq);
void irq_send_eoi(uint8_t irq);

#endif
