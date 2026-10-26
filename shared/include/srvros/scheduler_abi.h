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
/* Explicit current-thread diagnostic. BEGIN/END never change scheduling policy.
 * State durations include scheduler bookkeeping, not CPU time. Deadline ticks
 * report observed expiry, not an inferred wall-clock timer-interrupt deadline. */
#define SRV_SCHED_TRACE_VERSION 1u
#define SRV_SCHED_TRACE_BEGIN 0u
#define SRV_SCHED_TRACE_END 1u
struct srv_sched_trace {
    uint64_t version,start_us,end_us,start_tick,end_tick;
    uint64_t blocked_us,ready_us,blocks,signals,timeouts,dispatches,exclusions,invalid;
    uint64_t max_ready_us,wake_us,run_us,wake_tick,deadline_tick,reason,ready_exclusions,late_ticks;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_sched_trace)==168,"scheduler trace wire layout");
static_assert(sizeof(struct srv_scheduler_status)==48,"scheduler snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_sched_trace)==168,"scheduler trace wire layout");
_Static_assert(sizeof(struct srv_scheduler_status)==48,"scheduler snapshot wire layout");
#endif
#endif
