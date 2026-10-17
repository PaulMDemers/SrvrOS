#ifndef SRVROS_TIMER_H
#define SRVROS_TIMER_H

#include <stdbool.h>
#include <stdint.h>

bool timer_reference_init(void);
bool timer_reference_ready(void);
uint64_t timer_monotonic_us(void);
bool timer_delay_us(uint64_t delay);
void timer_init(uint64_t frequency_hz);
void timer_handle_tick(void);
uint64_t timer_ticks(void);
void timer_wait_ticks(uint64_t ticks);
bool timer_wait_ticks_bounded(uint64_t ticks, uint64_t spin_limit);

#endif
