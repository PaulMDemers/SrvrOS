#ifndef SRVROS_X86_64_SIGNAL_STATE_H
#define SRVROS_X86_64_SIGNAL_STATE_H
#include "signal_context.h"

/* Internal shape only; executable mappings and pointer ownership are checked
 * by the process owner. No allocation, lock, IRQ or pointer authentication. */
enum signal_action_mode {
    SIGNAL_ACTION_DEFAULT, SIGNAL_ACTION_IGNORE, SIGNAL_ACTION_LEGACY, SIGNAL_ACTION_INFO
};
#define SIGNAL_ACTION_RESTART 1u
#define SIGNAL_ACTION_RESET 2u
struct signal_action_descriptor {
    uint64_t mode,flags,handler,trampoline,mask;
};
bool signal_action_descriptor_valid(const struct signal_action_descriptor *action);

struct signal_state_identity { uint64_t image_generation,public_tid,scheduler_identity; };
struct signal_thread_state {
    struct signal_state_identity identity;
    struct signal_context_stack stack;
    struct signal_context_range ranges[SIGNAL_CONTEXT_MAX_DEPTH];
};
/* Begin zero-initialized. Caller owns stable membership and CPU0 IRQ exclusion.
 * Inputs and outputs must not alias state or each other. Reset is retirement,
 * not an implicit rebind. No operation establishes a user-memory transaction. */
void signal_thread_state_reset(struct signal_thread_state *state);
bool signal_thread_state_bind(struct signal_thread_state *state,struct signal_state_identity identity);
bool signal_thread_state_push(struct signal_thread_state *state,struct signal_state_identity identity,
    const struct signal_context_record *record,struct signal_context_range range);
bool signal_thread_state_pop(struct signal_thread_state *state,struct signal_state_identity identity,
    uint64_t token,uint64_t frame_identity,struct signal_context_record *record_out,
    struct signal_context_range *range_out);
#endif
