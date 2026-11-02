#ifndef SRVROS_MT76_RX_H
#define SRVROS_MT76_RX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum mt76_rx_status {
    MT76_RX_OK, MT76_RX_INVALID, MT76_RX_UNSUPPORTED, MT76_RX_CHANNEL_MISMATCH
};

/* One completed main-RX endpoint transfer, never the MCU response endpoint.
 * Views borrow immutable bytes only until the transport reuses its buffer.
 * consumed is the DMA header plus its declared entry; trailing USB bytes are
 * not interpreted as another entry. No FCE, FCS or RSSI calibration inference. */
struct mt76_rx_view {
    const uint8_t *header, *body;
    size_t body_size, mpdu_size, consumed;
    uint32_t rxinfo;
    uint16_t frame_control, rate, sequence;
    uint8_t rssi_raw[4];
};
enum mt76_rx_status mt76_rx_usb_view(const uint8_t *, size_t,
                                    struct mt76_rx_view *);

/* A copied passive observation. SSID is binary, not a C string. Security
 * fields report advertisements only; they do not validate suites or authorize
 * association. tuned_channel is supplied by the future transport's US plan,
 * not by the untrusted packet. Missing DS uses that supplied channel. */
struct mt76_rx_beacon {
    uint8_t bssid[6], transmitter[6], ssid[32], ssid_size, channel;
    uint16_t interval, capabilities, sequence;
    uint64_t timestamp;
    bool probe_response, hidden, ds_present, privacy, rsn_present, wpa_present;
    uint8_t rssi_raw[4];
};
enum mt76_rx_status mt76_rx_beacon(const uint8_t *, size_t,
                                  unsigned tuned_channel,
                                  struct mt76_rx_beacon *);
/* Anonymous diagnostic only: fully validate under the advertised US1-11
 * channel and return that scalar. Never licenses a mismatched receive frame. */
unsigned mt76_rx_advertised_channel(const uint8_t *,size_t);
#endif
