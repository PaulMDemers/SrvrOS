#ifndef SRVROS_INTEL_HELD_INTERVAL_H
#define SRVROS_INTEL_HELD_INTERVAL_H
#include <srvros/intel_engine_held_reset.h>
#include <srvros/scanout_transfer.h>
#include <srvros/intel_ggtt_window.h>
enum intel_held_interval_reason {IHI_INPUT=1,IHI_CLOCK=2,IHI_PARENT=4,
 IHI_GUARD=8,IHI_READ=16,IHI_CHANGED=32};
struct intel_held_interval_result {
 const struct intel_held_interval_result *object;
 const struct intel_engine_held_reset_result *reset;
 const struct intel_engine_prepare_result *preparation;
 const struct intel_gt_access *parent;
 const struct intel_gt_borrow *borrow;
 struct scanout_transfer_manifest manifest;
 uint64_t expected,origin_us,begin_us,last_us,end_us,clock_at,clock_prev;
 uint64_t sample_begin[2],sample_end[2];
 uint32_t epoch_before,epoch_after,words[2][3];
 unsigned attempts,reads,samples,counts[2],reason;
 bool stable,complete,bound;
};
struct intel_held_interval_io {
 void *ctx;
 bool (*guard)(void *);
 /* Exact high/low/high DWORD offsets at811004/811000/811004; read-only.
  * True is one complete DWORD even if the subsequent guard fails. */
 bool (*read)(void *,uint32_t,uint32_t *);
 uint64_t (*now)(void *);
};
/* Original one-use observation ONLY inside reset's provider-exit callback,
 * after both pulses/settling/post-snapshot, before cancellation/pin return.
 * Uses the existing active preparation borrow. No new/returned dependency,
 * PTE store, mapping import, cache operation or hardware assurance is issued.
 * All clocks use the original preparation100ms deadline, never a fresh budget.
 * A failed observation makes the enclosing provider fail and retain its pin. */
bool intel_held_interval_observe(struct intel_held_interval_result *,
 const struct intel_engine_held_reset_result *,const struct scanout_handoff_snapshot *,struct intel_held_interval_io);
/* Data only, still under the exact active source lifetime. It carries ZERO
 * hardware assurances and all255 open obligations, so transfer_open refuses.
 * The returned raw original pointer remains owned by the original result. */
bool intel_held_interval_manifest(const struct intel_held_interval_result *,struct scanout_transfer_manifest *);
#endif
