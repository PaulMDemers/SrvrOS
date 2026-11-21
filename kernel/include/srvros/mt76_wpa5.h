#ifndef SRVROS_MT76_WPA5_H
#define SRVROS_MT76_WPA5_H
#include <srvros/mt76_join5.h>
#include <srvros/wpa2_ccmp.h>
enum mt76_wpa5_tx {MT76_WPA5_AUTH,MT76_WPA5_ASSOC,MT76_WPA5_M2,MT76_WPA5_M4,MT76_WPA5_LEAVE};
enum mt76_wpa5_rx {MT76_WPA5_MANAGEMENT,MT76_WPA5_EAPOL,MT76_WPA5_DATA};
struct mt76_wpa5_io {
 void *context;
 bool (*owned)(void *,bool cleanup);
 /* Native implementation must use the strict RDRAND+CF nonce source. */
 bool (*nonce)(void *,uint8_t out[32]);
 enum mt76_join5_send (*send)(void *,enum mt76_wpa5_tx,const uint8_t *,size_t);
 /* Copied normalized management MPDU or exact peer-bound EAPOL,never DMA.
  * EAPOL admission requires the separate mt76_eapol5 decoder. */
 bool (*poll)(void *,enum mt76_wpa5_rx,uint8_t *,size_t,size_t *,bool *);
 bool (*delay)(void *,unsigned us);
 uint64_t (*now)(void *);
};
/* One exclusive,fully zero,static/heap object. Never copy/restart an owned
 * lifetime. No retries or key reinstall. Fresh AUTH/ASSOC/M2/M4/leave use
 * sequences4..8,distinct from the historical three-frame parent. A successful
 * run proves model M3 MIC and local M4 submission,then erases software keys
 * and submits leave. It sends no encrypted/IP data and proves no AP ACK.
 * Native endpoint/MAC/RF/DMA ownership and retirement remain external. */
/* Categorical post-run diagnostics only. Existing returned clock values are
 * reused; no extra I/O, clock read, retry, EAPOL acceptance or cleanup authority. */
enum mt76_wpa5_response_result {MT76_WPA5_RESPONSE_NONE,MT76_WPA5_RESPONSE_OK,
 MT76_WPA5_RESPONSE_TIMEOUT,MT76_WPA5_RESPONSE_CLOCK,MT76_WPA5_RESPONSE_OWNER,
 MT76_WPA5_RESPONSE_POLL,MT76_WPA5_RESPONSE_SPAN,MT76_WPA5_RESPONSE_DELAY,
 MT76_WPA5_RESPONSE_LIMIT,MT76_WPA5_RESPONSE_REJECTED};
struct mt76_wpa5_response {
 unsigned result,polls,present,ignored,kind,reject;
 uint64_t start,last;
};
struct mt76_wpa5 {
 bool attempted,complete,unsafe,auth_sent,auth_matched,assoc_sent,assoc_matched;
 bool m1_matched,m2_sent,m3_mic,keys_installed,m4_sent,keys_activated,keys_erased,leave_sent;
 uint64_t generation;unsigned sends,polls,ignored;uint16_t aid,status;
 struct wpa2_handshake handshake;struct wpa2_ccmp keys;
 struct mt76_wpa5_response responses[4];
 uint8_t reply[4096],packet[184],eapol[121],snonce[32];
 /* Active association. The original nonmoving owner retains these bindings
  * until terminal key erasure and leave. No PMK pointer is retained. */
 uintptr_t session_identity;
 bool session_active,session_busy;
 struct mt76_peer5 session_peer;
 struct mt76_wpa5_io session_io;
};
/* CPU0 owner only. Connect activates the original CCMP keys and returns
 * without sending leave. Invalid admission leaves the zero object alone;
 * an admitted handshake failure performs the same terminal cleanup as the
 * one-shot path. The copied peer and IO stay private to the owner. The IO
 * context must remain valid through disconnect, even on hardware failure. */
bool mt76_wpa5_connect(struct mt76_wpa5 *,const struct mt76_peer5 *,
 const uint8_t pmk[32],const struct mt76_wpa5_io *,bool defer_m1_duplicate);
/* The owner must first close all software key borrowers and DMA publication
 * callbacks. A matching disconnect consumes the association exactly once,
 * even when it returns false. REFUSED allows leave after erasure. UNCERTAIN
 * erases software keys but prohibits leave and hardware reuse. A mismatched
 * generation, moved object or reentry cannot consume the association. */
bool mt76_wpa5_disconnect(struct mt76_wpa5 *,uint64_t generation,enum mt76_join5_send);
/* Caller retains PMK ownership and scrubs its own temporary copy. Inputs and
 * IO/context cannot overlap output. Invalid/replayed admission is unchanged;
 * every admitted outcome scrubs local secret/packet scratch. Uncertain DMA
 * stops all publication,including leave,while software erasure still runs. */
bool mt76_wpa5_run(struct mt76_wpa5 *,const struct mt76_peer5 *,const uint8_t pmk[32],const struct mt76_wpa5_io *);
/* Explicit bounded extension only. Runs once after confirmed local M4 and
 * activation,while the original key/backend/peer/generation remain exclusive.
 * Callback must not copy/reinstall/retire keys or change the handshake. SENT
 * means its complete diagnostic succeeded,not an AP ACK. REFUSED still permits
 * normal erasure/leave;UNCERTAIN prohibits any further publication. The caller
 * supplies all transport/time/packet bounds and independent result receipts.
 * Historical mt76_wpa5_run never invokes a data callback. */
struct mt76_wpa5_data {
 void *context;
 enum mt76_join5_send (*run)(void *,struct wpa2_ccmp *,uint64_t,const struct mt76_peer5 *);
 /* The HTTP reconnect owner may skip one byte-equivalent accepted M1 during
  * the existing M3 wait. This never emits another M2 or changes key state. */
 bool defer_m1_duplicate;
};
bool mt76_wpa5_run_data(struct mt76_wpa5 *,const struct mt76_peer5 *,const uint8_t pmk[32],const struct mt76_wpa5_io *,const struct mt76_wpa5_data *);
#endif
