#ifndef SRVROS_MT76_HANDOFF5_H
#define SRVROS_MT76_HANDOFF5_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MT76_HANDOFF5_PROTECTED 32
/* Mechanical offline plan only;no device/controller/parent/DMA authority.
 * Captured US161 native admission, allocation and retained page identity belong
 * to a separate owner. Old stopped ring is never rewritten or recycled. */
struct mt76_handoff5_plan {
 uint64_t generation,old_ring,old_dequeue,new_ring;
 unsigned old_posts,new_slots,protected_count;
 uint8_t slot;
 uint64_t protected_pages[MT76_HANDOFF5_PROTECTED];
};
struct mt76_handoff5 {
 bool attempted,retarget_attempted,retargeted,inspected,restore_attempted,restored,complete,failed,unsafe;
 unsigned reads,commands;uint64_t generation;
};
struct mt76_handoff5_io {
 void *context;
 /* Serialized callbacks must not mutate inputs/state. Must bind the original
  * parent before retarget, a separately pinned fresh owner while switched,
  * and the unchanged old owner after exact restoration. No scalar plan grants
  * this authority;cleanup never revives uncertain command or DMA ownership. */
 bool (*owned)(void *,bool cleanup);
 bool (*read_context)(void *,uint32_t words[8]);
 /* Exact one-shot Set TR Dequeue on captured slot/DCI9, bounded completion;
  * false is uncertain,never a license to replay or restore speculatively. */
 bool (*set_dequeue)(void *,uint64_t dequeue);
 /* Optional bounded work under the separate new owner;must retire it to
  * stopped state3 with no pending DMA before returning,even on handled failure.
  * This engine itself posts no TD,starts no MAC and sends no network frame. */
 bool (*inspect)(void *);
};
bool mt76_handoff5_plan_valid(const struct mt76_handoff5_plan *);
/* Exact old snapshot -> fresh stopped ring -> inspection -> independently
 * proven fresh stop -> exact old dequeue/all context words restored. A handled
 * inspection failure can restore but is never complete. Uncertain commands,
 * lost ownership or unproved quiescence retain both rings and return unsafe. */
bool mt76_handoff5_run(struct mt76_handoff5 *,const struct mt76_handoff5_plan *,const struct mt76_handoff5_io *);
#endif
