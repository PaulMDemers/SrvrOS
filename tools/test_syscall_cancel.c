#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#define SRVROS_SYSCALL_CANCEL_HOST_TEST
#include "../kernel/src/arch/x86_64/idt.c"

static jmp_buf exited;
static bool killed, kill_during_call, in_call;
static unsigned calls;
static uint64_t exit_status;
static bool safe_call, user_safe, kill_while_waiting;
static unsigned waits;
static unsigned handoffs;
static bool kill_during_handoff;
void scheduler_io_handoff(void) {
    assert(user_safe && !in_call); /* Only a fully unwound syscall may hand off. */
    handoffs++;if(kill_during_handoff)killed=true;
}
void scheduler_io_syscall(uint64_t number) { (void)number; }
void scheduler_io_set_user_safe(bool safe) { user_safe=safe; }
void scheduler_user_enter(void) { user_safe=true; }
void scheduler_io_wait(void) { assert(!user_safe); waits++; if(kill_while_waiting) killed=true; }
bool syscall_io_safe(const struct isr_frame *frame) { (void)frame; return safe_call; }
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
    assert(waits==1 && user_safe);
    assert(handoffs==1); /* A fast writer must yield to awakened gate waiters. */
    safe_call=true; dispatch_syscall_with_cancellation(&frame);
    assert(calls==2 && waits==1 && user_safe);
    assert(handoffs==1); /* GUI/audio-safe calls keep their normal service turn. */
    safe_call=false;
    killed=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==2 && exit_status==130 && !user_safe);
    killed=false; kill_during_call=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==3 && exit_status==130 && !user_safe);
    frame.cs=0x08;
    dispatch_syscall_with_cancellation(&frame);
    assert(calls==4 && !user_safe); /* Kernel callers are not a userspace-return boundary. */
    assert(handoffs==1);
    frame.cs=0x23;killed=kill_during_call=false;kill_while_waiting=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==4 && exit_status==130); /* No side effects after a kill in the wait queue. */
    killed=kill_while_waiting=false;kill_during_handoff=true;
    if(setjmp(exited)==0) { dispatch_syscall_with_cancellation(&frame); assert(0); }
    assert(calls==5 && handoffs==2 && exit_status==130 && !user_safe);
    puts("syscall-cancel-host: PASS (pending kill, deferred I/O kill, normal return, kernel entry)");
}

uint64_t latency_clock(void) { return 0; }
void latency_gate(uint64_t a,uint64_t b) { (void)a;(void)b; }
bool scheduler_io_active(void) { return false; }
