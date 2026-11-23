#ifndef SRVROS_INTEL_BDW_GT_BOOTSTRAP_H
#define SRVROS_INTEL_BDW_GT_BOOTSTRAP_H
#include <srvros/intel_diagnostic_timing.h>
#include <srvros/intel_completion_audit.h>

#define BDW_BOOT_FENCES 32u
#define BDW_BOOT_GDRST 0x941cu
#define BDW_BOOT_FULL_RESET 1u
#define BDW_BOOT_FENCE_BASE 0x100000u
#define BDW_BOOT_POLLS 256u
#define BDW_BOOT_PHASE_US 10000ull
#define BDW_BOOT_SETTLE_US 50ull
#define BDW_BOOT_POLICY 0x47544231u
/* Protocol admission only, NOT HANDOFF_* or memory completion assurances. */
enum bdw_boot_prerequisite {
 BDW_BOOT_FIRMWARE=1u,BDW_BOOT_MMIO_CACHE=2u,BDW_BOOT_CPU_EXCLUSION=4u,
 BDW_BOOT_REQUESTER=8u,BDW_BOOT_ENGINE_HOLD=16u,BDW_BOOT_BACKING_PIN=32u,
 BDW_BOOT_DISPLAY=64u,BDW_BOOT_FENCED_CLIENTS=128u
};
#define BDW_BOOT_REQUIRED 255u
struct intel_bdw_gt_bootstrap;
struct scanout_transfer;
struct bdw_boot_binding {
 const void *owner,*provider,*lifetime;
 uint64_t root,mmio,mmio_bytes,aperture_bytes,epoch,sequence;
 uint64_t prepare,gt,hda,outer;
 uint32_t vendor,device,policy;
 uint64_t fences[BDW_BOOT_FENCES];
 const struct scanout_transfer *command_pages;
 uint64_t command_pages_epoch;
 const void *blitter_port_owner; /* Optional pre-reset exclusive submission claim. */
};
struct bdw_boot_source {
 const struct bdw_boot_source *object;
 const struct intel_bdw_gt_bootstrap *consumer;
 struct bdw_boot_binding binding;
};
struct bdw_boot_proof {
 const struct bdw_boot_source *source;
 const void *owner,*provider,*lifetime;
 uint64_t root,epoch,sequence,checked_at;
 uint32_t prerequisites;
};
struct bdw_boot_io {
 void *ctx;
 /* Independently admit original live source/resource/cache/holds, including
  * every CPU aperture/fence client stopped through restoration. No echoes. */
 bool (*prerequisite)(void *,const struct bdw_boot_source *,struct bdw_boot_proof *);
 bool (*read)(void *,uint32_t offset,uint32_t *);
 enum intel_gt_write (*write)(void *,uint32_t offset,uint32_t);
 /* Invalidate the actual parent's old engine epoch BEFORE the first possible
  * reset store. Exactly once; failed/uncertain advance never rolls back. */
 bool (*advance_epoch)(void *,const struct bdw_boot_source *,uint64_t before,uint64_t after);
 /* Fresh whole inherited display/HDA/backing preservation, with exact restored
  * fences, before admission may return. No backing/hold release callback. */
 bool (*preserved)(void *,const struct intel_bdw_gt_bootstrap *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
enum bdw_boot_stage {BDW_BOOT_EMPTY,BDW_BOOT_CAPTURE,BDW_BOOT_EPOCH,BDW_BOOT_RESET,
 BDW_BOOT_SETTLE,BDW_BOOT_POST,BDW_BOOT_RESTORE,BDW_BOOT_CANCEL,BDW_BOOT_VERIFY,
 BDW_BOOT_DONE,BDW_BOOT_FAULT};
enum bdw_boot_reason {BDW_BOOT_INPUT=1u,BDW_BOOT_SOURCE=2u,BDW_BOOT_CLOCK=4u,
 BDW_BOOT_PROOF=8u,BDW_BOOT_READ=16u,BDW_BOOT_WRITE=32u,BDW_BOOT_STATE=64u,
 BDW_BOOT_TIMEOUT=128u,BDW_BOOT_PRESERVE=256u};
struct intel_bdw_gt_bootstrap {
 const struct intel_bdw_gt_bootstrap *object;
 struct bdw_boot_source *source;
 struct bdw_boot_binding binding;
 struct bdw_boot_io io,original_io;
 uint64_t begin,last,end,clock_at,clock_previous,phase_begin,settle_begin,settle_end;
 uint32_t stage,reason,clock_reason,index,step,changed_mask,restored_mask;
 uint32_t proofs,read_attempts,reads,write_attempts,writes,reset_attempts,reset_writes;
 uint32_t fence_attempts,fence_writes,cancel_attempts,cancel_writes,reset_initial,reset_post,reset_close;
 uint32_t reset_first[2],reset_final[2],reset_polls[2],reset_completed;
 uint32_t control_before[5],control_after[5],cancel_first[5],cancel_final[5],cancel_polls[5];
 uint32_t cancelled,preserve_calls;
 bool busy,registered,epoch_attempted,epoch_advanced,preserved,retained,uncertain,complete;
 uint64_t fence_before[BDW_BOOT_FENCES],fence_after[BDW_BOOT_FENCES],fence_final[BDW_BOOT_FENCES];
};
/* The native bootstrap owns this borrow through the post-reset queue census.
 * A saved log, copied receipt or returned parent cannot stand in for it. */
static inline bool intel_bdw_reset_epoch_live(const struct intel_bdw_gt_bootstrap *reset,
                                             const struct intel_completion_result *post)
{
 if (!reset || reset->object != reset || reset->stage != BDW_BOOT_DONE ||
     !reset->complete || !reset->preserved || !reset->epoch_advanced ||
     reset->retained || reset->uncertain || reset->registered || reset->busy ||
     reset->reason || reset->reset_completed != 2 || !reset->source ||
     reset->source->object != reset->source || reset->source->consumer != reset ||
     !post || post->object != post || !post->complete || !post->stable || post->reason ||
     post->enabled || post->pending || post->flip_wait || post->tlb_pending ||
     !reset->end || post->begin_us < reset->end || post->end_us <= post->begin_us)
     return false;
 const struct intel_gt_access *gt = post->parent;
 const struct intel_gt_borrow *borrow = reset->binding.lifetime;
 return gt && gt == reset->binding.owner && gt->object == gt && gt->pinned &&
        gt->state == GT_ACCESS_AWAKE && !gt->reason &&
        reset->binding.epoch < UINT32_MAX && post->engine_epoch == reset->binding.epoch + 1 &&
        gt->engine_epoch == post->engine_epoch && gt->borrows == 1 &&
        borrow && gt->dependents == borrow && borrow->object == borrow &&
        borrow->active && borrow->parent == gt && !borrow->next && display_access_held(&gt->exclusion);
}
/* Distinct full-GT reset/fence-restoration protocol. Only the separate native
 * bootstrap image may call it. Caller already owns the original five-engine preparation,
 * requester/GT/HDA/backing/exclusion and all fenced clients. The core registers
 * on this one-use source before callbacks; the adapter MUST block parent return
 * while registered, including failures. No GTT PTE or command submission occurs.
 * Two known GDRST/1 pulses, self-clear, >=50us settle, exact 32-fence capture/
 * restoration and reverse request cancellation. Existing BCS8 APIs unchanged.
 * Overlapping source/result storage refuses before either object is modified.
 * Unknown state retains registration; no retry/forced rollback/free. Success is
 * only reset/preservation: not pipeline/DMA/cache/reader retirement or a grant. */
bool intel_bdw_gt_bootstrap_run(struct intel_bdw_gt_bootstrap *,struct bdw_boot_source *,struct bdw_boot_io);
/* Native providers must additionally check real mapping/cache/requester/clock
 * immediately before access. This pure predicate grants no native permission. */
bool intel_bdw_gt_bootstrap_write_ready(const struct intel_bdw_gt_bootstrap *,uint32_t,uint32_t);
#endif
