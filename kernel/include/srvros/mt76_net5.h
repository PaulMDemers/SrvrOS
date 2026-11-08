#ifndef SRVROS_MT76_NET5_H
#define SRVROS_MT76_NET5_H
#include <srvros/net_device.h>
#include <stddef.h>
#define MT76_NET5_QUEUE 4u
#define MT76_NET5_TX_US 750000u
enum mt76_net5_state { MT76_NET5_EMPTY, MT76_NET5_DOWN, MT76_NET5_UP,
 MT76_NET5_CLOSING, MT76_NET5_FAULT, MT76_NET5_RETIRED };
enum mt76_net5_submit { MT76_NET5_QUEUED, MT76_NET5_REFUSED, MT76_NET5_UNCERTAIN };
/* All callbacks are nonblocking and CPU0 serialized,possibly with IRQs off.
 * The transport retains the original exclusive CCMP/key/peer/DMA owner.
 * submit must consume/copy the borrowed Ethernet before returning,seal once
 * with that owner,and never borrow this buffer for DMA. A refused submission
 * is not retried here;reserved PNs must never roll back. UNCERTAIN retains the
 * pending identity and closes publication. service progresses asynchronous
 * completions;receive copies only already authenticated/normalized Ethernet.
 * retired must prove software keys erased AND all hardware/callback/DMA
 * ownership retired/restored. It is separate from a transfer completion.
 * Context stays valid until finish_detach returns true,including faults. */
struct mt76_net5_io {
 void *context;
 bool (*owned)(void *,uint64_t epoch);
 enum mt76_net5_submit (*submit)(void *,uint64_t epoch,uint64_t token,const uint8_t *,uint16_t);
 bool (*service)(void *,uint64_t epoch);
 bool (*receive)(void *,uint64_t epoch,uint8_t *,size_t,size_t *,bool *);
 bool (*retired)(void *,uint64_t epoch);
};
/* One zero static/heap object;never copy,reinitialize,or modify an owned
 * object. Register before net_init with the verified stable station MAC.
 * This module is dormant until a native owner explicitly binds its IO.
 * No keys,hardware access,ring reuse or retirement authority live here. */
struct mt76_net5 {
 uintptr_t identity;
 struct net_device_ops ops;
 struct mt76_net5_io io;
 enum mt76_net5_state state;
 uint64_t epoch,next_token,pending_token,last_us,tx_started;
 unsigned head,count,callbacks;
 bool attached,pending,io_busy,polling,delivering,retire_requested,registry_drained;
 uint16_t length[MT76_NET5_QUEUE],loan_size;
 uint8_t queue[MT76_NET5_QUEUE][NET_DEVICE_FRAME_MAX],loan[NET_DEVICE_FRAME_MAX],rx[NET_DEVICE_FRAME_MAX];
 uint64_t enqueued,submitted,completed,dropped,received,faults;
};
bool mt76_net5_init(struct mt76_net5 *,const uint8_t station[6]);
/* New epoch strictly exceeds every previously attached epoch. Call only after
 * local secured-port/key activation and native lifetime admission. No new
 * provider registration/selection or key copy occurs on attach/reconnect. */
bool mt76_net5_attach(struct mt76_net5 *,uint64_t epoch,const struct mt76_net5_io *);
bool mt76_net5_service(struct mt76_net5 *);
/* Native event owner validates the hardware event before forwarding this
 * exact epoch/token. Duplicate/wrong current tokens fail closed;old epochs
 * refuse unchanged. Completion alone never releases transport/DMA context. */
bool mt76_net5_tx_complete(struct mt76_net5 *,uint64_t epoch,uint64_t token,bool success);
bool mt76_net5_disconnect(struct mt76_net5 *,uint64_t epoch);
bool mt76_net5_finish_detach(struct mt76_net5 *,uint64_t epoch);
bool mt76_net5_retire(struct mt76_net5 *);
#endif
