#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/syscall.c"
static struct srv_sched_trace output;
static bool mapped=true,available=true;
static unsigned calls;
__attribute__((noinline)) bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(writable);return mapped && address==(uint64_t)&output && length==sizeof(output);
}
bool scheduler_trace(unsigned operation,struct srv_sched_trace *out) {
    assert(operation<=1);calls++;*out=(struct srv_sched_trace){.version=1,.start_us=123,.end_us=456};return available;
}
int main(void) {
    assert(syscall_sched_trace(0,&output,sizeof(output))==0 && calls==1 && output.version==1 && output.start_us==123);
    struct srv_sched_trace saved=output;
    assert(syscall_sched_trace(2,&output,sizeof(output))==-1 && calls==1);
    assert(syscall_sched_trace(0,NULL,sizeof(output))==-1 && calls==1);
    assert(syscall_sched_trace(0,&output,sizeof(output)-1)==-1 && calls==1);
    assert(syscall_sched_trace(0,&output,sizeof(output)+1)==-1 && calls==1);
    mapped=false;assert(syscall_sched_trace(0,&output,sizeof(output))==-1 && calls==1);
    mapped=true;available=false;assert(syscall_sched_trace(1,&output,sizeof(output))==-1 && calls==2);
    assert(!memcmp(&output,&saved,sizeof(output)));
    puts("scheduler-trace-syscall: PASS exact bounded copy, operation/size/mapping refusal and unavailable trace");
}
