#ifndef SRVROS_PRIVATE_PROCESS_SIGNAL_H
#define SRVROS_PRIVATE_PROCESS_SIGNAL_H
#include "arch/x86_64/signal_state.h"

/* Internal SIGUSR1 prerequisite, no syscall or delivery caller. Inputs/outputs
 * are stable caller-owned kernel objects, never user pointers or state aliases.
 * Exchange permits replacement==previous. Executable validation is not a lease.
 * Each operation owns/restores CPU0 saved IRQ exclusion; no SMP contract. */
bool process_signal_action_exchange(const struct signal_action_descriptor *replacement,
    struct signal_action_descriptor *previous,uint64_t *generation_out);
bool process_signal_record_push(const struct signal_context_record *,struct signal_context_range);
bool process_signal_record_pop(uint64_t token,uint64_t frame_identity,
    struct signal_context_record *,struct signal_context_range *);
#endif
