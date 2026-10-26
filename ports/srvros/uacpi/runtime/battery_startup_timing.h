#ifndef BATTERY_STARTUP_TIMING_H
#define BATTERY_STARTUP_TIMING_H
#include <stdint.h>
#include <stdbool.h>
/* CPU0 worker writes; terminal owner reads with scheduling excluded. Nested
 * categories overlap: wall times are observations, never CPU time or additive. */
struct battery_startup_span {
    uint64_t calls, requested, samples, elapsed_us, max_us, started_us, argument;
    bool active;
};
static inline bool battery_startup_begin(struct battery_startup_span *s,
        uint64_t now,uint64_t argument,bool sampled) {
    s->calls++;s->requested+=argument;
    if(!sampled)return false;
    s->samples++;s->started_us=now;s->argument=argument;s->active=true;return true;
}
static inline void battery_startup_end(struct battery_startup_span *s,uint64_t now) {
    if(!s->active)return;
    uint64_t elapsed=now>=s->started_us?now-s->started_us:0;
    s->elapsed_us+=elapsed;if(elapsed>s->max_us)s->max_us=elapsed;s->active=false;
}
#endif
