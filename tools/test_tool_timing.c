#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/timing.h"
static int active,fail_begin,fail_end,begins,ends,writes;
static char output[1024];
long srv_getpid(void) {return 21;}
long srv_monotonic_us(void) {return 100;}
void srv_puts(const char *text) {
    assert(!active); /* Logging must not contaminate the measured interval. */
    writes++;snprintf(output,sizeof(output),"%s",text);
}
long srv_sched_trace(unsigned op,struct srv_sched_trace *out) {
    if(op==SRV_SCHED_TRACE_BEGIN) {
        begins++;if(fail_begin)return -1;
        assert(!active);active=1;return 0;
    }
    assert(op==SRV_SCHED_TRACE_END && active);ends++;active=0;
    if(fail_end)return -2;
    *out=(struct srv_sched_trace){.version=1,.start_us=100,.end_us=900,
        .blocked_us=500,.ready_us=200,.blocks=2,.dispatches=3,.exclusions=4,
        .invalid=0,.max_ready_us=150,.reason=7};
    return 0;
}
int main(void) {
    struct tool_phase_trace phase;
    tool_phase_begin(&phase);assert(active && !writes);
    tool_phase_end(&phase,2,9,"document-unmap");
    assert(!active && begins==1 && ends==1 && writes==1);
    assert(strstr(output,"start_us=100 end_us=900 blocked_us=500 ready_us=200"));
    assert(strstr(output,"exclusions=4 invalid=0 max_ready_us=150 reason=7\n"));
    fail_begin=1;tool_phase_begin(&phase);tool_phase_end(&phase,2,9,"document-unmap");
    assert(ends==1 && writes==2 && strstr(output,"begin=-1 end=-1 available=no"));
    fail_begin=0;fail_end=1;
    tool_phase_begin(&phase);tool_phase_end(&phase,2,9,"document-unmap");
    assert(ends==2 && writes==3 && strstr(output,"begin=0 end=-2 available=no"));
    puts("tool phase timing: PASS (write exclusion and refused/failed trace)");
    return 0;
}
