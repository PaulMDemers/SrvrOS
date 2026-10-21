#ifndef SRVROS_IRQ_HANDLERS_H
#define SRVROS_IRQ_HANDLERS_H
#include <stdbool.h>
#include <stdint.h>
/* Shared CPU0 IRQ callbacks. Called with interrupts masked; callbacks must
 * neither sleep nor yield. Registration does not configure/unmask hardware.
 * Timer/keyboard/serial/mouse lines are reserved. Zero is an invalid token. */
typedef bool (*irq_handler_fn)(void *);
uint64_t irq_handler_register(uint8_t line,irq_handler_fn,void *context);
/* Optional bounded observer, same context/lifetime as the handler. Runs once
 * after generic EOI, still IRQ-masked and before iret; no claim result. */
typedef void (*irq_completion_fn)(void *);
uint64_t irq_handler_register_observed(uint8_t line,irq_handler_fn,irq_completion_fn,void *context);
/* Disable owned device sources and mask its route before removal. Success
 * excludes any later callback, so caller-owned context may then be freed.
 * Mutation from inside dispatch is rejected, including self-removal. */
bool irq_handler_remove(uint64_t token);
/* Dispatcher keeps IRQs masked across dispatch, EOI and completion. An observed
 * delivery excludes registry mutation/reentrant dispatch until completion. */
bool irq_handlers_dispatch(uint8_t line);
void irq_handlers_complete(uint8_t line);
#endif
