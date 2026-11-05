#include "signal_context.h"

_Static_assert(sizeof(struct signal_context_snapshot) == 688, "snapshot size");
_Static_assert(offsetof(struct signal_context_snapshot, flags) == 8, "flags offset");
_Static_assert(offsetof(struct signal_context_snapshot, r15) == 32, "register offset");
_Static_assert(offsetof(struct signal_context_snapshot, rip) == 152, "rip offset");
_Static_assert(offsetof(struct signal_context_snapshot, fpu) == 176, "fpu offset");
_Static_assert(_Alignof(struct signal_context_snapshot) >= 16, "snapshot alignment");

static bool user_address(uint64_t address) {
    return address != 0 && address < (UINT64_C(1) << 47);
}

static bool valid_frame(const struct isr_frame *f) {
    /* Only arithmetic flags, TF, IF, DF, OF, RF, AC and ID may be set.
     * Require architectural bit 1 and IF; reject IOPL, NT, VM and reserved bits. */
    const uint64_t allowed = UINT64_C(0x250fd7);
    return f && f->cs == 0x23 && f->ss == 0x1b &&
        user_address(f->rip) && user_address(f->rsp) &&
        (f->rflags & UINT64_C(0x202)) == UINT64_C(0x202) &&
        (f->rflags & ~allowed) == 0;
}

bool signal_context_capture(struct signal_context_snapshot *out,
    const struct isr_frame *f, const uint8_t fpu[SIGNAL_CONTEXT_FPU_SIZE],
    uint64_t flags, uint64_t token, uint64_t mask) {
    if (!out || !fpu || !valid_frame(f)) return false;
    struct signal_context_snapshot s = {0};
    s.size = sizeof(s); s.version = SIGNAL_CONTEXT_VERSION;
    s.flags = flags; s.token = token; s.interrupted_mask = mask;
    s.r15=f->r15; s.r14=f->r14; s.r13=f->r13; s.r12=f->r12;
    s.r11=f->r11; s.r10=f->r10; s.r9=f->r9; s.r8=f->r8;
    s.rbp=f->rbp; s.rdi=f->rdi; s.rsi=f->rsi; s.rdx=f->rdx;
    s.rcx=f->rcx; s.rbx=f->rbx; s.rax=f->rax;
    s.rip=f->rip; s.rsp=f->rsp; s.rflags=f->rflags;
    for (size_t i=0; i<SIGNAL_CONTEXT_FPU_SIZE; ++i) s.fpu[i]=fpu[i];
    *out=s;
    return true;
}

bool signal_context_plan_frame(struct signal_context_plan *out, uint64_t rsp,
    size_t bytes, const struct signal_context_range *active, size_t count) {
    if (!out || !user_address(rsp) || bytes < 8 + sizeof(struct signal_context_snapshot) ||
        bytes > SIGNAL_CONTEXT_MAX_FRAME || count > SIGNAL_CONTEXT_MAX_DEPTH ||
        (count && !active) || rsp < 128 || rsp - 128 < bytes + 8) return false;
    uint64_t limit=rsp-128;
    uint64_t entry=((limit-bytes-8) & ~UINT64_C(15))+8;
    uint64_t end=entry+bytes;
    if (!user_address(entry) || !user_address(end-1) || end > limit) return false;
    for (size_t i=0; i<count; ++i) {
        if (!user_address(active[i].start) || active[i].end <= active[i].start ||
            !user_address(active[i].end-1) ||
            (entry < active[i].end && active[i].start < end)) return false;
    }
    *out=(struct signal_context_plan){entry, entry+8, end};
    return true;
}

bool signal_context_push(struct signal_context_stack *stack,
    const struct signal_context_record *record, uint64_t image, uint64_t thread) {
    if (!stack || !record || !image || !thread || stack->depth >= SIGNAL_CONTEXT_MAX_DEPTH ||
        record->image_generation != image || record->thread_generation != thread ||
        !record->frame_identity || !record->token || record->token <= stack->last_token ||
        !valid_frame(&record->frame)) return false;
    if (stack->depth &&
        (stack->records[stack->depth-1].image_generation != image ||
         stack->records[stack->depth-1].thread_generation != thread)) return false;
    stack->records[stack->depth]=*record;
    stack->depth++;
    stack->last_token=record->token;
    return true;
}

bool signal_context_pop(struct signal_context_stack *stack, struct signal_context_record *out,
    uint64_t image, uint64_t thread, uint64_t token, uint64_t identity) {
    if (!stack || !out || !stack->depth || stack->depth > SIGNAL_CONTEXT_MAX_DEPTH ||
        !image || !thread || !token || !identity) return false;
    const struct signal_context_record *record=&stack->records[stack->depth-1];
    if (record->image_generation != image || record->thread_generation != thread ||
        record->token != token || record->frame_identity != identity) return false;
    *out=*record;
    stack->records[stack->depth-1]=(struct signal_context_record){0};
    stack->depth--;
    return true;
}

bool signal_context_next_generation(uint64_t *state, uint64_t *out) {
    if (!state || !out || state == out || *state == UINT64_MAX) return false;
    uint64_t next=*state+1;
    *state=next;
    *out=next;
    return true;
}
