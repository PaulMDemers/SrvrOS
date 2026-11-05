#ifndef SRVROS_PRIVATE_PROCESS_SIGNAL_H
#define SRVROS_PRIVATE_PROCESS_SIGNAL_H
#if defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) && !defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
#error "Experimental INFO requires a separate PUBLIC_GUEST_TEST kernel"
#endif
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
    PROCESS_SIGNAL_NONE,PROCESS_SIGNAL_DELIVERED,PROCESS_SIGNAL_DEFERRED,PROCESS_SIGNAL_FATAL,
    PROCESS_SIGNAL_DELIVERED_EXIT_USR1=138
};
/* Caller already owns common CPL3 final IF0 exclusion and cancellation priority.
 * These operations never enable IRQs or run termination. FATAL/false leaves
 * all signal/user/live state unchanged; caller takes terminal policy outside.
 * DELIVERED_EXIT_USR1 commits delivery plus a process-wide default-fatal
 * decision; caller must process_exit(138) immediately without any further
 * query or user resume. No normal kernel hook or public ABI is connected. */
enum process_signal_delivery_result process_signal_deliver_private(struct isr_frame *);
/* 0: refusal, unchanged. 1: restored. 128+signal: restored with a prepared
 * default-fatal decision; caller must process_exit(result) before user resume.
 * No post-commit admission query or further signal delivery is permitted. */
#define PROCESS_SIGNAL_RETURN_REFUSED 0u
#define PROCESS_SIGNAL_RETURN_RESTORED 1u
#define PROCESS_SIGNAL_RETURN_EXIT_MIN 129u
uint64_t process_signal_return_private(struct isr_frame *,uint64_t token,uint64_t frame_identity);
#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
/* Staged guest-only public frontend. Wire is already copied into trusted kernel
 * storage. Completion runs at the ordinary common-return seam under IF0. */
int64_t process_signal_public_stage(struct isr_frame *,uint64_t kind,uint64_t operation,
    const uint8_t wire[128],uint64_t output,uint64_t output_size);
/* 0: ordinary completion; 1: authoritative restored frame; >=129: direct exit.
 * UINT64_MAX: contained terminal contract failure. */
uint64_t process_signal_public_complete(struct isr_frame *);
/* Fixed bounded read used only after common-return ordinary admission. */
bool syscall_signal_copy_input(void *,uint64_t,size_t);
/* Pre-admission guard: 0 ordinary,1 compact END handled,2 allowed guarded
 * syscall,3 refused guarded syscall,4 guarded non-syscall entry, >=129 exit. */
uint64_t process_signal_public_entry(struct isr_frame *);
int64_t process_signal_io_check(struct isr_frame *,const struct srv_signal_io_v1 *,struct signal_io_slot *root_out);
int64_t process_signal_io_reserve(struct isr_frame *,const struct srv_signal_io_v1 *,
    const struct srv_signal_descriptor_v1 *,size_t count,uint64_t *cookie,uint64_t *request_sequence);
bool process_signal_io_capture(struct isr_frame *,const struct srv_signal_io_v1 *,uint64_t cookie,uint64_t sequence,
    int64_t result,uint64_t elapsed_us,uint64_t elapsed_ticks,bool mono_valid,bool ticks_valid);
bool process_signal_info_pending_current(void);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
bool process_signal_experimental_fd_allowed(uint64_t fd);
#endif
#endif
#endif
