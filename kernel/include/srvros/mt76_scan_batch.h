#ifndef SRVROS_MT76_SCAN_BATCH_H
#define SRVROS_MT76_SCAN_BATCH_H
#include <srvros/mt76_scan.h>
#define MT76_SCAN_BATCH_ROWS (MT76_SCAN_VISITS*MT76_RX_WINDOW_SLOTS)
/* One fixed US1/6/11 excursion each, with up to eight sequential INs under
 * one original10s deadline per visit. Each ring and all24 pages stay retained;
 * no buffer is reused within/between visits. The adapter must reject/drain
 * pre-enable data separately before publishing these enabled-window results. */
struct mt76_scan_batch_visit {
    struct mt76_channel_tune tune;
    struct mt76_rx_window window;
};
struct mt76_scan_batch {
    bool attempted,complete,failed;
    unsigned visits,inputs,pending,empty,parsed,unrelated,rejected,count;
    struct mt76_scan_row row[MT76_SCAN_BATCH_ROWS];
};
struct mt76_scan_batch_io {
    void *context;
    /* Prove ALL three immutable snapshots, exact channel tuning and original
     * channel1 restoration, stopped contexts, parent MAC quiescence and every
     * retained DMA identity on every call. Serialized/non-reentrant; never
     * mutate inputs, post, retune, or free DMA. This helper does no device I/O. */
    bool (*owned)(void *,const struct mt76_scan_batch_visit *,unsigned);
};
/* One-shot, all-or-nothing private table, after every window and excursion has
 * stopped/restored. Reparse first USB entry per normal completed transfer with
 * its explicit tuned channel, verify saved metadata, then deduplicate copied
 * advertisements. Hidden/name/security/cross-channel conflicts survive. Failed
 * proof publishes no identifiers. Quiet success is not reception acceptance.
 * Native adapter integration and FCS/security/IP are separate milestones. */
bool mt76_scan_batch_collect(struct mt76_scan_batch *,
    const struct mt76_scan_batch_visit *,unsigned,const struct mt76_scan_batch_io *);
#endif
