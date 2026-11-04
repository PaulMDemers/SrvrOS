/* Fixed selected-channel lifetime after the complete common VCO parent. */
#include <srvros/mt76_channel_vco5.h>
static struct {struct xhci_device *d;struct mt76_channel_vco5 state;unsigned submitted,delays;bool active;} wifi_channel_vco5;
static bool wifi_channel_vco5_parent(struct xhci_device *d){
 return wifi_channel_vco5_requested && d && d==wifi_vco5.d && !wifi_vco5.active && wifi_vco5_parent(d) && mt76_vco5_valid(&wifi_vco5.state) && wifi_vco5.submitted==105 && wifi_vco5.delays==6;
}
static bool wifi_channel_vco5_owned(void *context){
 struct mt76_channel_vco5 *s=&wifi_channel_vco5.state;
 return context==&wifi_channel_vco5 && wifi_channel_vco5.active && wifi_channel_vco5_parent(wifi_channel_vco5.d) && s->factory_trim==wifi_trim5.fields.trim && s->lna==wifi_bbp5.gain.lna && mt76_channel_vco5_owned(s) && wifi_channel_vco5.submitted==mt76_channel_vco5_operations(s) && wifi_channel_vco5.delays==s->vco.waits;
}
static bool wifi_channel_vco5_delay(void *context,uint32_t us){
 struct mt76_channel_vco5 *s=&wifi_channel_vco5.state;
 if(!wifi_channel_vco5_owned(context) || !s->in_child || !mt76_cvco5_delay_admit(&s->vco) || us!=mt76_rf_wait_delay(s->vco.waits))return false;
 wifi_channel_vco5.delays++;bool ok=timer_delay_us(us);
 console_printf("wifi-channel-vco5-delay: sample=%u us=%u ok=%u\n",(uint64_t)s->vco.waits,(uint64_t)us,(uint64_t)ok);
 return ok?true:wifi_mcu_fail(wifi_channel_vco5.d);
}
static bool wifi_channel_vco5_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    if(!v || !wifi_channel_vco5_owned(context) || !mt76_channel_vco5_transfer_valid(&wifi_channel_vco5.state,write,address,*v))return false;
    unsigned op=wifi_channel_vco5.submitted++,phase=mt76_channel_vco5_phase(&wifi_channel_vco5.state),local=mt76_channel_vco5_local(&wifi_channel_vco5.state);
    bool ok=wifi_mac_private_usb(wifi_channel_vco5.d,write,address,v);
    console_printf("wifi-channel-vco5-usb: op=%u phase=%u local=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)write,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_channel_vco5.d);
}
static bool wifi_channel_vco5_read(void *context,uint32_t address,uint32_t *v){if(!v)return false;*v=0;return wifi_channel_vco5_transfer(context,false,address,v);}
static bool wifi_channel_vco5_mac(void *context,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
    if(!data || length!=4 || !wifi_channel_vco5_owned(context) || wifi_channel_vco5.state.in_child || wifi_channel_vco5.state.trim.phase!=2 || wifi_channel_vco5.state.trim.channel.phase!=1 || op!=mt76_channel_vco5_local(&wifi_channel_vco5.state))return false;
    uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)data[i]<<(8*i);
    if(!wifi_channel_vco5_transfer(context,write,address,&v))return false;
    for(unsigned i=0;i<4;i++)data[i]=(uint8_t)(v>>(8*i));
    return true;
}
static int wifi_channel_vco5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    uint8_t allowed[16];
    if(!wifi_channel_vco5_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_channel_vco5_packet(&wifi_channel_vco5.state,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    unsigned op=wifi_channel_vco5.submitted++,phase=mt76_channel_vco5_phase(&wifi_channel_vco5.state),local=mt76_channel_vco5_local(&wifi_channel_vco5.state);
    struct xhci_device *d=wifi_channel_vco5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-channel-vco5-command: op=%u phase=%u local=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)((packet[2]>>4)==12),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_channel_vco5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_channel_vco5 *s=&wifi_channel_vco5.state;
    if(s->attempted || wifi_channel_vco5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_channel_vco5_parent(d);enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-channel-vco5-progress: same-boot common VCO parent;hold fresh trim44 and channel161/20MHz bank;one channel trigger;six samples;inverse originals;no RX/network OUT\n");
    if(parent){wifi_channel_vco5.d=d;wifi_channel_vco5.active=true;const struct mt76_rf_wait_io io={{&wifi_channel_vco5,wifi_channel_vco5_owned,wifi_channel_vco5_read,wifi_channel_vco5_exchange},wifi_channel_vco5_delay};const struct mt76_band_mac_io mac={&wifi_channel_vco5,wifi_channel_vco5_owned,wifi_channel_vco5_mac};const struct mt76_pa5_io pa={&wifi_channel_vco5,wifi_channel_vco5_owned,wifi_channel_vco5_transfer,wifi_channel_vco5_exchange};const struct mt76_bbp5_io bbp={&wifi_channel_vco5,wifi_channel_vco5_owned,wifi_channel_vco5_transfer};status=mt76_channel_vco5_capture(s,wifi_trim5.fields.nic0,wifi_trim5.fields.nic1,wifi_trim5.fields.trim,wifi_bbp5.gain.lna,&io,&mac,&pa,&bbp);}
    wifi_channel_vco5.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_channel_vco5_valid(s) && wifi_channel_vco5.submitted==3844 && wifi_channel_vco5.delays==6;
    console_printf("wifi-channel-vco5: status=%u complete=%u valid=%u restored=%u operations=%u submitted=%u trim=%u lna=%u parent=%u child_operations=%u child_valid=%u waits=%u delay_us=%u first_clear=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->restored,(uint64_t)mt76_channel_vco5_operations(s),(uint64_t)wifi_channel_vco5.submitted,(uint64_t)s->factory_trim,(uint64_t)s->lna,(uint64_t)parent,(uint64_t)s->vco.operations,(uint64_t)mt76_cvco5_valid(&s->vco),(uint64_t)s->vco.waits,(uint64_t)s->vco.delay_us,(uint64_t)s->vco.first_clear);
    return valid?true:wifi_mcu_fail(d);
}
bool xhci_wifi_capture_channel_vco5(void){
    if(wifi_channel_vco5_requested || wifi_channel_vco5.active || wifi_channel_vco5.state.attempted || wifi_vco5_requested || wifi_dac5_requested || wifi_trim5_requested || wifi_held_requested || wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_channel_vco5_requested=true;bool ok=xhci_wifi_capture_vco5();wifi_channel_vco5_requested=false;return ok;
}
