#ifndef SRVROS_INTEL_BLITTER_H
#define SRVROS_INTEL_BLITTER_H
#include <srvros/intel_engine_census.h>
#define INTEL_BCS_WORDS 8u
#define INTEL_BCS_CONTROL 0x2203cu
#define INTEL_BCS_DISABLED 0xf000u
struct intel_blitter_result {
 const struct intel_blitter_result *object; /* Zero-init; one invocation, no retry/copy reuse. */
 uint32_t words[4][INTEL_BCS_WORDS];
 uint64_t begin_us,end_us,last_us;
 unsigned samples,reads,attempts,writes,reason;
 bool admitted,disabled,uncertain;
};
struct intel_blitter_io {
 void *context;
 bool (*guard)(void *);
 bool (*read)(void *,uint32_t [INTEL_BCS_WORDS]);
 enum intel_gt_write (*write)(void *,uint32_t);
 uint64_t (*now)(void *);
};
/* Explicit trusted cooperative boot handoff of the BCS submission domain.
 * Not inferred from quiet values. No reset/stop/submission/restoration exists;
 * uncertain issued writes require retaining the parent wake lease until reset.
 * Disabling the idle command front-end grants no DMA or memory retirement. */
bool intel_blitter_disable(struct intel_blitter_result *,struct intel_gt_access *,
 bool submission_handoff,struct intel_blitter_io);
#endif
