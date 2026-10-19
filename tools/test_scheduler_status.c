#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static struct srv_scheduler_status output;
static bool mapped=true;
static unsigned reads;
/* Keep the test mapping boundary opaque like the separately compiled VMM.
 * GCC 13 otherwise folds this narrow fixture into unrelated, discarded
 * syscall paths and diagnoses their unreachable user copies. */
__attribute__((noinline)) bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(writable);
    return mapped && address==(uint64_t)&output && length==sizeof(output);
}
void scheduler_snapshot(struct scheduler_stats *out) {
    reads++;*out=(struct scheduler_stats){1000,900,4000,30};
}
uint64_t timer_monotonic_us(void) { return 10000000; }
int main(void) {
    memset(&output,0xff,sizeof(output));
    assert(syscall_scheduler_status(&output,sizeof(output))==0 && reads==1);
    assert(output.version==SRV_SCHEDULER_VERSION && !output.reserved && output.monotonic_us==10000000);
    assert(output.sampled_ticks==1000 && output.idle_ticks==900 && output.switches==4000 && output.preemptions==30);
    struct srv_scheduler_status saved=output;
    assert(syscall_scheduler_status(&output,sizeof(output)-1)==-1 && reads==1);
    assert(syscall_scheduler_status(&output,sizeof(output)+1)==-1 && reads==1);
    assert(syscall_scheduler_status(NULL,sizeof(output))==-1 && reads==1);
    mapped=false;assert(syscall_scheduler_status(&output,sizeof(output))==-1 && reads==1);
    assert(!memcmp(&saved,&output,sizeof(output)));
    puts("scheduler-status-host: PASS (bounded mapped copy, initialized version/reserved, cumulative counters, invalid-size refusal)");
}
