#ifndef SRVROS_MT76_DAC5_H
#define SRVROS_MT76_DAC5_H
#include <srvros/mt76_mcu.h>
/* Fixed common DAC reset with trim selected. Native admission must bind fresh
 * private fields and the completed same-boot parent. No VCO/RX or generic plan.
 * Word restoration does not undo internal DAC reset effects. */
#define MT76_DAC5_OPS 91u
struct mt76_dac5 {
    bool attempted,complete,words_restored,dac_reset;
    unsigned trim,operations,reads,exchanges,writes;
    int received;
    uint32_t guards[26][3],words[13][4];
};
bool mt76_dac5_owned(const struct mt76_dac5 *);
bool mt76_dac5_valid(const struct mt76_dac5 *);
bool mt76_dac5_transfer_valid(const struct mt76_dac5 *,bool,uint32_t);
size_t mt76_dac5_packet(const struct mt76_dac5 *,uint8_t *,size_t);
enum mt76_mcu_timing_status mt76_dac5_capture(struct mt76_dac5 *,uint16_t,uint16_t,unsigned,const struct mt76_mcu_timing_io *);
#endif
