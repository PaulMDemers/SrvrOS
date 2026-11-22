#ifndef SRVROS_MT76_CHANNEL_VCO5_H
#define SRVROS_MT76_CHANNEL_VCO5_H
#include <srvros/mt76_trim5.h>
#define MT76_CVCO5_OPS 77u
#define MT76_CHANNEL_VCO5_OPS 3844u
/* Separate fixed child, not a caller-selected RF plan. Scalar board/trim
 * admission is not private fresh-field proof; native owner must bind that. */
struct mt76_cvco5 {
    bool attempted,complete,trim_selected,triggered;
    unsigned trim,operations,reads,exchanges,writes,waits,delay_us,first_clear;
    int received;
    uint32_t guards[22][3],words[11][4];
};
bool mt76_cvco5_owned(const struct mt76_cvco5 *);
bool mt76_cvco5_valid(const struct mt76_cvco5 *);
bool mt76_cvco5_delay_admit(const struct mt76_cvco5 *);
bool mt76_cvco5_transfer_valid(const struct mt76_cvco5 *,bool,uint32_t);
size_t mt76_cvco5_packet(const struct mt76_cvco5 *,uint8_t *,size_t);
enum mt76_mcu_timing_status mt76_cvco5_capture(struct mt76_cvco5 *,uint16_t,uint16_t,unsigned,const struct mt76_rf_wait_io *);
/* Legacy trim/channel engines are unchanged. This coordinator alone owns the
 * fixed child at their still-selected checkpoint, before any inverse writes. */
struct mt76_channel_vco5 {
    bool attempted,complete,restored,in_child;
    uint16_t nic0,nic1;
    unsigned factory_trim,lna;
    enum mt76_mcu_timing_status child_status;
    struct mt76_trim5 trim;
    struct mt76_cvco5 vco;
};
bool mt76_channel_vco5_selected(const struct mt76_channel_vco5 *);
bool mt76_channel_vco5_owned(const struct mt76_channel_vco5 *);
bool mt76_channel_vco5_valid(const struct mt76_channel_vco5 *);
unsigned mt76_channel_vco5_operations(const struct mt76_channel_vco5 *);
unsigned mt76_channel_vco5_phase(const struct mt76_channel_vco5 *);
unsigned mt76_channel_vco5_local(const struct mt76_channel_vco5 *);
bool mt76_channel_vco5_transfer_valid(const struct mt76_channel_vco5 *,bool,uint32_t,uint32_t);
size_t mt76_channel_vco5_packet(const struct mt76_channel_vco5 *,uint8_t *,size_t);
enum mt76_mcu_timing_status mt76_channel_vco5_capture(struct mt76_channel_vco5 *,uint16_t,uint16_t,unsigned,unsigned,const struct mt76_rf_wait_io *,const struct mt76_band_mac_io *,const struct mt76_pa5_io *,const struct mt76_bbp5_io *);
/* Separate one-shot inspection at selected checkpoint after VCO clear and
 * before inverse writes. Callback must retain and restore selected state and
 * prove MAC/RX cessation. Failure stops the outer transaction without replay.
 * No authority is conveyed by this hook; old no-hook plans stay unchanged. */
enum mt76_mcu_timing_status mt76_channel_vco5_inspect(struct mt76_channel_vco5 *,uint16_t,uint16_t,unsigned,unsigned,const struct mt76_rf_wait_io *,const struct mt76_band_mac_io *,const struct mt76_pa5_io *,const struct mt76_bbp5_io *,bool (*)(void *));
#endif
