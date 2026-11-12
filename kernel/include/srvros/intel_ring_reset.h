#ifndef SRVROS_INTEL_RING_RESET_H
#define SRVROS_INTEL_RING_RESET_H
#include <srvros/intel_ring_transition.h>
#include <srvros/intel_takeover_preflight.h>
#define INTEL_RING_RESET_ADMIT_US 100000ull
enum intel_ring_reset_reason {IRH_INPUT=1,IRH_SOURCE=2,IRH_CLOCK=4,IRH_TAKEOVER=8,IRH_RETURN=16};
/* Original live entry and fresh census, never copied ownership or receipt data.
 * The takeover owns its own registered child. No reset is issued here. */
struct intel_ring_reset {
 const struct intel_ring_reset *object;
 const struct intel_ring_transition *entry;
 struct intel_gt_access *parent;
 struct intel_takeover_result *takeover;
 const struct intel_completion_result *source;
 void *provider;
 uint64_t begin,last,end,source_begin,source_end,source_last,entry_end,clock_at,clock_prev;
 uint64_t take_begin,take_end,take_last;
 uint32_t epoch;
 unsigned reason,checks;
 bool eligible,bound,complete,uncertain;
};
/* Zero-init both once. Only kind2 zero geometry can advance. Both source age
 * and the whole bridge/takeover keep100ms, inside unchanged GT/HDA/outer bounds.
 * Any lost source/clock/return after eligible admission retains the root.
 * On success use the original takeover object immediately in the existing
 * preparation/reset pipeline; completion is not authority for another boot. */
bool intel_ring_reset_admit(struct intel_ring_reset *,struct intel_gt_access *,
 const struct intel_ring_transition *,struct intel_takeover_result *,bool,struct intel_takeover_io);
#endif
