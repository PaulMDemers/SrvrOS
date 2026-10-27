#ifndef SRVROS_MT76_RX_WINDOW_H
#define SRVROS_MT76_RX_WINDOW_H
#include <srvros/mt76_rx.h>
#define MT76_RX_WINDOW_SLOTS 8u
#define MT76_RX_WINDOW_BYTES 4096u
#define MT76_RX_WINDOW_US 1000000u
/* Caller preallocates stable, distinct DMA pages and a ring with eight normal
 * TRBs at ring+16*i, before its link TRB. No wrapping, Set Dequeue, allocation,
 * DMA release or reuse occurs here. Native ownership/guards remain the caller's
 * responsibility; this helper is not a native endpoint adapter. */
struct mt76_rx_window_plan {
    uint64_t ring, physical[MT76_RX_WINDOW_SLOTS];
    const uint8_t *buffer[MT76_RX_WINDOW_SLOTS];
    unsigned slot, dci;
};
struct mt76_rx_window_event {
    uint64_t trb;
    unsigned slot, dci, completion, remaining;
    bool event_data;
};
struct mt76_rx_window_stop {
    uint64_t dequeue;
    unsigned slot, dci, state;
};
struct mt76_rx_window_io {
    void *context;
    /* Forward ownership includes the accepted active MAC/calibration context.
     * Cleanup ownership includes MAC disable, guards and retained DMA identity.
     * The stop callback must drain/disable MAC and stop the endpoint, even
     * after forward proof fails. Every callback is bounded and serialized;
     * no callback may release/repurpose retained DMA or reenter this state. */
    bool (*owned)(void *, bool cleanup);
    bool (*post)(void *, unsigned index, uint64_t trb, uint64_t physical);
    /* The adapter must continue dispatching shared controller events; report
     * a candidate RX Transfer Event without swallowing another client event. */
    bool (*poll)(void *, struct mt76_rx_window_event *, bool *present);
    bool (*stop)(void *, struct mt76_rx_window_stop *);
    uint64_t (*now)(void *);
};
struct mt76_rx_window_item {
    bool submitted, completed, inspected, beacon;
    unsigned completion, remaining, actual, consumed, mpdu;
    unsigned view_status, beacon_status;
    uint16_t frame_control, interval;
    uint8_t ssid_size;
    bool privacy, rsn, wpa, ds_present, probe_response;
};
struct mt76_rx_window {
    bool attempted, polled, poll_ok, finish_attempted, complete, failed, stop_attempted, stopped;
    struct mt76_rx_window_plan plan;
    unsigned attempts, completions, ignored, polls, beacons;
    uint64_t elapsed_us;
    struct mt76_rx_window_item item[MT76_RX_WINDOW_SLOTS];
};
/* Fixed US channel1 observation. A completed window can contain zero beacons.
 * Only normal CC1/13 supplies actual bytes. Output never retains identifiers,
 * payload pointers, borrowed views or raw RSSI. Every buffer remains retained,
 * including after callback failure or an unproven stop. State is one-shot. */
/* Split adapter: poll inside the active MAC hook; finish only after parent
 * cleanup and exact endpoint stop. NULL cessation proof refuses inspection.
 * No new submissions may occur between poll and finish; late completions do
 * not retroactively supply bytes to the saved observation. Both are one-shot. */
bool mt76_rx_window_poll(struct mt76_rx_window *,
                        const struct mt76_rx_window_plan *,
                        const struct mt76_rx_window_io *);
bool mt76_rx_window_finish(struct mt76_rx_window *,
                          const struct mt76_rx_window_io *,
                          const struct mt76_rx_window_stop *);
bool mt76_rx_window_run(struct mt76_rx_window *,
                        const struct mt76_rx_window_plan *,
                        const struct mt76_rx_window_io *);
#endif
