#ifndef SRVROS_INTEL_HDA_GPU_H
#define SRVROS_INTEL_HDA_GPU_H
#include <srvros/intel_hda_owner.h>
#include <srvros/intel_interval_claim.h>
#define INTEL_HDA_GPU_TOTAL_US 500000ull
enum intel_hda_gpu_state {IHG_EMPTY,IHG_OPENING,IHG_ACTIVE,IHG_CLOSING,IHG_CLOSED,IHG_FAULT};
enum intel_hda_gpu_reason {IHG_INPUT=1,IHG_SOURCE=2,IHG_CLOCK=4,IHG_HDA=8,IHG_GPU=16,IHG_CLAIM=32,IHG_RETIRE=64,IHG_EXCLUSION=128};
struct intel_hda_gpu_io {
 struct intel_hda_owner_io hda;
 void *ctx;
 /* Fresh exact original endpoint/map retirement, no forced cleanup. */
 bool (*retire)(void *);
};
/* Own in static/heap storage, not a kernel stack. The original outer500ms
 * origin is supplied by the enclosing native transaction, never restarted. */
struct intel_hda_gpu {
 const struct intel_hda_gpu *object;
 struct intel_hda_owner_result hda;
 struct intel_hda_owner_token token;
 struct intel_hda_owner_borrow borrow;
 struct intel_hda_result mid;
 struct intel_gt_access *gpu;
 struct intel_interval_claim *claim;
 struct display_access_token exclusion;
 struct intel_hda_gpu_io io;
 uint64_t outer_origin_us,begin_us,last_us,end_us,clock_at,clock_prev;
 unsigned state,reason,clock_cause,checks,attachments;
 bool busy,pinned,retained,gpu_complete,complete;
};
bool intel_hda_gpu_open(struct intel_hda_gpu *,struct intel_gt_access *,
 const struct display_access_token *,uint64_t outer_origin_us,
 struct intel_hda_owner_facts,struct intel_hda_gpu_io);
/* A live original borrow/CPU exclusion, not hardware retirement authority. */
bool intel_hda_gpu_live(const struct intel_hda_gpu *);
bool intel_hda_gpu_check(struct intel_hda_gpu *);
bool intel_hda_gpu_attach(struct intel_hda_gpu *,struct intel_interval_claim *);
/* Pure bounds predicate for immediate native I/O admission. deadline records
 * a failed raw sample; neither function takes a fresh clock or extends a bound. */
bool intel_hda_gpu_time(const struct intel_hda_gpu *,uint64_t);
bool intel_hda_gpu_deadline(struct intel_hda_gpu *,uint64_t);
/* Requires original GT cleanly closed/never opened and attached claim retired.
 * gpu_complete=false permits clean refusal retirement, never a pass. Retires
 * both HDA inhibition and the adapter's mappings before dropping the CPU pin. */
bool intel_hda_gpu_close(struct intel_hda_gpu *,bool gpu_complete);
#endif
