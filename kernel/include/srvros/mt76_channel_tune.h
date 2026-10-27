#ifndef SRVROS_MT76_CHANNEL_TUNE_H
#define SRVROS_MT76_CHANNEL_TUNE_H
#include <srvros/mt76_channel_plan.h>
/* One stopped-RX excursion from captured channel1 frequency fields to one
 * US1..11/20MHz plan, then back. No receiver restart, transmit or scan service.
 * Caller must freshly own exact captured board/firmware, same-band 20MHz RF/BBP
 * setup and retained stopped DMA before EVERY callback. No native caller yet.
 * VCO trigger clearing is a register observation, never PLL-lock/RF acceptance.
 * Cleanup is independent of forward cancellation, but never of live ownership.
 */
struct mt76_channel_tune_io {
    void *context;
    bool (*owned)(void *,bool cleanup);
    bool (*read_guard)(void *,uint32_t,uint32_t *,bool cleanup);
    bool (*read_rf)(void *,uint8_t,uint8_t *,bool cleanup);
    bool (*write_rf)(void *,uint8_t,uint8_t,bool cleanup);
    /* Must guarantee minimum elapsed wall time, bounded wait and fresh transport
     * ownership. Per VCO attempt the total requested delay is at most100000us. */
    bool (*delay)(void *,unsigned minimum_us,bool cleanup);
};
struct mt76_channel_tune {
    bool attempted,complete,failed,target_complete;
    bool cleanup_attempted,cleanup_complete,vco_attempted,cleanup_vco_attempted;
    bool target_vco_clear,cleanup_vco_clear;
    enum mt76_channel_status status;
    unsigned channel,steps,guard_reads,rf_reads,rf_writes;
    unsigned target_polls,cleanup_polls,target_delay_us,cleanup_delay_us;
    uint16_t saved_mask,touched_mask,restored_mask;
    uint8_t saved[MT76_CHANNEL_REGS];
};
/* One-shot: bad/missing callbacks or non-US/non20MHz/out-of-range channel refuse
 * without I/O. Save all13 exact channel1 originals and require idle VCO0:4=30
 * before writes. Fresh readbacks preserve unmasked fields and detect drift.
 * An ambiguous write remains touched; cleanup may run on safe retained transport.
 * Cleanup restores originals in reverse order, then triggers/re-settles channel1.
 * If VCO remains busy, no cleanup RF writes are permitted. A failed restore
 * prevents the cleanup VCO pulse. Completed cleanup never erases forward failure.
 */
bool mt76_channel_tune_run(struct mt76_channel_tune *,const uint8_t country[2],
    unsigned channel,unsigned bandwidth,const struct mt76_channel_tune_io *);
#endif
