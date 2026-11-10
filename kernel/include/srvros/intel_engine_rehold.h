#ifndef SRVROS_INTEL_ENGINE_REHOLD_H
#define SRVROS_INTEL_ENGINE_REHOLD_H
#include <srvros/intel_engine_held_reset.h>
enum intel_engine_rehold_reason {IER_INPUT=1,IER_CLOCK=2,IER_PARENT=4,
 IER_GUARD=8,IER_READ=16,IER_WRITE=32,IER_STATE=64,IER_TIMEOUT=128,
 IER_CHILD=256};
struct intel_engine_rehold_child;
struct intel_engine_rehold {
 const struct intel_engine_rehold *object;
 struct intel_engine_held_reset_result *reset;
 const struct intel_engine_prepare_result *preparation;
 const struct intel_gt_access *parent;
 const struct intel_gt_borrow *borrow;
 struct intel_engine_rehold_child *child;
 uint64_t origin_us,owner_origin_us,begin_us,last_us,end_us,clock_at,clock_prev;
 uint64_t phase_begin[2],phase_end[2];
 uint32_t epoch,words[4][6],ack[2],final[2];
 unsigned reason,stage,attempts,writes,reads,polls,acks,samples,sample_attempts;
 unsigned counts[4],phase_polls[2],child_borrows,child_returns;
 bool busy,child_started,child_active,child_completed,restored,uncertain,complete;
};
/* One exclusive child of this synchronous source, not another GT borrow.
 * Its context/storage must survive retained failure. Copies cannot return it. */
struct intel_engine_rehold_child {
 const struct intel_engine_rehold_child *object;
 struct intel_engine_rehold *parent;
 bool active;
};
struct intel_engine_rehold_io {
 void *ctx;
 bool (*guard)(void *);
 /* Five RESET_CTLs in existing engine order, then GDRST. Preserve prefixes. */
 bool (*sample)(void *,uint32_t [6],unsigned *);
 bool (*control)(void *,uint32_t *); /* BCS RESET_CTL only */
 enum intel_gt_write (*transition)(void *,uint32_t,uint32_t *);
 bool (*observe)(void *,struct intel_engine_rehold *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* Sole original reset stage4 after BCS8 pulses/settling. Re-arm only BCS,
 * hold all five requests while the child runs, then restore mixed3/3/3/3/0.
 * Existing original100ms origin and10ms/256-poll phase caps, never restarted.
 * Live children/faults prevent cancellation. No failure cleanup or GT return,
 * epoch change, PTE writer, GPU memory grant or completion assurance. */
bool intel_engine_rehold_run(struct intel_engine_rehold *,
 struct intel_engine_held_reset_result *,struct intel_engine_rehold_io);
bool intel_engine_rehold_live(const struct intel_engine_rehold *);
bool intel_engine_rehold_borrow(struct intel_engine_rehold *,struct intel_engine_rehold_child *);
bool intel_engine_rehold_child_live(const struct intel_engine_rehold_child *);
bool intel_engine_rehold_return(struct intel_engine_rehold_child *);
#endif
