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
void fpu_trap_enter(uint64_t cs);
void fpu_trap_exit(uint64_t cs);
void fpu_enter_user(void);

#endif
