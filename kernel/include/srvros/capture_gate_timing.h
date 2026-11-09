#ifndef SRVROS_CAPTURE_GATE_TIMING_H
#define SRVROS_CAPTURE_GATE_TIMING_H
#include <stdint.h>
/* CPU0 diagnostic metadata only; no userspace ABI or eligibility authority. */
struct capture_gate_counts {
    uint64_t calls,wall_us,owned_us,scopes,blocked_us,ready_us,running_us;
    uint64_t blocks,dispatches,exclusions,invalid;
};
struct capture_gate_owner {
    uint64_t thread,pid,syscall,scope_start_us,scope_end_us,overlap_start_us,overlap_end_us;
};
struct capture_gate_wait {
    uint64_t start_us,end_us;
    struct capture_gate_counts counts;
    struct capture_gate_owner owner;
};
struct capture_gate_summary {
    struct capture_gate_counts total;
    struct capture_gate_wait worst;
    uint64_t active;
};
#endif
