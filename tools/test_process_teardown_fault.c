/* Keep the actual fault dispatcher in a separate translation unit so its
 * normal process API calls enter the process fixture's production exit path. */
#include <assert.h>
#define SRVROS_SYSCALL_CANCEL_HOST_TEST
#include "../kernel/src/arch/x86_64/idt.c"
void test_process_teardown_fault(void) {
    struct isr_frame frame={.cs=0x23,.vector=6};
    (void)dispatch_user_fault(&frame);
    assert(0 && "faulting continuation returned");
}
