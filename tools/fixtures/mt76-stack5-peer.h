#ifndef SRVROS_TEST_MT76_STACK5_PEER_H
#define SRVROS_TEST_MT76_STACK5_PEER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host-fixture AP only. Uses the public synthetic handshake TK/GTK through
 * OpenSSL EVP AES-CCM, never the production crypto implementation. No native
 * credentials, radio/controller ownership, replay state or PN allocation.
 * The caller tracks independent monotonically increasing TX/RX PNs, including
 * the public GTK's initial RSC. Input and output spans must be disjoint.
 * Failure sets available length/PN outputs to zero and leaves data output alone.
 */
bool stack5_peer_open(const uint8_t *usb, size_t usb_n,
                      uint8_t *eth, size_t cap, size_t *eth_n, uint64_t *pn);

/* Ethernet -> protected non-QoS FromDS MPDU, without USB RX metadata/FCS.
 * Unicast destination must be the public station; group uses GTK/key index 1.
 * IPv4/ARP only, matching the bounded production adapter's supported envelope.
 */
bool stack5_peer_seal(const uint8_t *eth, size_t n, uint64_t pn,
                      uint8_t *mpdu, size_t cap, size_t *mpdu_n);

#endif
