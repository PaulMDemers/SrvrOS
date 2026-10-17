/* Shared diagnostic patterns. Compare architecturally defined register bytes,
 * not FXSAVE padding, instruction pointers, or the implementation's MXCSR mask. */
#ifndef SRVROS_FPU_TEST_STATE_H
#define SRVROS_FPU_TEST_STATE_H
#include <srvros/fpu.h>
static void fpu_test_pattern(uint8_t *area, uint8_t seed) {
    for (unsigned i = 0; i < 512; i++) area[i] = 0;
    area[0] = 0x7f; area[1] = 3 | ((seed & 3) << 2); /* Mask exceptions, vary rounding. */
    area[4] = 1; /* ST0 is a valid 80-bit value near 1.0, TOP=0. */
    area[32] = seed; area[33] = seed ^ 0x5a;
    area[39] = 0x80; area[40] = 0xff; area[41] = 0x3f;
    area[24] = 0x80; area[25] = 0x1f | ((seed & 3) << 5);
    for (unsigned i = 160; i < 416; i++) area[i] = (uint8_t)(seed + i * 17);
}
static bool fpu_test_equal(const uint8_t *a, const uint8_t *b) {
    for (unsigned i = 0; i < 5; i++) if (a[i] != b[i]) return false;
    for (unsigned i = 24; i < 28; i++) if (a[i] != b[i]) return false;
    for (unsigned i = 32; i < 42; i++) if (a[i] != b[i]) return false;
    for (unsigned i = 160; i < 416; i++) if (a[i] != b[i]) return false;
    return true;
}
#endif
