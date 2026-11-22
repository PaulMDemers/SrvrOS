#ifndef SRVROS_WPA2_HANDSHAKE_H
#define SRVROS_WPA2_HANDSHAKE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Pure one-shot PSK/CCMP model with no I/O. Exclusive owner serializes all
 * steps,filters unrelated traffic and enforces deadlines/transport retirement.
 * Every call carries a nonzero external lifetime generation. Stale-generation
 * and alias calls refuse unchanged;current-lifetime protocol errors fail and
 * erase secrets. No retries/rekey/reinstallation are admitted in this profile.
 * Start needs a fully zero object,fresh externally supplied SNonce and PMK.
 * CLOSED/FAILED objects cannot restart. Do not zero an owned object to reuse it.
 * No logging,hardware writes or controlled-port authority follows from this API. */
struct wpa2_hs_peer {uint8_t aa[6],spa[6],rsne[64];size_t rsne_size;};
enum wpa2_hs_phase {WPA2_HS_EMPTY,WPA2_HS_M1,WPA2_HS_M2_READY,WPA2_HS_M2_PENDING,
 WPA2_HS_M3,WPA2_HS_KEYS_READY,WPA2_HS_INSTALL_PENDING,WPA2_HS_M4_READY,
 WPA2_HS_M4_PENDING,WPA2_HS_COMPLETE,WPA2_HS_FAILED,WPA2_HS_CLOSED};
/* Last current-lifetime RX result only; no packet, nonce, replay or key bytes.
 * Observation has no admission/retirement authority; stale/alias calls leave it. */
enum wpa2_hs_rx_result {WPA2_HS_RX_NONE,WPA2_HS_RX_ACCEPTED,WPA2_HS_RX_ENVELOPE,
 WPA2_HS_RX_M1_FIELDS,WPA2_HS_RX_PTK,WPA2_HS_RX_PHASE,WPA2_HS_RX_VERSION,
 WPA2_HS_RX_KEY_INFO,WPA2_HS_RX_REPLAY,WPA2_HS_RX_NONCE,WPA2_HS_RX_RESERVED,
 WPA2_HS_RX_MIC,WPA2_HS_RX_UNWRAP,WPA2_HS_RX_KEYDATA};
enum wpa2_hs_result {WPA2_HS_CONFIRMED,WPA2_HS_REFUSED,WPA2_HS_UNCERTAIN};
struct wpa2_hs_keys {uint8_t pairwise[16],group[16],group_rsc[6],group_id;uint64_t generation;};
struct wpa2_handshake {
 enum wpa2_hs_phase phase;enum wpa2_hs_rx_result last_rx;uint64_t generation,replay;uintptr_t key_owner;
 struct wpa2_hs_peer peer;unsigned emissions,install_handoffs;
 bool keys_may_live,unsafe;uint8_t version,group_id,group_rsc[6];
 uint8_t pmk[32],snonce[32],anonce[32],ptk[48],gtk[16],scratch[1016];
};
bool wpa2_hs_start(struct wpa2_handshake *,const struct wpa2_hs_peer *,
 const uint8_t pmk[32],const uint8_t snonce[32],uint64_t generation);
/* Whole EAPOL only;the independently validated outer transport supplies exact
 * AA/SPA. M1->M2_READY;MIC+unwrap+selected-RSNE/GTK-validated M3->KEYS_READY.
 * No install handoff before all M3 checks pass. Max1123 bytes. */
bool wpa2_hs_receive(struct wpa2_handshake *,uint64_t generation,
 const uint8_t source[6],const uint8_t destination[6],const uint8_t *,size_t);
/* M2(121bytes) or M4(99bytes),immutable input/state secret spans never alias
 * output. One emission per phase,then exactly one matching completion receipt.
 * Capacity1..121;unsupported/aliased spans refuse unchanged. Other invalid
 * current-lifetime calls clear capacity bytes and terminate. */
size_t wpa2_hs_emit(struct wpa2_handshake *,uint64_t generation,uint8_t *,size_t);
bool wpa2_hs_tx_result(struct wpa2_handshake *,uint64_t generation,enum wpa2_hs_result);
/* One export,then one installation receipt. Exported keys remain owned by the
 * external installer and must be scrubbed there. Export conservatively marks
 * keys possibly live even if the installer later reports refusal. No hardware
 * receipt is fabricated here;the future owner must verify full install/readback.
 * Only confirmed install permits M4;only confirmed M4 TX permits COMPLETE.
 * COMPLETE is local model state,not AP ACK,controlled port or native acceptance. */
bool wpa2_hs_take_keys(struct wpa2_handshake *,uint64_t generation,struct wpa2_hs_keys *);
/* One backend claim after key export;opaque nonzero owner token persists
 * through retirement. Active handshake objects must never be copied/reused.
 * A confirmed install receipt requires the claim;claiming twice refuses. */
bool wpa2_hs_claim_keys(struct wpa2_handshake *,uint64_t generation,uintptr_t owner);
bool wpa2_hs_install_result(struct wpa2_handshake *,uint64_t generation,enum wpa2_hs_result);
/* True only when this call aborts a current active lifetime. */
bool wpa2_hs_abort(struct wpa2_handshake *,uint64_t generation);
/* Scrub secrets on every current-lifetime retirement;return false and preserve
 * keys_may_live if external keys lack a removal proof. Caller must retain that
 * hardware ownership even though this local object is scrubbed. */
bool wpa2_hs_retire(struct wpa2_handshake *,uint64_t generation,bool keys_removed);
#endif
