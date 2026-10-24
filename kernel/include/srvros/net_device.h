#ifndef SRVROS_NET_DEVICE_H
#define SRVROS_NET_DEVICE_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/syscall_numbers.h>
#define NET_DEVICE_FRAME_MAX 1514
struct net_device_link { bool up, full_duplex, duplex_known;uint16_t speed_mbps; };
/* Register before net_init. The selected provider is immutable for this boot.
 * All callbacks may run with interrupts disabled and must not block on another
 * thread. send must copy/queue the frame before returning; the caller retains
 * ownership. poll delivers complete frames through net_handle_ethernet_frame and must
 * tolerate a send callback during delivery. USB providers must use asynchronous
 * DMA, never their synchronous control/storage transaction path here. */
struct net_device_ops {
    const char *name;
    void *context;
    uint8_t mac[6];
    unsigned priority;
    bool (*send)(void *context,const uint8_t *frame,uint16_t length);
    uint64_t (*poll)(void *context,uint64_t budget,bool verbose);
    bool (*link)(void *context,struct net_device_link *out);
    /* Optional, side-effect-free live readiness. True means an independent IRQ
     * or service calls net_notify_rx for receive work; TX servicing must also
     * progress without net_worker polling. False/absent keeps one-tick polling.
     * A bounded recovery poll remains even when notifications are available. */
    bool (*rx_wake_ready)(void *context);
};
bool net_device_register(const struct net_device_ops *ops);
bool net_device_select(void);
/* Read-only enumeration; never polls hardware or delivers receive frames. */
unsigned net_device_inventory(struct srv_net_adapter_info *out,unsigned capacity);
const char *net_device_name(void);
const uint8_t *net_device_mac(void);
bool net_device_send(const uint8_t *frame,uint16_t length);
uint64_t net_device_poll(uint64_t budget,bool verbose);
bool net_device_link(struct net_device_link *out);
bool net_device_rx_wake_ready(void);
#endif
