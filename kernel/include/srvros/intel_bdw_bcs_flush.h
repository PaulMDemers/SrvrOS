#ifndef SRVROS_INTEL_BDW_BCS_FLUSH_H
#define SRVROS_INTEL_BDW_BCS_FLUSH_H
#include <srvros/intel_gt_access.h>
#include <srvros/intel_diagnostic_timing.h>

#define BCS_FLUSH_DWORDS 6u
#define BCS_FLUSH_POLLS 256u
#define BCS_FLUSH_WAIT_US 10000ull
#define BCS_FLUSH_PREPARE_US INTEL_DIAGNOSTIC_PREPARE_US
#define BCS_FLUSH_GT_US INTEL_DIAGNOSTIC_GT_TOTAL_US
#define BCS_FLUSH_HDA_US INTEL_DIAGNOSTIC_HDA_US
#define BCS_FLUSH_OUTER_US INTEL_DIAGNOSTIC_OUTER_US
#define BCS_FLUSH_REQUIRED 31u
enum bcs_flush_requirement {
 BCS_FLUSH_RING=1,BCS_FLUSH_CONTEXT=2,BCS_FLUSH_COMPLETION=4,
 BCS_FLUSH_MAPPINGS=8,BCS_FLUSH_VISIBILITY=16
};
struct intel_bdw_bcs_flush;
/* Trusted ORIGINAL owner descriptor. The provider must independently own and
 * register this exact source and its private ring/context/completion mappings.
 * No current native provider supplies these prerequisites. Never adopt an
 * inherited ring, infer ownership from idle/TP clear, or bootstrap a mapping
 * using the command that requires that mapping. CPU0 serialized, consumed once. */
struct bcs_flush_binding {
 const void *owner,*provider,*lifetime,*context;
 uint64_t epoch,sequence,root,nonce;
 uint64_t ring_gpu,ring_physical,completion_gpu,completion_physical;
 uint64_t aperture_bytes,fence_begin,fence_end;
 uint64_t prepare_begin,gt_begin,hda_begin,outer_begin;
 uint32_t vendor,device;
};
struct bcs_flush_source {
 const struct bcs_flush_source *object;
 const struct intel_bdw_bcs_flush *consumer;
 struct bcs_flush_binding binding;
};
struct bcs_flush_proof {
 const struct bcs_flush_source *source;
 const void *owner,*provider,*lifetime,*context;
 uint64_t epoch,sequence,root,checked_at;
 uint32_t requirements;
};
struct bcs_flush_io {
 void *ctx;
 bool (*ownership)(void *,const struct bcs_flush_source *,struct bcs_flush_proof *);
 /* Ordered acquire QWord read of this exact private completion address.
  * Initial value MUST be zero; a pre-existing nonce cannot complete a request.
  * Provider validates actual mappings/cache/ownership after its last clock. */
 bool (*read)(void *,uint64_t gpu_address,uint64_t *);
 /* Publish exactly six DWORDs at offset0 of a private empty 4KiB ring, while
  * stopped. DONE means every byte is GPU-visible under the admitted policy.
  * Partial/unknown publication retains backing; no tail store is then allowed. */
 enum intel_gt_write (*publish)(void *,uint64_t ring_gpu,const uint32_t *,unsigned);
 /* The provider submits this exact owned context, ring and QWord-aligned tail.
  * DONE is known issuance, never completion. No inherited state is modified. */
 enum intel_gt_write (*submit)(void *,const void *context,uint64_t ring_gpu,uint32_t tail);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
enum bcs_flush_state {BCS_FLUSH_EMPTY,BCS_FLUSH_OPENING,BCS_FLUSH_PUBLISHING,
 BCS_FLUSH_SUBMITTING,BCS_FLUSH_POLLING,BCS_FLUSH_VERIFYING,BCS_FLUSH_COMPLETE,BCS_FLUSH_FAULT};
enum bcs_flush_reason {BCS_FLUSH_SOURCE=1,BCS_FLUSH_CLOCK=2,BCS_FLUSH_OWNERSHIP=4,
 BCS_FLUSH_READ=8,BCS_FLUSH_STALE=16,BCS_FLUSH_PUBLISH=32,BCS_FLUSH_SUBMIT=64,
 BCS_FLUSH_TIMEOUT=128,BCS_FLUSH_RESULT=256};
enum bcs_flush_clock {BCS_CLOCK_ZERO=1,BCS_CLOCK_BACKWARD,BCS_CLOCK_PREPARE,
 BCS_CLOCK_GT,BCS_CLOCK_HDA,BCS_CLOCK_OUTER,BCS_CLOCK_WAIT,BCS_CLOCK_FROZEN};
struct bcs_flush_progress {
 uint64_t begin,last,submit_begin,initial,observed;
 uint32_t state,proofs,reads,publish_attempts,published,submit_attempts,submitted,polls;
 bool uncertain;
};
struct intel_bdw_bcs_flush {
 const struct intel_bdw_bcs_flush *object;
 struct bcs_flush_source *source;
 struct bcs_flush_binding binding;
 struct bcs_flush_io io,original_io;
 struct bcs_flush_progress p;
 uint32_t packet[BCS_FLUSH_DWORDS];
 uint64_t end,clock_at,clock_previous;
 unsigned reason,clock_reason;
 bool busy,complete,retained;
};
/* Pure codec: five-DWORD QWord MI_FLUSH_DW with BCS TLB invalidation, direct
 * GGTT destination (not status-page indexing), plus MI_NOOP for 8-byte tail
 * alignment. Uses a 64-byte aligned destination, avoiding the documented QWord
 * destination bit5 workaround. No MMIO, native grant or ring publication. */
bool intel_bdw_bcs_flush_packet(uint64_t completion_gpu,uint64_t nonce,
                              uint32_t out[BCS_FLUSH_DWORDS]);
enum bdw_barrier_engine {
    BDW_BARRIER_RCS, BDW_BARRIER_BCS, BDW_BARRIER_VCS0, BDW_BARRIER_VCS1,
    BDW_BARRIER_VECS, BDW_BARRIER_ENGINES
};
/* Completion of preceding work on this engine under an owned private PPGTT.
 * Requires separately owned mappings and scheduling. Never a device-wide hold. */
bool intel_bdw_ppgtt_barrier(enum bdw_barrier_engine, uint64_t address, uint64_t marker,
                             uint32_t packet[BCS_FLUSH_DWORDS]);

/* Pure command/completion protocol. Original clocks never restart, including
 * the 10ms submit-through-completion interval. Unknown state is terminal and
 * retains ALL original backing; no rollback, unmap, retry or free callback.
 * Success covers only this BCS request, not other engines, firmware/display
 * readers or a HANDOFF assurance. No native image links this component. */
bool intel_bdw_bcs_flush_run(struct intel_bdw_bcs_flush *,struct bcs_flush_source *,struct bcs_flush_io);
#endif
