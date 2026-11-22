#ifndef SRVROS_TEST_MT76_STACK5_PEER_H
#define SRVROS_TEST_MT76_STACK5_PEER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Test-only synthetic association selector. The default is generation1, exactly
 * matching the historical public nonce/M3/TK bytes. Generation2 uses a different
 * deterministic SNonce and independently derives a new PTK with OpenSSL HMAC;
 * M3 carries the same AP GTK rewrapped/MICed under that association's KEK/KCK.
 * Invalid generations refuse without changing current state. No production key,
 * epoch, replay, radio, ownership or PN authority is provided by this fixture. */
bool stack5_peer_generation(unsigned generation);
bool stack5_peer_nonce(uint8_t out[32]);
bool stack5_peer_message3(uint8_t *out, size_t cap, size_t *size);

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
