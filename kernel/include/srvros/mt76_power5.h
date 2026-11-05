#ifndef SRVROS_MT76_POWER5_H
#define SRVROS_MT76_POWER5_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Private copied mapped-EEPROM fields. Numeric validity is NOT fresh EFUSE,
 * regulatory,device/channel or transmission authority;native owner supplies it. */
struct mt76_power5_fields {
 uint64_t generation;
 uint16_t chip,nic0,nic1,channel_pair,cck,ofdm_lo,ofdm_hi,ht_lo,ht_hi,vht;
};
struct mt76_power5_write {uint32_t address,mask,value;};
struct mt76_power5_plan {
 bool valid;uint64_t generation;
 unsigned factory_base,limit_half_dbm,maximum_half_dbm;
 struct mt76_power5_write write[9];
};
enum mt76_power5_status {MT76_POWER5_OK,MT76_POWER5_INVALID,MT76_POWER5_UNSUPPORTED};
/* Logical512-byte source only;copies wanted fields,never factory MAC/serial.
 * Caller binds a fresh mapped-read gate to this result before hardware use. */
enum mt76_power5_status mt76_power5_fields(const uint8_t *,size_t,uint64_t,struct mt76_power5_fields *);
/* Captured7610/fd11/1004,TSSI-off,US161/20MHz profile. Source-modelled Linux
 * rate-power arithmetic,ceiling<=15dBm,no invalid-data fallback. Unrepresentable
 * signed6 deltas refuse instead of wrapping upward. Preserves ALC high bits
 * through explicit mask;remaining eight words are fixed rate-power registers.
 * MT7610-specific CFG7/CFG9 reserved VHT positions are zero;VHT8/9
 * belongs only in CFG8. Readback is still exact across the full word.
 * No I/O,caller-selected address,power measurement or analog-quality claim. */
enum mt76_power5_status mt76_power5_plan(const struct mt76_power5_fields *,
 const uint8_t country[2],unsigned channel,unsigned bandwidth,unsigned limit_half_dbm,
 struct mt76_power5_plan *);
#endif
