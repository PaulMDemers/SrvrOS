#ifndef SRVROS_MT76_CALIBRATION_H
#define SRVROS_MT76_CALIBRATION_H
#include <stdbool.h>
#include <stdint.h>

/* Semantic operations, NOT firmware command IDs. A future native adapter must
 * frame and validate each reply. This helper has no hardware caller. */
enum mt76_calibration_command { MT76_CAL_FULL, MT76_CAL_LC, MT76_CAL_RXDCOC };
enum mt76_calibration_status {
    MT76_CAL_OK, MT76_CAL_REFUSED, MT76_CAL_IO, MT76_CAL_COMMAND,
    MT76_CAL_DELAY, MT76_CAL_RESTORE
};
struct mt76_calibration_io {
    void *context;
    /* Require exclusive live MAC/RX ownership, current board/power/channel
     * evidence and no cancellation. Called before EVERY forward operation. */
    bool (*authorize)(void *);
    bool (*read)(void *, uint32_t address, uint32_t *value);
    bool (*write)(void *, uint32_t address, uint32_t value);
    bool (*command)(void *, enum mt76_calibration_command, uint32_t argument);
    bool (*delay)(void *, unsigned minimum_us);
    /* Cleanup authorization is independent of forward cancellation. Restore
     * exactly this saved word and independently read it back. Never retry,
     * free uncertain DMA or issue another calibration command on failure. */
    bool (*restore)(void *, uint32_t address, uint32_t saved, uint32_t *observed);
};
struct mt76_calibration {
    bool attempted, complete, cleanup_complete;
    uint8_t saved_mask, modified_mask, restore_attempted, restored_mask;
    unsigned reads, writes, commands, waits;
    uint32_t saved[2], restored[2]; /* TX_ALC0, IBI9; never EEPROM/key bytes */
    enum mt76_calibration_status failure;
};
/* One-shot fixed channel1/20MHz, non-power-on FULL/LC/RXDCOC transaction.
 * Does NOT start MAC, implement R/VCO/TSSI/power-on calibration, set transmit
 * power, authorize radio use, stop endpoints or accept calibrated reception.
 * IBI9 all-ones is refused, including this unit's earlier observation. */
enum mt76_calibration_status mt76_calibration_run(struct mt76_calibration *,
    const struct mt76_calibration_io *, unsigned channel, unsigned bandwidth);

struct mt76_calibration_probe {
    bool attempted, complete, ibi_write_matched, ibi_changed;
    uint8_t saved_mask, modified_mask, restore_attempted, raw_restore_mask;
    unsigned reads, writes, waits;
    uint32_t saved[2], observed_ibi, restored[2];
    enum mt76_calibration_status failure;
};
/* Separate captured-profile observation. Fresh TX_ALC0 must be 2f2f1b1b.
 * IBI9 is saved AFTER TX_ALC0 suppression and the reference 500us delay.
 * Its raw uint32 value may be all-ones. Independent readback records write
 * effect; no matching readback is required to finish this observation. Exact
 * raw saved-word restoration IS required. No command callback is called.
 * This never relaxes mt76_calibration_run or authorizes calibration. */
enum mt76_calibration_status mt76_calibration_probe_run(
    struct mt76_calibration_probe *, const struct mt76_calibration_io *);
#endif
