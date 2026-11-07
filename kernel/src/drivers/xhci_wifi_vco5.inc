/* Fixed common VCO observation; trigger clearance is not RF-lock acceptance. */
#include <srvros/mt76_vco5.h>
static struct {struct xhci_device *d;struct mt76_vco5 state;unsigned submitted,delays;bool active;} wifi_vco5;
static bool wifi_vco5_parent(struct xhci_device *d){
    return wifi_vco5_requested && d && d==wifi_dac5.d && !wifi_dac5.active && wifi_dac5_parent(d) &&
        mt76_dac5_valid(&wifi_dac5.state) && wifi_dac5.submitted==91 &&
        wifi_dac5.state.trim==wifi_trim5.fields.trim;
}
static bool wifi_vco5_owned(void *context){
    return context==&wifi_vco5 && wifi_vco5.active && wifi_vco5_parent(wifi_vco5.d) &&
        wifi_vco5.state.trim==wifi_trim5.fields.trim && mt76_vco5_owned(&wifi_vco5.state) && wifi_vco5.submitted==wifi_vco5.state.operations && wifi_vco5.delays==wifi_vco5.state.waits;
}
static bool wifi_vco5_read(void *context,uint32_t address,uint32_t *v){
    if(!v || !wifi_vco5_owned(context) || !mt76_vco5_transfer_valid(&wifi_vco5.state,false,address))return false;
    unsigned op=wifi_vco5.submitted++;*v=0;bool ok=wifi_mac_private_usb(wifi_vco5.d,false,address,v);
    console_printf("wifi-vco5-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_vco5.d);
}
static bool wifi_vco5_delay(void *context,uint32_t us){
    if(!wifi_vco5_owned(context) || !mt76_vco5_delay_admit(&wifi_vco5.state) || us!=mt76_rf_wait_delay(wifi_vco5.state.waits))return false;
    wifi_vco5.delays++;bool ok=timer_delay_us(us);
    console_printf("wifi-vco5-delay: sample=%u us=%u ok=%u\n",(uint64_t)wifi_vco5.state.waits,(uint64_t)us,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_vco5.d);
}
static int wifi_vco5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    uint8_t allowed[16];
    if(!wifi_vco5_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_vco5_packet(&wifi_vco5.state,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    unsigned op=wifi_vco5.submitted++;struct xhci_device *d=wifi_vco5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-vco5-command: op=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)((packet[2]>>4)==12),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_vco5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_vco5 *s=&wifi_vco5.state;
    if(s->attempted || wifi_vco5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_vco5_parent(d);enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-vco5-progress: same-boot fresh DAC parent;trim44;one VCO trigger;six timed samples;no force-clear/RX/network OUT\n");
    if(parent){wifi_vco5.d=d;wifi_vco5.active=true;const struct mt76_rf_wait_io io={{&wifi_vco5,wifi_vco5_owned,wifi_vco5_read,wifi_vco5_exchange},wifi_vco5_delay};status=mt76_vco5_capture(s,wifi_trim5.fields.nic0,wifi_trim5.fields.nic1,wifi_trim5.fields.trim,&io);}
    wifi_vco5.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_vco5_valid(s) && wifi_vco5.submitted==105 && wifi_vco5.delays==6;
    console_printf("wifi-vco5: status=%u complete=%u valid=%u words_restored=%u triggered=%u operations=%u submitted=%u trim=%u parent=%u waits=%u delay_us=%u first_clear=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->words_restored,(uint64_t)s->triggered,(uint64_t)s->operations,(uint64_t)wifi_vco5.submitted,(uint64_t)s->trim,(uint64_t)parent,(uint64_t)s->waits,(uint64_t)s->delay_us,(uint64_t)s->first_clear);
    return valid?(!wifi_channel_vco5_requested || wifi_mcu_channel_vco5(d)):wifi_mcu_fail(d);
}
bool xhci_wifi_capture_vco5(void){
    if(wifi_vco5_requested || wifi_vco5.active || wifi_vco5.state.attempted || wifi_dac5_requested || wifi_trim5_requested || wifi_held_requested || wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_vco5_requested=true;bool ok=xhci_wifi_capture_dac5();wifi_vco5_requested=false;return ok;
}
