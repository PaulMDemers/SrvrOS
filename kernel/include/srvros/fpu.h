#ifndef SRVROS_FPU_H
#define SRVROS_FPU_H

#include <stdbool.h>
#include <stdint.h>

#define FPU_FXSAVE_AREA_SIZE 512

struct fpu_state {
    uint8_t fxsave_area[FPU_FXSAVE_AREA_SIZE] __attribute__((aligned(16)));
    bool initialized;
};

bool fpu_init(void);
bool fpu_available(void);
void fpu_init_state(struct fpu_state *state);
void fpu_save(struct fpu_state *state);
void fpu_restore(struct fpu_state *state);
void fpu_set_current_kernel_state(struct fpu_state *state);
void fpu_set_current_user_state(struct fpu_state *state);
void fpu_switch_kernel_state(struct fpu_state *old_state,
    struct fpu_state *new_state,
    struct fpu_state *new_user_state);
/* Assembly supplies a separate 512-byte, 16-byte-aligned area on each trap's
 * stack. Kernel traps must preserve caller-saved SIMD registers too: the
 * interrupted instruction is not a C call boundary. fpu.c must remain no-SSE. */
void fpu_trap_enter(uint64_t cs, uint8_t *kernel_trap_area);
void fpu_trap_exit(uint64_t cs, const uint8_t *kernel_trap_area);
void fpu_selftest(void);
bool fpu_selftest_trap(uint64_t cookie);
void fpu_enter_user(void);

#endif
