#ifndef SRVROS_WPA2_NONCE_H
#define SRVROS_WPA2_NONCE_H
#include <stdbool.h>
#include <stdint.h>
/* Private handshake nonce source,not the general getrandom fallback API.
 * x86 RDRAND+CF only,ten bounded attempts per word;no timer/software fallback.
 * Zero/ones/repeated words,repeated blocks,exhaustion or changed state fail
 * closed. Unsupported/faulted source is unavailable;no nonce/key logging.
 * Caller provides32 distinct writable bytes. No association/key authority. */
bool wpa2_nonce_generate(uint8_t out[32]);
#endif
