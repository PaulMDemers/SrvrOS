/* v98 only: stopped-RX channel6/20MHz excursion and channel1 re-settling.
 * A separate bounded event ledger admits exact callback order and values.
 * No network data OUT, endpoint restart, DMA recycling or private identifiers. */
static void wifi_tune_add(struct xhci_wifi_tune *t,unsigned kind,unsigned a,unsigned v,bool poll) {
    if(t->count<384)t->event[t->count++]=(struct xhci_wifi_tune_event){v,(uint16_t)a,(uint8_t)kind,(uint8_t)poll};
}
static void wifi_tune_guards(struct xhci_wifi_tune *t) {
    const unsigned a[4]={0x1004,0x2000,0x208,0x1200},v[4]={0,0xf000f200,0x40000050,0};
    for(unsigned i=0;i<4;i++)wifi_tune_add(t,0,a[i],v[i],false);
}
static void wifi_tune_vco(struct xhci_wifi_tune *t) {
    const unsigned delays[5]={10,90,900,9000,90000};
    wifi_tune_guards(t);wifi_tune_add(t,1,4,0x30,false);wifi_tune_add(t,2,4,0xb0,false);
    for(unsigned i=0;i<6;i++) {
        if(i)wifi_tune_add(t,3,0,delays[i-1],false);
        wifi_tune_guards(t);wifi_tune_add(t,1,4,0,true);
    }
    t->after_vco=t->count;wifi_tune_guards(t);
}
static void wifi_tune_forward(struct xhci_wifi_tune *t) {
    t->pos=t->count=0;wifi_tune_guards(t);wifi_tune_add(t,1,4,0x30,false);
    uint8_t value[13];
    for(unsigned i=0;i<13;i++) {
        value[i]=(uint8_t)mt76_channel_snapshot_initial(i);
        wifi_tune_add(t,1,mt76_channel_snapshot_reg(i),value[i],false);
    }
    struct mt76_channel_step plan[21];(void)mt76_channel_plan(6,plan);
    for(unsigned k=0;k<21;k++) {
        unsigned i=wifi_channel_index(plan[k].reg);wifi_tune_guards(t);
        wifi_tune_add(t,1,plan[k].reg,value[i],false);
        value[i]=(uint8_t)((value[i]&~plan[k].mask)|plan[k].bits);
        wifi_tune_add(t,2,plan[k].reg,value[i],false);wifi_tune_add(t,1,plan[k].reg,value[i],false);
        wifi_tune_guards(t);
    }
    wifi_tune_vco(t);
    for(unsigned i=0;i<13;i++)wifi_tune_add(t,1,mt76_channel_snapshot_reg(i),value[i],false);
    wifi_tune_guards(t);
}
static void wifi_tune_cleanup(struct xhci_wifi_tune *t) {
    t->cleanup_gate=true;t->pos=t->count=0;wifi_tune_guards(t);wifi_tune_add(t,1,4,0x30,false);
    for(unsigned n=13;n;n--)if(t->published&(1u<<(n-1))) {
        unsigned a=mt76_channel_snapshot_reg(n-1),v=mt76_channel_snapshot_initial(n-1);
        wifi_tune_guards(t);wifi_tune_add(t,2,a,v,false);wifi_tune_add(t,1,a,v,false);wifi_tune_guards(t);
    }
    wifi_tune_vco(t);
    for(unsigned i=0;i<13;i++)wifi_tune_add(t,1,mt76_channel_snapshot_reg(i),mt76_channel_snapshot_initial(i),false);
    wifi_tune_guards(t);
}
static bool wifi_tune_owned(void *context,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_tune *t=&m->tune;t->owned_checks++;
    if(!m->tune_mode || t->observing || !t->active || !t->pinned || !t->state.attempted || t->state.channel!=6 ||
       !wifi_mcu_owned(d) || !d->present || d->vendor_id!=0x7392 || d->product_id!=0xa711 || d->device_bcd!=0x100 ||
       !d->slot_id || !d->ep0_ring || !d->descriptor_buffer || !m->configured || m->pending ||
       !d->firmware.ready || !d->firmware.buffer || !m->buffer || !d->firmware.ep.ring || !m->response.ring ||
       d->firmware.ep.dci!=16 || m->response.dci!=11 || d->firmware.ep.packet!=512 || m->response.packet!=512 ||
       t->count>384 || t->pos>t->count || t->callbacks>600 || t->exchanges>160 || t->published>8191 ||
       firmware_command_failed || strict_control.device || !(m->receive_mode?wifi_receive_quiesced(d):wifi_channel_stopped(d)))return false;
    uint64_t now[18];wifi_channel_identity(d,now);
    for(unsigned i=0;i<18;i++)if(!now[i] || now[i]!=t->identity[i])return false;
    if(d->firmware.buffer==m->buffer || pmm_phys_to_virt(d->descriptor_buffer_phys)!=d->descriptor_buffer ||
       pmm_phys_to_virt(d->firmware.buffer_phys)!=d->firmware.buffer || pmm_phys_to_virt(m->buffer_phys)!=m->buffer ||
       pmm_phys_to_virt(d->firmware.ep.ring_phys)!=d->firmware.ep.ring || pmm_phys_to_virt(m->response.ring_phys)!=m->response.ring)return false;
    return clean || ((!m->receive_mode || !m->receive.attempted || (m->receive.complete && !m->receive.failed)) && !t->forward_failed && !t->state.failed && m->window.state.complete && !m->window.state.failed &&
        m->window.state.stopped && wifi_window_owned(d,true) && m->discovery.complete && !m->discovery.failed &&
        mt76_ant_fields_valid(&m->ant_fields) && mt76_trim_fields_valid(&m->trim_fields));
}
static bool wifi_tune_request(struct xhci_device *d,bool clean,unsigned kind,unsigned a,unsigned v) {
    struct xhci_wifi_tune *t=&d->firmware.mcu.tune;
    if(!wifi_tune_owned(d,clean) || t->callbacks>=600)return false;
    if(clean && t->state.cleanup_attempted && !t->cleanup_gate) {
        if(kind || a!=0x1004 || t->state.touched_mask!=t->published)return false;
        wifi_tune_cleanup(t);
    }
    if(clean!=t->cleanup_gate && !(clean && !t->state.cleanup_attempted && t->last_failed_write && kind==1))return false;
    if(t->pos>=t->count)return false;
    struct xhci_wifi_tune_event e=t->event[t->pos];
    if(e.kind!=kind || e.address!=a || ((kind==2 || kind==3) && e.value!=v))return false;
    if(kind==2 && a!=4) {
        unsigned i=wifi_channel_index(a);if(i>=13 || t->state.saved_mask!=8191 ||
           t->state.saved[i]!=mt76_channel_snapshot_initial(i))return false;
        uint16_t bit=(uint16_t)(1u<<i);
        if(!clean && t->state.touched_mask!=(t->published|bit))return false;
        if(!clean)t->published|=bit;
    }
    t->callbacks++;t->pos++;t->last_failed_write=false;return true;
}
static bool wifi_tune_guard(void *context,uint32_t a,uint32_t *v,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_tune *t=&d->firmware.mcu.tune;
    if(!v || !wifi_tune_request(d,clean,0,a,0))return false;
    uint32_t want=t->event[t->pos-1].value;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v) && *v==want;
    console_printf("wifi-tune-guard: index=%u cleanup=%u address=%x value=%x ok=%u\n",(uint64_t)(t->callbacks-1),(uint64_t)clean,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    if(!ok && !clean)t->forward_failed=true;
    return ok;
}
static bool wifi_tune_rf(struct xhci_device *d,bool clean,bool write,uint8_t reg,uint8_t *value) {
    struct xhci_wifi_tune *t=&d->firmware.mcu.tune;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!value || !wifi_tune_request(d,clean,write?2:1,reg,write?*value:0))return false;
    struct xhci_wifi_tune_event event=t->event[t->pos-1];
    unsigned seq=t->exchanges%15+1;uint32_t address=0x80000000u|reg;uint8_t payload[8]={0};
    for(unsigned i=0;i<4;i++)payload[i]=(uint8_t)(address>>(8*i));
    if(write)payload[4]=*value;
    if(mt76_mcu_encode(write?12:10,seq,payload,8,d->firmware.buffer,16)!=16)return false;
    t->exchanges++;int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    uint32_t h=0x4a000008u|(seq<<16);
    unsigned contract=(words[0]==h?1u:0u)|(words[1]==address?2u:0u)|(words[2]<=255?4u:0u)|
        (words[3]==h?8u:0u)|(!write || words[2]==*value?16u:0u);
    bool ok=sent==16 && received==16 && m->completion==13 && m->remaining==MT76_MCU_RESPONSE_MAX-16 && contract==31;
    if(ok && !write)ok=event.poll?(words[2]==0x30 || words[2]==0xb0):words[2]==event.value;
    console_printf("wifi-tune-rf: index=%u cleanup=%u write=%u reg=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x ok=%u contract_mask=%u response_redacted=%u\n",(uint64_t)(t->callbacks-1),(uint64_t)clean,(uint64_t)write,(uint64_t)reg,(uint64_t)seq,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)(ok?words[0]:0),(uint64_t)(ok?words[1]:0),(uint64_t)(ok?words[2]:0),(uint64_t)(ok?words[3]:0),(uint64_t)ok,(uint64_t)contract,(uint64_t)!ok);
    if(ok && !write) {
        *value=(uint8_t)words[2];
        if(event.poll && *value==0x30)t->pos=t->after_vco;
    }
    if(!ok && !clean)t->forward_failed=true;
    if(write && !ok && !clean)t->last_failed_write=true;
    return ok;
}
static bool wifi_tune_read(void *d,uint8_t reg,uint8_t *v,bool clean){return wifi_tune_rf(d,clean,false,reg,v);}
static bool wifi_tune_write(void *d,uint8_t reg,uint8_t v,bool clean){return wifi_tune_rf(d,clean,true,reg,&v);}
static bool wifi_tune_delay(void *context,unsigned us,bool clean) {
    struct xhci_device *d=context;struct xhci_wifi_tune *t=&d->firmware.mcu.tune;
    if(!wifi_tune_request(d,clean,3,0,us))return false;
    uint64_t start=timer_monotonic_us();bool ok=timer_delay_us(us);uint64_t end=timer_monotonic_us();
    ok=ok && end>=start && end-start>=us && wifi_tune_owned(d,clean);
    console_printf("wifi-tune-delay: index=%u cleanup=%u requested_us=%u elapsed_us=%u ok=%u\n",(uint64_t)(t->callbacks-1),(uint64_t)clean,(uint64_t)us,end>=start?end-start:0,(uint64_t)ok);
    if(!ok && !clean)t->forward_failed=true;
    return ok;
}
#include "xhci_wifi_receive.inc"
static bool wifi_tune_observe(void *context,unsigned channel) {
    struct xhci_device *d=context;struct xhci_wifi_tune *t=&d->firmware.mcu.tune;
    if(!d->firmware.mcu.receive_mode || t->observing || t->pos!=t->observe_at ||
       !t->state.observe_attempted || !wifi_tune_owned(d,false))return false;
    t->observing=true;bool ok=wifi_receive_capture(d,channel);t->observing=false;return ok;
}
static void wifi_tune_observe_forward(struct xhci_wifi_tune *t) {
    t->observe_at=t->count;wifi_tune_guards(t);wifi_tune_add(t,1,4,0x30,false);
    uint8_t v[13];struct mt76_channel_step plan[21];
    for(unsigned i=0;i<13;i++)v[i]=(uint8_t)mt76_channel_snapshot_initial(i);
    (void)mt76_channel_plan(6,plan);
    for(unsigned k=0;k<21;k++){unsigned i=wifi_channel_index(plan[k].reg);v[i]=(uint8_t)((v[i]&~plan[k].mask)|plan[k].bits);}
    for(unsigned i=0;i<13;i++)wifi_tune_add(t,1,mt76_channel_snapshot_reg(i),v[i],false);
    wifi_tune_guards(t);
}
static bool wifi_tune_capture(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_tune *t=&m->tune;
    if(!m->tune_mode || t->state.attempted || t->active || !m->window.state.complete ||
       m->window.state.failed || !m->window.state.stopped || !m->discovery.complete || m->discovery.failed ||
       !wifi_window_owned(d,true) || !wifi_channel_stopped(d) ||
       !mt76_ant_fields_valid(&m->ant_fields) || !mt76_trim_fields_valid(&m->trim_fields))return false;
    *t=(struct xhci_wifi_tune){.active=true,.pinned=true};wifi_channel_identity(d,t->identity);wifi_tune_forward(t);
    if(m->receive_mode)wifi_tune_observe_forward(t);
    console_write(m->receive_mode?"wifi-tune-progress: channel6 US20MHz frequency/VCO with fresh bounded RX then channel1 restoration; no network data OUT\n":"wifi-tune-progress: main RX stopped; channel6 US20MHz frequency/VCO excursion then channel1 restoration; no reception or network data OUT\n");
    const uint8_t us[2]={'U','S'};
    const struct mt76_channel_tune_io io={d,wifi_tune_owned,wifi_tune_guard,wifi_tune_read,wifi_tune_write,wifi_tune_delay};
    const struct mt76_channel_tune_observer observer={d,wifi_tune_observe};
    bool ok=m->receive_mode?mt76_channel_tune_observe_run(&t->state,us,6,20,&io,&observer):mt76_channel_tune_run(&t->state,us,6,20,&io);
    ok=ok && t->cleanup_gate && t->pos==t->count && !t->forward_failed && wifi_tune_owned(d,false);
    if(!ok){t->state.complete=false;t->state.failed=true;if(t->state.status==MT76_CHANNEL_OK)t->state.status=MT76_CHANNEL_STATE;}
    t->active=false;return ok;
}
static void wifi_tune_log(struct xhci_device *d) {
    struct xhci_wifi_tune *t=&d->firmware.mcu.tune;struct mt76_channel_tune *s=&t->state;
    console_printf("wifi-tune: attempted=%u complete=%u failed=%u status=%u channel=%u steps=%u guard_reads=%u rf_reads=%u rf_writes=%u cleanup_attempted=%u cleanup_complete=%u target_complete=%u target_vco_clear=%u cleanup_vco_clear=%u target_polls=%u cleanup_polls=%u target_delay_us=%u cleanup_delay_us=%u saved_mask=%u touched_mask=%u restored_mask=%u callbacks=%u exchanges=%u owned_checks=%u retained=1 network_data_out=0 receiver_restart=%u\n",(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)s->status,(uint64_t)s->channel,(uint64_t)s->steps,(uint64_t)s->guard_reads,(uint64_t)s->rf_reads,(uint64_t)s->rf_writes,(uint64_t)s->cleanup_attempted,(uint64_t)s->cleanup_complete,(uint64_t)s->target_complete,(uint64_t)s->target_vco_clear,(uint64_t)s->cleanup_vco_clear,(uint64_t)s->target_polls,(uint64_t)s->cleanup_polls,(uint64_t)s->target_delay_us,(uint64_t)s->cleanup_delay_us,(uint64_t)s->saved_mask,(uint64_t)s->touched_mask,(uint64_t)s->restored_mask,(uint64_t)t->callbacks,(uint64_t)t->exchanges,(uint64_t)t->owned_checks,(uint64_t)d->firmware.mcu.receive.posted);
    if(d->firmware.mcu.receive_mode)wifi_receive_log(d);
}
