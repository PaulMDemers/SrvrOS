#ifndef SRVROS_MT76_POWER5_CYCLE_H
#define SRVROS_MT76_POWER5_CYCLE_H
#include <srvros/mt76_power5_capture.h>
struct mt76_power5_cycle_io {
 void *context;
 /* Forward ownership includes a fresh mapped gate and the same selected
  * calibrated lifetime. Cleanup ownership survives forward cancellation and
  * additionally requires transport retirement before power restoration. */
 bool (*owned)(void *,bool cleanup);
 bool (*read)(void *,uint32_t,uint32_t *,bool cleanup);
 bool (*write)(void *,uint32_t,uint32_t,bool cleanup);
 /* Called with all nine selected words verified. Must drain/disable MAC and
  * stop every new endpoint before returning,even on failure. No transfer or
  * key authority is supplied by this power helper. */
 bool (*inspect)(void *);
};
struct mt76_power5_cycle {
 bool attempted,snapshot_complete,selected,inspected,cleanup_complete,complete,failed;
 unsigned reads,writes,saved_mask,modified_mask,verified_mask,restored_mask;
 uint64_t generation;
 uint32_t saved[9],chosen[9],restored[9];
};
/* Fixed nine-register plan,<=15dBm source-model ceiling. Disabled MAC/core/DMA
 * guard before every snapshot/apply/restore step;full snapshot before the first write;
 * potentially modified words marked BEFORE submission;reverse original-word
 * restoration including readback after uncertain writes. Never retries OUT.
 * All hardware waits are bounded by the owning transport. */
bool mt76_power5_cycle_run(struct mt76_power5_cycle *,const struct mt76_power5_cycle_io *,
 const struct mt76_power5_capture *,unsigned limit_half_dbm);
#endif
