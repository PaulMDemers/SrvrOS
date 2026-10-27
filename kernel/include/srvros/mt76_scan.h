#ifndef SRVROS_MT76_SCAN_H
#define SRVROS_MT76_SCAN_H
#include <srvros/mt76_channel_tune.h>
#include <srvros/mt76_discovery.h>
#define MT76_SCAN_VISITS 3
/* Fixed US1/6/11,20MHz pilot. Transport owns each independent tune/receive/
 * stop/restore transaction and retains all DMA. No buffer/ring may be reused. */
struct mt76_scan_input {
    unsigned channel;
    struct mt76_channel_tune tune;
    uint64_t ring,physical,elapsed_us;
    const uint8_t *buffer;
    unsigned events,completion,remaining,actual;
    bool complete,stopped,quiescent,retained,during_window;
};
struct mt76_scan_row {
    struct mt76_discovery_row advertisement;
    uint16_t channels;
    bool channel_conflict;
};
struct mt76_scan {
    bool attempted,complete,failed;
    unsigned inputs,unobserved,empty,parsed,unrelated,rejected,count;
    struct mt76_scan_row row[MT76_SCAN_VISITS];
};
struct mt76_scan_io {
    void *context;
    /* Serialized, non-reentrant: prove all snapshots and immutable retained
     * buffers/rings, exact stops, last live context and channel1 restoration.
     * This callback must not mutate input/state. No hardware work is done here. */
    bool (*owned)(void *,const struct mt76_scan_input *,unsigned);
};
unsigned mt76_scan_channel(unsigned index);
/* One-shot publication only after all three visits. Reparse each first USB
 * entry with its explicit tuned channel. Keep only copied advertisements;
 * no borrowed pointers/transmitter/raw signal survives. Ownership/ledger
 * failure scrubs all rows. Zero APs can complete, never accepts reception.
 * BSSID deduplication is private; cross-channel/name/security conflicts are
 * retained as flags, not silently treated as association-ready candidates. */
bool mt76_scan_collect(struct mt76_scan *,const uint8_t country[2],
    const struct mt76_scan_input *,unsigned,const struct mt76_scan_io *);
#endif
