#ifndef SRVROS_INTEL_ENTRY_ADMIT_H
#define SRVROS_INTEL_ENTRY_ADMIT_H
#include <srvros/intel_blitter.h>
#include <srvros/intel_completion_audit.h>
#define INTEL_ENTRY_TOTAL_US 100000ull
enum intel_entry_reason {IEA_INPUT=1,IEA_CLOCK=2,IEA_SOURCE=4,IEA_READ=8,IEA_ENTRY=16,IEA_RETURN=32};
/* A read-only child of the original GT/census, not a shutdown/reset grant.
 * Only snapshot data is copied; owners, census and the sealed borrow remain
 * original objects. Caller guard binds those exact objects to its provider. */
struct intel_entry_result {
 const struct intel_entry_result *object;
 struct intel_gt_access *parent;
 const struct intel_completion_result *census;
 struct intel_gt_borrow borrow;
 struct intel_blitter_result bcs;
 uint32_t source_words[2][INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS];
 uint32_t source_context[2][INTEL_COMPLETION_CONTEXT];
 uint64_t begin_us,last_us,end_us,source_begin,source_end,source_last;
 uint64_t clock_at,clock_prev;
 uint32_t epoch,epoch_after,parent_attempts_before,parent_attempts_after;
 unsigned reason,checks;
 bool policy,blocked,returned,bound,complete;
};
struct intel_entry_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*read)(void *,uint32_t [INTEL_BCS_WORDS]);
 uint64_t (*now)(void *);
};
/* Zero-init once. Never stores. Refused entry cannot advance the caller's reset
 * pipeline. Source age, whole child including return and BCS core all retain
 * 100ms bounds inside the original GT300/HDA400/outer500ms lifetimes. */
bool intel_entry_admit(struct intel_entry_result *,struct intel_gt_access *,
 const struct intel_completion_result *,bool,struct intel_entry_io);
#endif
