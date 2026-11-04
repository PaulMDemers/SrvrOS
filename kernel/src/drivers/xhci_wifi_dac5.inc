/* Explicit common DAC child. Never included by ordinary device enumeration. */
#include <srvros/mt76_dac5.h>
static struct {struct xhci_device *d;struct mt76_dac5 state;unsigned submitted;bool active;} wifi_dac5;
static bool wifi_dac5_parent(struct xhci_device *d){
    const struct mt76_trim_fields *f=&wifi_trim5.fields;const struct mt76_efuse_map_gate *g=&wifi_trim5.gate;
    return wifi_dac5_requested && d && d==wifi_trim5.d && !wifi_trim5.active && !wifi_trim5.fields_active && wifi_trim5_parent(d) &&
        mt76_trim_fields_valid(f) && g->phase==15 && g->block==2 && !g->failed && !g->pending &&
        g->polls==f->polls && g->submitted==f->reads+f->writes && mt76_trim5_valid(&wifi_trim5.state) &&
        wifi_trim5.state.with_channel && wifi_trim5.state.trim==f->trim && wifi_trim5.submitted==3767;
}
static bool wifi_dac5_owned(void *context){
    return context==&wifi_dac5 && wifi_dac5.active && wifi_dac5_parent(wifi_dac5.d) &&
        wifi_dac5.state.trim==wifi_trim5.fields.trim && mt76_dac5_owned(&wifi_dac5.state) && wifi_dac5.submitted==wifi_dac5.state.operations;
}
static bool wifi_dac5_read(void *context,uint32_t address,uint32_t *v){
    if(!v || !wifi_dac5_owned(context) || !mt76_dac5_transfer_valid(&wifi_dac5.state,false,address))return false;
    unsigned op=wifi_dac5.submitted++;*v=0;bool ok=wifi_mac_private_usb(wifi_dac5.d,false,address,v);
    console_printf("wifi-dac5-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_dac5.d);
}
static int wifi_dac5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    uint8_t allowed[16];
    if(!wifi_dac5_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_dac5_packet(&wifi_dac5.state,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    unsigned op=wifi_dac5.submitted++;struct xhci_device *d=wifi_dac5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-dac5-command: op=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)((packet[2]>>4)==12),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_dac5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_dac5 *s=&wifi_dac5.state;
    if(s->attempted || wifi_dac5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_dac5_parent(d);enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-dac5-progress: same-boot fresh parent;trim44;DAC set-clear-set;independent reads;word originals;no VCO/RX/network OUT\n");
    if(parent){wifi_dac5.d=d;wifi_dac5.active=true;const struct mt76_mcu_timing_io io={&wifi_dac5,wifi_dac5_owned,wifi_dac5_read,wifi_dac5_exchange};status=mt76_dac5_capture(s,wifi_trim5.fields.nic0,wifi_trim5.fields.nic1,wifi_trim5.fields.trim,&io);}
    wifi_dac5.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_dac5_valid(s) && wifi_dac5.submitted==91;
    console_printf("wifi-dac5: status=%u complete=%u valid=%u words_restored=%u dac_reset=%u operations=%u submitted=%u trim=%u parent=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->words_restored,(uint64_t)s->dac_reset,(uint64_t)s->operations,(uint64_t)wifi_dac5.submitted,(uint64_t)s->trim,(uint64_t)parent);
    return valid?true:wifi_mcu_fail(d);
}
bool xhci_wifi_capture_dac5(void){
    if(wifi_dac5_requested || wifi_dac5.active || wifi_dac5.state.attempted || wifi_trim5_requested || wifi_held_requested || wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_dac5_requested=true;bool ok=xhci_wifi_capture_trim5_channel();wifi_dac5_requested=false;return ok;
}
