#ifndef SRVROS_LAUNCH_GATE_TRACE_H
#define SRVROS_LAUNCH_GATE_TRACE_H
#include <stdbool.h>
#include <stdint.h>
/* Observation codes only. They never grant admission or retain pointers. */
enum launch_trace_reason {
    LAUNCH_TRACE_REQUEST=1, LAUNCH_TRACE_PARENT, LAUNCH_TRACE_MAPPINGS,
    LAUNCH_TRACE_PATH, LAUNCH_TRACE_ATOMIC, LAUNCH_TRACE_STDIO,
    LAUNCH_TRACE_RESOLVE, LAUNCH_TRACE_SOURCE, LAUNCH_TRACE_ELF,
    LAUNCH_TRACE_IMAGE_BOUND, LAUNCH_TRACE_HANDLED, LAUNCH_TRACE_ERROR,
    LAUNCH_TRACE_REASON_COUNT
};
enum launch_atomic_reason {
    LAUNCH_ATOMIC_OK, LAUNCH_ATOMIC_STATE, LAUNCH_ATOMIC_NO_FOREIGN,
    LAUNCH_ATOMIC_SAME_IMAGE, LAUNCH_ATOMIC_CALLER, LAUNCH_ATOMIC_ROOT,
    LAUNCH_ATOMIC_SIBLING, LAUNCH_ATOMIC_SIBLING_ROOT
};
struct launch_gate_context {
    uint64_t kind,epoch; /* 1 exec, 2 fcntl; reset at every raw syscall entry. */
    union {
        struct {uint64_t reason,request_mask,mappings,atomic,detail,reserved;} exec;
        struct {uint64_t fd,command,scalar,arg,type,flags;} fcntl;
    };
};
bool scheduler_launch_trace_enable(void);
bool scheduler_launch_trace_enabled(void);
void scheduler_launch_trace_entry(void);
void scheduler_launch_trace_exec(uint64_t reason,uint64_t request_mask,uint64_t mappings,uint64_t atomic,uint64_t detail);
void scheduler_launch_trace_fcntl(uint64_t fd,uint64_t command,uint64_t arg,uint64_t type,uint64_t flags);
void scheduler_print_launch_trace(void);
/* Same existing atomic predicate, with its first refusal exposed read-only. */
uint64_t scheduler_io_process_atomic_reason(void *process);
#endif
