#ifndef SRVROS_INTEL_GGTT_INVALIDATE_H
#define SRVROS_INTEL_GGTT_INVALIDATE_H
#include <srvros/intel_diagnostic_timing.h>
#include <srvros/intel_gt_access.h>

#define INTEL_GGTT_INVALIDATE_REGISTER 0x101008u
#define INTEL_GGTT_INVALIDATE_PREPARE_US INTEL_DIAGNOSTIC_PREPARE_US
#define INTEL_GGTT_INVALIDATE_GT_US 300000ull
#define INTEL_GGTT_INVALIDATE_HDA_US 400000ull
#define INTEL_GGTT_INVALIDATE_OUTER_US 500000ull
struct intel_ggtt_invalidate;
/* Trusted provider description, NOT an exclusion/mapping capability. The
 * provider must match this original registered child to its maintained native
 * owner, exact MMIO resource, CPU root, cache/alias policy and original clocks.
 * Ordinary source defaults100ms; a separately built VT-d child selects150ms. */
struct intel_ggtt_invalidate_binding {
 const void *owner,*provider,*lifetime;
 uint64_t root,epoch,sequence,mmio,mmio_bytes;
 uint64_t prepare_begin,gt_begin,hda_begin,outer_begin;
 uint32_t vendor,device;
};
struct intel_ggtt_invalidate_source {
 const struct intel_ggtt_invalidate_source *object;
 const struct intel_ggtt_invalidate *consumer;
 struct intel_ggtt_invalidate_binding binding;
};
struct intel_ggtt_invalidate_io {
 void *ctx;
 bool (*guard)(void *,const struct intel_ggtt_invalidate_source *);
 /* Ordered UC DWORD at exactly MMIO+101008. The first read must precede the
  * one write; the second is its posting read. Both return0 by specification.
  * Zero is NOT a self-clear ACK. Provider must preserve store/read ordering
  * and freshly admit the actual hardware after its last clock before I/O. */
 bool (*read)(void *,uint32_t,uint32_t *);
 enum intel_gt_write (*write)(void *,uint32_t,uint32_t);
 uint64_t (*now)(void *);
};
enum intel_ggtt_invalidate_state {GGTT_INVALIDATE_EMPTY,GGTT_INVALIDATE_OPENING,
 GGTT_INVALIDATE_WRITING,GGTT_INVALIDATE_POSTING,GGTT_INVALIDATE_VERIFYING,
 GGTT_INVALIDATE_COMPLETE,GGTT_INVALIDATE_FAULT};
enum intel_ggtt_invalidate_reason {GGTT_INVALIDATE_SOURCE=1,GGTT_INVALIDATE_CLOCK=2,
 GGTT_INVALIDATE_GUARD=4,GGTT_INVALIDATE_READ=8,GGTT_INVALIDATE_PROFILE=16,
 GGTT_INVALIDATE_WRITE=32,GGTT_INVALIDATE_RESULT=64};
enum intel_ggtt_invalidate_clock {GGTT_INVALIDATE_ZERO=1,GGTT_INVALIDATE_BACKWARD,
 GGTT_INVALIDATE_PREPARE_EXPIRED,GGTT_INVALIDATE_GT_EXPIRED,
 GGTT_INVALIDATE_HDA_EXPIRED,GGTT_INVALIDATE_OUTER_EXPIRED,GGTT_INVALIDATE_FROZEN};
/* Copyable DATA only; never copy the live source/result as an owner. */
struct intel_ggtt_invalidate_progress {
 uint64_t begin,last,state,reads,guards,attempts,writes,initial,posted,uncertain;
};
struct intel_ggtt_invalidate {
 const struct intel_ggtt_invalidate *object;
 struct intel_ggtt_invalidate_source *source;
 struct intel_ggtt_invalidate_binding binding;
 struct intel_ggtt_invalidate_io io,original_io;
 struct intel_ggtt_invalidate_progress p;
 uint64_t end,clock_at,clock_previous;
 uint32_t reason,clock_reason;
 bool busy,complete,retained;
};
/* Isolated protocol; native use only in the separately selected combined image.
 * One DWORD1 and one
 * subsequent posting read, no PTE or engine-TLB stores, no polling/retry.
 * Unknown store/order/identity/clock keeps the original graph and backing pinned.
 * Success is this System Agent translation invalidation sequence only, not
 * GPU pipeline/prefetch/cache/PPGTT, DMA, firmware/display-reader or interval
 * authority. Reads of this WO register cannot prove anything about old work. */
bool intel_ggtt_invalidate_run(struct intel_ggtt_invalidate *,
 struct intel_ggtt_invalidate_source *,struct intel_ggtt_invalidate_io);
#endif
