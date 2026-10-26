#ifndef SRVROS_MT76_POWERON_CALIBRATION_H
#define SRVROS_MT76_POWERON_CALIBRATION_H
#include <srvros/mt76_calibration.h>
#include <srvros/mt76_bringup.h>

/* Semantic operations, not wire IDs. A native adapter must encode and validate
 * bounded, sequence-matched replies. No native caller exists yet. */
enum mt76_poweron_command {
    MT76_POWERON_R, MT76_POWERON_VCO, MT76_POWERON_FULL,
    MT76_POWERON_LC, MT76_POWERON_RXDCOC
};
struct mt76_poweron_io {
    void *context;
    /* Every forward callback requires exclusive live MAC/RX, validated firmware,
     * same-lifetime fresh mapped board fields, channel/power setup and no
     * cancellation. Struct fields alone cannot prove freshness or ownership. */
    bool (*authorize)(void *);
    bool (*read)(void *, uint32_t address, uint32_t *value);
    bool (*write)(void *, uint32_t address, uint32_t value);
    bool (*command)(void *, enum mt76_poweron_command, uint32_t argument);
    bool (*delay)(void *, unsigned minimum_us);
    /* Independent cleanup admission; write exactly the supplied saved word and
     * read it back. Must refuse lost/quarantined transport ownership. */
    bool (*restore)(void *, uint32_t address, uint32_t saved, uint32_t *observed);
};
/* One-shot initial power-on R/VCO/FULL/LC/RXDCOC transaction for captured
 * ASIC76100002/MAC76502000, NICfd11/1004, channel1/20MHz only. TSSI bit13 must
 * be clear. Does not implement TSSI DC, power tables, MAC/endpoint ownership,
 * retries or command framing. Caller must always drain/disable MAC and stop RX
 * even after failure; successful commands cannot be rolled back by this helper.
 * TX_ALC0 must be 2f2f1b1b. Fresh IBI9 AFTER suppression/500us must be ffffffff;
 * the temporary ffffff7e write MUST independently match before FULL/LC.
 * Reverse raw saved-word restoration is mandatory after any attempted write.
 * Completion records the transaction, not calibrated RF or packet reception.
 * Original strict non-power-on helper and observation helper are unchanged. */
enum mt76_calibration_status mt76_poweron_calibration_run(
    struct mt76_calibration *, const struct mt76_poweron_io *,
    const struct mt76_identity *, const struct mt76_ant_fields *,
    unsigned channel, unsigned bandwidth);
#endif
