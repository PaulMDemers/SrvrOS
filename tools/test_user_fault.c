#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#define SRVROS_SYSCALL_CANCEL_HOST_TEST
#include "../kernel/src/arch/x86_64/idt.c"
static jmp_buf exited;
static uint64_t captured;
static bool safe=true;
static unsigned waits;
void scheduler_io_set_user_safe(bool value) { safe=value; }
void scheduler_io_wait(void) { assert(!safe); waits++; }
bool process_record_user_fault(uint64_t signal) { assert(!safe && waits); captured=signal; return true; }
uint64_t process_current_kill_status(void) { return 128+captured; }
void process_exit(uint64_t status) { assert(status==128+captured); longjmp(exited,1); }
void halt_forever(void) { assert(0); }
int main(void) {
    struct isr_frame f={.cs=0x23};
    const struct {uint64_t vector, signal;} cases[]={{0,SRV_SIGNAL_FPE},{6,SRV_SIGNAL_ILL},
        {13,SRV_SIGNAL_SEGV},{14,SRV_SIGNAL_SEGV},{19,SRV_SIGNAL_FPE}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        f.vector=cases[i].vector; safe=true; captured=0; waits=0;
        if(!setjmp(exited)) { (void)dispatch_user_fault(&f); assert(0); }
        assert(captured==cases[i].signal && waits==1);
    }
    f.cs=0x08;f.vector=6; assert(!dispatch_user_fault(&f));
    f.cs=0x23;f.vector=2; assert(!dispatch_user_fault(&f));
    f.vector=8; assert(!dispatch_user_fault(&f));
    f.vector=18; assert(!dispatch_user_fault(&f));
    return 0;
}
