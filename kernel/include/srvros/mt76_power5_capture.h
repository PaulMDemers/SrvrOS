#ifndef SRVROS_MT76_POWER5_CAPTURE_H
#define SRVROS_MT76_POWER5_CAPTURE_H
#include <srvros/mt76_bringup.h>
#include <srvros/mt76_power5.h>
/* Separate logical mapped-read profile:00,30,90,d0,120. Hardware mode0 is
 * READ only. No raw block bytes or factory identifiers persist in this gate.
 * A native caller owns a separate gate and live controller/device lease. */
struct mt76_power5_gate {
 bool pending,failed;
 unsigned phase,block,submitted,polls;
 uint32_t control,command;
};
struct mt76_power5_capture {
 bool attempted,complete;
 unsigned reads,writes,polls,blocks;
 struct mt76_power5_fields fields;
};
bool mt76_power5_command(uint32_t previous,unsigned block,uint32_t *);
bool mt76_power5_begin(struct mt76_power5_gate *,bool write,uint32_t address,uint32_t value);
bool mt76_power5_finish(struct mt76_power5_gate *,bool ok,uint32_t value);
bool mt76_power5_delay(struct mt76_power5_gate *,unsigned us);
bool mt76_power5_capture_valid(const struct mt76_power5_capture *);
/* One-shot, bounded mapped reads with MAC/core/DMA guards each block.
 * Partial/failing results publish no fields. Authorize must retain the same
 * selected161 calibration lifetime; this helper does not grant radio OUT. */
enum mt76_capture_status mt76_power5_capture_run(struct mt76_power5_capture *,
 const struct mt76_efuse_map_io *,const struct mt76_identity *,uint64_t generation);
#endif
