#ifndef SRVROS_INTEL_RING_TRANSITION_H
#define SRVROS_INTEL_RING_TRANSITION_H
#include <srvros/intel_blitter.h>
#include <srvros/intel_completion_audit.h>
#define INTEL_RING_TOTAL_US 150000ull
#define INTEL_RING_SOURCE_US 100000ull
#define INTEL_RING_READER_WORDS 37u
/* An enabled submission transition, never a reset or GPU-memory grant. */
enum intel_ring_reason {IRT_INPUT=1,IRT_CLOCK=2,IRT_SOURCE=4,IRT_READ=8,IRT_WRITE=16,IRT_POST=32,IRT_READER=64,IRT_RETURN=128};
struct intel_ring_readers {uint32_t planes[3][9],audio[10];};
struct intel_ring_transition {
 const struct intel_ring_transition *object;
 struct intel_gt_access *parent;
 const struct intel_completion_result *source;
 struct intel_gt_borrow borrow;
 struct intel_blitter_result bcs;
 struct intel_completion_result post;
 uint32_t source_words[2][INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS];
 uint32_t source_context[2][INTEL_COMPLETION_CONTEXT];
 struct intel_ring_readers readers[2];
 uint64_t source_begin,source_end,source_last,begin,last,end,clock_at,clock_prev;
 uint64_t store_at,store_previous,return_begin,post_begin,post_end,post_last;
 uint32_t epoch_before,epoch_after,parent_attempts;
 unsigned reason,checks,read_attempts,reader_attempts,reader_counts[2],store_calls;
 bool policy,blocked,epoch_advanced,post_bound,readers_bound,returned,complete,uncertain;
};
struct intel_ring_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*read)(void *,uint32_t [INTEL_BCS_WORDS]);
 enum intel_gt_write (*write)(void *,uint32_t);
 bool (*census)(void *,struct intel_completion_result *);
 bool (*reader)(void *,unsigned,struct intel_ring_readers *,unsigned *);
 uint64_t (*now)(void *);
};
/* Zero-init once, original source/parent/provider through close. Only captured
 * enabled legacy BCS is eligible; no synthetic zero-geometry source. The epoch
 * advances before a possible disable, invalidating the old census. A new full
 * census must bind the disabled nonzero geometry. No old reset admission.
 * After any possible store, failed proof/terminal clock retains the root even
 * if logical child return already succeeded. Caller must never close/retry an
 * uncertain result. Neither result nor post snapshot is future live authority. */
bool intel_ring_transition_store_ready(const struct intel_ring_transition *);
bool intel_ring_transition_run(struct intel_ring_transition *,struct intel_gt_access *,const struct intel_completion_result *,bool,struct intel_ring_io);
#endif
