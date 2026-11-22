#ifndef SRVROS_INTEL_GPU_REQUESTER_HOLD_H
#define SRVROS_INTEL_GPU_REQUESTER_HOLD_H
#include <srvros/intel_interval_claim.h>
#include <srvros/intel_pci_requester.h>
#define INTEL_GPU_HOLD_POLLS 32u
#define INTEL_GPU_HOLD_PHASE_US 10000ull
enum intel_gpu_hold_state {IGH_EMPTY,IGH_OPENING,IGH_INHIBITING,IGH_INHIBITED,IGH_RESTORING,IGH_CLOSED,IGH_FAULT};
enum intel_gpu_hold_reason {IGH_INPUT=1,IGH_SOURCE=2,IGH_CLOCK=4,IGH_READ=8,IGH_PROFILE=16,IGH_WRITE=32,IGH_PENDING=64,IGH_RETURN=128};
struct intel_gpu_hold_io {
 void *ctx;
 /* Ordered exact nine-word profile. Completed prefixes survive false. */
 bool (*sample)(void *,uint32_t [INTEL_PCI_REQUESTER_WORDS],unsigned *);
 /* Only Command halfword 7->3->7. DONE means issued, not completed. */
 enum intel_gt_write (*command16)(void *,uint16_t);
 /* Fresh Command and AF DWORDs; completed prefixes survive false. */
 bool (*pending)(void *,uint32_t [2],unsigned *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Original zero-init one-use object, owned static/heap through retained faults.
 * Uses the original registered interval consumer and preparation/GT origins.
 * No new transfer token, PTE, command ring, flush or invalidation authority. */
struct intel_gpu_requester_hold {
 const struct intel_gpu_requester_hold *object;
 struct intel_interval_claim *claim;
 struct intel_interval_borrow borrow;
 struct intel_gpu_hold_io io;
 uint64_t origin_us,owner_origin_us,begin_us,last_us,end_us,clock_at,clock_prev,drain_begin_us;
 uint64_t sample_begin[3],sample_end[3],poll_us[INTEL_GPU_HOLD_POLLS];
 uint32_t words[3][INTEL_PCI_REQUESTER_WORDS],poll_words[INTEL_GPU_HOLD_POLLS][2];
 unsigned counts[3],poll_counts[INTEL_GPU_HOLD_POLLS];
 unsigned state,reason,phase,samples,reads,attempts,writes,polls;
 bool busy,restored,returned,complete;
};
bool intel_gpu_requester_hold_run(struct intel_gpu_requester_hold *,struct intel_interval_claim *,struct intel_gpu_hold_io);
/* Pure original registration/time predicates for immediate native I/O admission.
 * They sample nothing and cannot restart or replace the original clock bounds. */
bool intel_gpu_requester_hold_live(const struct intel_gpu_requester_hold *);
bool intel_gpu_requester_hold_time(const struct intel_gpu_requester_hold *,uint64_t);
/* Advances both this consumer and the original claim's shared watermark. */
bool intel_gpu_requester_hold_deadline(struct intel_gpu_requester_hold *,uint64_t);
#endif
