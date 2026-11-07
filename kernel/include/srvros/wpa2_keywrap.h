#ifndef SRVROS_WPA2_KEYWRAP_H
#define SRVROS_WPA2_KEYWRAP_H
#include <stddef.h>
#include <stdint.h>
/* AES128 RFC3394 default-IV unwrap only. Cipher24..1024,multiple8;plaintext
 * size=cipher_size-8. Output capacity1..1016,disjoint from KEK/cipher. Caller
 * exclusively owns spans. Alias/invalid capacity refuses unchanged;other
 * refusals clear capacity bytes. Return0 on integrity/shape failure. No table
 * indexed by secrets;fixed-work field arithmetic. No key installation/logging.
 * EAPOL MIC MUST be authenticated separately before calling this primitive. */
size_t wpa2_key_unwrap(const uint8_t kek[16],const uint8_t *cipher,size_t size,
 uint8_t *out,size_t capacity);
#endif
