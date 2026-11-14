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
struct mt76_wpa5 {
 bool attempted,complete,unsafe,auth_sent,auth_matched,assoc_sent,assoc_matched;
 bool m1_matched,m2_sent,m3_mic,keys_installed,m4_sent,keys_activated,keys_erased,leave_sent;
 uint64_t generation;unsigned sends,polls,ignored;uint16_t aid,status;
 struct wpa2_handshake handshake;struct wpa2_ccmp keys;
 uint8_t reply[4096],packet[184],eapol[121],snonce[32];
};
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
};
bool mt76_wpa5_run_data(struct mt76_wpa5 *,const struct mt76_peer5 *,const uint8_t pmk[32],const struct mt76_wpa5_io *,const struct mt76_wpa5_data *);
#endif
