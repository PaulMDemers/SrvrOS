#ifndef SRVROS_MT76_JOIN5_H
#define SRVROS_MT76_JOIN5_H
#include <srvros/mt76_peer5.h>
#define MT76_JOIN5_REPLY_US 750000u
/* Management only. Open-system authentication/association do not establish
 * WPA2 keys or authorize the controlled data port. USB OUT is not an AP ACK. */
enum mt76_join5_send {MT76_JOIN5_SENT,MT76_JOIN5_REFUSED,MT76_JOIN5_UNCERTAIN};
struct mt76_join5_io {
 void *context;
 bool (*owned)(void *,bool cleanup);
 enum mt76_join5_send (*send)(void *,enum mt76_mgmt5_kind,const uint8_t *,size_t);
 /* Normalized copied MPDU,not borrowed DMA. Dispatches shared events. */
 bool (*poll)(void *,uint8_t *,size_t,size_t *,bool *present);
 bool (*delay)(void *,unsigned us);
 uint64_t (*now)(void *);
};
/* Diagnostic observations only. No permission to retry, extend a deadline or
 * accept a reply. span_us covers only clock values already read by response(). */
enum mt76_join5_response_reason {
 MT76_JOIN5_RESPONSE_UNATTEMPTED,MT76_JOIN5_RESPONSE_MATCHED,
 MT76_JOIN5_RESPONSE_OWNER,MT76_JOIN5_RESPONSE_POLL,MT76_JOIN5_RESPONSE_SHAPE,
 MT76_JOIN5_RESPONSE_INVALID,MT76_JOIN5_RESPONSE_REJECTED,
 MT76_JOIN5_RESPONSE_ROLLBACK,MT76_JOIN5_RESPONSE_DEADLINE,
 MT76_JOIN5_RESPONSE_TURN_CAP,MT76_JOIN5_RESPONSE_DELAY
};
struct mt76_join5_response_observation {
 unsigned attempted,reason,polls,present,ignored,peer,clocks;
 uint64_t span_us;
};
struct mt76_join5 {
 bool attempted,complete,failed,unsafe,auth_sent,auth_matched,assoc_sent,assoc_matched,leave_sent;
 uint64_t generation;
 unsigned sends,polls,ignored;
 uint16_t aid,status;
 uint8_t reply[4096],packet[124];
 struct mt76_join5_response_observation response[2]; /* AUTH,ASSOC */
};
/* One-shot bounded fixed three-frame exchange,manual sequences1/2/3,no retry.
 * Caller owns calibrated RF/power/live endpoints/MAC and mandatory retirement.
 * Uncertain transport ends publication;no leave or replay from uncertain DMA.
 * Completed failure can send leave only under independently valid ownership.
 * All packet/address scratch is scrubbed;results contain counters only. */
bool mt76_join5_run(struct mt76_join5 *,const struct mt76_peer5 *,const struct mt76_join5_io *);
/* Optional trusted,bounded post-association inspection before leave. A false
 * result is a semantic failure unless unsafe is set or ownership is lost.
 * Existing run() supplies no hook and retains its original behavior. */
bool mt76_join5_run_after(struct mt76_join5 *,const struct mt76_peer5 *,
 const struct mt76_join5_io *,bool (*after_assoc)(void *,bool *unsafe));
#endif
