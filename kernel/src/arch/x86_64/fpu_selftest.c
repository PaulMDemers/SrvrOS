/* Compiled without SIMD: only the assembly probe loads/captures test registers. */
#include <srvros/console.h>
#include <srvros/fpu.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include "fpu_test_state.h"

bool fpu_probe(const uint8_t *expected, uint8_t *actual, uint64_t wait_tick);
static unsigned depth, failures, traps;
static bool active;

bool fpu_selftest_trap(uint64_t cookie) {
    if (!active || cookie != 0x46505554) return false;
    uint8_t corrupt[512] __attribute__((aligned(16)));
    fpu_test_pattern(corrupt, 0xee);
    __asm__ volatile ("fxrstor64 (%0)" : : "r"(corrupt) : "memory");
    traps++;
    if (++depth == 1) {
        uint8_t nested[512] __attribute__((aligned(16)));
        uint8_t actual[512] __attribute__((aligned(16)));
        fpu_test_pattern(nested, 0x53);
        fpu_probe(nested, actual, 0);
        if (!fpu_test_equal(nested, actual)) failures++;
        /* Suspend this trap's stack while another kernel/user thread runs. */
        scheduler_yield();
    }
    depth--;
    __asm__ volatile ("fxrstor64 (%0)" : : "r"(corrupt) : "memory");
    return true;
}

bool fpu_selftest(void) {
    if (!fpu_available() || timer_ticks() == 0 || active) {
        console_write("fpustress: unavailable (requires FPU and running timer)\n");
        return false;
    }
    uint8_t expected[512] __attribute__((aligned(16)));
    uint8_t actual[512] __attribute__((aligned(16)));
    depth = failures = traps = 0;
    active = true;
    uint64_t ticks = timer_ticks(), switches = scheduler_preemptions();
    for (unsigned i = 0; i < 128; i++) {
        fpu_test_pattern(expected, (uint8_t)i);
        if (!fpu_probe(expected, actual, 1)) { failures++; break; }
        if (!fpu_test_equal(expected, actual)) failures++;
    }
    active = false;
    console_printf("fpustress: %s iterations=128 traps=%u failures=%u ticks=%u preemptions=%u\n",
        failures == 0 && traps == 256 ? "PASS" : "FAIL",
        (uint64_t)traps, (uint64_t)failures, timer_ticks() - ticks,
        scheduler_preemptions() - switches);
    return failures == 0 && traps == 256 && timer_ticks() > ticks;
}
