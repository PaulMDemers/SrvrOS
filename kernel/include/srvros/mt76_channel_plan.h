#ifndef SRVROS_MT76_CHANNEL_PLAN_H
#define SRVROS_MT76_CHANNEL_PLAN_H
#include <stdbool.h>
#include <stdint.h>
#define MT76_CHANNEL_STEPS 21u
#define MT76_CHANNEL_REGS 13u
#define MT76_CHANNEL_MAX 11u
struct mt76_channel_step { uint8_t reg, mask, bits; };
/* Frequency fields only: fixed US1..11/20MHz, non-SDM reference table/order.
 * Does not perform band/baseband setup, VCO, calibration or receiver startup.
 * Channel1 exactly matches the separate accepted synthesizer helper. */
bool mt76_channel_plan(unsigned channel,struct mt76_channel_step out[MT76_CHANNEL_STEPS]);
struct mt76_channel_preview_plan {
    uint8_t country[2], bandwidth, count, channel[MT76_CHANNEL_MAX];
};
struct mt76_channel_preview_io {
    void *context;
    /* Before every callback. Forward cancellation must not block independent
     * cleanup ownership when the retained transport is still safe. Native
     * caller must pin board/firmware, stopped RX/DMA and bounded I/O. */
    bool (*owned)(void *,bool cleanup);
    bool (*read_guard)(void *,uint32_t address,uint32_t *value,bool cleanup);
    bool (*read_rf)(void *,uint8_t reg,uint8_t *value,bool cleanup);
    bool (*write_rf)(void *,uint8_t reg,uint8_t value,bool cleanup);
};
enum mt76_channel_status { MT76_CHANNEL_OK,MT76_CHANNEL_REFUSED,
    MT76_CHANNEL_OWNER,MT76_CHANNEL_IO,MT76_CHANNEL_STATE };
struct mt76_channel_preview {
    bool attempted,complete,failed,cleanup_attempted,cleanup_complete;
    enum mt76_channel_status status;
    struct mt76_channel_preview_plan plan;
    unsigned guard_reads,rf_reads,rf_writes,channels_completed,steps_completed;
    unsigned cleanup_reads,cleanup_writes;
    uint16_t saved_mask,touched_mask,restored_mask;
    uint8_t saved[MT76_CHANNEL_REGS];
    uint8_t before[MT76_CHANNEL_MAX][MT76_CHANNEL_STEPS];
    uint8_t after[MT76_CHANNEL_MAX][MT76_CHANNEL_STEPS];
};
/* Unique snapshot register order, never an arbitrary RF register accessor. */
unsigned mt76_channel_snapshot_reg(unsigned index);
unsigned mt76_channel_snapshot_initial(unsigned index);
/* One-shot reversible register preview. Snapshot all13 exact channel1 register
 * words before any write; apply each unique requested channel; restore touched
 * originals in reverse snapshot order, including after ambiguous failed writes.
 * Independent readback follows every write when cleanup ownership permits.
 * Restoration is a register observation, not RF lock/performance acceptance.
 * Failed cleanup guards/ownership leave restoration explicitly unproven.
 * No native caller yet. No waits, MAC writes, VCO pulses, DMA reuse or logging. */
bool mt76_channel_preview_run(struct mt76_channel_preview *,
    const struct mt76_channel_preview_plan *,const struct mt76_channel_preview_io *);
#endif
