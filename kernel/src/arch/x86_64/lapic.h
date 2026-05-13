#ifndef SRVROS_X86_64_LAPIC_H
#define SRVROS_X86_64_LAPIC_H

#include <stdbool.h>
#include <stdint.h>

bool lapic_init(void);
void lapic_timer_init(uint8_t vector, uint32_t initial_count);
void lapic_send_eoi(void);

#endif
