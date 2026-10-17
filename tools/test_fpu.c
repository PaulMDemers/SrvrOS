#include <assert.h>
#include <stdio.h>
#include "../kernel/src/arch/x86_64/fpu.c"
#include "../kernel/src/arch/x86_64/fpu_test_state.h"
void console_write(const char *s) { (void)s; }

int main(void) {
    uint8_t caller[512] __attribute__((aligned(16)));
    uint8_t outer[512] __attribute__((aligned(16)));
    uint8_t inner[512] __attribute__((aligned(16)));
    uint8_t actual[512] __attribute__((aligned(16)));
    uint8_t original[512] __attribute__((aligned(16)));
    uint8_t handler[512] __attribute__((aligned(16)));
    uint8_t other[512] __attribute__((aligned(16)));
    fxsave(caller);
    copy_fx_area(initial_fxsave_area, caller);
    fpu_test_pattern(original, 0x17);
    fpu_test_pattern(handler, 0x58);
    fpu_test_pattern(other, 0xd3);
    /* Before CR4.OSFXSR setup no FXSAVE area is accessed. */
    available = false;
    fpu_trap_enter(8, NULL); fpu_trap_exit(8, NULL);
    available = true;
    struct fpu_state kernel = {0}, user = {0}, worker = {0};
    fpu_init_state(&kernel); fpu_init_state(&user); fpu_init_state(&worker);
    fpu_set_current_kernel_state(&kernel);
    fpu_set_current_user_state(&user);

    fxrstor(original);
    fpu_trap_enter(8, outer);
    fxrstor(handler);
    fpu_trap_enter(8, inner);
    fxrstor(other);
    fpu_trap_exit(8, inner);
    fxsave(actual); assert(fpu_test_equal(actual, handler));
    /* Switch away and back while the outer trap is suspended. */
    fpu_switch_kernel_state(&kernel, &worker, NULL);
    fxrstor(other);
    fpu_switch_kernel_state(&worker, &kernel, &user);
    fpu_trap_exit(8, outer);
    fxsave(actual); assert(fpu_test_equal(actual, original));

    copy_fx_area(user.fxsave_area, original);
    fxrstor(handler);
    fpu_enter_user();
    fpu_trap_enter(0x23, NULL);
    fxsave(actual); assert(fpu_test_equal(actual, handler));
    /* A kernel IRQ nested inside a user syscall must preserve kernel state
     * without disturbing the saved user state or user_state_loaded flag. */
    fpu_trap_enter(8, inner);
    fxrstor(other);
    fpu_trap_exit(8, inner);
    assert(!user_state_loaded);
    fpu_trap_exit(0x23, NULL);
    fxsave(actual); assert(fpu_test_equal(actual, original));
    assert(user_state_loaded);
    fxrstor(caller);
    puts("fpu-host: PASS (all XMM, x87, MXCSR, nested kernel traps, task switch, user syscall, early boot)");
}
