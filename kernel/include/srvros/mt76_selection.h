#ifndef SRVROS_MT76_SELECTION_H
#define SRVROS_MT76_SELECTION_H
#include <srvros/mt76_scan_batch.h>
/* Private, serialized owner state. Zero-initialize once; never reset/reuse its
 * generation during the owner's lifetime. No DMA pointers or credentials. */
struct mt76_selection {
    uint64_t generation;
    bool complete;
    unsigned count;
    struct mt76_scan_row row[MT76_SCAN_BATCH_ROWS];
};
struct mt76_selection_io {
    void *context;
    /* Prove this exact completed batch remains immutable and owned, after all
     * native stops/restorations. No reentry, mutation or concurrent calls. */
    bool (*owned)(void *,const struct mt76_scan_batch *);
};
struct mt76_selection_choice {
    uint64_t generation;
    unsigned index;
    uint8_t country[2],bandwidth;
    /* Advertisement only: RSN/WPA presence is NOT validated cipher/AKM support
     * or authentication. A later association parser must validate full IEs. */
    struct mt76_scan_row row;
};
enum mt76_selection_status {
    MT76_SELECTION_OK, MT76_SELECTION_INVALID, MT76_SELECTION_UNAVAILABLE,
    MT76_SELECTION_STALE, MT76_SELECTION_HIDDEN, MT76_SELECTION_CONFLICT
};
/* Every refresh attempt invalidates old choices, including a failed refresh.
 * UINT64_MAX permanently exhausts the owner; generation never wraps/repeats.
 * Failed refresh publishes zero rows. Input/output storage must not overlap. */
bool mt76_selection_invalidate(struct mt76_selection *);
bool mt76_selection_publish(struct mt76_selection *,const struct mt76_scan_batch *,
                             const struct mt76_selection_io *);
/* Explicit user choice only, no automatic strongest/first/open-network fallback.
 * A rejected choice is scrubbed, except overlapping owner storage is untouched.
 * Success only copies a candidate; it does not tune, transmit or join. */
enum mt76_selection_status mt76_selection_choose(const struct mt76_selection *,
    uint64_t generation,unsigned index,struct mt76_selection_choice *);
/* Binary SSID display, not a C-string input. Printable ASCII except backslash
 * is literal; other bytes become \xNN. Requires space for the entire result
 * and terminator (at most129 bytes). Failure emits an empty string except on overlap.
 * Caller owns non-overlapping storage; this is not HTML or shell escaping. */
bool mt76_selection_name(const uint8_t *,unsigned,char *,size_t);
#endif
