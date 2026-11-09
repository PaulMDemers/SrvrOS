#ifndef SRVROS_INTEL_ENGINE_PREPARE_H
#define SRVROS_INTEL_ENGINE_PREPARE_H
#include <srvros/intel_takeover_preflight.h>
#define INTEL_ENGINE_PREPARE_TOTAL_US 100000ull
#define INTEL_ENGINE_PREPARE_PHASE_US 10000ull
#define INTEL_ENGINE_PREPARE_POLLS 256u
#define INTEL_ENGINE_PREPARE_REQUEST 0x00010001u
#define INTEL_ENGINE_PREPARE_CANCEL 0x00010000u
enum intel_engine_prepare_reason {IEP_INPUT=1,IEP_CLOCK=2,IEP_PARENT=4,
 IEP_GUARD=8,IEP_READ=16,IEP_WRITE=32,IEP_STATE=64,IEP_TIMEOUT=128,
 IEP_PRESERVE=256,IEP_RETURN=512,IEP_COUNT=1024,IEP_ACTION=2048};
struct intel_engine_prepare_result {
 const struct intel_engine_prepare_result *object;
 struct intel_gt_access *parent;
 const struct intel_takeover_result *preflight;
 struct intel_gt_borrow borrow;
 uint32_t epoch,words[4][6],ack[10],final[10];
 uint64_t begin_us,last_us,end_us,clock_at,clock_prev;
 /* Callback bracket clocks; last valid clock if the post-guard fails.
  * action_completed additionally requires the matching held sample2. */
 uint64_t action_begin_us,action_end_us;
 unsigned reason,attempts,writes,reads,polls,snapshots,sample_attempts,counts[4],acks,ready,cancelled,phase_polls[10],stage,index;
 bool blocked,admitted,held,preserved,uncertain,returned,complete;
 bool action_attempted,action_completed;
};
struct intel_engine_prepare_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*sample)(void *,uint32_t [6],unsigned *);
 bool (*control)(void *,unsigned,uint32_t *);
 enum intel_gt_write (*transition)(void *,unsigned,uint32_t,uint32_t *);
 bool (*preserve)(void *,const struct intel_completion_result *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Optional same-lifetime READ-ONLY observation, called once between held
 * samples1 and2 under the original active borrow after all five ready ACKs.
 * The callback must not change epochs, request/control state, mappings or this
 * result. An observer requiring its own independent borrow cannot nest here;
 * a future native sampler must explicitly bind this existing borrow.
 * It grants no reset/PTE/memory authority. False means uncertainty,
 * with no optimistic cancellation/return after the earlier request writes. */
struct intel_engine_prepare_action {
 void *ctx;
 bool (*observe)(void *,const struct intel_engine_prepare_result *);
};
/* Exact BDW GT3 five-engine request/hold/cancel trial, no GDRST or memory grant.
 * Zero-init original result; consume once. Partial/uncertain writes retain the
 * registered dependent. No retry, forced cancellation or recovery API. */
bool intel_engine_prepare(struct intel_engine_prepare_result *,struct intel_gt_access *,
 const struct intel_takeover_result *,bool,struct intel_engine_prepare_io);
/* The action and all existing cancellation/preservation/return remain inside
 * the SAME100ms budget. Separate full guards bracket the action. The legacy
 * entry point has no action and retains its prior callback/clock sequence. */
bool intel_engine_prepare_with_action(struct intel_engine_prepare_result *,struct intel_gt_access *,
 const struct intel_takeover_result *,bool,struct intel_engine_prepare_io,struct intel_engine_prepare_action);
#endif
