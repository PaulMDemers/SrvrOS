#ifndef SRVROS_VIRTIO_INPUT_H
#define SRVROS_VIRTIO_INPUT_H
#include <stdbool.h>
#include <stdint.h>

struct virtio_input_stats {
    uint32_t devices, keyboards, mice, failed, quarantined;
    uint64_t events, reports;
};
/* VM-gated caller only. Init has one boot attempt, and accepts QEMU modern
 * keyboard/relative-mouse profiles, never tablet/host passthrough. All mutating
 * calls require kernel CR3 and external serialization. Poll drains at most 64
 * events per device (four devices), commits only complete SYN_REPORT batches,
 * and advances one software key repeat per device without catch-up bursts.
 * A lost/invalid/stalled report retires that source; there is no trustworthy
 * pressed-state query for recovery. Reset must succeed before event DMA free.
 * Status queue is configured empty; LED feedback is not implemented. */
bool virtio_input_init(void);
void virtio_input_poll(void);
bool virtio_input_ready(void);
bool virtio_input_stop(void);
void virtio_input_get_stats(struct virtio_input_stats *out);
#endif
