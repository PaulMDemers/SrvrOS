#ifndef SRVROS_MT76_DMA5_H
#define SRVROS_MT76_DMA5_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MT76_DMA5_PAGES 4u
#define MT76_DMA5_TRBS 255u
#define MT76_DMA5_BYTES 4096u
#define MT76_DMA5_TIMEOUT_US 750000u
enum mt76_dma5_direction { MT76_DMA5_TX, MT76_DMA5_RX };
enum mt76_dma5_state { MT76_DMA5_EMPTY, MT76_DMA5_RUNNING, MT76_DMA5_CLOSING, MT76_DMA5_FAULT, MT76_DMA5_RETIRED };
enum mt76_dma5_page_state { MT76_DMA5_FREE, MT76_DMA5_PREPARED, MT76_DMA5_DEVICE, MT76_DMA5_COMPLETE, MT76_DMA5_BORROWED };
struct mt76_dma5_lease {
 uint64_t epoch,ticket,token,page,trb;
 unsigned direction,index,length,received,cycle;
};
/* Bufferless TX descriptor identity:no page,length or receive authority. */
struct mt76_dma5_noop_lease {uint64_t epoch,ticket,token,trb;unsigned cycle;};
struct mt76_dma5_page {struct mt76_dma5_lease lease;enum mt76_dma5_page_state state;uint64_t started;};
/* Physical ranges are distinct retained 4KiB pages,including each ring.
 * No raw DMA/key buffers,MMIO,allocation or hardware authority live here.
 * CPU0 caller serializes calls. A zero object is initialized once,never copied.
 * Native binding must separately validate owner,slot,endpoint,event cycle and
 * event-ring consumption BEFORE mapping a real event to the issued lease.
 * Never stamp a current ticket onto an unvalidated old raw TRB pointer. */
struct mt76_dma5 {
 uintptr_t identity;uint64_t epoch,next_ticket,last_us,ring[2],last_token[2],last_fence;
 unsigned positions,cursor[2],cycle[2],next_page[2];
 enum mt76_dma5_state state;
 struct mt76_dma5_page pages[2][MT76_DMA5_PAGES];
 uint64_t prepared,published,completed,borrowed,released,cancelled,wraps,faults;
 struct mt76_dma5_noop_lease noop;enum mt76_dma5_page_state noop_state;
 uint64_t noop_started,noop_prepared,noop_published,noop_completed,noop_cancelled;bool noop_only;
};
/* These are external proofs,not requests to stop/reset hardware. The native
 * binding must collect all fields from its independent checked owner. Wrap
 * requires stopped endpoint,event debt drained,and successful Set TR Dequeue
 * to ring base with the opposite cycle;all pages of that direction are free.
 * An ordinary completion does not authorize descriptor-address alias reuse. */
struct mt76_dma5_fence {uint64_t epoch,sequence,dequeue;unsigned direction;bool stopped,events_drained,dequeue_set;};
struct mt76_dma5_retirement {uint64_t epoch;bool keys_erased,mac_disabled,tx_stopped,rx_stopped,events_drained,callbacks_drained,parent_restored;};
/* Native rings may be smaller than the standalone255-position fixture. */
bool mt76_dma5_init_ring(struct mt76_dma5 *,uint64_t epoch,const uint64_t rings[2],const uint64_t pages[2][MT76_DMA5_PAGES],unsigned positions,uint64_t now);
bool mt76_dma5_init(struct mt76_dma5 *,uint64_t epoch,const uint64_t rings[2],const uint64_t pages[2][MT76_DMA5_PAGES],uint64_t now);
/* Explicit bufferless mode retains only the real TX descriptor ring,no dummy
 * RX ring/data pages. Data prepare/acquire remain unavailable in this mode. */
bool mt76_dma5_init_noop_ring(struct mt76_dma5 *,uint64_t epoch,uint64_t tx_ring,unsigned positions,uint64_t now);
/* NoOp APIs use only type8 TX descriptor identity. Native owner must validate
 * exact raw event before completion. No payload/PN/page/receive authority. */
bool mt76_dma5_noop_prepare(struct mt76_dma5 *,uint64_t token,uint64_t now,struct mt76_dma5_noop_lease *);
bool mt76_dma5_noop_publish(struct mt76_dma5 *,const struct mt76_dma5_noop_lease *,uint64_t now);
bool mt76_dma5_noop_complete(struct mt76_dma5 *,const struct mt76_dma5_noop_lease *,unsigned cc,unsigned residue,uint64_t now);
bool mt76_dma5_noop_cancel(struct mt76_dma5 *,const struct mt76_dma5_noop_lease *);
/* Reserve before copying/sealing. Tokens strictly increase per direction even
 * after cancellation;reserve a CCMP PN only in the original key owner,once. */
bool mt76_dma5_prepare(struct mt76_dma5 *,unsigned direction,uint64_t token,unsigned length,uint64_t now,struct mt76_dma5_lease *);
bool mt76_dma5_cancel(struct mt76_dma5 *,const struct mt76_dma5_lease *);
/* Publish makes the lease hardware-owned BEFORE doorbell. Any uncertain
 * descriptor/doorbell publication must call fault,never cancel or roll back PN. */
bool mt76_dma5_publish(struct mt76_dma5 *,const struct mt76_dma5_lease *,uint64_t now);
bool mt76_dma5_complete(struct mt76_dma5 *,const struct mt76_dma5_lease *,unsigned cc,unsigned residue,uint64_t now);
bool mt76_dma5_acquire(struct mt76_dma5 *,uint64_t epoch,struct mt76_dma5_lease *);
/* Release only after copying/authentication/consumer callback returns. No
 * authenticated frame may reference a DMA page beyond this release. */
bool mt76_dma5_release(struct mt76_dma5 *,const struct mt76_dma5_lease *);
/* TX has a bounded pending deadline. A quiet posted RX does not time out;
 * closing it still requires independent native stop/event-drain retirement. */
bool mt76_dma5_tick(struct mt76_dma5 *,uint64_t now);
bool mt76_dma5_close(struct mt76_dma5 *,uint64_t epoch);
bool mt76_dma5_wrap(struct mt76_dma5 *,const struct mt76_dma5_fence *);
bool mt76_dma5_retire(struct mt76_dma5 *,const struct mt76_dma5_retirement *);
void mt76_dma5_fault(struct mt76_dma5 *);
#endif
