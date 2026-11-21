#ifndef SRVROS_INTEL_BDW_TLB_H
#define SRVROS_INTEL_BDW_TLB_H
#include <srvros/intel_gt_access.h>

#define INTEL_BDW_TLB_ENGINES 5u
#define INTEL_BDW_TLB_MASK 31u
#define INTEL_BDW_TLB_POLLS 256u
#define INTEL_BDW_TLB_PREPARE_US 100000ull
#define INTEL_BDW_TLB_GT_US 300000ull
#define INTEL_BDW_TLB_HDA_US 400000ull
#define INTEL_BDW_TLB_OUTER_US 500000ull
extern const uint32_t intel_bdw_tlb_registers[INTEL_BDW_TLB_ENGINES];
struct intel_bdw_tlb;
/* Trusted adapter descriptor, not a hardware ownership token. Zero-init then
 * bind to the ORIGINAL live owner/provider/lifetime. Consumed once, including
 * refusal. A new descriptor/sequence needs independent provider admission.
 * Keep descriptor, result and enclosing pins alive through every fault. */
struct intel_bdw_tlb_binding {
 const void *owner,*provider,*lifetime;
 uint64_t epoch,sequence,root,physical,aperture_bytes,fence_begin,fence_end;
 uint64_t prepare_begin,gt_begin,hda_begin,outer_begin;
 uint32_t vendor,device,first,pages;
};
struct intel_bdw_tlb_source {
 const struct intel_bdw_tlb_source *object;
 const struct intel_bdw_tlb *consumer;
 struct intel_bdw_tlb_binding binding;
};
/* Each callback must independently establish flushed HW pipelines and retirement
 * of ALL their memory accesses, for all five engines under the same live owner.
 * Ready-for-reset, AF TP clear, idle parser, reset ACK, CPU CLFLUSH and initially
 * clear TLB bits CANNOT supply this proof. Provider-private original child
 * registration must match the descriptor address; echoing supplied fields or
 * masks is not evidence. No current native provider supplies this proof. */
struct intel_bdw_tlb_proof {
 const struct intel_bdw_tlb_source *source;
 const void *owner,*provider,*lifetime;
 uint64_t epoch,sequence,root,checked_at;
 uint32_t engines,pipelines_flushed,memory_retired;
};
struct intel_bdw_tlb_io {
 void *ctx;
 /* Trusted provider admits the original resource/root/cache/exclusion on each
  * callback and supplies ordered UC DWORD access. No native adapter exists. */
 bool (*prerequisite)(void *,const struct intel_bdw_tlb_source *,struct intel_bdw_tlb_proof *);
 bool (*read)(void *,unsigned engine,uint32_t offset,uint32_t *);
 /* Write exactly bit0 with a DWORD. NONE guarantees no store; UNKNOWN may
  * have stored. Never clear a pending request or write reserved fields. */
 enum intel_gt_write (*write)(void *,unsigned engine,uint32_t offset,uint32_t);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
enum intel_bdw_tlb_state {BDW_TLB_EMPTY,BDW_TLB_OPENING,BDW_TLB_READING,
 BDW_TLB_WRITING,BDW_TLB_POLLING,BDW_TLB_COMPLETE,BDW_TLB_FAULT};
enum intel_bdw_tlb_reason {BDW_TLB_INPUT=1,BDW_TLB_SOURCE=2,BDW_TLB_CLOCK=4,
 BDW_TLB_PREREQUISITE=8,BDW_TLB_READ=16,BDW_TLB_BUSY=32,BDW_TLB_WRITE=64,
 BDW_TLB_PENDING=128,BDW_TLB_STATE=256};
enum intel_bdw_tlb_clock {BDW_TLB_ZERO=1,BDW_TLB_BACKWARD=2,
 BDW_TLB_PREPARE_EXPIRED=3,BDW_TLB_GT_EXPIRED=4,BDW_TLB_HDA_EXPIRED=5,BDW_TLB_OUTER_EXPIRED=6,BDW_TLB_FROZEN=7};
struct intel_bdw_tlb {
 const struct intel_bdw_tlb *object;
 struct intel_bdw_tlb_source *input;
 struct intel_bdw_tlb_binding binding;
 struct intel_bdw_tlb_io io,original_io;
 uint64_t begin,last,end,clock_at,clock_previous;
 uint32_t state,reason,clock_reason,index,initial[5],final[5],polls[5];
 uint32_t reads,attempts,writes,completed,proofs,requested_mask,completed_mask;
 bool busy,complete,retained,uncertain;
};
/* Pure protocol only: no native MMIO implementation or image links this core.
 * Exact Broadwell 8086:1626; one original source and original 100/300/400/500ms
 * budgets, never restarted. Read all initial controls before any request.
 * Completion requires an issued request and fresh self-clear per engine.
 * Failure never retries, rolls back a request, releases pins or grants a range.
 * Even success is only this TLB operation; no DMA/cache/reader/PTE authority. */
bool intel_bdw_tlb_invalidate(struct intel_bdw_tlb *,struct intel_bdw_tlb_source *,struct intel_bdw_tlb_io);
#endif
