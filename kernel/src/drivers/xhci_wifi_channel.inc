/* v97 only: reversible frequency-field preview with main RX already stopped.
 * Independent request ordering/value gate; cleanup survives forward metadata or
 * reply loss, but never quarantined/changed transport or lost stopped context. */
static void wifi_channel_identity(struct xhci_device *d,uint64_t v[18]) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_firmware *f=&d->firmware;
    const uint64_t a[18]={d->slot_id,(uintptr_t)d->ep0_ring,d->ep0_ring_phys,
        (uintptr_t)d->descriptor_buffer,d->descriptor_buffer_phys,(uintptr_t)d->output_context,
        (uintptr_t)f->buffer,f->buffer_phys,(uintptr_t)f->ep.ring,f->ep.ring_phys,
        (uintptr_t)m->buffer,m->buffer_phys,(uintptr_t)m->response.ring,m->response.ring_phys,
        m->rx.trb,m->rx.stop_dequeue,m->window.plan.ring,((uint64_t)f->ep.dci<<32)|m->response.dci};
    for(unsigned i=0;i<18;i++)v[i]=a[i];
}
static bool wifi_channel_stopped(struct xhci_device *d) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    if(!d->output_context || !r->stopped || r->pending || r->failed || !r->stop_attempted ||
       r->stop_state!=3 || (r->stop_dequeue&15)!=1 ||
       ((r->stop_dequeue&~15ull)!=r->trb && (r->stop_dequeue&~15ull)!=r->trb+16) ||
       !wifi_rx_guards_valid(r,6) || !wifi_window_layout(d))return false;
    __asm__ volatile("" ::: "memory");
    return (context_read32(d->output_context,9,0)&7)==3 &&
        (context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32))==r->stop_dequeue;
}
static bool wifi_channel_owned(void *context,bool cleanup) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_channel *c=&m->channel;c->owned_checks++;
    if(!m->channel_mode || !c->active || !c->pinned || !c->state.attempted ||
       !wifi_mcu_owned(d) || !d->present || d->vendor_id!=0x7392 || d->product_id!=0xa711 ||
       d->device_bcd!=0x100 || !d->slot_id || !d->ep0_ring || !d->descriptor_buffer ||
       !m->configured || m->pending || !d->firmware.ready || !d->firmware.buffer ||
       !m->buffer || !d->firmware.ep.ring || !m->response.ring ||
       d->firmware.ep.dci!=16 || m->response.dci!=11 ||
       d->firmware.ep.packet!=512 || m->response.packet!=512 ||
       c->forward>2558 || c->callbacks>2592 || c->restore_index>13 || c->cleanup_phase>4 ||
       c->guard_index>4 || c->published>8191 || c->cleanup_mask>8191 ||
       firmware_command_failed || strict_control.device || !wifi_channel_stopped(d))return false;
    uint64_t now[18];wifi_channel_identity(d,now);
    for(unsigned i=0;i<18;i++)if(!now[i] || now[i]!=c->identity[i])return false;
    if(d->firmware.buffer==m->buffer ||
       pmm_phys_to_virt(d->descriptor_buffer_phys)!=d->descriptor_buffer ||
       pmm_phys_to_virt(d->firmware.buffer_phys)!=d->firmware.buffer ||
       pmm_phys_to_virt(m->buffer_phys)!=m->buffer ||
       pmm_phys_to_virt(d->firmware.ep.ring_phys)!=d->firmware.ep.ring ||
       pmm_phys_to_virt(m->response.ring_phys)!=m->response.ring)return false;
    const struct mt76_channel_preview_plan *p=&c->state.plan;
    if(p->country[0]!='U' || p->country[1]!='S' || p->bandwidth!=20 || p->count!=11)return false;
    for(unsigned i=0;i<11;i++)if(p->channel[i]!=i+1)return false;
    return cleanup || (!c->forward_failed && !c->state.failed && m->window.state.complete && !m->window.state.failed &&
        m->window.state.stopped && wifi_window_owned(d,true) &&
        m->discovery.complete && !m->discovery.failed &&
        mt76_ant_fields_valid(&m->ant_fields) && mt76_trim_fields_valid(&m->trim_fields));
}
static unsigned wifi_channel_index(unsigned reg) {
    for(unsigned i=0;i<13;i++)if(mt76_channel_snapshot_reg(i)==reg)return i;
    return 13;
}
static unsigned wifi_channel_target(unsigned q) {
    uint8_t current[13];struct mt76_channel_step plan[21];
    for(unsigned i=0;i<13;i++)current[i]=(uint8_t)mt76_channel_snapshot_initial(i);
    for(unsigned n=0;n<=q;n++) {
        if(!(n%21) && !mt76_channel_plan(n/21+1,plan))return UINT32_MAX;
        unsigned i=wifi_channel_index(plan[n%21].reg);if(i==13)return UINT32_MAX;
        current[i]=(uint8_t)((current[i]&~plan[n%21].mask)|plan[n%21].bits);
        if(n==q)return current[i];
    }
    return UINT32_MAX;
}
static bool wifi_channel_forward(unsigned op,unsigned *kind,unsigned *address,unsigned *value) {
    static const unsigned guards[4]={0x1004,0x2000,0x208,0x1200};
    if(op<4){*kind=0;*address=guards[op];return true;}
    if(op<17){*kind=1;*address=mt76_channel_snapshot_reg(op-4);return true;}
    if(op>=2558)return false;
    unsigned q=(op-17)/11,n=(op-17)%11;struct mt76_channel_step plan[21];
    if(!mt76_channel_plan(q/21+1,plan))return false;
    if(n<4 || n>=7){*kind=0;*address=guards[n<4?n:n-7];}
    else {*kind=n==5?2:1;*address=plan[q%21].reg;if(n==5)*value=wifi_channel_target(q);}
    return true;
}
static void wifi_channel_next_restore(struct xhci_wifi_channel *c) {
    while(c->restore_index && !(c->cleanup_mask&(1u<<(c->restore_index-1))))c->restore_index--;
    if(!c->restore_index){c->cleanup_phase=3;c->guard_index=0;}
}
static bool wifi_channel_request(struct xhci_device *d,bool cleanup,unsigned kind,unsigned a,unsigned value) {
    struct xhci_wifi_channel *c=&d->firmware.mcu.channel;
    if(!wifi_channel_owned(d,cleanup) || c->callbacks>=2592)return false;
    if(!cleanup || (!c->cleanup_started && kind==1 && c->last_failed_write && !c->state.cleanup_attempted)) {
        unsigned k=99,addr=UINT32_MAX,v=0;
        if(c->cleanup_started || !wifi_channel_forward(c->forward,&k,&addr,&v) || k!=kind || a!=addr || (kind==2 && value!=v))return false;
        unsigned bit=kind==2?1u<<wifi_channel_index(a):0;
        if(c->state.touched_mask!=(c->published|bit))return false;
        c->published|=(uint16_t)bit;c->forward++;c->callbacks++;c->last_failed_write=false;return true;
    }
    static const unsigned address[4]={0x1004,0x2000,0x208,0x1200};
    if(!c->cleanup_started) {
        if(kind!=0 || a!=address[0] || c->state.touched_mask!=c->published)return false;
        c->cleanup_started=true;c->cleanup_mask=c->published;c->cleanup_phase=0;c->guard_index=0;c->restore_index=13;
    }
    if(c->cleanup_phase==0 || c->cleanup_phase==3) {
        if(kind!=0 || c->guard_index>=4 || a!=address[c->guard_index])return false;
        c->guard_index++;c->callbacks++;return true;
    }
    wifi_channel_next_restore(c);
    if(!c->restore_index || kind!=(c->cleanup_phase==1?2u:1u) ||
       a!=mt76_channel_snapshot_reg(c->restore_index-1) ||
       (kind==2 && value!=mt76_channel_snapshot_initial(c->restore_index-1)) ||
       c->state.saved[c->restore_index-1]!=mt76_channel_snapshot_initial(c->restore_index-1))return false;
    c->callbacks++;
    if(kind==2)c->cleanup_phase=2;
    else {c->restore_index--;c->cleanup_phase=1;wifi_channel_next_restore(c);}
    return true;
}
static bool wifi_channel_guard(void *context,uint32_t a,uint32_t *value,bool cleanup) {
    struct xhci_device *d=context;struct xhci_wifi_channel *c=&d->firmware.mcu.channel;
    if(!value || !wifi_channel_request(d,cleanup,0,a,0))return false;
    *value=0;bool ok=wifi_mac_private_usb(d,false,a,value);
    console_printf("wifi-channel-guard: index=%u cleanup=%u address=%x value=%x ok=%u\n",(uint64_t)(c->callbacks-1),(uint64_t)cleanup,(uint64_t)a,(uint64_t)*value,(uint64_t)ok);
    if(cleanup) {
        uint32_t expected=a==0x2000?0xf000f200:a==0x208?0x40000050:0;
        c->cleanup_bad|=!ok || *value!=expected;
        if(c->guard_index==4) {
            if(c->cleanup_phase==0) {
                c->cleanup_phase=(c->cleanup_bad || !c->cleanup_mask)?3:1;
                c->guard_index=0;wifi_channel_next_restore(c);
            } else c->cleanup_phase=4;
        }
    }
    return ok;
}
static bool wifi_channel_rf(struct xhci_device *d,bool cleanup,bool write,uint8_t reg,uint8_t *value) {
    struct xhci_wifi_channel *c=&d->firmware.mcu.channel;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!value || !wifi_channel_request(d,cleanup,write?2:1,reg,write?*value:0))return false;
    unsigned seq=c->exchanges%15+1;uint32_t address=0x80000000u|reg;uint8_t payload[8]={0};
    for(unsigned i=0;i<4;i++)payload[i]=(uint8_t)(address>>(8*i));
    if(write)payload[4]=*value;
    if(mt76_mcu_encode(write?12:10,seq,payload,8,d->firmware.buffer,16)!=16)return false;
    c->exchanges++;
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    uint32_t h=0x4a000008u|(seq<<16);
    unsigned contract=(words[0]==h?1u:0u)|(words[1]==address?2u:0u)|
        (words[2]<=255?4u:0u)|(words[3]==h?8u:0u)|(!write || words[2]==*value?16u:0u);
    bool ok=sent==16 && received==16 && m->completion==13 &&
        m->remaining==MT76_MCU_RESPONSE_MAX-16 && contract==31;
    console_printf("wifi-channel-rf: index=%u cleanup=%u write=%u reg=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x ok=%u contract_mask=%u response_redacted=%u\n",(uint64_t)(c->callbacks-1),(uint64_t)cleanup,(uint64_t)write,(uint64_t)reg,(uint64_t)seq,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)(ok?words[0]:0),(uint64_t)(ok?words[1]:0),(uint64_t)(ok?words[2]:0),(uint64_t)(ok?words[3]:0),(uint64_t)ok,(uint64_t)contract,(uint64_t)!ok);
    if(ok && !write)*value=(uint8_t)words[2];
    if(!ok && !cleanup)c->forward_failed=true;
    if(write && !ok && !cleanup)c->last_failed_write=true;
    return ok;
}
static bool wifi_channel_read(void *d,uint8_t reg,uint8_t *v,bool cleanup){return wifi_channel_rf(d,cleanup,false,reg,v);}
static bool wifi_channel_write(void *d,uint8_t reg,uint8_t v,bool cleanup){return wifi_channel_rf(d,cleanup,true,reg,&v);}
static bool wifi_channel_capture(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_channel *c=&m->channel;
    if(!m->channel_mode || c->state.attempted || c->active || !m->window.state.complete ||
       m->window.state.failed || !m->window.state.stopped || !m->discovery.complete ||
       m->discovery.failed || !wifi_window_owned(d,true) || !wifi_channel_stopped(d) ||
       !mt76_ant_fields_valid(&m->ant_fields) || !mt76_trim_fields_valid(&m->trim_fields))return false;
    *c=(struct xhci_wifi_channel){.active=true,.pinned=true};wifi_channel_identity(d,c->identity);
    struct mt76_channel_preview_plan plan={.country={'U','S'},.bandwidth=20,.count=11};
    for(unsigned i=0;i<11;i++)plan.channel[i]=(uint8_t)(i+1);
    console_write("wifi-channel-progress: main RX stopped; previewing US1-11 frequency fields then restoring channel1; no VCO or reception\n");
    const struct mt76_channel_preview_io io={d,wifi_channel_owned,wifi_channel_guard,wifi_channel_read,wifi_channel_write};
    bool ok=mt76_channel_preview_run(&c->state,&plan,&io);
    ok=ok && m->window.state.complete && !m->window.state.failed && m->window.state.stopped && wifi_window_owned(d,true) && m->discovery.complete && !m->discovery.failed &&
        mt76_ant_fields_valid(&m->ant_fields) && mt76_trim_fields_valid(&m->trim_fields) &&
        c->forward==2558 && c->cleanup_phase==4 && c->callbacks==2592 && c->exchanges==732;
    if(!ok){c->state.complete=false;c->state.failed=true;if(c->state.status==MT76_CHANNEL_OK)c->state.status=MT76_CHANNEL_STATE;}
    c->active=false;return ok;
}
static void wifi_channel_log(struct xhci_device *d) {
    struct xhci_wifi_channel *c=&d->firmware.mcu.channel;struct mt76_channel_preview *s=&c->state;
    console_printf("wifi-channel: attempted=%u complete=%u failed=%u status=%u channels=%u steps=%u guard_reads=%u rf_reads=%u rf_writes=%u cleanup_attempted=%u cleanup_complete=%u cleanup_reads=%u cleanup_writes=%u saved_mask=%u touched_mask=%u restored_mask=%u callbacks=%u exchanges=%u owned_checks=%u forward=%u cleanup_phase=%u retained=1 network_data_out=0 vco=0\n",(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->failed,(uint64_t)s->status,(uint64_t)s->channels_completed,(uint64_t)s->steps_completed,(uint64_t)s->guard_reads,(uint64_t)s->rf_reads,(uint64_t)s->rf_writes,(uint64_t)s->cleanup_attempted,(uint64_t)s->cleanup_complete,(uint64_t)s->cleanup_reads,(uint64_t)s->cleanup_writes,(uint64_t)s->saved_mask,(uint64_t)s->touched_mask,(uint64_t)s->restored_mask,(uint64_t)c->callbacks,(uint64_t)c->exchanges,(uint64_t)c->owned_checks,(uint64_t)c->forward,(uint64_t)c->cleanup_phase);
}
