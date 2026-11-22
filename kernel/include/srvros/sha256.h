#ifndef SRVROS_SHA256_H
#define SRVROS_SHA256_H
#include <stddef.h>
#include <stdint.h>
/* Callers bound input size and retain immutable input during calculation. */
void sha256_digest(const uint8_t *data, size_t size, uint8_t out[32]);
#endif
