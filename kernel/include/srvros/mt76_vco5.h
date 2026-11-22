#ifndef SRVROS_MT76_VCO5_H
#define SRVROS_MT76_VCO5_H
#include <srvros/mt76_mcu.h>
/* Fixed common VCO observation, not RF-lock or receiver admission. Native
 * caller must bind fresh same-boot fields and completed common DAC proof.
 * A pending/failed trigger is never force-cleared, replayed or rolled back. */
#define MT76_VCO5_OPS 105u
struct mt76_vco5 {
    bool attempted,complete,words_restored,triggered;
    unsigned trim,operations,reads,exchanges,writes,waits,delay_us,first_clear;
    int received;
    uint32_t guards[30][3],words[15][4];
};
bool mt76_vco5_owned(const struct mt76_vco5 *);
bool mt76_vco5_valid(const struct mt76_vco5 *);
bool mt76_vco5_delay_admit(const struct mt76_vco5 *);
bool mt76_vco5_transfer_valid(const struct mt76_vco5 *,bool,uint32_t);
size_t mt76_vco5_packet(const struct mt76_vco5 *,uint8_t *,size_t);
enum mt76_mcu_timing_status mt76_vco5_capture(struct mt76_vco5 *,uint16_t,uint16_t,unsigned,const struct mt76_rf_wait_io *);
#endif
