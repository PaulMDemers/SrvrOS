#ifndef SRVROS_INTEL_VTD_DRAIN_H
#define SRVROS_INTEL_VTD_DRAIN_H
#include <srvros/intel_gt_access.h>

#define INTEL_VTD_DRAIN_CAP 0x01c0000c40660462ull
#define INTEL_VTD_DRAIN_ECAP 0x7e1ff0505eull
#define INTEL_VTD_DRAIN_HASH 0x722775ad66626863ull
#define INTEL_VTD_DRAIN_UNIT 0xfed90000ull
#define INTEL_VTD_DRAIN_REQUEST 0x90030000u
#define INTEL_VTD_DRAIN_POLLS 256u
#define INTEL_VTD_DRAIN_PREPARE_US 100000ull
#define INTEL_VTD_DRAIN_GT_US 300000ull
#define INTEL_VTD_DRAIN_HDA_US 400000ull
#define INTEL_VTD_DRAIN_OUTER_US 500000ull
struct intel_vtd_drain;
/* Scalar description only, not authority. An adapter must independently match
 * the ORIGINAL registered child against its actual maintained requester/engine/
 * firmware/resource/root/cache owner graph at EVERY guard. BME clear, TES clear
 * or echoing this description is not exclusive invalidation-register control.
 * Zero-init, consume once; retain the source/result and enclosing pins on fault. */
struct intel_vtd_drain_binding {
 const void *owner,*provider,*lifetime;
 uint64_t root,epoch,sequence,hash,unit,prepare_begin,gt_begin,hda_begin,outer_begin;
 uint32_t vendor,device;
};
struct intel_vtd_drain_source {
 const struct intel_vtd_drain_source *object;
 const struct intel_vtd_drain *consumer;
 struct intel_vtd_drain_binding binding;
};
struct intel_vtd_drain_io {
 void *ctx;
 bool (*guard)(void *,const struct intel_vtd_drain_source *);
 /* Ordered UC DWORDs, strictly within the one admitted graphics-unit page.
  * No native adapter is supplied. No GCMD, RTADDR, IVA, queue or fault writes. */
 bool (*read)(void *,uint32_t offset,uint32_t *);
 /* Only offset0x50c, value0x90030000: IVT, global, DR and DW. Writing this
  * high DWORD triggers the 64-bit register. Its low DWORD is reserved, never
  * written. DONE means issued, not completed; UNKNOWN retains the owner. */
 enum intel_gt_write (*write)(void *,uint32_t offset,uint32_t);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
enum intel_vtd_drain_state {VTD_DRAIN_EMPTY,VTD_DRAIN_OPENING,VTD_DRAIN_REQUESTING,
 VTD_DRAIN_POLLING,VTD_DRAIN_VERIFYING,VTD_DRAIN_COMPLETE,VTD_DRAIN_FAULT};
enum intel_vtd_drain_reason {VTD_DRAIN_SOURCE=1,VTD_DRAIN_CLOCK=2,VTD_DRAIN_GUARD=4,
 VTD_DRAIN_READ=8,VTD_DRAIN_PROFILE=16,VTD_DRAIN_BUSY=32,VTD_DRAIN_WRITE=64,
 VTD_DRAIN_PENDING=128,VTD_DRAIN_RESULT=256};
enum intel_vtd_drain_clock {VTD_DRAIN_ZERO=1,VTD_DRAIN_BACKWARD,VTD_DRAIN_PREPARE_EXPIRED,
 VTD_DRAIN_GT_EXPIRED,VTD_DRAIN_HDA_EXPIRED,VTD_DRAIN_OUTER_EXPIRED,VTD_DRAIN_FROZEN};
/* Snapshot/progress DATA may be copied; never copy the original source/result
 * as an ownership object. Word-shaped fields permit explicit equality checks
 * without padding/memcmp or a freestanding runtime dependency. */
struct intel_vtd_drain_progress {
 uint64_t begin,last,state,reads,guards,attempts,writes,polls,uncertain;
 uint64_t context[2][8]; /* GSTS, VER, CAP, ECAP, RTADDR, CCMD, FSTS, GSTS */
 uint64_t initial,final;
 uint32_t observed[INTEL_VTD_DRAIN_POLLS];
};
struct intel_vtd_drain {
 const struct intel_vtd_drain *object;
 struct intel_vtd_drain_source *source;
 struct intel_vtd_drain_binding binding;
 struct intel_vtd_drain_io io,original_io;
 struct intel_vtd_drain_progress p;
 uint64_t end,clock_at,clock_previous;
 uint32_t reason,clock_reason;
 bool busy,complete,retained;
};
/* Exact captured Air7,2 graphics DRHD/8086:1626 only. Requires TES/QIES and all
 * GSTS clear, root0, idle context command and no fault. One global DR/DW request,
 * at most256 fresh IVT polls, IAIG=global, final unchanged policy. Preserve the
 * ORIGINAL 100/300/400/500ms budgets. No retry, clearing, mapping or release API.
 * Success describes AT=00 memory requests already processed by this remapper;
 * not GPU pipeline/prefetch/cache, ATS/peers, later requests, readers or a PTE
 * grant. No native image currently links this protocol. */
bool intel_vtd_drain_run(struct intel_vtd_drain *,struct intel_vtd_drain_source *,struct intel_vtd_drain_io);
#endif
