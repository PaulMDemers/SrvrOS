#ifndef SRVROS_X86_64_LAPIC_H
#define SRVROS_X86_64_LAPIC_H

#include <stdbool.h>
#include <stdint.h>

bool lapic_init(void);
void lapic_timer_init(uint8_t vector, uint32_t initial_count);
void lapic_send_eoi(void);

/* Sequential read-only xAPIC snapshot, not an atomic source attribution.
 * Caller serializes CPU0 use; false clears output. bits: ISR=1, IRR=2, TMR=4.
 * Does not send EOI, read ESR, or change controller configuration. */
struct lapic_vector_state { uint32_t bits,lint0; };
bool lapic_vector_snapshot(uint8_t vector,struct lapic_vector_state *out);

#endif
