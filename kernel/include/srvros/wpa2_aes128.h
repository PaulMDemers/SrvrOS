#ifndef SRVROS_WPA2_AES128_H
#define SRVROS_WPA2_AES128_H
#include <stdint.h>
/* Private fixed-size AES128 core for RFC3394/CCMP. Trusted internal callers
 * supply valid exclusively owned spans;round keys and block must be disjoint.
 * Block operations are in-place. Caller scrubs round keys after use.
 * Fixed-work arithmetic,no secret-indexed tables;not a formal timing audit. */
void wpa2_aes128_expand(const uint8_t key[16],uint8_t rounds[176]);
void wpa2_aes128_encrypt(const uint8_t rounds[176],uint8_t block[16]);
void wpa2_aes128_decrypt(const uint8_t rounds[176],uint8_t block[16]);
#endif
