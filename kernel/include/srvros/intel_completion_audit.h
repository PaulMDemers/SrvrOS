#ifndef SRVROS_INTEL_COMPLETION_AUDIT_H
#define SRVROS_INTEL_COMPLETION_AUDIT_H
#include <srvros/intel_engine_census.h>
#define INTEL_COMPLETION_WORDS 8u
#define INTEL_COMPLETION_CONTEXT 6u
/* All obligations stay open: observations cannot issue flush/reset/invalidate,
 * release firmware backing or confer an unobserved completion fence. */
enum intel_completion_obligation {
 INTEL_COMPLETION_POSTED=1u,INTEL_COMPLETION_PREFETCH=2u,INTEL_COMPLETION_PIPELINE=4u,
 INTEL_COMPLETION_TLB=8u,INTEL_COMPLETION_CACHE=16u,INTEL_COMPLETION_FLIP=32u,
 INTEL_COMPLETION_FIRMWARE=64u,INTEL_COMPLETION_BACKING=128u
};
#define INTEL_COMPLETION_OPEN 255u
extern const uint32_t intel_completion_tlb[INTEL_ENGINE_COUNT];
extern const uint32_t intel_completion_context[INTEL_COMPLETION_CONTEXT];
struct intel_completion_result {
 const struct intel_completion_result *object;
 const struct intel_gt_access *parent;
 uint32_t words[2][INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS];
 uint32_t context[2][INTEL_COMPLETION_CONTEXT];
 uint64_t begin_us,end_us,last_us;
 unsigned groups,samples,contexts,reads,reason,enabled,pending,flip_wait,tlb_pending,parser_idle;
 bool stable,complete;
};
struct intel_completion_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*group)(void *,unsigned,uint32_t [INTEL_COMPLETION_WORDS]);
 bool (*context)(void *,uint32_t [INTEL_COMPLETION_CONTEXT]);
 uint64_t (*now)(void *);
};
/* Exact Broadwell RO register census. Zero-init and consume once; the caller
 * binds same-boot resource/cache/profile and retains the live GT parent. */
bool intel_completion_observe(struct intel_completion_result *,struct intel_gt_access *,struct intel_completion_io);
#endif
