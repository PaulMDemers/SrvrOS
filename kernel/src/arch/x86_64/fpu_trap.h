#ifndef SRVROS_X86_64_FPU_TRAP_H
#define SRVROS_X86_64_FPU_TRAP_H

#define FPU_COMMON_SAVED_RSP 0
#define FPU_COMMON_TRAP_AREA 16
#define FPU_COMMON_PROOF 528
#define FPU_COMMON_RESERVATION 592

#ifndef __ASSEMBLER__
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "isr_frame.h"
#include <srvros/fpu.h>

struct fpu_user_trap_proof {
    const struct isr_frame *frame;
    uint64_t thread_identity, address_space, image_generation;
    struct fpu_state *user_fpu, *kernel_fpu;
    void *process;
    uint64_t state;
} __attribute__((aligned(16)));
struct fpu_user_snapshot { _Alignas(16) uint8_t bytes[FPU_FXSAVE_AREA_SIZE]; };
_Static_assert(FPU_COMMON_SAVED_RSP==0 && FPU_COMMON_TRAP_AREA==16, "saved stack and trap offsets");
_Static_assert(FPU_COMMON_TRAP_AREA+FPU_FXSAVE_AREA_SIZE==FPU_COMMON_PROOF, "trap/proof boundary");
_Static_assert(FPU_COMMON_PROOF+64==FPU_COMMON_RESERVATION && FPU_COMMON_RESERVATION%16==0, "reservation layout");
_Static_assert(sizeof(struct fpu_user_trap_proof)==64, "proof size");
_Static_assert(_Alignof(struct fpu_user_trap_proof)==16, "proof alignment");
_Static_assert(offsetof(struct fpu_user_trap_proof, frame)==0, "frame offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, thread_identity)==8, "identity offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, address_space)==16, "root offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, image_generation)==24, "image offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, user_fpu)==32, "user FPU offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, kernel_fpu)==40, "kernel FPU offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, process)==48, "process offset");
_Static_assert(offsetof(struct fpu_user_trap_proof, state)==56, "proof state");
_Static_assert(sizeof(struct fpu_user_snapshot)==512 && _Alignof(struct fpu_user_snapshot)==16, "snapshot layout");
void fpu_common_trap_enter(const struct isr_frame *,uint8_t *,struct fpu_user_trap_proof *);
void fpu_common_trap_exit(const struct isr_frame *,const uint8_t *,struct fpu_user_trap_proof *);
/* Output is a resident, caller-owned kernel object, valid for the entire call.
 * Numeric range checks do not establish allocation or lifetime. Aliases of the
 * live trap reservation/frame and either FPU object are refused. */
bool fpu_snapshot_user_trap(const struct isr_frame *,struct fpu_user_snapshot *);
bool fpu_consume_user_trap(const struct isr_frame *);
#endif
#endif
