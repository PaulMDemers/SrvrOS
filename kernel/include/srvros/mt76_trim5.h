#ifndef SRVROS_MT76_TRIM5_H
#define SRVROS_MT76_TRIM5_H
#include <srvros/mt76_mcu.h>
/* Fixed board trim lifetime. Numeric admission is not fresh EFUSE proof;
 * a separate native owner must bind that proof before any hardware use.
 * No native caller, DAC/VCO pulse, receiver or caller-selected register plan. */
#define MT76_TRIM5_OPS 35u
struct mt76_trim5 {
    bool attempted,complete,restored,with_channel;
    unsigned phase,trim,lna,operations,reads,exchanges,writes;
    int received;
    uint32_t guards[10][3],words[5][4];
    struct mt76_channel5 channel;
};
bool mt76_trim5_owned(const struct mt76_trim5 *s);
bool mt76_trim5_valid(const struct mt76_trim5 *s);
unsigned mt76_trim5_operations(const struct mt76_trim5 *s);
unsigned mt76_trim5_phase(const struct mt76_trim5 *s);
unsigned mt76_trim5_local(const struct mt76_trim5 *s);
bool mt76_trim5_transfer_valid(const struct mt76_trim5 *s,bool write,uint32_t address,uint32_t value);
size_t mt76_trim5_packet(const struct mt76_trim5 *s,uint8_t *out,size_t capacity);
enum mt76_mcu_timing_status mt76_trim5_capture(struct mt76_trim5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,const struct mt76_mcu_timing_io *rf);
enum mt76_mcu_timing_status mt76_trim5_channel_capture(struct mt76_trim5 *s,uint16_t nic0,uint16_t nic1,unsigned trim,unsigned lna,const struct mt76_mcu_timing_io *rf,const struct mt76_band_mac_io *mac,const struct mt76_pa5_io *pa,const struct mt76_bbp5_io *bbp);
#endif
