#ifndef SRVROS_SCHEDULER_ABI_H
#define SRVROS_SCHEDULER_ABI_H
#include <stdint.h>
#define SRV_SCHEDULER_VERSION 1u
/* CPU0 scheduler sampling, not CPU frequency, energy or all-core utilization.
 * Counters are cumulative since boot. Compare two monotonic snapshots. */
struct srv_scheduler_status {
    uint32_t version,reserved;
    uint64_t monotonic_us,sampled_ticks,idle_ticks,switches,preemptions;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_scheduler_status)==48,"scheduler snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_scheduler_status)==48,"scheduler snapshot wire layout");
#endif
#endif
