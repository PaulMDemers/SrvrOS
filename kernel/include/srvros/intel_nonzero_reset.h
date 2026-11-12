#ifndef SRVROS_INTEL_NONZERO_RESET_H
#define SRVROS_INTEL_NONZERO_RESET_H
#include <srvros/intel_ring_transition.h>
#include <srvros/intel_engine_prepare.h>
#include <srvros/intel_bcs_reset.h>
#define INTEL_NONZERO_ADMIT_US 100000ull
_Static_assert(INTEL_ENGINE_PREPARE_TOTAL_US==100000ull &&
 INTEL_ENGINE_PREPARE_PHASE_US==10000ull && INTEL_ENGINE_PREPARE_POLLS==256u &&
 GT_ACCESS_TOTAL_US==300000ull && INTEL_BCS_RESET_SETTLE_US==50ull,
 "Nonzero reset must retain the reviewed deadlines");
/* Separate protocol; none of the zero-geometry preparation/reset predicates
 * are used or relaxed. Both budgets retain their original phase meanings. */
enum intel_nonzero_reason {INZ_INPUT=1,INZ_SOURCE=2,INZ_CLOCK=4,INZ_GUARD=8,
 INZ_READ=16,INZ_STATE=32,INZ_WRITE=64,INZ_TIMEOUT=128,INZ_RETURN=256,INZ_PRESERVE=512};
enum intel_nonzero_stage {INZ_ADMIT=1,INZ_PREPARE,INZ_HELD,INZ_RESET,INZ_SETTLE,
 INZ_POST,INZ_CANCEL,INZ_PRESERVE_STAGE,INZ_RETURN_STAGE,INZ_DONE};
enum intel_nonzero_post {INZ_POST_NONE=0,INZ_POST_PRESERVED=1,INZ_POST_RESET_DEFAULT=2};
struct intel_nonzero_reset {
 const struct intel_nonzero_reset *object;
 struct intel_gt_access *parent;
 const struct intel_ring_transition *entry;
 const struct intel_completion_result *source;
 void *provider;
 struct intel_gt_borrow borrow;
 struct intel_completion_result post;
 struct intel_ring_readers readers;
 uint64_t source_begin,source_end,source_last,entry_end,parent_begin;
 uint64_t admit_begin,admit_end,begin,last,end,return_begin,guard_begin,guard_end;
 uint64_t clock_at,clock_prev,settle_begin,settle_end;
 uint64_t control_begin[10],control_end[10],pulse_begin[2],pulse_end[2];
 uint32_t epoch_before,epoch_after,parent_attempts;
 uint32_t admission[2][6],control[5][6],bcs[2][8],domain[4];
 uint32_t ack[10],final[10],reset_ack[2],reset_final[2];
 unsigned admission_count[2],control_count[5],bcs_count[2],reader_count;
 unsigned polls[10],reset_polls[2],admission_attempts,control_samples,bcs_samples;
 unsigned controls,control_writes,control_acks,resets,reset_writes,reset_acks;
 unsigned control_reads,bcs_reads,checks,stage,index,post_mode,reason;
 bool policy,eligible,blocked,epoch_advanced,preserved,returned,uncertain,complete;
};
struct intel_nonzero_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*sample)(void *,uint32_t [6],unsigned *);
 bool (*control)(void *,unsigned,uint32_t *);
 enum intel_gt_write (*request)(void *,unsigned,uint32_t,uint32_t *);
 bool (*bcs)(void *,uint32_t [8],uint32_t *,unsigned *);
 bool (*domain)(void *,uint32_t *);
 enum intel_gt_write (*reset)(void *,uint32_t,uint32_t *);
 bool (*post)(void *,struct intel_completion_result *);
 bool (*readers)(void *,struct intel_ring_readers *,unsigned *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Original zero-init, one-use result, original completed kind1 entry and actual
 * original post-census. One registered child spans admission through return.
 * No geometry/config/PTE restoration, arbitrary reset domain or memory grant.
 * Unknown post-state or lost source/clock/return retains the original root. */
bool intel_nonzero_reset_run(struct intel_nonzero_reset *,struct intel_gt_access *,
 const struct intel_ring_transition *,bool,struct intel_nonzero_io);
/* Pure original-object predicates for the native adapter's final admission. */
bool intel_nonzero_reset_live(const struct intel_nonzero_reset *);
bool intel_nonzero_reset_write_ready(const struct intel_nonzero_reset *,bool,unsigned,uint32_t);
#endif
