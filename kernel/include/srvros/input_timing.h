#ifndef SRVROS_INPUT_TIMING_H
#define SRVROS_INPUT_TIMING_H
#include <stdint.h>
/* Software timestamps only. Reject unavailable/backwards clocks and saturate
 * by refusing samples that cannot be represented in the aggregate. */
struct input_timing { uint64_t count,total_us,max_us; };
static inline void input_timing_add(struct input_timing *s,uint64_t start,uint64_t end) {
    if(!start || end<start || s->count==UINT64_MAX || UINT64_MAX-s->total_us<end-start) return;
    uint64_t delta=end-start;
    s->count++;s->total_us+=delta;if(delta>s->max_us)s->max_us=delta;
}
#endif
