#ifndef SRVROS_INTEL_ENGINE_HELD_RESET_H
#define SRVROS_INTEL_ENGINE_HELD_RESET_H
#include <srvros/intel_engine_prepare.h>
#include <srvros/intel_bcs_reset.h>
enum intel_engine_held_reset_reason {IHR_INPUT=1,IHR_CLOCK=2,IHR_PARENT=4,
 IHR_GUARD=8,IHR_READ=16,IHR_WRITE=32,IHR_STATE=64,IHR_TIMEOUT=128,IHR_PROVIDER=256};
struct intel_engine_held_reset_result {
 const struct intel_engine_held_reset_result *object;
 struct intel_engine_prepare_result *preparation;
 struct intel_gt_access *parent;
 uint32_t epoch_before,epoch_after,words[2][8],domains[4],ack[2],final[2];
 uint64_t origin_us,begin_us,last_us,end_us,clock_at,clock_prev,settle_begin_us,settle_end_us;
 uint64_t phase_begin_us[2],phase_end_us[2];
 unsigned reason,attempts,writes,reads,polls,acks,snapshot_attempts,snapshots,counts[2],phase_polls[2],stage;
 bool attempted,entered,active,epoch_advanced,provider_enter,provider_exit,uncertain,complete;
};
struct intel_engine_held_reset_io {
 void *ctx;
 bool (*guard)(void *);
 /* Ordered BCS8 then GDRST, count0..9 complete words even on false. */
 bool (*snapshot)(void *,uint32_t [8],uint32_t *,unsigned *);
 bool (*domain)(void *,uint32_t *);
 /* Exact GDRST/8 store and first acknowledgement. NONE guarantees no store;
  * unknown readback after possible issue MUST return UNCERTAIN. */
 enum intel_gt_write (*transition)(void *,uint32_t,uint32_t *);
 /* Independent full backing/endpoint entry and exit of this synchronous scope.
  * All inner accesses still require fresh guard/alias/requester/deadline checks. */
 bool (*begin)(void *);
 bool (*end)(void *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Defined only in the distinct reset-capable image. Same original preparation
 * borrow/origin, no nested borrow, configuration/PTE write or memory authority.
 * Read-only callback and matching held sample2 finish BEFORE reset starts.
 * Core-owned epoch invalidation precedes the first possible BCS8 pulse; both
 * fixed pulses, >=50us settling, mixed post-state, cancellation, preservation
 * and verified pin return share the ORIGINAL100ms budget. Any uncertain outcome
 * retains the original pin; no forced cancellation/cleanup/epoch rollback. */
bool intel_engine_prepare_with_bcs_reset(struct intel_engine_prepare_result *,
 struct intel_gt_access *,const struct intel_takeover_result *,bool,
 struct intel_engine_prepare_io,struct intel_engine_prepare_action,
 struct intel_engine_held_reset_result *,struct intel_engine_held_reset_io);
/* Internal original-object predicates/operation; neither is reusable authority.
 * Only the enclosing core activates the reset window. */
bool intel_engine_prepare_reset_live(const struct intel_engine_held_reset_result *);
bool intel_engine_held_reset_run(struct intel_engine_held_reset_result *,struct intel_engine_held_reset_io);
#endif
