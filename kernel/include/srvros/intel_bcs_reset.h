#ifndef SRVROS_INTEL_BCS_RESET_H
#define SRVROS_INTEL_BCS_RESET_H
#include <srvros/intel_reset_ready.h>
#include <srvros/intel_blitter.h>
#define INTEL_BCS_GDRST 0x941cu
#define INTEL_BCS_RESET_DOMAIN 8u
#define INTEL_BCS_RESET_SETTLE_US 50ull
enum intel_bcs_reset_reason {IBR_OK,IBR_INPUT,IBR_CLOCK,IBR_GUARD,IBR_ENTRY,
 IBR_READ,IBR_WRITE,IBR_TIMEOUT,IBR_DRIFT,IBR_PARENT};
struct intel_bcs_reset_result {
 const struct intel_bcs_reset_result *object;
 struct intel_gt_borrow borrow;
 struct intel_blitter_result shutdown;
 uint32_t controls[4],domains[4],words[2][8],epoch_before,epoch_after;
 uint64_t begin_us,end_us,last_us,settle_begin_us,settle_end_us;
 uint64_t clock_sample,clock_previous;
 unsigned clock_cause,clock_stage,snapshots;
 unsigned reads,polls,attempts,writes,reset_attempts,reset_writes,reset_acks;
 unsigned samples,controls_read,domains_read,reason;
 bool admitted,ready,cancelled,blocked_close,uncertain,complete;
};
struct intel_bcs_reset_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*control)(void *,uint32_t *);
 bool (*domain)(void *,uint32_t *);
 bool (*group)(void *,uint32_t [8]);
 enum intel_gt_write (*control_write)(void *,uint32_t);
 enum intel_gt_write (*domain_write)(void *,uint32_t);
 uint64_t (*now)(void *);
 void (*pause)(void *);
 /* Optional ordered BCS8/control/domain snapshot, with fresh provider admission. */
 bool (*snapshot)(void *,uint32_t [8],uint32_t *,uint32_t *);
};
/* Linked only by the distinct BCS-reset diagnostic ELF, never ordinary boot.
 * Native adapter and recipe enforce exact entry. BDW PRM2c pp609-610: GDRST bit3 only. Preserve
 * every inherited allocation. Acknowledgement is NOT a drain/cache witness.
 * Zero-init once. After a possibly issued store, any uncertain outcome retains
 * the borrow/exclusion until platform reset; no retry or forced cleanup API.
 * Caller must establish exact resource/cache/profile/firmware submission policy.
 * Limits match readiness: 10ms/256 polls per phase, 100ms including pin return.
 * Two BCS-domain pulses and a measured >=50us settling interval follow the
 * pinned Linux v6.12 ordering. This is not a portable hardware admission gate. */
bool intel_bcs_reset(struct intel_bcs_reset_result *,struct intel_gt_access *,
 const struct intel_completion_result *,bool,struct intel_bcs_reset_io);
/* Fresh already-disabled observation within this reset borrow. No control writer.
 * The four snapshots bind the same-parent/epoch census before reset request. */
bool intel_bcs_reset_after_shutdown(struct intel_bcs_reset_result *,struct intel_gt_access *,
 const struct intel_completion_result *,bool,struct intel_bcs_reset_io);
#endif
