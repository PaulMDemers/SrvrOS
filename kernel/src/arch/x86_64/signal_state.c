#include "signal_state.h"

static bool user_address(uint64_t p) { return p && p < (UINT64_C(1)<<47); }
static bool valid_identity(struct signal_state_identity i) {
    return i.image_generation && i.public_tid && i.public_tid<=INT64_MAX && i.scheduler_identity;
}
static bool same_identity(struct signal_state_identity a,struct signal_state_identity b) {
    return a.image_generation==b.image_generation && a.public_tid==b.public_tid &&
        a.scheduler_identity==b.scheduler_identity;
}
bool signal_action_descriptor_valid(const struct signal_action_descriptor *a) {
    if (!a || a->mode>SIGNAL_ACTION_INFO || (a->flags & ~(uint64_t)(SIGNAL_ACTION_RESTART|SIGNAL_ACTION_RESET)) ||
        (a->mask & (UINT64_C(1)|(UINT64_C(1)<<9)|(UINT64_C(1)<<19)))) return false;
    switch (a->mode) {
    case SIGNAL_ACTION_DEFAULT: case SIGNAL_ACTION_IGNORE: return !a->handler && !a->trampoline;
    case SIGNAL_ACTION_LEGACY: return !a->trampoline && (!a->handler || (a->handler>1 && user_address(a->handler)));
    case SIGNAL_ACTION_INFO: return a->handler>1 && a->trampoline>1 &&
        user_address(a->handler) && user_address(a->trampoline);
    default: return false;
    }
}
void signal_thread_state_reset(struct signal_thread_state *state) {
    if (state) *state=(struct signal_thread_state){0};
}
static bool valid_bound(const struct signal_thread_state *state,struct signal_state_identity id) {
    if (!state || !valid_identity(id) || !same_identity(state->identity,id) ||
        state->stack.depth>SIGNAL_CONTEXT_MAX_DEPTH) return false;
    uint64_t previous=0;
    for (unsigned i=0;i<state->stack.depth;i++) {
        const struct signal_context_record *r=&state->stack.records[i];
        struct signal_context_range range=state->ranges[i];
        struct signal_context_snapshot check;
        if (r->image_generation!=id.image_generation || r->thread_generation!=id.scheduler_identity ||
            !r->token || r->token<=previous || r->token>state->stack.last_token ||
            !user_address(range.start) || range.end<=range.start || !user_address(range.end-1) ||
            range.end-range.start>SIGNAL_CONTEXT_MAX_FRAME || r->frame_identity!=range.start ||
            !signal_context_capture(&check,&r->frame,r->fpu,0,r->token,r->mask)) return false;
        for (unsigned j=0;j<i;j++)
            if (range.start<state->ranges[j].end && state->ranges[j].start<range.end) return false;
        previous=r->token;
    }
    return true;
}
bool signal_thread_state_bind(struct signal_thread_state *state,struct signal_state_identity id) {
    if (!state || !valid_identity(id) || state->stack.depth>SIGNAL_CONTEXT_MAX_DEPTH) return false;
    if (same_identity(state->identity,id)) return valid_bound(state,id);
    if (state->identity.image_generation || state->identity.public_tid || state->identity.scheduler_identity ||
        state->stack.depth || state->stack.last_token) return false;
    state->identity=id;
    return true;
}

bool signal_thread_state_push(struct signal_thread_state *state,struct signal_state_identity id,
    const struct signal_context_record *record,struct signal_context_range range) {
    if (!valid_bound(state,id) || !record || state->stack.depth==SIGNAL_CONTEXT_MAX_DEPTH ||
        !user_address(range.start) || range.end<=range.start || !user_address(range.end-1) ||
        range.end-range.start>SIGNAL_CONTEXT_MAX_FRAME || record->frame_identity!=range.start) return false;
    for (unsigned i=0;i<state->stack.depth;i++)
        if (range.start<state->ranges[i].end && state->ranges[i].start<range.end) return false;
    struct signal_thread_state next=*state;
    if (!signal_context_push(&next.stack,record,id.image_generation,id.scheduler_identity)) return false;
    next.ranges[next.stack.depth-1]=range;
    *state=next;
    return true;
}
bool signal_thread_state_pop(struct signal_thread_state *state,struct signal_state_identity id,
    uint64_t token,uint64_t frame_identity,struct signal_context_record *record_out,
    struct signal_context_range *range_out) {
    if (!record_out || !range_out || !valid_bound(state,id) || !state->stack.depth) return false;
    struct signal_thread_state next=*state;
    unsigned index=next.stack.depth-1;
    struct signal_context_record record;
    struct signal_context_range range=next.ranges[index];
    if (!signal_context_pop(&next.stack,&record,id.image_generation,id.scheduler_identity,token,frame_identity)) return false;
    next.ranges[index]=(struct signal_context_range){0};
    *state=next;*record_out=record;*range_out=range;
    return true;
}
