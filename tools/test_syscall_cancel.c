#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/idt.c"

static jmp_buf exited;
static bool killed, kill_during_call, in_call;
static unsigned calls;
static uint64_t exit_status;
bool process_should_exit_current(void) { return killed; }
uint64_t process_current_kill_status(void) { return 130; }
void process_exit(uint64_t status) {
    assert(!in_call); /* Filesystem/driver cleanup must have returned first. */
    exit_status=status;
    longjmp(exited,1);
}
void syscall_dispatch(struct isr_frame *frame) {
    in_call=true; calls++;
    if(kill_during_call) killed=true;
    frame->rax=42;
    in_call=false;
}

int main(void) {
    struct isr_frame frame={.cs=0x23};
    dispatch_syscall_with_cancellation(&frame);
    assert(calls==1 && frame.rax==42);
    killed=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==1 && exit_status==130);
    killed=false; kill_during_call=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==2 && exit_status==130);
    frame.cs=0x08;
    dispatch_syscall_with_cancellation(&frame);
    assert(calls==3); /* Kernel callers are not a userspace-return boundary. */
    puts("syscall-cancel-host: PASS (pending kill, deferred I/O kill, normal return, kernel entry)");
}
