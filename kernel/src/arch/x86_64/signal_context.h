#ifndef SRVROS_X86_64_SIGNAL_CONTEXT_H
#define SRVROS_X86_64_SIGNAL_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "isr_frame.h"

#define SIGNAL_CONTEXT_VERSION 1u
#define SIGNAL_CONTEXT_FPU_SIZE 512u
#define SIGNAL_CONTEXT_MAX_DEPTH 4u
#define SIGNAL_CONTEXT_MAX_FRAME 1024u

/* Internal representation only; no user ABI is published by this module. */
struct signal_context_snapshot {
    uint32_t size, version;
    uint64_t flags, token, interrupted_mask;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, rsp, rflags;
    _Alignas(16) uint8_t fpu[SIGNAL_CONTEXT_FPU_SIZE];
};

struct signal_context_range { uint64_t start, end; }; /* [start, end) */
struct signal_context_plan {
    uint64_t entry_rsp, snapshot, end;
};

struct signal_context_record {
    struct isr_frame frame;
    _Alignas(16) uint8_t fpu[SIGNAL_CONTEXT_FPU_SIZE];
    uint64_t mask, image_generation, thread_generation, token, frame_identity;
};
struct signal_context_stack {
    struct signal_context_record records[SIGNAL_CONTEXT_MAX_DEPTH];
    uint64_t last_token; /* strictly increasing over this stack's lifetime */
    unsigned depth;
};

/* All failures leave outputs and stack unchanged. Inputs must not alias outputs. */
bool signal_context_capture(struct signal_context_snapshot *out,
    const struct isr_frame *frame, const uint8_t fpu[SIGNAL_CONTEXT_FPU_SIZE],
    uint64_t flags, uint64_t token, uint64_t mask);
/* frame_bytes includes the return slot; snapshot follows that slot at +8.
 * This plans addresses only. It does not establish mapping or copy safety. */
bool signal_context_plan_frame(struct signal_context_plan *out, uint64_t interrupted_rsp,
    size_t frame_bytes, const struct signal_context_range *active, size_t active_count);
bool signal_context_push(struct signal_context_stack *stack,
    const struct signal_context_record *record, uint64_t image_generation,
    uint64_t thread_generation);
bool signal_context_pop(struct signal_context_stack *stack, struct signal_context_record *out,
    uint64_t image_generation, uint64_t thread_generation, uint64_t token,
    uint64_t frame_identity);
/* State starts at zero; UINT64_MAX is the last issued ID, then exhausted. */
bool signal_context_next_generation(uint64_t *state, uint64_t *out);

#endif
