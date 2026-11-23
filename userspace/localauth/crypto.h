#ifndef SRVROS_LOCAL_AUTH_CRYPTO_H
#define SRVROS_LOCAL_AUTH_CRYPTO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Argon2id v19: 32768 KiB, three passes, one lane, 16-byte salt.
 * On failure the output is cleared. The caller must wipe its own secret. */
bool local_auth_verifier(const char *secret,size_t length,const uint8_t salt[16],uint8_t out[32]);
void local_auth_wipe(void *data,size_t length);
bool local_auth_equal(const uint8_t a[32],const uint8_t b[32]);
#endif
