#ifndef SRVROS_WPA2_CRYPTO_H
#define SRVROS_WPA2_CRYPTO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Private,allocation-free WPA2-PSK/CCMP primitives;not a handshake owner.
 * No logging,entropy,hardware or controlled-port authority. All inputs remain
 * immutable and exclusively owned until return. Output is disjoint from every
 * input;alias refusal leaves memory unchanged,other refusals clear output.
 * Callers must scrub successful outputs after their sole lifetime.
 * Printable ASCII passphrase8..63 and binary SSID1..32;no raw hex-PSK parsing.
 * Fixed PBKDF2-HMAC-SHA1/4096,32-byte PMK. */
bool wpa2_psk_pmk(const uint8_t *pass,size_t pass_size,
 const uint8_t *ssid,size_t ssid_size,uint8_t pmk[32]);
/* Ordered AA/SPA and ANonce/SNonce,PRF-SHA1 label including NUL,counter0.
 * 48-byte CCMP PTK = KCK16 || KEK16 || TK16. Distinct unicast addresses and
 * distinct nonzero/non-ones nonces required;fresh nonce authority is external.
 * Derivation alone does not validate a peer or install a key. */
bool wpa2_ccmp_ptk(const uint8_t pmk[32],const uint8_t aa[6],
 const uint8_t spa[6],const uint8_t anonce[32],const uint8_t snonce[32],uint8_t ptk[48]);
/* EAPOL v1/v2,type3,RSN descriptor2,descriptor version2 with MIC bit.
 * Exact4+95+key-data framing,at most1024 key-data bytes. HMAC-SHA1 first16
 * bytes with the frame MIC logically zeroed;frame is never modified.
 * Other flags,key length,peer,replay,nonce,RSNE and state MUST be validated
 * separately. Verify is constant-work for well-framed MIC comparisons.
 * A valid MIC alone does not authorize key installation or a controlled port. */
bool wpa2_eapol_mic(const uint8_t kck[16],const uint8_t *frame,size_t size,uint8_t mic[16]);
bool wpa2_eapol_mic_verify(const uint8_t kck[16],const uint8_t *frame,size_t size);
#endif
