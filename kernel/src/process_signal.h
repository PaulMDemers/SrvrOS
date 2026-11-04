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

/* Private test-operation shape, not a published user ABI. Entire serialized
 * range is8 return-slot +688 snapshot +64 info. Only these valid facts exist. */
#define PROCESS_SIGNAL_PRIVATE_FRAME_BYTES 760u
#define PROCESS_SIGNAL_INFO_SIGNO 1u
#define PROCESS_SIGNAL_INFO_TARGET_TID 2u
struct process_signal_private_info {
    uint32_t size,version,signo,code;
    uint64_t valid,target_tid,reserved[4];
};
enum process_signal_delivery_result {
    PROCESS_SIGNAL_NONE,PROCESS_SIGNAL_DELIVERED,PROCESS_SIGNAL_DEFERRED,PROCESS_SIGNAL_FATAL
};
/* Caller already owns common CPL3 final IF0 exclusion and cancellation priority.
 * These operations never enable IRQs or run termination. FATAL/false leaves
 * all signal/user/live state unchanged; caller takes terminal policy outside.
 * No normal kernel hook, syscall or userspace registration is connected. */
enum process_signal_delivery_result process_signal_deliver_private(struct isr_frame *);
/* 0: refusal, unchanged. 1: restored. 128+signal: restored with a prepared
 * default-fatal decision; caller must process_exit(result) before user resume.
 * No post-commit admission query or further signal delivery is permitted. */
#define PROCESS_SIGNAL_RETURN_REFUSED 0u
#define PROCESS_SIGNAL_RETURN_RESTORED 1u
#define PROCESS_SIGNAL_RETURN_EXIT_MIN 129u
uint64_t process_signal_return_private(struct isr_frame *,uint64_t token,uint64_t frame_identity);
#endif
