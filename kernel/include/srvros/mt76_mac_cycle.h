#ifndef SRVROS_MT76_MAC_CYCLE_H
#define SRVROS_MT76_MAC_CYCLE_H
#include <stdbool.h>
#include <stdint.h>
struct mt76_mac_cycle_io {
    void *context;
    bool (*owned)(void *, bool cleanup);
    bool (*read)(void *, uint32_t, uint32_t *, bool cleanup);
    bool (*write)(void *, uint32_t, uint32_t, bool cleanup);
    bool (*delay)(void *, unsigned, bool cleanup);
    uint64_t (*now)(void *);
};
struct mt76_mac_cycle {
    bool attempted, complete, failed, mac_touched, filter_touched, txop_touched;
    bool started, snapshot_complete, mac_disabled, tx_drained, rx_drained;
    bool filter_restored, txop_restored;
    unsigned operations, reads, writes, waits, tx_rounds, rx_rounds;
    uint32_t filter, txop, tx_alc, ibi9;
    uint64_t hold_us;
};
/* Captured MT7610 channel1 profile. RX transport ownership is supplied by the
 * caller and must survive every MAC transition and cleanup. No calibration,
 * power programming, transmission submission or packet inspection. IBI9 is a
 * raw observation only; all-ones there is not calibration admission. */
bool mt76_mac_cycle_run(struct mt76_mac_cycle *, const struct mt76_mac_cycle_io *);
#endif
