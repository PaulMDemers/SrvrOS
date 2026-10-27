#ifndef SRVROS_MT76_DISCOVERY_H
#define SRVROS_MT76_DISCOVERY_H
#include <srvros/mt76_rx_window.h>
#define MT76_DISCOVERY_ROWS MT76_RX_WINDOW_SLOTS
/* Private, copied advertisements, never credentials or authenticated security.
 * Binary SSIDs must be escaped by a future presentation layer. No borrowed
 * DMA pointers, raw signal readings, transmitter address or payload survives. */
struct mt76_discovery_row {
    uint8_t bssid[6], ssid[32], ssid_size, channel;
    uint16_t interval;
    unsigned observations;
    bool hidden, privacy, rsn, wpa, ds_present;
    bool beacon_seen, probe_response_seen, ssid_conflict, security_conflict;
};
struct mt76_discovery {
    bool attempted, complete, failed;
    unsigned inputs, empty, parsed, unrelated, rejected, count;
    struct mt76_discovery_row row[MT76_DISCOVERY_ROWS];
};
struct mt76_discovery_io {
    void *context;
    /* Must prove the exact window/plan/ring/pages remain stable and retained,
     * parent MAC cleanup, exact endpoint stop and all six guards are current.
     * Serialized with callbacks; may not reenter, mutate or free input/state. */
    bool (*owned)(void *, const struct mt76_rx_window *);
};
/* One-shot bounded collection after successful window_finish only. Fixed US
 * channel1; first entry of each normal-completed transfer only. All-or-nothing
 * publication: ownership/ledger failure leaves no identifiers or result rows.
 * Valid no-AP observations complete with count0; not AP discovery acceptance.
 * Does not poll, post, stop, allocate, retune, transmit or log identifiers. */
bool mt76_discovery_collect(struct mt76_discovery *,
                            const struct mt76_rx_window *,
                            const struct mt76_discovery_io *);
#endif
