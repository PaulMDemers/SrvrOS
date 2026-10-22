/* One-shot MCU identity diagnostic, optionally preceded by guarded MAC/BBP
 * reset. No RF, channel, power-table or transmitter-enable writes.
 * No timeout recovery/retry and no DMA reclamation in this path. */
static bool wifi_mcu_fail(struct xhci_device *d) {
    d->firmware.mcu.failed=true;
    return wifi_firmware_fail(d);
}
static bool wifi_mcu_owned(struct xhci_device *d) {
    return d->present && wifi_firmware_owned(d) && d->firmware.ready &&
        !d->firmware.pending && !d->firmware.mcu.failed;
}
static bool wifi_mcu_event(struct xhci_device *d,const struct xhci_trb *event) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    uint64_t pointer=event->parameter_low|((uint64_t)event->parameter_high<<32);
    if(!m->pending || ((event->control>>16)&31)!=m->dci || pointer!=m->trb)return false;
    m->completion=event->status>>24;m->remaining=event->status&0xffffff;
    m->done=true;m->pending=false;
    bool short_in=m->dci==11 && m->completion==13;
    if((event->control&4) || (m->completion!=1 && !short_in) ||
        m->remaining>m->requested || (m->dci==16 && m->remaining))wifi_mcu_fail(d);
    return true;
}
static bool configure_wifi_mcu(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_mcu_owned(d) || m->response.ring || m->buffer || firmware_command_failed)return false;
    void *virt;
    if(!alloc_frame_zero(&m->response.ring_phys,&virt))return wifi_mcu_fail(d);
    m->response.ring=virt;m->response.cycle=true;m->response.dci=11;m->response.packet=512;
    write_link_trb(m->response.ring,m->response.ring_phys,true);
    if(!alloc_frame_zero(&m->buffer_phys,&virt))return wifi_mcu_fail(d);
    m->buffer=virt;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<11));
    context_write32(d->input_context,1,0,(context_read32(d->output_context,0,0)&~(31u<<27))|(16u<<27));
    for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    context_write32(d->input_context,12,1,(3u<<1)|(6u<<3)|(512u<<16));
    context_write32(d->input_context,12,2,(uint32_t)(m->response.ring_phys|1));
    context_write32(d->input_context,12,3,(uint32_t)(m->response.ring_phys>>32));
    context_write32(d->input_context,12,4,MT76_MCU_RESPONSE_MAX);
    struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,
        .parameter_high=(uint32_t)(d->input_context_phys>>32),
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t flags=event_irq_save(),slot=0;
    firmware_command_exact=true;
    bool ok=submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&slot);
    firmware_command_exact=false;
    if(!ok || slot!=d->slot_id)firmware_command_failed=true;
    event_irq_restore(flags);
    if(firmware_command_failed)return wifi_mcu_fail(d);
    m->configured=true;return true;
}
static int wifi_mcu_bulk(struct xhci_device *d,bool in,size_t size) {
    struct xhci_wifi_firmware *f=&d->firmware;struct xhci_wifi_mcu *m=&f->mcu;
    if(!wifi_mcu_owned(d) || !m->configured || m->pending || !size ||
        size>(in?MT76_MCU_RESPONSE_MAX:MT76_MCU_PACKET_MAX))return -1;
    struct xhci_bulk_endpoint *ep=in?&m->response:&f->ep;
    uint64_t phys=in?m->buffer_phys:f->buffer_phys;
    if(in)zero_bytes(m->buffer,size);
    struct xhci_trb trb={.parameter_low=(uint32_t)phys,.parameter_high=(uint32_t)(phys>>32),
        .status=(uint32_t)size,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    uint64_t flags=event_irq_save();
    m->dci=ep->dci;m->requested=(uint32_t)size;m->remaining=0;m->completion=0;
    m->done=false;m->pending=true;
    m->trb=ring_push(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&trb);
    db_write32(d->slot_id,m->dci);event_irq_restore(flags);
    uint64_t start=timer_monotonic_us(),clock=start;unsigned stalled=0;
    for(;;) {
        flags=event_irq_save();struct xhci_trb event;
        if(!m->done && poll_event(&event))dispatch_transfer_event(&event);
        bool done=m->done;event_irq_restore(flags);
        if(done)return m->failed?-1:(int)(size-m->remaining);
        uint64_t now=timer_monotonic_us();
        if(now-start>=500000 || (now==clock && ++stalled>=XHCI_TRANSFER_TIMEOUT))break;
        if(now!=clock){clock=now;stalled=0;}
        __asm__ volatile("pause");
    }
    wifi_mcu_fail(d);return -1;
}
static int wifi_mcu_read_identity(struct xhci_device *d,int *queue_sent,int *read_sent) {
    struct xhci_wifi_firmware *f=&d->firmware;
    *queue_sent=-1;*read_sent=-1;
    if(!wifi_mcu_owned(d) || !f->mcu.configured || f->mcu.pending)return -1;
    size_t bytes=mt76_mcu_select_response_queue(f->buffer,MT76_MCU_PACKET_MAX);
    if(!bytes)return -1;
    *queue_sent=wifi_mcu_bulk(d,false,bytes);
    if(*queue_sent!=(int)bytes)return -1;
    /* Same OUT ring guarantees queue selection precedes this read. */
    const struct mt76_mcu_register_pair identity_read={0x1000,0};
    bytes=mt76_mcu_register_packet(false,1,&identity_read,1,f->buffer,MT76_MCU_PACKET_MAX);
    if(!bytes)return -1;
    *read_sent=wifi_mcu_bulk(d,false,bytes);
    if(*read_sent!=(int)bytes)return -1;
    return wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
}

static bool wifi_radio_owned(void *context) {
    struct xhci_device *d=context;
    return wifi_mcu_owned(d) && d->firmware.radio_reset_active &&
        !d->firmware.mcu.pending && !d->firmware.mcu.configured;
}
static bool wifi_radio_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_radio_owned(d) ||
        (address!=0x1000 && address!=0x1004 && address!=0x208 && address!=0x1200))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_radio_write(void *context,uint32_t address,uint32_t value) {
    struct xhci_device *d=context;struct xhci_wifi_firmware *f=&d->firmware;
    if(!wifi_radio_owned(d) || address!=0x1004 || !f->radio_reset.reset_attempted ||
        f->radio_reset_writes>=2 || f->radio_reset.writes!=f->radio_reset_writes+1 ||
        value!=(f->radio_reset_writes?0u:3u) ||
        (f->radio_reset_writes && f->radio_reset.delay_us<200000))return false;
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=(uint8_t)(value>>(8*i));
    struct usb_setup_packet setup={.request_type=0x40,.request=6,.index=0x1004,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,false,&actual) || actual!=4)return wifi_mcu_fail(d);
    f->radio_reset_writes++;return true;
}
static bool wifi_radio_delay(void *context,uint32_t us) {
    return wifi_radio_owned(context) && (us==1000 || us==5000) && timer_delay_us(us);
}
static bool wifi_radio_reset(struct xhci_device *d) {
    struct xhci_wifi_firmware *f=&d->firmware;
    if(!wifi_mcu_owned(d) || f->radio_reset.attempted || f->mcu.configured)return false;
    f->radio_reset_active=true;
    const struct mt76_radio_io io={{d,wifi_radio_read,wifi_radio_write,wifi_radio_delay},wifi_radio_owned};
    enum mt76_radio_status status=mt76_radio_prepare_reset(&f->radio_reset,&io,&d->wifi_identity);
    f->radio_reset_active=false;
    console_printf("wifi-radio-reset: result=%u attempted=%u reset_attempted=%u released=%u prepared=%u reads=%u writes=%u hold_us=%u last_register=%x last_value=%x\n",
        (uint64_t)status,(uint64_t)f->radio_reset.attempted,(uint64_t)f->radio_reset.reset_attempted,
        (uint64_t)f->radio_reset.reset_released,(uint64_t)f->radio_reset.prepared,
        (uint64_t)f->radio_reset.reads,(uint64_t)f->radio_reset.writes,(uint64_t)f->radio_reset.delay_us,
        (uint64_t)f->radio_reset.last_address,(uint64_t)f->radio_reset.last_value);
    if(status!=MT76_RADIO_OK)return wifi_mcu_fail(d);
    return true;
}

/* Fixed diagnostic only: no caller-supplied register/value and no retry. */
static int wifi_mcu_write_disabled(struct xhci_device *d,int *sent) {
    struct xhci_wifi_firmware *f=&d->firmware;
    struct xhci_wifi_mcu *m=&f->mcu;
    *sent=-1;
    if(!wifi_mcu_owned(d) || !m->configured || m->pending ||
        !m->identity_validated || !f->radio_reset.prepared || m->write_attempted)return -1;
    m->write_attempted=true;
    const struct mt76_mcu_register_pair disabled={0x1004,0};
    size_t bytes=mt76_mcu_register_packet(true,2,&disabled,1,f->buffer,MT76_MCU_PACKET_MAX);
    if(!bytes)return -1;
    *sent=wifi_mcu_bulk(d,false,bytes);
    if(*sent!=(int)bytes)return -1;
    return wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
}
static bool wifi_mcu_capture_write(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    uint32_t before=~0u,after=~0u;int sent=-1,received=-1;
    bool disabled=wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&before) && before==0;
    if(disabled)received=wifi_mcu_write_disabled(d,&sent);
    struct mt76_mcu_response r={0};
    bool decoded=received>=0 && mt76_mcu_decode(m->buffer,(size_t)received,&r);
    /* Capture only a bounded command response, not network data. No radio RX
     * has been enabled. Only the exact captured fixed-write reply is accepted. */
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;
        for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-mcu-write-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    bool observed=decoded && received<=64 && !(received&3) && r.sequence==2 && r.event==0;
    bool validated=received>=0 && mt76_mcu_validate_disabled_write_reply(m->buffer,(size_t)received);
    disabled=disabled && wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&after) && after==0;
    console_printf("wifi-mcu-write-capture: attempted=%u sent=%d received=%d decoded=%u sequence=%u event=%u reported_length=%u cc=%u residue=%u before=%x after=%x mac_disabled=%u observed=%u ack_validated=%u\n",
        (uint64_t)m->write_attempted,(int64_t)sent,(int64_t)received,(uint64_t)decoded,
        (uint64_t)r.sequence,(uint64_t)r.event,(uint64_t)r.reported_length,
        (uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)before,(uint64_t)after,
        (uint64_t)disabled,(uint64_t)observed,(uint64_t)validated);
    if(!validated || !disabled)return wifi_mcu_fail(d);
    m->write_validated=true;
    return true; /* Fixed disabled-MAC write only; no table acceptance. */
}

/* Fixed duplicate-MAC or opt-in timing pairs; neither enables MAC. */
static int wifi_mcu_write_pair(struct xhci_device *d,int *sent,bool timing) {
    struct xhci_wifi_firmware *f=&d->firmware;
    struct xhci_wifi_mcu *m=&f->mcu;
    *sent=-1;
    if(!wifi_mcu_owned(d) || !m->configured || m->pending ||
        !m->identity_validated || !f->radio_reset.prepared || !m->write_validated ||
        !m->write_attempted || m->pair_attempted || (timing && !m->timing_active))return -1;
    m->pair_attempted=true;
    const struct mt76_mcu_register_pair pairs[2]={
        {timing?0x1100u:0x1004u,timing?0x33a41010u:0u},
        {timing?0x1104u:0x1004u,timing?0x209u:0u}};
    size_t bytes=mt76_mcu_register_packet(true,3,pairs,2,f->buffer,MT76_MCU_PACKET_MAX);
    if(!bytes)return -1;
    *sent=wifi_mcu_bulk(d,false,bytes);
    if(*sent!=(int)bytes)return -1;
    return wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
}
/* Read-only binding restricted to this timing diagnostic and exact offsets. */
static bool wifi_mcu_timing_read(struct xhci_device *d,uint32_t address,uint32_t *value) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!value || !wifi_mcu_owned(d) || !m->timing_active || !m->write_validated ||
        m->pending || (address!=0x1100 && address!=0x1104))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static bool wifi_timing_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->timing_active && !m->pending &&
        m->identity_validated && m->write_validated && d->firmware.radio_reset.prepared;
}
static bool wifi_timing_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!wifi_timing_owned(d))return false;
    return address==0x1004?wifi_fw_read(d,address,value):wifi_mcu_timing_read(d,address,value);
}
static int wifi_timing_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    static const uint8_t allowed[24]={16,0,0xc3,0x50,0,0x11,0x41,0,
        0x10,0x10,0xa4,0x33,4,0x11,0x41,0,9,2,0,0,0,0,0,0};
    if(!wifi_timing_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    int received=wifi_mcu_write_pair(d,&m->timing_sent,true);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_timing(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    m->timing_active=true;m->timing_sent=-1;
    const struct mt76_mcu_timing_io io={d,wifi_timing_owned,wifi_timing_read,wifi_timing_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_timing(&m->timing,&io);
    m->timing_active=false;
    struct mt76_mcu_timing *s=&m->timing;
    console_printf("wifi-timing-init: status=%u attempted=%u write_attempted=%u sent=%d received=%d ack_validated=%u complete=%u reads=%u mac_before=%x mac_after=%x before0=%x before1=%x after0=%x after1=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->write_attempted,
        (int64_t)m->timing_sent,(int64_t)s->received,(uint64_t)s->ack_validated,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after,
        (uint64_t)s->before[0],(uint64_t)s->before[1],(uint64_t)s->after[0],(uint64_t)s->after[1]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}
static bool wifi_mcu_capture_pair(struct xhci_device *d,bool timing) {
    if(timing)return wifi_mcu_initialize_timing(d);
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    uint32_t before=~0u,after=~0u;int sent=-1,received=-1;
    bool disabled=wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&before) && before==0;
    if(disabled)received=wifi_mcu_write_pair(d,&sent,false);
    struct mt76_mcu_response r={0};
    bool decoded=received>=0 && mt76_mcu_decode(m->buffer,(size_t)received,&r);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;
        for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-mcu-pair-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    bool observed=decoded && received<=64 && !(received&3) && r.sequence==3 && r.event==0;
    disabled=disabled && wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&after) && after==0;
    console_printf("wifi-mcu-pair-capture: attempted=%u sent=%d received=%d decoded=%u sequence=%u event=%u reported_length=%u cc=%u residue=%u before=%x after=%x mac_disabled=%u observed=%u ack_validated=0\n",
        (uint64_t)m->pair_attempted,(int64_t)sent,(int64_t)received,(uint64_t)decoded,
        (uint64_t)r.sequence,(uint64_t)r.event,(uint64_t)r.reported_length,
        (uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)before,(uint64_t)after,
        (uint64_t)disabled,(uint64_t)observed);
    if(!observed || !disabled)return wifi_mcu_fail(d);
    return true; /* Captured evidence only, no multi-pair ACK acceptance yet. */
}

static bool wifi_capture_mcu(bool reset,bool write_probe,bool pair_probe,bool timing) {
    unsigned found=0;bool saved=true;
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *d=&xhci.devices[i];struct xhci_wifi_firmware *f=&d->firmware;
        struct xhci_wifi_mcu *m=&f->mcu;
        if(!d->present || !d->wifi_probe_attempted)continue;
        found++;
        if(!wifi_firmware_enabled || !f->ready || f->active || m->attempted){saved=false;continue;}
        m->attempted=true;f->active=true;
        uint32_t mac=0;int sent=-1,received=-1,queue_sent=-1;struct mt76_mcu_response r={0};
        bool disabled=wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&mac) && mac==0x20;
        if(disabled && reset)disabled=wifi_radio_reset(d);
        if(disabled && configure_wifi_mcu(d)) {
            received=wifi_mcu_read_identity(d,&queue_sent,&sent);
        }
        console_printf("wifi-mcu-queue: sent=%d expected=16 reply_requested=0\n",(int64_t)queue_sent);
        bool decoded=received>=0 && mt76_mcu_decode(m->buffer,(size_t)received,&r);
        uint32_t words[4]={0};
        if(received>=0)for(unsigned n=0;n<4 && (int)(n*4+4)<=received;n++)
            for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[n*4+b]<<(8*b);
        /* Fixed register-read reply only, never packet/credential capture. */
        console_printf("wifi-mcu-reply: word0=%x word1=%x word2=%x word3=%x\n",
            (uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
        disabled=disabled && wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&mac) && mac==(reset?0u:0x20u);
        bool validated=decoded && mt76_mcu_validate_identity_reply(m->buffer,(size_t)received);
        console_printf("wifi-mcu-capture: slot=%u sent=%d received=%d decoded=%u sequence=%u event=%u reported_length=%u cc=%u residue=%u mac_disabled=%u failed=%u validated=%u\n",
            (uint64_t)d->slot_id,(int64_t)sent,(int64_t)received,(uint64_t)decoded,
            (uint64_t)r.sequence,(uint64_t)r.event,(uint64_t)r.reported_length,
            (uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)disabled,(uint64_t)m->failed,(uint64_t)validated);
        /* Only this identity-read exchange is validated; RF remains untested. */
        if(!validated || !disabled){wifi_mcu_fail(d);saved=false;}
        else {
            m->identity_validated=true;
            if(write_probe && !wifi_mcu_capture_write(d))saved=false;
            else if(pair_probe && !wifi_mcu_capture_pair(d,timing))saved=false;
        }
        f->active=false;
    }
    return found && saved;
}

bool xhci_wifi_capture_mcu(void) { return wifi_capture_mcu(false,false,false,false); }
bool xhci_wifi_capture_radio_reset(void) { return wifi_capture_mcu(true,false,false,false); }

bool xhci_wifi_capture_write_probe(void) { return wifi_capture_mcu(true,true,false,false); }

bool xhci_wifi_capture_pair_probe(void) { return wifi_capture_mcu(true,true,true,false); }

bool xhci_wifi_capture_timing_probe(void) { return wifi_capture_mcu(true,true,true,true); }
