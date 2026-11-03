#ifndef SRVROS_VIRTIO_NET_H
#define SRVROS_VIRTIO_NET_H
#include <stdbool.h>
#include <stdint.h>
/* Invoke only for the explicitly enabled VM profile, before net_init(). */
bool virtio_net_init(void);
/* Runtime send and poll accept arbitrary CR3 and preserve IRQ state. */
bool virtio_net_available(void);
bool virtio_net_send_frame(const uint8_t *frame, uint16_t length);
uint64_t virtio_net_poll(uint64_t budget, bool verbose);
void virtio_net_print_status(void);
#endif
