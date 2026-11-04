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
struct signal_wait_slot { uint64_t ticket,anchor,previous_mask,temporary_mask,request_sequence,phase_flags,record_token,reserved; };
struct signal_request_slot { struct signal_state_identity identity; uint64_t sequence,trap,operation,output_address,output_size; uint8_t wire[128]; uint64_t kind,completion_flags; };
struct signal_io_slot { uint64_t ticket,parent,depth,flags,deadline,anchor,previous_mask,temporary_mask; int64_t result; uint64_t elapsed_us,elapsed_ticks,cookie; };
struct signal_local_guard { uint64_t owner_ticket,cookie,previous_mask,flags; };
struct signal_thread_state {
    struct signal_state_identity identity;
    struct signal_context_stack stack;
    struct signal_context_range ranges[SIGNAL_CONTEXT_MAX_DEPTH];
    struct signal_wait_slot waits[4];
    struct signal_request_slot request;
    uint64_t request_sequence,request_reserved;
    struct signal_io_slot io[4];
    uint64_t io_sequence,io_top;
    struct signal_local_guard local;
};
/* Begin zero-initialized. Caller owns stable membership and CPU0 IRQ exclusion.
 * Inputs and outputs must not alias state or each other. Reset is retirement,
 * not an implicit rebind. No operation establishes a user-memory transaction. */
void signal_thread_state_reset(struct signal_thread_state *state);
/* Constructor-only pending child: no ordinary admission until bind installs
 * its actual scheduler identity. Destination must be all-zero and distinct. */
#define SIGNAL_STATE_NEEDS_SCHEDULER_BIND UINT64_C(0x535256464f524b31)
bool signal_state_fork_clone(struct signal_thread_state *child,const struct signal_thread_state *parent,
    struct signal_state_identity parent_identity,uint64_t child_image_generation);
bool signal_thread_state_bind(struct signal_thread_state *state,struct signal_state_identity identity);
bool signal_thread_state_push(struct signal_thread_state *state,struct signal_state_identity identity,
    const struct signal_context_record *record,struct signal_context_range range);
bool signal_thread_state_pop(struct signal_thread_state *state,struct signal_state_identity identity,
    uint64_t token,uint64_t frame_identity,struct signal_context_record *record_out,
    struct signal_context_range *range_out);
/* Wait tickets share the nonwrapping IO/cookie sequence. All helpers operate
 * on trusted objects; the owner controls masks, scheduler and user copies. */
#define SIGNAL_WAIT_PARKED 1u
#define SIGNAL_WAIT_RETURNING 2u
#define SIGNAL_WAIT_LEGACY_READY 3u
#define SIGNAL_WAIT_HANDLER 4u
#define SIGNAL_WAIT_INFO_DONE 5u
#define SIGNAL_WAIT_INFO_ONLY (UINT64_C(1)<<8)
#define SIGNAL_WAIT_INFO_COMPLETED (UINT64_C(1)<<9)
#define SIGNAL_WAIT_MASK_OVERRIDE 1u
#define SIGNAL_WAIT_COMPLETION_ONLY 2u
#define SIGNAL_REQUEST_CONTROL 1u
#define SIGNAL_REQUEST_RETURN 2u
#define SIGNAL_REQUEST_SUSPEND 3u
#define SIGNAL_REQUEST_IO 4u
#define SIGNAL_REQUEST_COMPLETION_MASK UINT64_C(0) /* no completion flags defined yet */
bool signal_state_validate(const struct signal_thread_state *,struct signal_state_identity);
bool signal_request_stage(struct signal_thread_state *,struct signal_state_identity,uint64_t trap,uint64_t operation,
    uint64_t output,uint64_t output_size,const uint8_t wire[128],uint64_t kind,uint64_t flags,uint64_t *sequence_out);
bool signal_request_clear(struct signal_thread_state *,struct signal_state_identity,uint64_t trap,uint64_t sequence);
bool signal_wait_begin(struct signal_thread_state *,struct signal_state_identity,uint64_t anchor,uint64_t oldmask,
    uint64_t temporary_mask,uint64_t request_sequence,bool info_only,uint64_t *ticket_out);
bool signal_wait_wake(struct signal_thread_state *,struct signal_state_identity,uint64_t ticket);
bool signal_wait_no_delivery(struct signal_thread_state *,struct signal_state_identity,uint64_t ticket);
bool signal_wait_rearm(struct signal_thread_state *,struct signal_state_identity,uint64_t ticket);
bool signal_wait_finish(struct signal_thread_state *,struct signal_state_identity,uint64_t ticket,bool legacy_handler,
    uint64_t *oldmask_out,bool *restore_mask_out);
bool signal_wait_push_record(struct signal_thread_state *,struct signal_state_identity,uint64_t ticket,
    const struct signal_context_record *,struct signal_context_range);
bool signal_wait_pop_record(struct signal_thread_state *,struct signal_state_identity,uint64_t token,uint64_t frame,
    struct signal_context_record *,struct signal_context_range *);
/* Pure staged transitions; caller supplies trusted values and owns exclusion. */
#define SIGNAL_IO_LIVE (UINT64_C(1)<<48)
#define SIGNAL_IO_INTERRUPTED (UINT64_C(1)<<49)
#define SIGNAL_IO_LEGACY_MASKED (UINT64_C(1)<<50)
#define SIGNAL_IO_COMPLETED (UINT64_C(1)<<51)
#define SIGNAL_IO_FINALIZED (UINT64_C(1)<<52)
#define SIGNAL_IO_MONO_VALID (UINT64_C(1)<<53)
#define SIGNAL_IO_TICKS_VALID (UINT64_C(1)<<54)
#define SIGNAL_IO_IN_FLIGHT (UINT64_C(1)<<55)
#define SIGNAL_LOCAL_ACTIVE UINT64_C(1)
#define SIGNAL_LOCAL_NO_MASK UINT64_C(2)
bool signal_io_begin(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,
    uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,
    uint64_t flags,uint64_t previous_mask,uint64_t temporary_mask,uint64_t *ticket_out);
bool signal_io_admit(const struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket);
/* Backend owner validates the whitelist and derives completed from root/operation.
 * Begin reserves before effects; commit is the only operation permitted while
 * IN_FLIGHT. No user return or handler delivery is allowed between them. */
bool signal_io_prepare_outcome(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket,uint64_t raw_syscall,uint64_t *cookie_out);
bool signal_io_commit_outcome(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket,
    uint64_t cookie,uint64_t raw_syscall,int64_t result,uint64_t elapsed_us,uint64_t elapsed_ticks,
    bool mono_valid,bool ticks_valid,bool completed);
bool signal_io_mark_interrupted(struct signal_thread_state *,struct signal_state_identity);
bool signal_io_end(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket);
bool signal_io_local_begin(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket,
    uint64_t previous_mask,bool finalize,uint64_t outcome_cookie,uint64_t *guard_cookie_out);
bool signal_io_local_end(struct signal_thread_state *,struct signal_state_identity,uint64_t depth,uint64_t ticket,
    uint64_t guard_cookie,uint64_t *previous_mask_out,bool *no_mask_out);
#endif
