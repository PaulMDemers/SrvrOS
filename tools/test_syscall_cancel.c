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
static unsigned kernel_entries, user_entries, exclusions, irq_calls;
static unsigned safe_publications;
static bool simulated_if;
void syscall_cancel_test_exclude_irqs(void) { exclusions++; simulated_if=false; }
uint64_t syscall_cancel_test_read_cr3(void) { return 0x1000; }
void scheduler_account_kernel_enter(void) { kernel_entries++; }
void scheduler_io_handoff(void) {
    assert(user_safe && !in_call); /* Only a fully unwound syscall may hand off. */
    handoffs++;if(kill_during_handoff)killed=true;
}
void scheduler_io_syscall(uint64_t number) { (void)number; }
void scheduler_io_set_user_safe(bool safe) { if(safe) safe_publications++; user_safe=safe; }
void scheduler_user_enter(void) { assert(!simulated_if); user_entries++; user_safe=true; }
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
    assert(user_safe == (safe_call && (frame->cs & 3) == 3));
    in_call=true; calls++;
    if(kill_during_call) killed=true;
    frame->rax=42;
    in_call=false;
}

void irq_dispatch(uint64_t vector) {
    assert(vector>=IRQ_VECTOR_BASE && vector<IRQ_VECTOR_BASE+IRQ_COUNT && !user_safe);
    unsigned k=kernel_entries,u=user_entries,e=exclusions,s=safe_publications;
    bool flags=simulated_if;
    struct isr_frame nested={.cs=0x08,.vector=255};
    isr_dispatch(&nested);
    assert(kernel_entries==k && user_entries==u && exclusions==e && safe_publications==s);
    assert(simulated_if==flags && !user_safe);
    irq_calls++;
}
bool fpu_selftest_trap(uint64_t cookie) { (void)cookie; return true; }
bool process_record_user_fault(uint64_t sig) { (void)sig; return true; }
struct process *process_current(void) { return NULL; }
uint64_t process_pid(const struct process *p) { (void)p; return 0; }
const char *process_name(const struct process *p) { (void)p; return "test"; }
bool vmm_virt_to_phys(uint64_t address,uint64_t *out) { (void)address;(void)out;return false; }
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) { (void)address;(void)length;(void)writable;return false; }
void *pmm_phys_to_virt(uint64_t address) { (void)address;return NULL; }
void console_emergency(void) { }
void console_clear(void) { }
void console_write(const char *s) { (void)s; }
void console_printf(const char *format,...) { (void)format; }
void bootlog_dump(uint64_t count) { (void)count; }
__attribute__((noreturn)) void halt_forever(void) { assert(0); longjmp(exited,1); }

static void outer_tests(void)
{
    static struct isr_frame frame;
    frame=(struct isr_frame){.cs=0x23,.vector=0x80,.rax=7};
    killed=kill_during_call=kill_while_waiting=kill_during_handoff=false;
    volatile unsigned c,w,h,k,u,e,s;
    for(unsigned initial_if=0;initial_if<2;initial_if++) for(unsigned safe=0;safe<2;safe++) {
        safe_call=safe;user_safe=true;simulated_if=initial_if;
        c=calls;w=waits;h=handoffs;k=kernel_entries;u=user_entries;e=exclusions;s=safe_publications;
        isr_dispatch(&frame);
        assert(calls==c+1 && waits==w+!safe && handoffs==h+!safe && frame.rax==42);
        /* Audited safe syscalls intentionally admit service during dispatch;
         * unsafe syscalls do so only for the fully unwound handoff. */
        assert(safe_publications==s+1);
        assert(kernel_entries==k+1 && user_entries==u+1 && exclusions==e+1 && user_safe && !simulated_if);
    }
    unsigned irq_before=irq_calls;
    frame.vector=IRQ_VECTOR_BASE; simulated_if=true;k=kernel_entries;u=user_entries;e=exclusions;s=safe_publications;
    isr_dispatch(&frame);
    assert(irq_calls==irq_before+1 && kernel_entries==k+1 && user_entries==u+1 && exclusions==e+1 && !simulated_if && safe_publications==s);
    frame.vector=255; simulated_if=true;k=kernel_entries;u=user_entries;e=exclusions;s=safe_publications;
    isr_dispatch(&frame);
    assert(kernel_entries==k+1 && user_entries==u+1 && exclusions==e+1 && !simulated_if && safe_publications==s);
    frame.vector=3; frame.rip=0;frame.rsp=0;simulated_if=true;k=kernel_entries;u=user_entries;e=exclusions;s=safe_publications;
    isr_dispatch(&frame);
    assert(kernel_entries==k+1 && user_entries==u+1 && exclusions==e+1 && !simulated_if && safe_publications==s);
    frame.cs=0x08;frame.vector=255;
    for(unsigned initial_if=0;initial_if<2;initial_if++) for(unsigned initial_safe=0;initial_safe<2;initial_safe++) {
        simulated_if=initial_if;user_safe=initial_safe;k=kernel_entries;u=user_entries;e=exclusions;s=safe_publications;
        isr_dispatch(&frame);
        assert(kernel_entries==k && user_entries==u && exclusions==e && simulated_if==!!initial_if && user_safe==!!initial_safe && safe_publications==s);
    }
    frame.cs=0x23;frame.vector=0x80;safe_call=false;
    for(unsigned mode=0;mode<4;mode++) {
        killed=mode==0;kill_while_waiting=mode==1;kill_during_call=mode==2;kill_during_handoff=mode==3;
        user_safe=true;simulated_if=true;k=kernel_entries;u=user_entries;e=exclusions;
        c=calls;w=waits;h=handoffs;s=safe_publications;frame.rax=7;
        if(setjmp(exited)==0) { isr_dispatch(&frame); assert(0); }
        assert(kernel_entries==k+1 && user_entries==u && exclusions==e && !user_safe && exit_status==130);
        assert(calls==c+(mode>=2) && waits==w+(mode==0 ? 1 : 2));
        assert(handoffs==h+(mode==3) && safe_publications==s+(mode==3));
        assert(frame.rax==(mode>=2 ? 42u : 7u));
    }
    killed=kill_while_waiting=kill_during_call=kill_during_handoff=false;
    puts("syscall-return-host: PASS (outer interrupt return and cancellation)");
}

int main(void) {
    struct isr_frame frame={.cs=0x23};
    dispatch_syscall_with_cancellation(&frame);
    assert(calls==1 && frame.rax==42);
    assert(waits==1 && !user_safe);
    assert(handoffs==1); /* A fast writer must yield to awakened gate waiters. */
    safe_call=true; dispatch_syscall_with_cancellation(&frame);
    assert(calls==2 && waits==1 && !user_safe);
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
    /* An admitted signal-mask update can expose fatal pending cancellation.
     * It finishes metadata work first, then regains the ordinary exit gate. */
    killed=kill_while_waiting=kill_during_handoff=false;safe_call=kill_during_call=true;
    unsigned before_waits=waits,before_calls=calls,before_handoffs=handoffs;
    if(setjmp(exited)==0){dispatch_syscall_with_cancellation(&frame);assert(0);}
    assert(calls==before_calls+1 && waits==before_waits+1 && handoffs==before_handoffs && !user_safe && exit_status==130);
    outer_tests();
    puts("syscall-cancel-host: PASS (pending kill, deferred I/O kill, normal return, kernel entry)");
}

uint64_t latency_clock(void) { return 0; }
void latency_gate(uint64_t a,uint64_t b) { (void)a;(void)b; }
bool scheduler_io_active(void) { return false; }
