#ifndef SRVROS_VIRTIO_RNG_H
#define SRVROS_VIRTIO_RNG_H
#include <stdbool.h>
#include <stdint.h>
/* VM-gated initialization and bounded polling require kernel CR3, CPU0 IRQ
 * exclusion. Consumption only touches a private pool, and is safe from any CR3
 * with IRQs excluded. Exhaustion is nonblocking; callers retain their fallback. */
bool virtio_rng_init(void);
void virtio_rng_poll(void);
bool virtio_rng_ready(void);
void virtio_rng_stop(void);
bool virtio_rng_take_u64(uint64_t *value);
void virtio_rng_print_status(void);
#endif
