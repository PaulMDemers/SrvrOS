#ifndef SRVROS_INTEL_INTERVAL_CLAIM_H
#define SRVROS_INTEL_INTERVAL_CLAIM_H
#include <srvros/intel_engine_rehold.h>
#include <srvros/intel_held_interval.h>
#include <srvros/intel_bdw_ggtt.h>
enum intel_interval_claim_state {IIC_EMPTY,IIC_OPENING,IIC_ACTIVE,IIC_CLOSING,IIC_CLOSED,IIC_FAULT};
enum intel_interval_claim_reason {IIC_INPUT=1,IIC_SOURCE=2,IIC_CLOCK=4,IIC_GUARD=8,IIC_READ=16,IIC_CHANGED=32,IIC_CHILD=64};
struct intel_interval_borrow;
/* Read-only source claim, NOT a transferred/address-allocatable GPU interval.
 * Static/heap, zero-init and one-use. Context survives retained faults. */
struct intel_interval_claim {
 const struct intel_interval_claim *object;
 struct intel_engine_rehold_child *child;
 const struct intel_engine_rehold *rehold;
 const struct intel_engine_held_reset_result *reset;
 const struct intel_gt_access *parent;
 struct intel_held_interval_io io;
 struct scanout_handoff_snapshot captured;
 struct intel_held_interval_result opening,closing;
 uint64_t expected,origin_us,owner_origin_us,begin_us,last_us,end_us,clock_at,clock_prev;
 uint32_t epoch,state,reason;
 struct intel_interval_borrow *dependent;
 const struct intel_interval_borrow *registered;
 uint32_t borrows,returns;
 bool busy,pinned;
};
/* One active registered consumer of the original claim. Storage remains alive
 * through return or a retained fault. A returned object cannot be reused. */
struct intel_interval_borrow {
 const struct intel_interval_borrow *object;
 struct intel_interval_claim *claim;
 uint32_t epoch,sequence;
 bool active,busy,fault;
};
/* Pins the original live re-arm child before the first observer callback.
 * Captures existing exact one-page candidate, never a free/usable address.
 * Open and close each own two high/low/high original-PTE observations. */
bool intel_interval_claim_open(struct intel_interval_claim *,struct intel_engine_rehold_child *,
 const struct scanout_handoff_snapshot *,struct intel_held_interval_io);
bool intel_interval_claim_check(struct intel_interval_claim *);
/* Data only: assurances0 and obligations255, so scanout_transfer_open refuses.
 * No callback or new clock sample; this does not authorize target accesses. */
bool intel_interval_claim_manifest(const struct intel_interval_claim *,struct scanout_transfer_manifest *);
bool intel_interval_borrow_open(struct intel_interval_claim *,struct intel_interval_borrow *);
bool intel_interval_borrow_check(struct intel_interval_borrow *);
/* Pure original source predicate, also usable inside its serialized callback.
 * No I/O, fresh clock, data output, reentry permission or hardware grant. */
bool intel_interval_borrow_live(const struct intel_interval_borrow *);
/* Publish a just-sampled timestamp from the SAME source clock. No I/O or
 * restarted origin. Source/time loss latches fault and retains registration. */
bool intel_interval_borrow_deadline(struct intel_interval_borrow *,uint64_t);
bool intel_interval_borrow_manifest(const struct intel_interval_borrow *,struct scanout_transfer_manifest *);
/* Captured data for a future owned transaction. No fresh clock or RAM
 * ownership is established by planning a replacement entry. */
bool intel_interval_borrow_page(const struct intel_interval_borrow *,uint64_t physical,
                               struct intel_bdw_ggtt_page *);
bool intel_interval_borrow_return(struct intel_interval_borrow *);
/* Fresh source/clock/guard and exact original-state proof precede child unpin.
 * Lost source, clock or observation latches FAULT and retains the dependent.
 * No retry, forced cleanup, PTE write, new GT borrow or deadline extension. */
bool intel_interval_claim_close(struct intel_interval_claim *);
#endif
