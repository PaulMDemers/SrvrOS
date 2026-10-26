#ifndef SRVROS_TOOL_TIMING_H
#define SRVROS_TOOL_TIMING_H
#include <srvros/desk_log.h>

/* Sparse lifecycle checkpoints, never per-frame/poll logging. Times are wall
 * clock (including scheduling/I/O waits), not CPU time. Begin survives a stall;
 * elapsed includes its diagnostic write. No deadline or ownership is changed. */
static inline uint64_t tool_timing(int role,uint64_t window,const char *stage,
                                  const char *edge,uint64_t start) {
    uint64_t now=(uint64_t)srv_monotonic_us();
    desk_log("app-timing: pid=%llu role=%d window=%llu stage=%s edge=%s at_us=%llu elapsed_us=%llu\n",
        (unsigned long long)srv_getpid(),role,(unsigned long long)window,stage,edge,
        (unsigned long long)now,(unsigned long long)(start && now>=start?now-start:0));
    return now;
}
static inline uint64_t tool_timing_begin(int role,uint64_t window,const char *stage) {
    return tool_timing(role,window,stage,"begin",0);
}
static inline void tool_timing_end(int role,uint64_t window,const char *stage,uint64_t start) {
    tool_timing(role,window,stage,"end",start);
}
static inline void tool_timing_event(int role,uint64_t window,const char *stage) {
    tool_timing(role,window,stage,"event",0);
}
#endif
