#ifndef SRVROS_MT76_EAPOL5_H
#define SRVROS_MT76_EAPOL5_H
#include <srvros/mt76_rx.h>
struct mt76_eapol5_peer {uint8_t station[6],bssid[6];unsigned channel,bandwidth;};
struct mt76_eapol5_view {
 const uint8_t *eapol;size_t eapol_size,mpdu_size,consumed;
 uint32_t rxinfo;uint16_t frame_control,sequence;uint8_t tid;bool qos;
};
/* Separate unprotected fixed161/20MHz pre-key EAPOL view;the old management
 * decoder remains unchanged. Exact FromDS RA/TA/SA binding,plain data or QoS
 * TID0..7/normal ACK (optional EOSP),hardware L2 pad and RFC1042 LLC/SNAP888e.
 * No encryption/decryption,fragmentation,aggregate,HT-control or PN processing.
 * Exact EAPOL1/2,type3,RSN descriptor2,M1/M3 flags/key length/body bounds only;
 * MIC/nonce/replay/RSNE/state validation remains with the handshake owner.
 * Borrowed spans expire with this immutable RX completion. No DMA publication,
 * key or controlled-port authority. Alias refusal leaves output unchanged;
 * other refusal clears it. Caller exclusively owns all supplied spans. */
enum mt76_rx_status mt76_eapol5_rx(const struct mt76_eapol5_peer *,
 const uint8_t *,size_t,struct mt76_eapol5_view *);
/* Construct only unprotected non-QoS ToDS M2/M4 from a complete private model
 * output. Does not prove MIC/freshness/send completion. Sequence1..4095,no
 * retry,OFDM6/ACK/WCIDff,no hardware crypto,existing USB/TXWI framing.
 * Output capacity1..4096 and disjoint from peer/EAPOL;success clears unused
 * capacity bytes. Return184(M2) or160(M4),0 on refusal. Alias/invalid span
 * refuses unchanged;other refusal clears capacity bytes. */
size_t mt76_eapol5_usb_tx(const struct mt76_eapol5_peer *,const uint8_t *,size_t,
 unsigned sequence,uint8_t *,size_t capacity);
#endif
