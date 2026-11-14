#ifndef SRVROS_INTEL_NONZERO_SOURCE_H
#define SRVROS_INTEL_NONZERO_SOURCE_H
#include <srvros/intel_nonzero_reset.h>
#include <srvros/scanout_handoff.h>
#include <srvros/intel_ggtt_window.h>
enum intel_nonzero_source_reason {INS_INPUT=1,INS_PARENT=2,INS_CLOCK=4,
 INS_GUARD=8,INS_READ=16,INS_CHANGED=32,INS_RETURN=64};
struct intel_nonzero_source {
 const struct intel_nonzero_source *object;
 struct intel_nonzero_reset *reset;
 const struct intel_completion_result *post;
 const struct intel_ring_transition *entry;
 struct intel_gt_access *parent;
 const struct scanout_handoff_snapshot *input;
 void *provider;
 uint64_t expected,origin,parent_begin,post_begin,post_end,post_last;
 uint64_t begin,last,end,read_begin,read_end,return_begin,clock_at,clock_prev;
 uint32_t epoch,post_mode,words[2][3];
 unsigned count,reason;
 bool registered,blocked,stable,returned,complete;
#ifdef SRVROS_INTEL_STARTUP_BUFFERS
 const void *dependent,*registered_child;
 unsigned child_borrows,child_returns;
 bool observer_entered,observer_complete;
#endif
};
struct intel_nonzero_source_io {
 void *ctx;
 bool (*guard)(void *);
 /* Exactly six read-only DWORDs: high/low/high twice. Complete prefixes
  * survive false. A batch brackets the full hardware validation once. */
 bool (*sample)(void *,uint32_t [2][3],unsigned *);
 uint64_t (*now)(void *);
};
/* One-use original child, static/heap through faults. Shares preparation100ms
 * and GT300ms origins; no new budget or transfer/re-arm/requester authority. */
bool intel_nonzero_source_run(struct intel_nonzero_source *,struct intel_nonzero_reset *,
 const struct scanout_handoff_snapshot *,struct intel_nonzero_source_io);
bool intel_nonzero_source_live(const struct intel_nonzero_source *);
#ifdef SRVROS_INTEL_STARTUP_BUFFERS
/* A separately compiled child inside the original stable source lifetime.
 * Ordinary source_run never invokes this observer. No new deadline/grant. */
bool intel_startup_buffers_source_run(struct intel_nonzero_source *,struct intel_nonzero_reset *,
 const struct scanout_handoff_snapshot *,struct intel_nonzero_source_io,
 bool (*observe)(void *,struct intel_nonzero_source *),void *);
#endif
#endif
