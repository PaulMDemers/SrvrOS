#ifndef SRVROS_INTEL_SOURCE_REQUESTER_H
#define SRVROS_INTEL_SOURCE_REQUESTER_H
#include <srvros/intel_source_transfer.h>
#include <srvros/intel_pci_requester.h>
#define INTEL_SOURCE_REQUESTER_POLLS 32u
#define INTEL_SOURCE_REQUESTER_DRAIN_US 10000ull
enum intel_source_requester_state {ISR_EMPTY,ISR_OPENING,ISR_INHIBITING,ISR_INHIBITED,ISR_RESTORING,ISR_CLOSED,ISR_FAULT};
enum intel_source_requester_reason {ISR_INPUT=1,ISR_SOURCE=2,ISR_CLOCK=4,ISR_GUARD=8,ISR_READ=16,ISR_PROFILE=32,ISR_WRITE=64,ISR_PENDING=128,ISR_CHILD=256};
struct intel_source_requester_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*sample)(void *,uint32_t [9],unsigned *);
 enum intel_gt_write (*command16)(void *,uint16_t);
 bool (*pending)(void *,uint32_t [2],unsigned *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Original static/heap one-use owner. Command 3 remains in force throughout
 * the child; failures retain the actual source/RAM/root, never a saved receipt.
 * This controls the PCI requester only, not internal DMA/cache/reader ownership. */
struct intel_source_requester {
 const struct intel_source_requester *object;
 struct intel_source_transfer *transfer;
 struct intel_source_buffers *buffers;
 struct intel_nonzero_source *source;
 const void *provider,*lifetime;
 const struct scanout_handoff_snapshot *input;
 struct intel_source_requester_io io,original_io;
 uint64_t origin,parent_begin,begin,last,end,clock_at,clock_prev,drain_begin,child_begin,child_end;
 uint64_t sample_begin[4],sample_end[4],poll_at[INTEL_SOURCE_REQUESTER_POLLS];
 uint32_t epoch,kind,post_mode,state,reason,words[4][9],poll_words[INTEL_SOURCE_REQUESTER_POLLS][2];
 unsigned counts[4],poll_counts[INTEL_SOURCE_REQUESTER_POLLS],samples,reads,attempts,writes,polls,phase,borrows,returns;
 bool busy,registered,parent_blocked,holds_blocked,child_entered,child_complete,restored,complete;
};
bool intel_source_requester_run(struct intel_source_requester *,struct intel_source_transfer *,
 struct intel_source_requester_io,bool (*)(void *,struct intel_source_requester *),void *);
/* Pure final-admission predicates. deadline advances the original whole-chain
 * clock; no new budget or authority is created by calling it. */
bool intel_source_requester_live(const struct intel_source_requester *);
bool intel_source_requester_time(const struct intel_source_requester *,uint64_t);
bool intel_source_requester_deadline(struct intel_source_requester *,uint64_t);
#endif
