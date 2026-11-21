#ifndef SRVROS_INTEL_RESET_READY_H
#define SRVROS_INTEL_RESET_READY_H
#include <srvros/intel_completion_audit.h>
#define INTEL_BCS_RESET_CTL 0x220d0u
#define INTEL_BCS_RESET_REQUEST 0x00010001u
#define INTEL_BCS_RESET_CANCEL 0x00010000u
#define INTEL_RESET_READY_PHASE_US 10000ull
#define INTEL_RESET_READY_TOTAL_US 100000ull
#define INTEL_RESET_READY_POLLS 256u
enum intel_reset_ready_reason {IRR_OK,IRR_INPUT,IRR_CLOCK,IRR_GUARD,IRR_ENTRY,
 IRR_READ,IRR_WRITE,IRR_TIMEOUT,IRR_DRIFT,IRR_PARENT};
struct intel_reset_ready_result {
 const struct intel_reset_ready_result *object;
 struct intel_gt_borrow borrow;
 uint32_t controls[4],words[2][8];
 uint64_t begin_us,end_us,last_us;
 unsigned reads,polls,attempts,writes,reason,samples,controls_read;
 bool admitted,ready,cancelled,complete,uncertain,blocked_close;
};
struct intel_reset_ready_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*control)(void *,uint32_t *);
 bool (*group)(void *,uint32_t [8]);
 enum intel_gt_write (*write)(void *,uint32_t);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* BDW PRM2c p165, only Request Reset mask+bit0; no GDRST/Stop Ring/TLB
 * command. Exact empty disabled BCS entry, ready acknowledgement, cancellation
 * and unchanged BCS state. NOT outstanding-write/prefetch/cache completion.
 * A possibly issued request retains its dependent on any failed postcondition.
 * Zero-init result, consume once; no recovery/force-close API. */
bool intel_reset_ready(struct intel_reset_ready_result *,struct intel_gt_access *,
 const struct intel_completion_result *,bool policy,struct intel_reset_ready_io);
#endif
