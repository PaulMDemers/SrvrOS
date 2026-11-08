#ifndef SRVROS_WPA2_CCMP_H
#define SRVROS_WPA2_CCMP_H
#include <srvros/wpa2_handshake.h>
#define WPA2_CCMP_DATA_MAX 2304u
#define WPA2_CCMP_PN_MAX UINT64_C(0x0000ffffffffffff)
enum wpa2_ccmp_phase {WPA2_CCMP_EMPTY,WPA2_CCMP_INSTALLED,WPA2_CCMP_ACTIVE,WPA2_CCMP_CLOSED};
/* Exclusive private software-key owner,not a transport or hardware installer.
 * Fully zero object before first install;never restart an owned/closed object.
 * Calls/spans are exclusively owned and serialized externally. No callbacks,
 * logging,hardware key writes,entropy or general controlled-port authority.
 * Native owner must freeze peer/generation,retain DMA on uncertainty and never
 * roll back a reserved TX PN. Do not place this scratch-bearing object on a
 * small kernel stack. Pairwise/group rekey and fragmented/A-MSDU/A4/HT/PMF
 * frames are unsupported. Group decryption is software-only for MT76x0. */
struct wpa2_ccmp {
 enum wpa2_ccmp_phase phase;uint64_t generation,tx_pn,rx_pairwise[17],rx_group[17];
 struct wpa2_hs_peer peer;uint8_t tk[16],gtk[16],group_id;
 uint8_t scratch[WPA2_CCMP_DATA_MAX];
};
/* Bind exactly one validated handshake key export in INSTALL_PENDING.
 * Caller scrubs its export separately and sends the resulting installation
 * receipt to the handshake. No data operation before confirmed M4/activate. */
bool wpa2_ccmp_install(struct wpa2_ccmp *,struct wpa2_handshake *,const struct wpa2_hs_keys *);
bool wpa2_ccmp_activate(struct wpa2_ccmp *,const struct wpa2_handshake *);
/* One unprotected ToDS non-QoS header24,exact RA/TA binding;DA in addr3 may be
 * unicast or multicast. Produces Protected MPDU with CCMP header+MIC8.
 * PN reserved once before sealing,including packets never published afterward.
 * Sequence supplied by caller,fragment0,no retry. Payload1..2304.
 * Output capacity1..4096 disjoint from context/header/payload;alias/stale calls
 * refuse unchanged,other refusals clear output. No TX completion is inferred. */
size_t wpa2_ccmp_seal(struct wpa2_ccmp *,uint64_t generation,const uint8_t header[24],
 const uint8_t *plain,size_t size,uint8_t *out,size_t capacity);
/* Whole contiguous Protected FromDS MPDU (hardware padding already removed).
 * Non-QoS or QoS TID0..7/normal ACK/optional EOSP;Retry/PM/MoreData are masked
 * in AAD per CCMP. Exact TA and unicast RA or group-key multicast RA;valid
 * unicast addr3. Key ID,ExtIV/reserved fields and per-key/per-TID PN checked.
 * No plaintext published and no RX PN advanced until MIC passes. Duplicate,
 * corrupt,unsupported and short-capacity frames drop without changing key/PN
 * state. Returned payload excludes802.11/CCMP headers and MIC.
 * Same output/alias/stale rules as seal;capacity1..2304. */
size_t wpa2_ccmp_open(struct wpa2_ccmp *,uint64_t generation,const uint8_t *mpdu,
 size_t size,uint8_t *plain,size_t capacity);
/* Same strict receive parser,peer/key-ID/replay/MIC checks,verification only.
 * Exclusive live owner,zero idle scratch required;no key copy/reinstall,
 * plaintext output or PN commit. Entire context unchanged after return.
 * Caller freezes ownership around this diagnostic;true is not publication. */
bool wpa2_ccmp_verify(struct wpa2_ccmp *,uint64_t generation,const uint8_t *mpdu,size_t size);
bool wpa2_ccmp_retire(struct wpa2_ccmp *,uint64_t generation);
bool wpa2_ccmp_erased(const struct wpa2_ccmp *,uint64_t generation);
#endif
