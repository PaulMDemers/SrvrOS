#ifndef SRVROS_E1000_H
#define SRVROS_E1000_H

#include <stdbool.h>
#include <stdint.h>

/* Boot-owner initialization is attempted once; repeated calls return the
 * recorded result without replacing DMA rings or retrying partial setup. */
bool e1000_init(void);
bool e1000_available(void);
/* Read-only bounded software/descriptor observation for explicit stack tests.
 * Selected-provider identity and ring addresses are stable boot ownership only;
 * all submitted TX DD/no active RX dispatch/no current RX DD is a sampled drain,
 * never controller halt, DMA retirement, memory release or native Wi-Fi proof. */
struct e1000_probe_info {
    uint64_t rx_ring,tx_ring,rx_packets,tx_packets,interrupts,rx_interrupts;
    uint64_t tx_pending,rx_ready,rx_dispatching,selected,irqs_enabled;
    uint8_t mac[6];
};
bool e1000_probe_sample(struct e1000_probe_info *out);
struct e1000_link_info { bool up,full_duplex;uint16_t speed_mbps; };
struct e1000_link_info e1000_decode_link_status(uint32_t status);
bool e1000_link_info(struct e1000_link_info *info);
const uint8_t *e1000_mac(void);
uint8_t e1000_irq_line(void);
bool e1000_enable_interrupts(void);
bool e1000_handle_interrupt(void);
void e1000_print_status(void);
uint64_t e1000_poll(uint64_t max_frames, bool verbose);
bool e1000_send_frame(const uint8_t *frame, uint16_t length);
bool e1000_send_test_frame(void);

#endif
