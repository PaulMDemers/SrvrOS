#ifndef SRVROS_BATTERY_SUPERVISOR_H
#define SRVROS_BATTERY_SUPERVISOR_H
#include <stdbool.h>
#include <stdint.h>
/* Existing ordinary-session diagnostic limits; zero means no session deadline.
 * These do not control the firmware operation/service watchdogs. */
static inline uint64_t battery_supervisor_limit(bool reopen,bool power,unsigned audio,bool capacity) {
    uint64_t limit=0,candidate;
    if(reopen)limit=20000000;
    if(power && (!limit || limit>300000000))limit=300000000;
    candidate=audio==1?1950000000ull:audio==2?210000000ull:0;
    if(candidate && (!limit || limit>candidate))limit=candidate;
    if(capacity && (!limit || limit>240000000))limit=240000000;
    return limit;
}
/* Wake no later than the next tick at a sub-tick boundary, otherwise round
 * down. The caller rechecks the original monotonic deadline, never extends it.
 * An early/spurious wake simply computes the remaining interval again. */
static inline uint64_t battery_supervisor_tick_deadline(uint64_t now_us,uint64_t ticks,uint64_t end_us) {
    if(!end_us)return 0;
    uint64_t remaining=end_us>now_us?(end_us-now_us)/10000:0;
    if(!remaining)remaining=1;
    return remaining>UINT64_MAX-ticks?UINT64_MAX:ticks+remaining;
}
#endif
