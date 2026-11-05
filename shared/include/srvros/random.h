#ifndef SRVROS_RANDOM_H
#define SRVROS_RANDOM_H

#include <srvros/syscall_numbers.h>

/* SYS_RANDOM flags0 retains its historical best-effort, non-cryptographic
 * fallback. This opt-in mode returns only bytes from the existing virtio-RNG
 * pool or successful RDRAND instructions, without waiting or software fallback.
 * A positive short result describes only the written prefix. If no provider
 * bytes are available, return SRV_RANDOM_UNAVAILABLE without writing anything.
 * Zero length succeeds without a provider. Invalid flags/ranges return -1.
 * Older kernels reject this flag with -1; callers must fail closed. */
#define SRV_RANDOM_REQUIRE_ENTROPY (1ULL << 0)
#define SRV_RANDOM_UNAVAILABLE SRV_ERR_AGAIN

#endif
