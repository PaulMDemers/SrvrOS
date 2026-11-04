#include <srvros/mt76_trim5.h>
static bool wifi_5ghz_prereq_requested;
static bool wifi_5ghz_band_requested;
static bool wifi_5ghz_channel_requested;
static bool wifi_a20_inventory_requested;
static bool wifi_a20_cycle_requested;
static bool wifi_frontend_requested;
static bool wifi_pa5_requested;
static bool wifi_bbp5_requested;
static bool wifi_held_requested;
static bool wifi_trim5_requested;
static bool wifi_channel_vco5_requested;
static bool wifi_cal5_requested;
static bool wifi_mcu_channel_vco5(struct xhci_device *d);
static bool wifi_vco5_requested;
static bool wifi_mcu_vco5(struct xhci_device *d);
static bool wifi_dac5_requested;
static bool wifi_mcu_dac5(struct xhci_device *d);
static bool wifi_mcu_trim5(struct xhci_device *d);
static bool wifi_mcu_held(struct xhci_device *d);
/* One-shot MCU identity diagnostic, optionally preceded by guarded MAC/BBP
 * reset. Opt-in fixed RF initialization follows the full captured chain.
 * Fixed frequency-field preview is a separate explicit stopped-RX mode.
 * No network frames, scan service or general power-table programming.
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

static bool wifi_mac_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->mac_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && d->firmware.radio_reset.prepared;
}
static bool wifi_mac_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_mac_owned(d) || (address!=0x1004 && address!=0x1408 &&
        address!=0x140c && address!=0x1400 && address!=0x1018))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_mac_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->mac_submitted;uint8_t allowed[24];
    if(!wifi_mac_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=MT76_MCU_MAC_BATCHES ||
        !m->mac.attempted || m->mac.batches_attempted!=batch+1 ||
        m->mac.batches_completed!=batch || m->mac.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_mac_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(4+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-mac-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_mac(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->mac_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_owned,wifi_mac_read,wifi_mac_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_mac_basics(&m->mac,&io);
    m->mac_active=false;struct mt76_mcu_mac *s=&m->mac;
    console_printf("wifi-mac-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_format_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->format_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && m->mac.complete && m->mac.ack_mask==3 &&
        m->mac.batches_completed==2 && d->firmware.radio_reset.prepared;
}
static bool wifi_format_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_format_owned(d) || (address!=0x1004 && address!=0x0250 &&
        address!=0x0260 && address!=0x1410 && address!=0x150c))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_format_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->format_submitted;uint8_t allowed[24];
    if(!wifi_format_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=MT76_MCU_MAC_BATCHES ||
        !m->format.attempted || m->format.batches_attempted!=batch+1 ||
        m->format.batches_completed!=batch || m->format.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_format_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->format_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-format-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(6+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-format-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_format(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->format_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_format_owned,wifi_format_read,wifi_format_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_mac_format(&m->format,&io);
    m->format_active=false;struct mt76_mcu_mac *s=&m->format;
    console_printf("wifi-format-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->format_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-format-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_bbp_setup_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->bbp_setup_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && m->mac.complete && m->mac.ack_mask==3 &&
        m->mac.batches_completed==2 && m->format.complete && m->format.ack_mask==3 &&
        m->bbp.complete && m->bbp.core==0xf000f200 && d->firmware.radio_reset.prepared;
}
static bool wifi_bbp_setup_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_bbp_setup_owned(d) || (address!=0x1004 && address!=0x2004 &&
        address!=0x2010 && address!=0x2060 && address!=0x2080 && address!=0x2000))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_bbp_setup_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->bbp_setup_submitted;uint8_t allowed[24];
    if(!wifi_bbp_setup_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=MT76_MCU_MAC_BATCHES ||
        !m->bbp_setup.attempted || m->bbp_setup.batches_attempted!=batch+1 ||
        m->bbp_setup.batches_completed!=batch || m->bbp_setup.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_bbp_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->bbp_setup_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-bbp-core-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(8+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-bbp-core-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_bbp_core(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->bbp_setup_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_bbp_setup_owned,wifi_bbp_setup_read,wifi_bbp_setup_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_bbp_core(&m->bbp_setup,&io);
    m->bbp_setup_active=false;struct mt76_mcu_mac *s=&m->bbp_setup;
    console_printf("wifi-bbp-core-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->bbp_setup_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    console_printf("wifi-bbp-core-identity: before=%x after=%x\n",(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-core-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_bbp_front_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->bbp_front_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && m->mac.complete && m->mac.ack_mask==3 &&
        m->mac.batches_completed==2 && m->format.complete && m->format.ack_mask==3 &&
        m->bbp_setup.complete && m->bbp_setup.ack_mask==3 && m->bbp_setup.batches_completed==2 &&
        m->bbp.complete && m->bbp.core==0xf000f200 && d->firmware.radio_reset.prepared;
}
static bool wifi_bbp_front_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_bbp_front_owned(d) || (address!=0x1004 && address!=0x20a8 &&
        address!=0x20b0 && address!=0x212c && address!=0x2300 && address!=0x2000))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_bbp_front_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->bbp_front_submitted;uint8_t allowed[24];
    if(!wifi_bbp_front_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=MT76_MCU_MAC_BATCHES ||
        !m->bbp_front.attempted || m->bbp_front.batches_attempted!=batch+1 ||
        m->bbp_front.batches_completed!=batch || m->bbp_front.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_bbp_front_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->bbp_front_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-bbp-front-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(10+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-bbp-front-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_bbp_front(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->bbp_front_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_bbp_front_owned,wifi_bbp_front_read,wifi_bbp_front_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_bbp_front(&m->bbp_front,&io);
    m->bbp_front_active=false;struct mt76_mcu_mac *s=&m->bbp_front;
    console_printf("wifi-bbp-front-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->bbp_front_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    console_printf("wifi-bbp-front-identity: before=%x after=%x\n",(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-front-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_bbp_agc_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->bbp_agc_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && m->mac.complete && m->mac.ack_mask==3 &&
        m->mac.batches_completed==2 && m->format.complete && m->format.ack_mask==3 &&
        m->bbp_front.complete && m->bbp_front.ack_mask==3 && m->bbp_front.batches_completed==2 &&
        m->bbp_setup.complete && m->bbp_setup.ack_mask==3 && m->bbp_setup.batches_completed==2 &&
        m->bbp.complete && m->bbp.core==0xf000f200 && d->firmware.radio_reset.prepared;
}
static bool wifi_bbp_agc_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_bbp_agc_owned(d) || (address!=0x1004 && address!=0x2304 &&
        address!=0x2308 && address!=0x233c && address!=0x2358 && address!=0x2000))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_bbp_agc_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->bbp_agc_submitted;uint8_t allowed[24];
    if(!wifi_bbp_agc_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=MT76_MCU_MAC_BATCHES ||
        !m->bbp_agc.attempted || m->bbp_agc.batches_attempted!=batch+1 ||
        m->bbp_agc.batches_completed!=batch || m->bbp_agc.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_bbp_agc_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->bbp_agc_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-bbp-agc-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(12+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-bbp-agc-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_bbp_agc(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->bbp_agc_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_bbp_agc_owned,wifi_bbp_agc_read,wifi_bbp_agc_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_bbp_agc(&m->bbp_agc,&io);
    m->bbp_agc_active=false;struct mt76_mcu_mac *s=&m->bbp_agc;
    console_printf("wifi-bbp-agc-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->bbp_agc_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    console_printf("wifi-bbp-agc-identity: before=%x after=%x\n",(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-agc-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_bbp_batch_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->bbp_batch_active &&
        m->timing.complete && m->timing.ack_validated && m->identity_validated &&
        m->write_validated && m->mac.complete && m->mac.ack_mask==3 &&
        m->mac.batches_completed==2 && m->format.complete && m->format.ack_mask==3 &&
        m->bbp_front.complete && m->bbp_front.ack_mask==3 && m->bbp_front.batches_completed==2 &&
        m->bbp_setup.complete && m->bbp_setup.ack_mask==3 && m->bbp_setup.batches_completed==2 &&
        m->bbp.complete && m->bbp.core==0xf000f200 && d->firmware.radio_reset.prepared;
}
static bool wifi_bbp_batch_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_bbp_batch_owned(d) || (address!=0x1004 && address!=0x2304 &&
        address!=0x2308 && address!=0x233c && address!=0x2358 && address!=0x2000))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_bbp_batch_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned batch=m->bbp_batch_submitted;uint8_t allowed[40];
    if(!wifi_bbp_batch_owned(d) || !packet || !reply || length!=sizeof allowed ||
        capacity<MT76_MCU_RESPONSE_MAX || batch>=1 ||
        !m->bbp_batch.attempted || m->bbp_batch.batches_attempted!=batch+1 ||
        m->bbp_batch.batches_completed!=batch || m->bbp_batch.ack_mask!=((1u<<batch)-1) ||
        mt76_mcu_bbp_batch_packet(batch,allowed,sizeof allowed)!=sizeof allowed)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->bbp_batch_submitted++; /* Burn the command even if the transfer fails. */
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-bbp-batch-command: batch=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)batch,(uint64_t)(14+batch),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-bbp-batch-word: batch=%u index=%u value=%x\n",(uint64_t)batch,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_initialize_bbp_batch(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->bbp_batch_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_bbp_batch_owned,wifi_bbp_batch_read,wifi_bbp_batch_exchange};
    enum mt76_mcu_timing_status status=mt76_mcu_apply_bbp_batch(&m->bbp_batch,&io);
    m->bbp_batch_active=false;struct mt76_mcu_mac *s=&m->bbp_batch;
    console_printf("wifi-bbp-batch-init: status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->bbp_batch_submitted,
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after);
    console_printf("wifi-bbp-batch-identity: before=%x after=%x\n",(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-batch-readback: index=%u before=%x after=%x\n",
        (uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_compact_prefix(struct xhci_wifi_mcu *m,unsigned count) {
    if(count>MT76_MCU_COMPACT_PHASES)return false;
    for(unsigned i=0;i<count;i++)if(!m->compact[i].complete ||
        m->compact[i].ack_mask!=1 || m->compact[i].batches_completed!=1 ||
        m->compact_submitted[i]!=1)return false;
    return true;
}
static bool wifi_compact_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned phase=m->compact_next;
    return (!m->clock_pair_mode || (m->clock_mode && m->compact_limit==12 && (phase<2 || (m->clock.complete && m->clock.written && m->clock_submitted && !m->clock_active && m->clock.pbf_after==0x80c00 && m->bbp.pbf==0x80c00)))) && ((m->compact_limit>=5 && m->compact_limit<=12) || (m->table_mode && m->compact_limit==2)) && phase<m->compact_limit &&
        phase<MT76_MCU_COMPACT_PHASES && m->compact_mode && m->compact_active &&
        wifi_mcu_owned(d) && m->configured && !m->pending &&
        m->identity_validated && m->write_validated && m->timing.complete &&
        m->timing.ack_validated && d->firmware.radio_reset.prepared &&
        wifi_compact_prefix(m,phase) &&
        (phase<2 || (m->bbp.complete && m->bbp.core==0xf000f200));
}
static bool wifi_compact_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(d->firmware.mcu.clock_pair_mode && d->firmware.mcu.compact_next==11 &&
        address!=0x1004 && address!=0x2000 && address!=0x2724 && address!=0x2728)return false;
    if(!value || !wifi_compact_owned(d) ||
        !mt76_mcu_compact_read_allowed(d->firmware.mcu.compact_next,address))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return *value!=UINT32_MAX;
}
static int wifi_compact_exchange(void *context,const uint8_t *packet,size_t length,
    uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_compact_owned(d))return -1;
    unsigned phase=m->compact_next;struct mt76_mcu_mac *s=&m->compact[phase];
    uint8_t allowed[40];bool pair=m->clock_pair_mode && phase==11;size_t expected=pair?24:40;
    if(!packet || !reply || length!=expected || capacity<MT76_MCU_RESPONSE_MAX ||
        m->compact_submitted[phase] || !s->attempted || s->batches_attempted!=1 ||
        s->batches_completed || s->ack_mask ||
        (pair?mt76_mcu_clock_pair_packet(allowed,sizeof allowed):mt76_mcu_compact_packet(phase,allowed,sizeof allowed))!=expected)return -1;
    for(size_t i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->compact_submitted[phase]=1;
    for(size_t i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-compact-command: phase=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",
        (uint64_t)phase,(uint64_t)(4+phase),(int64_t)sent,(int64_t)received,
        (uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-compact-word: phase=%u index=%u value=%x\n",(uint64_t)phase,(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_compact_step(struct xhci_device *d,unsigned phase) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(phase>=MT76_MCU_COMPACT_PHASES || phase>=m->compact_limit || phase!=m->compact_next)return wifi_mcu_fail(d);
    m->compact_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_compact_owned,wifi_compact_read,wifi_compact_exchange};
    struct mt76_mcu_mac *s=&m->compact[phase];
    enum mt76_mcu_timing_status status=m->clock_pair_mode && phase==11?mt76_mcu_apply_clock_pair(s,&io):mt76_mcu_apply_compact(phase,s,&io);
    m->compact_active=false;
    console_printf("wifi-compact-init: phase=%u status=%u attempted=%u submitted=%u batches_attempted=%u batches_completed=%u ack_mask=%u complete=%u reads=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",
        (uint64_t)phase,(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->compact_submitted[phase],
        (uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,
        (uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->mac_before,(uint64_t)s->mac_after,
        (uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<((m->clock_pair_mode && phase==11)?2u:4u);i++)console_printf("wifi-compact-readback: phase=%u index=%u before=%x after=%x\n",
        (uint64_t)phase,(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    m->compact_next++;return true;
}

static bool wifi_txbe_base_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->compact_mode && !m->compact_active &&
        m->compact_limit==11 && m->compact_next==11 && wifi_compact_prefix(m,11) &&
        !m->compact_submitted[11] && !m->compact[11].attempted &&
        wifi_mcu_owned(d) && m->configured && !m->pending &&
        m->identity_validated && m->write_validated && m->timing.complete &&
        m->timing.ack_validated && d->firmware.radio_reset.prepared &&
        m->bbp.complete && m->bbp.core==0xf000f200;
}
static bool wifi_txbe_observe_owned(void *context) {
    struct xhci_device *d=context;
    return !d->firmware.mcu.txbe_access.attempted && !d->firmware.mcu.txbe_access_active && d->firmware.mcu.txbe_observe_active && !d->firmware.mcu.txbe_first_active && wifi_txbe_base_owned(d);
}
static bool wifi_txbe_observe_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!value || !wifi_txbe_observe_owned(d) || !mt76_txbe_read_allowed(address) ||
        m->txbe_observation.samples>=MT76_TXBE_SAMPLES)return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    if(address==0x2728)m->txbe_sample_us[m->txbe_observation.samples]=timer_monotonic_us();
    return true;
}
static bool wifi_mcu_observe_txbe(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    m->txbe_observe_active=true;
    const struct mt76_txbe_io io={d,wifi_txbe_observe_owned,wifi_txbe_observe_read};
    struct mt76_txbe_observation *s=&m->txbe_observation;
    enum mt76_mcu_timing_status status=mt76_txbe_observe(s,&io);
    m->txbe_observe_active=false;
    console_printf("wifi-txbe-observe: status=%u attempted=%u complete=%u reads=%u samples=%u\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->samples);
    /* Only emit fully guarded samples, never zero-initialized missing reads. */
    for(unsigned i=0;i<s->samples;i++) {
        uint32_t *v=s->values[i];
        console_printf("wifi-txbe-sample: index=%u monotonic_us=%u mac_before=%x core_before=%x txbe9=%x txbe10=%x txbe12=%x txbe13=%x mac_after=%x core_after=%x\n",
            (uint64_t)i,m->txbe_sample_us[i],(uint64_t)v[0],(uint64_t)v[1],
            (uint64_t)v[2],(uint64_t)v[3],(uint64_t)v[4],(uint64_t)v[5],(uint64_t)v[6],(uint64_t)v[7]);
    }
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_txbe_first_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return !m->txbe_access.attempted && !m->txbe_access_active && m->txbe_first_active && !m->txbe_observe_active && !m->txbe_observation.attempted && wifi_txbe_base_owned(d);
}
static bool wifi_txbe_first_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned stage=m->txbe_first.write_attempted?1:0;
    struct mt76_txbe_observation *s=stage?&m->txbe_first.after:&m->txbe_first.before;
    if(!value || !wifi_txbe_first_owned(d) || !mt76_txbe_read_allowed(address) ||
        s->samples>=MT76_TXBE_SAMPLES || (stage && !m->txbe_first.ack_validated))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    if(address==0x2728)m->txbe_first_us[stage][s->samples]=timer_monotonic_us();
    return true;
}
static int wifi_txbe_first_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_txbe_first *s=&m->txbe_first;uint8_t allowed[16];
    if(!wifi_txbe_first_owned(d) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX ||
        !s->attempted || !s->write_attempted || !s->before.complete || s->before.reads!=64 || s->before.samples!=8 ||
        s->after.attempted || s->ack_validated || m->txbe_first_submitted ||
        (m->txbe_second_mode?mt76_txbe_second_packet(allowed,sizeof allowed):mt76_txbe_first_packet(allowed,sizeof allowed))!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->txbe_first_submitted=true;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-txbe-first-command: sequence=15 sent=%d received=%d cc=%u residue=%u\n",
        (int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-txbe-first-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_txbe_first(struct xhci_device *d,bool second) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(m->txbe_first.attempted)return wifi_mcu_fail(d);
    m->txbe_first_active=true;m->txbe_second_mode=second;
    console_printf("wifi-txbe-target: register=%u\n",(uint64_t)(second?10:9));
    const struct mt76_mcu_timing_io io={d,wifi_txbe_first_owned,wifi_txbe_first_read,wifi_txbe_first_exchange};
    struct mt76_txbe_first *s=&m->txbe_first;
    enum mt76_mcu_timing_status status=second?mt76_txbe_second_apply(s,&io):mt76_txbe_first_apply(s,&io);
    m->txbe_first_active=false;
    console_printf("wifi-txbe-first: status=%u attempted=%u write_attempted=%u submitted=%u received=%d ack_validated=%u complete=%u before_reads=%u before_samples=%u after_reads=%u after_samples=%u\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->write_attempted,(uint64_t)m->txbe_first_submitted,
        (int64_t)s->received,(uint64_t)s->ack_validated,(uint64_t)s->complete,
        (uint64_t)s->before.reads,(uint64_t)s->before.samples,(uint64_t)s->after.reads,(uint64_t)s->after.samples);
    for(unsigned stage=0;stage<2;stage++) {
        struct mt76_txbe_observation *o=stage?&s->after:&s->before;
        for(unsigned i=0;i<o->samples;i++) {
            uint32_t *v=o->values[i];
            console_printf("wifi-txbe-first-sample: stage=%u index=%u monotonic_us=%u mac_before=%x core_before=%x txbe9=%x txbe10=%x txbe12=%x txbe13=%x mac_after=%x core_after=%x\n",
                (uint64_t)stage,(uint64_t)i,m->txbe_first_us[stage][i],(uint64_t)v[0],(uint64_t)v[1],
                (uint64_t)v[2],(uint64_t)v[3],(uint64_t)v[4],(uint64_t)v[5],(uint64_t)v[6],(uint64_t)v[7]);
        }
    }
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_txbe_access_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return (!m->clock_mode || (m->clock.complete && m->clock.written && m->clock_submitted && !m->clock_active && m->clock.pbf_after==0x80c00 && m->bbp.pbf==0x80c00)) && !m->txbe_first.attempted && !m->txbe_first_active && m->txbe_access_active && !m->txbe_observe_active && !m->txbe_observation.attempted && wifi_txbe_base_owned(d);
}
static bool wifi_txbe_access_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    unsigned stage=m->txbe_access.request_attempted?1:0;
    struct mt76_txbe_observation *s=stage?&m->txbe_access.after:&m->txbe_access.before;
    if(!value || !wifi_txbe_access_owned(d) || !mt76_txbe_read_allowed(address) ||
        s->samples>=MT76_TXBE_SAMPLES || (stage && !m->txbe_access.reply_validated))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    if(address==0x2728)m->txbe_access_us[stage][s->samples]=timer_monotonic_us();
    return true;
}
static int wifi_txbe_access_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_txbe_access *s=&m->txbe_access;uint8_t allowed[16];
    if(!wifi_txbe_access_owned(d) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX ||
        !s->attempted || !s->request_attempted || !s->before.complete || s->before.reads!=64 || s->before.samples!=8 ||
        s->after.attempted || s->reply_validated || m->txbe_access_submitted ||
        (m->txbe_direct_mode && (!s->control_written || !m->txbe_direct_submitted)) ||
        mt76_txbe_access_packet(allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->txbe_access_submitted=true;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-txbe-access-command: sequence=15 sent=%d received=%d cc=%u residue=%u\n",
        (int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++) {
        uint32_t word=0;for(unsigned b=0;b<4;b++)word|=(uint32_t)m->buffer[4*n+b]<<(8*b);
        console_printf("wifi-txbe-access-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_txbe_direct_write(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_txbe_access *s=&m->txbe_access;
    if(!wifi_txbe_access_owned(d) || !m->txbe_direct_mode || m->txbe_direct_submitted ||
        !s->attempted || !s->control_attempted || s->control_written ||
        !s->before.complete || s->before.reads!=64 || s->before.samples!=8 ||
        s->request_attempted || s->after.attempted || m->txbe_access_submitted)return false;
    for(unsigned n=0;n<8;n++)if(s->before.values[n][2]!=0)return false;
    m->txbe_direct_submitted=true;
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=0;
    struct usb_setup_packet setup={.request_type=0x40,.request=6,.index=0x2728,.length=4};
    int actual=0;
    bool ok=wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,false,&actual) && actual==4;
    console_printf("wifi-txbe-direct-write: index=%x value=0 actual=%d completed=%u\n",(uint64_t)0x2728,(int64_t)actual,(uint64_t)ok);
    if(!ok)return wifi_mcu_fail(d);
    return true;
}
static bool wifi_mcu_txbe_access(struct xhci_device *d,bool direct) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(m->txbe_access.attempted)return wifi_mcu_fail(d);
    m->txbe_access_active=true;m->txbe_direct_mode=direct;

    const struct mt76_mcu_timing_io io={d,wifi_txbe_access_owned,wifi_txbe_access_read,wifi_txbe_access_exchange};
    struct mt76_txbe_access *s=&m->txbe_access;
    const struct mt76_txbe_direct_io direct_io={io,wifi_txbe_direct_write};
    enum mt76_mcu_timing_status status=direct?mt76_txbe_direct_apply(s,&direct_io):mt76_txbe_access_apply(s,&io);
    m->txbe_access_active=false;
    console_printf("wifi-txbe-access: status=%u attempted=%u request_attempted=%u submitted=%u received=%d reply_validated=%u complete=%u before_reads=%u before_samples=%u after_reads=%u after_samples=%u\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->request_attempted,(uint64_t)m->txbe_access_submitted,
        (int64_t)s->received,(uint64_t)s->reply_validated,(uint64_t)s->complete,
        (uint64_t)s->before.reads,(uint64_t)s->before.samples,(uint64_t)s->after.reads,(uint64_t)s->after.samples);
    console_printf("wifi-txbe-access-value: value=%x matches=%u expected_matches=%u\n",(uint64_t)s->value,(uint64_t)s->matches,(uint64_t)(direct?8:16));
    if(direct)console_printf("wifi-txbe-direct: attempted=%u submitted=%u written=%u\n",(uint64_t)s->control_attempted,(uint64_t)m->txbe_direct_submitted,(uint64_t)s->control_written);
    for(unsigned stage=0;stage<2;stage++) {
        struct mt76_txbe_observation *o=stage?&s->after:&s->before;
        for(unsigned i=0;i<o->samples;i++) {
            uint32_t *v=o->values[i];
            console_printf("wifi-txbe-access-sample: stage=%u index=%u monotonic_us=%u mac_before=%x core_before=%x txbe9=%x txbe10=%x txbe12=%x txbe13=%x mac_after=%x core_after=%x\n",
                (uint64_t)stage,(uint64_t)i,m->txbe_access_us[stage][i],(uint64_t)v[0],(uint64_t)v[1],
                (uint64_t)v[2],(uint64_t)v[3],(uint64_t)v[4],(uint64_t)v[5],(uint64_t)v[6],(uint64_t)v[7]);
        }
    }
    if(status!=MT76_TIMING_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_table_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->table_mode && m->table_active && wifi_mcu_owned(d) && m->configured && !m->pending &&
        m->compact_mode && !m->compact_active && m->compact_limit==2 && m->compact_next==2 && wifi_compact_prefix(m,2) &&
        !m->compact[2].attempted && !m->compact_submitted[2] &&
        m->clock_mode && m->clock.complete && m->clock.written && m->clock_submitted && !m->clock_active && m->clock.pbf_after==0x80c00 &&
        m->bbp.complete && m->bbp.core==0xf000f200 && m->bbp.pbf==0x80c00 &&
        m->identity_validated && m->write_validated && m->timing.complete && m->timing.ack_validated && d->firmware.radio_reset.prepared;
}
/* All previous responses are consumed before reusing the table's sequence6.
 * Its exact address/value ACK differs from the earlier route ACK. */
static bool wifi_table_capture_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_table_owned(d))return false;
    if(!m->bbp_after_mac_mode)return true;
    return !m->mac_raw_mode && !m->mac_snapshot_mode && m->mac_policy_validate && m->mac_fce_validate &&
        m->mac_route_mode && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active &&
        m->mac_policy_mode && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active &&
        m->mac_protect_mode && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active &&
        m->mac_contention_mode && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active &&
        m->mac_txsw_mode && m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active &&
        m->mac_beacon_mode && m->mac_beacon.complete && m->mac_beacon_submitted && !m->mac_beacon_active &&
        m->mac_vht_mode && m->mac_vht.complete && m->mac_vht_submitted && !m->mac_vht_active &&
        m->mac_buffer_mode && m->mac_buffer.complete && m->mac_buffer_submitted && !m->mac_buffer_active &&
        m->mac_fce_mode && m->mac_fce.complete && m->mac_fce_submitted && !m->mac_fce_active &&
        m->mac_ampdu_mode && m->mac_ampdu.complete && m->mac_ampdu_submitted && !m->mac_ampdu_active;
}
static bool wifi_table_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_table_capture_owned(d) || !mt76_bbp_table_read_allowed(address))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_table_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp_table *s=&m->table;
    unsigned chunk=m->table_submitted;uint8_t allowed[200];
    if((m->mac_route_mode && !m->bbp_after_mac_mode) || m->mac_raw_mode || (m->mac_snapshot_mode && (!m->mac_snapshot.complete || m->mac_snapshot_active)) || !wifi_table_capture_owned(d) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX || chunk>=3 ||
        !s->attempted || s->complete || s->ack_validated || s->reads!=62 || s->chunks_attempted!=chunk+1 || s->chunks_completed!=chunk ||
        length!=mt76_bbp_table_packet(chunk,allowed,sizeof allowed))return -1;
    for(unsigned i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->table_submitted++;
    for(unsigned i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=chunk<2?0:wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-bbp-table-command: chunk=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",(uint64_t)chunk,(uint64_t)(chunk==2?6:0),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-bbp-table-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mac_route_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_route_mode && m->mac_route_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_route_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_route_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x080c && address!=0x0230))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_route_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_route;uint8_t allowed[24];
    if(!wifi_mac_route_owned(d) || !packet || !reply || length!=24 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_route_submitted || !s->attempted || s->complete || s->reads!=4 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x03ff0233 || s->before[1]!=0x41040 ||
        mt76_mac_route_packet(allowed,sizeof allowed)!=24)return -1;
    for(unsigned i=0;i<24;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_route_submitted=true;
    for(unsigned i=0;i<24;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,24),received=-1;
    if(sent==24)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-route-command: sequence=6 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-route-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_route(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_route;
    m->table_active=true;m->mac_route_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_route_owned,wifi_mac_route_read,wifi_mac_route_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_route_apply(s,&io);
    m->mac_route_active=false;m->table_active=false;
    console_printf("wifi-mac-route: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_route_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-route-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_mac_policy_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_policy_mode && m->mac_policy_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_policy_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_policy_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x1350 && address!=0x1348 && address!=0x134c && address!=0x1404))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_policy_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_policy;uint8_t allowed[40];
    if(!wifi_mac_policy_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_policy_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x7f0020 || s->before[1]!=0x10f0a90 || s->before[2]!=0x4bb80407 || s->before[3]!=7 ||
        mt76_mac_policy_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_policy_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-policy-command: sequence=7 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-policy-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_policy(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_policy;
    m->table_active=true;m->mac_policy_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_policy_owned,wifi_mac_policy_read,wifi_mac_policy_exchange};
    enum mt76_mcu_timing_status status=m->mac_policy_validate?mt76_mac_policy_validate(s,&io,&m->mac_policy_mismatch):m->mac_policy_observe?mt76_mac_policy_observe(s,&io,&m->mac_policy_mismatch):mt76_mac_policy_apply(s,&io);
    m->mac_policy_active=false;m->table_active=false;
    console_printf("wifi-mac-policy: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_policy_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-policy-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(m->mac_policy_observe)console_printf("wifi-mac-policy-observation: complete=%u mismatch_mask=%u register_acceptance=NOT_TESTED\n",(uint64_t)s->complete,(uint64_t)m->mac_policy_mismatch);
    if(m->mac_policy_validate)console_printf("wifi-mac-policy-validation: complete=%u write_mismatch_mask=%u exact_profile=1\n",(uint64_t)s->complete,(uint64_t)m->mac_policy_mismatch);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_mac_protect_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_protect_mode && m->mac_protect_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_protect_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_protect_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x1364 && address!=0x1368 && address!=0x1374 && address!=0x1378))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_protect_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_protect;uint8_t allowed[40];
    if(!wifi_mac_protect_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_protect_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x100003 || s->before[1]!=0x200003 || s->before[2]!=0x1002004 || s->before[3]!=0x2002084 ||
        mt76_mac_protect_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_protect_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-protect-command: sequence=8 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-protect-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_protect(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_protect;
    m->table_active=true;m->mac_protect_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_protect_owned,wifi_mac_protect_read,wifi_mac_protect_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_protect_apply(s,&io);
    m->mac_protect_active=false;m->table_active=false;
    console_printf("wifi-mac-protect: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_protect_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-protect-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_contention_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_contention_mode && m->mac_contention_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_contention_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_contention_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x136c && address!=0x1370 && address!=0x1340 && address!=0x1344))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_contention_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_contention;uint8_t allowed[40];
    if(!wifi_mac_contention_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_contention_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x402004 || s->before[1]!=0x802084 || s->before[2]!=0x400243f || s->before[3]!=0xffff07 ||
        mt76_mac_contention_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_contention_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-contention-command: sequence=9 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-contention-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_contention(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_contention;
    m->table_active=true;m->mac_contention_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_contention_owned,wifi_mac_contention_read,wifi_mac_contention_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_contention_apply(s,&io);
    m->mac_contention_active=false;m->table_active=false;
    console_printf("wifi-mac-contention: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_contention_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-contention-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_txsw_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_txsw_mode && m->mac_txsw_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_txsw_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_txsw_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x1330 && address!=0x1334 && address!=0x1338 && address!=0x0260))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_txsw_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_txsw;uint8_t allowed[40];
    if(!wifi_mac_txsw_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_txsw_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x204080c || s->before[1]!=0xc0808 || s->before[2]!=0xc0408 || s->before[3]!=0 ||
        mt76_mac_txsw_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_txsw_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-txsw-command: sequence=10 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-txsw-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_txsw(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_txsw;
    m->table_active=true;m->mac_txsw_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_txsw_owned,wifi_mac_txsw_read,wifi_mac_txsw_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_txsw_apply(s,&io);
    m->mac_txsw_active=false;m->table_active=false;
    console_printf("wifi-mac-txsw: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_txsw_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-txsw-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_vht_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_beacon.complete && m->mac_beacon_submitted && !m->mac_beacon_active && m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_vht_mode && m->mac_vht_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_vht_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_vht_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x13e0 && address!=0x13e4 && address!=0x13e8 && address!=0x1358))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_vht_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_vht;uint8_t allowed[40];
    if(!wifi_mac_vht_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_vht_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0xe3f02004 || s->before[1]!=0xe3f02084 || s->before[2]!=0xe3f02104 || s->before[3]!=0xedcba988 ||
        mt76_mac_vht_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_vht_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-vht-command: sequence=12 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-vht-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_vht(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_vht;
    m->table_active=true;m->mac_vht_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_vht_owned,wifi_mac_vht_read,wifi_mac_vht_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_vht_apply(s,&io);
    m->mac_vht_active=false;m->table_active=false;
    console_printf("wifi-mac-vht: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_vht_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-vht-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_buffer_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_vht.complete && m->mac_vht_submitted && !m->mac_vht_active && m->mac_beacon.complete && m->mac_beacon_submitted && !m->mac_beacon_active && m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_buffer_mode && m->mac_buffer_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_buffer_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_buffer_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x0408 && address!=0x040c && address!=0x0404 && address!=0x0250))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_buffer_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_buffer;uint8_t allowed[40];
    if(!wifi_mac_buffer_owned(d) || !packet || !reply || length!=40 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_buffer_submitted || !s->attempted || s->complete || s->reads!=6 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0x1f9f1f1f || s->before[1]!=0xfe9f || s->before[2]!=0x3fdc1cf5 || s->before[3]!=0 ||
        mt76_mac_buffer_packet(allowed,sizeof allowed)!=40)return -1;
    for(unsigned i=0;i<40;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_buffer_submitted=true;
    for(unsigned i=0;i<40;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,40),received=-1;
    if(sent==40)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-buffer-command: sequence=13 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-buffer-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_buffer(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_buffer;
    m->table_active=true;m->mac_buffer_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_buffer_owned,wifi_mac_buffer_read,wifi_mac_buffer_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_buffer_apply(s,&io);
    m->mac_buffer_active=false;m->table_active=false;
    console_printf("wifi-mac-buffer: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_buffer_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-buffer-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_beacon_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_beacon_mode && m->mac_beacon_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_beacon_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_beacon_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x041c && address!=0x0420))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_beacon_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_beacon;uint8_t allowed[24];
    if(!wifi_mac_beacon_owned(d) || !packet || !reply || length!=24 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_beacon_submitted || !s->attempted || s->complete || s->reads!=4 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=0xece8e4e0 || s->before[1]!=0xfcf8f4f0 ||
        mt76_mac_beacon_packet(allowed,sizeof allowed)!=24)return -1;
    for(unsigned i=0;i<24;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_beacon_submitted=true;
    for(unsigned i=0;i<24;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,24),received=-1;
    if(sent==24)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-beacon-command: sequence=11 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-beacon-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_beacon(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_beacon;
    m->table_active=true;m->mac_beacon_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_beacon_owned,wifi_mac_beacon_read,wifi_mac_beacon_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_beacon_apply(s,&io);
    m->mac_beacon_active=false;m->table_active=false;
    console_printf("wifi-mac-beacon: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_beacon_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-beacon-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_fce_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_buffer.complete && m->mac_buffer_submitted && !m->mac_buffer_active && m->mac_vht.complete && m->mac_vht_submitted && !m->mac_vht_active && m->mac_beacon.complete && m->mac_beacon_submitted && !m->mac_beacon_active && m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_fce_mode && m->mac_fce_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_fce_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_fce_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x0800 && address!=0x080c))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_fce_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_fce;uint8_t allowed[24];
    if(!wifi_mac_fce_owned(d) || !packet || !reply || length!=24 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_fce_submitted || !s->attempted || s->complete || s->reads!=4 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=5 || s->before[1]!=0x03ff0223 ||
        mt76_mac_fce_packet(allowed,sizeof allowed)!=24)return -1;
    for(unsigned i=0;i<24;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_fce_submitted=true;
    for(unsigned i=0;i<24;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,24),received=-1;
    if(sent==24)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-fce-command: sequence=14 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-fce-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_fce(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_fce;
    m->table_active=true;m->mac_fce_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_fce_owned,wifi_mac_fce_read,wifi_mac_fce_exchange};
    uint32_t mismatch_mask=0;
    enum mt76_mcu_timing_status status=m->mac_fce_validate?mt76_mac_fce_validate(s,&io,&mismatch_mask):mt76_mac_fce_observe(s,&io,&mismatch_mask);
    m->mac_fce_active=false;m->table_active=false;
    console_printf("wifi-mac-fce: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_fce_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-fce-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    if(m->mac_fce_validate)console_printf("wifi-mac-fce-validation: complete=%u write_mismatch_mask=%u exact_profile=%u\n",(uint64_t)s->complete,(uint64_t)mismatch_mask,(uint64_t)(status==MT76_TIMING_OK && s->complete));
    else console_printf("wifi-mac-fce-observation: complete=%u write_mismatch_mask=%u register_acceptance=NOT_TESTED\n",(uint64_t)s->complete,(uint64_t)mismatch_mask);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_ampdu_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_fce_validate && m->mac_fce.complete && m->mac_fce_submitted && !m->mac_fce_active && m->mac_buffer.complete && m->mac_buffer_submitted && !m->mac_buffer_active && m->mac_vht.complete && m->mac_vht_submitted && !m->mac_vht_active && m->mac_beacon.complete && m->mac_beacon_submitted && !m->mac_beacon_active && m->mac_txsw.complete && m->mac_txsw_submitted && !m->mac_txsw_active && m->mac_contention.complete && m->mac_contention_submitted && !m->mac_contention_active && m->mac_protect.complete && m->mac_protect_submitted && !m->mac_protect_active && m->mac_policy_validate && m->mac_policy.complete && m->mac_policy_submitted && !m->mac_policy_active && m->mac_route.complete && m->mac_route_submitted && !m->mac_route_active && m->mac_ampdu_mode && m->mac_ampdu_active && !m->mac_raw_mode && !m->mac_snapshot_mode &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_ampdu_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_ampdu_owned(d) || (address!=0x1004 && address!=0x2000 && address!=0x1030 && address!=0x080c))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    bool ok=wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual);
    struct xhci_wifi_firmware *f=&d->firmware;
    console_printf("wifi-mac-ampdu-transfer: index=%u address=%x ok=%u actual=%d cc=%u residue=%u timeout=%u\n",(uint64_t)f->mcu.mac_ampdu.reads,(uint64_t)address,(uint64_t)ok,(int64_t)actual,(uint64_t)f->control_cc,(uint64_t)f->control_residue,(uint64_t)f->control_timeout);
    if(!ok || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_mac_ampdu_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_ampdu;uint8_t allowed[24];
    if(!wifi_mac_ampdu_owned(d) || !packet || !reply || length!=24 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->mac_ampdu_submitted || !s->attempted || s->complete || s->reads!=4 || s->batches_attempted!=1 || s->batches_completed || s->ack_mask ||
        s->mac_before || s->core_before!=0xf000f200 || s->before[0]!=UINT32_MAX || s->before[1]!=0x03ff0223 ||
        mt76_mac_ampdu_packet(allowed,sizeof allowed)!=24)return -1;
    for(unsigned i=0;i<24;i++)if(packet[i]!=allowed[i])return -1;
    m->mac_ampdu_submitted=true;
    for(unsigned i=0;i<24;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,24),received=-1;
    if(sent==24)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-mac-ampdu-command: sequence=15 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-mac-ampdu-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_mac_ampdu(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mcu_mac *s=&m->mac_ampdu;
    m->table_active=true;m->mac_ampdu_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_ampdu_owned,wifi_mac_ampdu_read,wifi_mac_ampdu_exchange};
    enum mt76_mcu_timing_status status=mt76_mac_ampdu_apply(s,&io);
    m->mac_ampdu_active=false;m->table_active=false;
    console_printf("wifi-mac-ampdu: status=%u attempted=%u submitted=%u complete=%u reads=%u batches_attempted=%u batches_completed=%u ack_mask=%u mac_before=%x mac_after=%x core_before=%x core_after=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)m->mac_ampdu_submitted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->batches_attempted,(uint64_t)s->batches_completed,(uint64_t)s->ack_mask,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->core_before,(uint64_t)s->core_after);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-ampdu-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_snapshot_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_snapshot_mode && m->mac_snapshot_active && !m->mac_snapshot.complete &&
        !m->table.attempted && !m->table_submitted && wifi_table_owned(d);
}
static bool wifi_mac_snapshot_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;
    if(!value || !wifi_mac_snapshot_owned(d) || !(mt76_mac_snapshot_read_allowed(address) || (d->firmware.mcu.mac_raw_mode && address==0x1000)))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    bool ok=wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual);
    if(d->firmware.mcu.mac_raw_mode){
        struct xhci_wifi_firmware *f=&d->firmware;
        console_printf("wifi-mac-raw-transfer: index=%u address=%x ok=%u actual=%d cc=%u residue=%u timeout=%u\n",(uint64_t)f->mcu.mac_snapshot.reads,(uint64_t)address,(uint64_t)ok,(int64_t)actual,(uint64_t)f->control_cc,(uint64_t)f->control_residue,(uint64_t)f->control_timeout);
    }
    if(!ok || actual!=4)return wifi_mcu_fail(d);
    *value=0;for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_mcu_mac_snapshot(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_snapshot *s=&m->mac_snapshot;
    m->table_active=true;m->mac_snapshot_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_mac_snapshot_owned,wifi_mac_snapshot_read,NULL};
    enum mt76_mcu_timing_status status=m->mac_raw_mode?mt76_mac_raw_capture(s,&io):mt76_mac_snapshot_capture(s,&io);
    m->mac_snapshot_active=false;m->table_active=false;
    if(m->mac_raw_mode)console_printf("wifi-mac-raw: status=%u attempted=%u complete=%u reads=%u writes=0 all_ones=%u asic_before=%x asic_after=%x repeat_1030=%x register_acceptance=NOT_TESTED\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->all_ones,(uint64_t)s->asic[0],(uint64_t)s->asic[1],(uint64_t)s->repeat_1030);
    else
    console_printf("wifi-mac-snapshot: status=%u attempted=%u complete=%u reads=%u writes=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-snapshot-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<MT76_MAC_SNAPSHOT_ENTRIES;i++)console_printf("wifi-mac-snapshot-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_mac_snapshot_address(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_mcu_table_capture(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp_table *s=&m->table;
    m->table_active=true;
    if(m->bbp_after_mac_mode)console_printf("wifi-bbp-after-mac: prerequisites=%u sequence_reuse=6\n",(uint64_t)wifi_table_capture_owned(d));
    const struct mt76_mcu_timing_io io={d,wifi_table_capture_owned,wifi_table_read,wifi_table_exchange};
    enum mt76_mcu_timing_status status=mt76_bbp_table_capture(s,&io);m->table_active=false;
    console_printf("wifi-bbp-table: status=%u attempted=%u complete=%u reads=%u attempted_chunks=%u submitted=%u completed_chunks=%u ack_validated=%u mismatches=%u register_acceptance=NOT_TESTED\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->chunks_attempted,(uint64_t)m->table_submitted,(uint64_t)s->chunks_completed,(uint64_t)s->ack_validated,(uint64_t)s->mismatches);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-table-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<MT76_BBP_TABLE_ENTRIES;i++)console_printf("wifi-bbp-table-value: index=%u address=%x expected=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_bbp_table_address(i),(uint64_t)mt76_bbp_table_value(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_bbp_switch_prerequisites(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->bbp_switch_mode && m->bbp_after_mac_mode &&
        wifi_table_capture_owned(d) && m->table.attempted && m->table.complete &&
        m->table.ack_validated && m->table.reads==124 && m->table.chunks_attempted==3 &&
        m->table.chunks_completed==3 && m->table_submitted==3;
}
static bool wifi_bbp_switch_owned(void *context){
    struct xhci_device *d=context;return d->firmware.mcu.bbp_switch_active && wifi_bbp_switch_prerequisites(d);
}
static bool wifi_bbp_switch_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;
    if(!v || !wifi_bbp_switch_owned(d) || !mt76_bbp_switch_read_allowed(a))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)a,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *v=0;for(unsigned i=0;i<4;i++)*v|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_bbp_switch_write(void *context,unsigned i,uint32_t a,uint32_t v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp_switch *s=&m->bbp_switch;
    if(!wifi_bbp_switch_owned(d) || i>=20 || a!=mt76_bbp_switch_address(i) || v!=mt76_bbp_switch_value(i) ||
        !s->attempted || s->complete || s->reads!=26+2*i || s->writes_attempted!=i+1 ||
        s->writes_completed!=i || m->bbp_switch_submitted!=i || s->write_mac[i] || s->write_core[i]!=0xf000f200)return false;
    m->bbp_switch_submitted++;
    for(unsigned n=0;n<4;n++)d->descriptor_buffer[n]=(uint8_t)(v>>(8*n));
    struct usb_setup_packet setup={.request_type=0x40,.request=6,.index=(uint16_t)a,.length=4};int actual=0;
    bool ok=wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,false,&actual) && actual==4;
    console_printf("wifi-bbp-switch-write: index=%u address=%x value=%x actual=%d completed=%u\n",(uint64_t)i,(uint64_t)a,(uint64_t)v,(int64_t)actual,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_bbp_switch(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp_switch *s=&m->bbp_switch;
    m->table_active=true;m->bbp_switch_active=true;
    const struct mt76_bbp_switch_io io={d,wifi_bbp_switch_owned,wifi_bbp_switch_read,wifi_bbp_switch_write};
    enum mt76_mcu_timing_status status=mt76_bbp_switch_apply(s,&io);
    m->bbp_switch_active=false;m->table_active=false;
    console_printf("wifi-bbp-switch: status=%u attempted=%u complete=%u reads=%u attempted_writes=%u submitted=%u completed_writes=%u mismatches=%u register_acceptance=NOT_TESTED\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->writes_attempted,(uint64_t)m->bbp_switch_submitted,(uint64_t)s->writes_completed,(uint64_t)s->mismatches);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bbp-switch-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<20;i++)console_printf("wifi-bbp-switch-value: index=%u address=%x expected=%x before=%x after=%x write_mac=%x write_core=%x\n",(uint64_t)i,(uint64_t)mt76_bbp_switch_address(i),(uint64_t)mt76_bbp_switch_value(i),(uint64_t)s->before[i],(uint64_t)s->after[i],(uint64_t)s->write_mac[i],(uint64_t)s->write_core[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_dcoc_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp_switch *s=&m->bbp_switch;
    if(!m->dcoc_mode || !m->dcoc_active || m->bbp_switch_active || !wifi_bbp_switch_prerequisites(d) ||
        !s->attempted || !s->complete || s->reads!=88 || s->writes_attempted!=20 || s->writes_completed!=20 ||
        m->bbp_switch_submitted!=20 || s->mismatches)return false;
    for(unsigned i=0;i<20;i++)if(s->after[i]!=mt76_bbp_switch_value(i))return false;
    return true;
}
static bool wifi_dcoc_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;
    if(!v || !wifi_dcoc_owned(d) || !mt76_dcoc_read_allowed(a))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)a,.length=4};int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    *v=0;for(unsigned i=0;i<4;i++)*v|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static int wifi_dcoc_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_dcoc *s=&m->dcoc;uint8_t allowed[80];
    if(!wifi_dcoc_owned(d) || !packet || !reply || length!=80 || capacity<MT76_MCU_RESPONSE_MAX ||
        m->dcoc_submitted || !s->attempted || !s->command_attempted || s->complete || s->ack_validated || s->reads!=13 ||
        s->mac[0] || s->mac[1] || s->core[0]!=0xf000f200 || s->core[1]!=0xf000f200 || mt76_dcoc_packet(allowed,80)!=80)return -1;
    for(unsigned i=0;i<80;i++)if(packet[i]!=allowed[i])return -1;
    m->dcoc_submitted=true;
    for(unsigned i=0;i<80;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,80),received=-1;
    if(sent==80)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-dcoc-command: sequence=7 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received>0 && received<=64)for(unsigned n=0;n<(unsigned)received/4;n++){
        uint32_t word=0;for(unsigned i=0;i<4;i++)word|=(uint32_t)m->buffer[4*n+i]<<(8*i);
        console_printf("wifi-dcoc-word: index=%u value=%x\n",(uint64_t)n,(uint64_t)word);
    }
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_dcoc(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_dcoc *s=&m->dcoc;
    m->table_active=true;m->dcoc_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_dcoc_owned,wifi_dcoc_read,wifi_dcoc_exchange};
    enum mt76_mcu_timing_status status=mt76_dcoc_capture(s,&io);m->dcoc_active=false;m->table_active=false;
    console_printf("wifi-dcoc: status=%u attempted=%u complete=%u reads=%u command_attempted=%u submitted=%u ack_validated=%u mismatches=%u register_acceptance=NOT_TESTED\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)s->command_attempted,(uint64_t)m->dcoc_submitted,(uint64_t)s->ack_validated,(uint64_t)s->mismatches);
    for(unsigned i=0;i<4;i++)console_printf("wifi-dcoc-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<9;i++)console_printf("wifi-dcoc-value: index=%u address=%x expected=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_dcoc_address(i),(uint64_t)mt76_dcoc_value(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_skey_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!m->skey_mode || !m->skey_active || !wifi_dcoc_owned(d) || !m->dcoc.complete ||
        !m->dcoc.ack_validated || !m->dcoc_submitted || m->dcoc.reads!=26 || m->dcoc.mismatches)return false;
    for(unsigned i=0;i<9;i++)if(m->dcoc.after[i]!=mt76_dcoc_value(i))return false;
    return true;
}
static bool wifi_skey_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_skey_owned(d) || m->skey_submitted!=op || !mt76_skey_transfer_valid(&m->skey,op,write,a,data,n))return false;
    m->skey_submitted++;
    for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=write?data[i]:0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=(uint16_t)n};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,n,!write,&actual) && actual==(int)n;
    if(ok && !write)for(unsigned i=0;i<n;i++)data[i]=d->descriptor_buffer[i];
    /* Never log or retain verification bytes in the shared descriptor buffer. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=0;
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_skey(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_skey *s=&m->skey;
    m->table_active=true;m->dcoc_active=true;m->skey_active=true;
    const struct mt76_skey_io io={d,wifi_skey_owned,wifi_skey_transfer};
    enum mt76_mcu_timing_status status=mt76_skey_reset(s,&io);
    m->skey_active=false;m->dcoc_active=false;m->table_active=false;
    console_printf("wifi-skey: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u slots=%u mode_verified=%u key_zero=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->skey_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->slots,(uint64_t)s->mode_verified,(uint64_t)s->key_zero);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_wcid_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_skey *s=&m->skey;
    return m->wcid_mode && m->wcid_active && wifi_skey_owned(d) && s->attempted && s->complete &&
        s->operations==576 && m->skey_submitted==576 && s->reads==448 && s->writes==128 &&
        s->slots==64 && s->mode_verified==64 && s->key_zero==64;
}
static bool wifi_wcid_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_wcid_owned(d) || m->wcid_submitted!=op || !mt76_wcid_transfer_valid(&m->wcid,op,write,a,data,n))return false;
    m->wcid_submitted++;
    for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=write?data[i]:0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=(uint16_t)n};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,n,!write,&actual) && actual==(int)n;
    if(ok && !write)for(unsigned i=0;i<n;i++)data[i]=d->descriptor_buffer[i];
    /* Never log or retain verification bytes in the shared descriptor buffer. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=0;
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_wcid(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_wcid *s=&m->wcid;
    m->table_active=true;m->dcoc_active=true;m->skey_active=true;m->wcid_active=true;
    const struct mt76_wcid_io io={d,wifi_wcid_owned,wifi_wcid_transfer};
    enum mt76_mcu_timing_status status=mt76_wcid_reset(s,&io);
    m->wcid_active=false;m->skey_active=false;m->dcoc_active=false;m->table_active=false;
    console_printf("wifi-wcid: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u slots=%u attributes_zero=%u addresses_zero=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->wcid_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->slots,(uint64_t)s->attributes_zero,(uint64_t)s->addresses_zero);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_mac_tail_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_wcid *w=&m->wcid;
    return m->mac_tail_mode && m->mac_tail_active && wifi_wcid_owned(d) && w->attempted && w->complete &&
        w->operations==1792 && m->wcid_submitted==1792 && w->reads==1408 && w->writes==384 &&
        w->slots==256 && w->attributes_zero==256 && w->addresses_zero==128;
}
static bool wifi_mac_tail_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_tail *s=&m->mac_tail;
    if(!value || !wifi_mac_tail_owned(d) || !s->attempted || s->complete || s->reads>=26 ||
       m->mac_tail_submitted!=s->reads || address!=mt76_mac_tail_request(s->reads))return false;
    m->mac_tail_submitted++;*value=0;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    if(!wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_mcu_mac_tail(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_tail *s=&m->mac_tail;
    m->table_active=true;m->dcoc_active=true;m->skey_active=true;m->wcid_active=true;m->mac_tail_active=true;
    const struct mt76_mcu_timing_io io={.context=d,.authorize=wifi_mac_tail_owned,.read=wifi_mac_tail_read};
    enum mt76_mcu_timing_status status=mt76_mac_tail_capture(s,&io);
    m->mac_tail_active=false;m->wcid_active=false;m->skey_active=false;m->dcoc_active=false;m->table_active=false;
    console_printf("wifi-mac-tail: status=%u attempted=%u complete=%u reads=%u submitted=%u all_ones=%u register_acceptance=NOT_TESTED\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->mac_tail_submitted,(uint64_t)s->all_ones);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-tail-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<22;i++)console_printf("wifi-mac-tail-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_mac_tail_address(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_mac_fixed_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!m->mac_fixed_mode || !m->mac_fixed_active || !wifi_mac_tail_owned(d) ||
       !m->mac_tail.complete || m->mac_tail.reads!=26 || m->mac_tail_submitted!=26)return false;
    const unsigned indexes[4]={7,8,10,19};
    for(unsigned i=0;i<4;i++)if(m->mac_tail.values[indexes[i]]!=mt76_mac_fixed_initial(i))return false;
    return true;
}
static bool wifi_mac_fixed_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_mac_fixed_owned(d) || m->mac_fixed_submitted!=op || !mt76_mac_fixed_transfer_valid(&m->mac_fixed,op,write,a,data,n))return false;
    m->mac_fixed_submitted++;
    for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=write?data[i]:0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=(uint16_t)n};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,n,!write,&actual) && actual==(int)n;
    if(ok && !write)for(unsigned i=0;i<n;i++)data[i]=d->descriptor_buffer[i];
    /* Never log or retain verification bytes in the shared descriptor buffer. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=0;
    return ok?true:wifi_mcu_fail(d);
}

static bool wifi_mcu_mac_fixed(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_fixed *s=&m->mac_fixed;
    m->table_active=true;m->dcoc_active=true;m->skey_active=true;m->wcid_active=true;m->mac_tail_active=true;m->mac_fixed_active=true;
    const struct mt76_mac_fixed_io io={d,wifi_mac_fixed_owned,wifi_mac_fixed_transfer};
    enum mt76_mcu_timing_status status=mt76_mac_fixed_apply(s,&io);
    m->mac_fixed_active=false;m->mac_tail_active=false;m->wcid_active=false;m->skey_active=false;m->dcoc_active=false;m->table_active=false;
    console_printf("wifi-mac-fixed: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->mac_fixed_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<8;i++)console_printf("wifi-mac-fixed-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-fixed-value: index=%u address=%x before=%x after=%x dma=%x\n",(uint64_t)i,(uint64_t)mt76_mac_fixed_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i],(uint64_t)s->dma[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_efuse_map_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_fixed *s=&m->mac_fixed;
    if(!m->efuse_map_mode || !m->efuse_map_active || m->efuse_map_gate.failed || !wifi_mac_fixed_owned(d) ||
        !s->complete || s->operations!=32 || m->mac_fixed_submitted!=32 || s->reads!=28 || s->writes!=4 || s->verified!=4)return false;
    for(unsigned i=0;i<4;i++)if(s->after[i]!=mt76_mac_fixed_value(i) || s->dma[i]&15)return false;
    for(unsigned i=0;i<8;i++)if(s->mac[i] || s->core[i]!=0xf000f200)return false;
    return true;
}
static bool wifi_efuse_map_transfer(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
    struct mt76_efuse_map_gate *g=&d->firmware.mcu.efuse_map_gate;
    if(!v || !wifi_efuse_map_owned(d) || !mt76_efuse_map_begin(g,write,a,*v))return false;
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=write?(uint8_t)(*v>>(8*i)):0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=4};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,4,!write,&actual) && actual==4;
    if(ok && !write){*v=0;for(unsigned i=0;i<4;i++)*v|=(uint32_t)d->descriptor_buffer[i]<<(8*i);}
    /* Do not clear potentially owned DMA on timeout. Never log map bytes. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=0;
    return mt76_efuse_map_finish(g,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_efuse_map_read(void *context,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_efuse_map_transfer(context,false,a,v);}
static bool wifi_efuse_map_write(void *context,uint32_t a,uint32_t v){return wifi_efuse_map_transfer(context,true,a,&v);}
static bool wifi_efuse_map_wait(void *context,uint32_t us){
    struct xhci_device *d=context;struct mt76_efuse_map_gate *g=&d->firmware.mcu.efuse_map_gate;
    if(!wifi_efuse_map_owned(d) || !mt76_efuse_map_delay(g,us))return false;
    if(timer_delay_us(us))return true;
    g->failed=true;return wifi_mcu_fail(d);
}
static bool wifi_mcu_efuse_map(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_efuse_map *s=&m->efuse_map;struct mt76_efuse_map_gate *g=&m->efuse_map_gate;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=true;
    const struct mt76_efuse_map_io io={{d,wifi_efuse_map_read,wifi_efuse_map_write,wifi_efuse_map_wait},wifi_efuse_map_owned};
    enum mt76_capture_status status=mt76_capture_efuse_map(&io,&d->wifi_identity,s);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=false;
    bool complete=status==MT76_CAPTURE_OK && g->phase==15 && g->block==2 && !g->pending && !g->failed && g->submitted==s->reads+s->writes && g->polls==s->polls;
    console_printf("wifi-efuse-map: status=%u attempted=%u complete=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->valid,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->polls,(uint64_t)s->blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed);
    console_printf("wifi-efuse-map-summary: zeros=%u first=%u last=%u span=%u reference_used=%u\n",(uint64_t)s->zero_count,(uint64_t)s->first_zero,(uint64_t)s->last_zero,(uint64_t)s->free_span,(uint64_t)s->reference_used);
    for(unsigned i=0;i<4;i++)console_printf("wifi-efuse-map-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)g->mac[i],(uint64_t)g->core[i]);
    for(unsigned i=0;i<2;i++)console_printf("wifi-efuse-map-dma: index=%u value=%x\n",(uint64_t)i,(uint64_t)g->dma[i]);
    return complete?true:wifi_mcu_fail(d);
}

static bool wifi_mac_private_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_efuse_map *s=&m->efuse_map;struct mt76_efuse_map_gate *g=&m->efuse_map_gate;
    return m->mac_address_mode && wifi_efuse_map_owned(d) && s->attempted && s->complete && s->valid && s->blocks==2 && s->writes==2 &&
        s->polls>=2 && s->polls<=1024 && s->reads==22+s->polls && g->phase==15 && g->block==2 && !g->pending && !g->failed && g->submitted==s->reads+s->writes && g->polls==s->polls;
}
static bool wifi_mac_header_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->mac_header.attempted && m->mac_header_active && !m->mac_address_active && !m->mac_header_gate.failed && !m->mac_header.complete && wifi_mac_private_owned(d);
}
/* Uses normal DMA-backed control data; no immediate address bytes in diagnostics. */
static bool wifi_mac_private_usb(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=write?(uint8_t)(*v>>(8*i)):0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=4};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,4,!write,&actual) && actual==4;
    if(ok && !write){*v=0;for(unsigned i=0;i<4;i++)*v|=(uint32_t)d->descriptor_buffer[i]<<(8*i);}
    /* Quarantined DMA remains allocated and untouched, never dumped. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=0;
    return ok;
}
static bool wifi_mac_header_transfer(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
    struct mt76_efuse_map_gate *g=&d->firmware.mcu.mac_header_gate;
    if(!v || !wifi_mac_header_owned(d) || !mt76_mac_header_begin(g,write,a,*v))return false;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    return mt76_mac_header_finish(g,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_mac_header_read(void *context,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_mac_header_transfer(context,false,a,v);}
static bool wifi_mac_header_write(void *context,uint32_t a,uint32_t v){return wifi_mac_header_transfer(context,true,a,&v);}
static bool wifi_mac_header_wait(void *context,uint32_t us){
    struct xhci_device *d=context;struct mt76_efuse_map_gate *g=&d->firmware.mcu.mac_header_gate;
    if(!wifi_mac_header_owned(d) || !mt76_efuse_map_delay(g,us))return false;
    if(timer_delay_us(us))return true;
    g->failed=true;return wifi_mcu_fail(d);
}
static bool wifi_mac_address_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_header_result *s=&m->mac_header;struct mt76_efuse_map_gate *g=&m->mac_header_gate;
    return m->mac_address_gate.captured_readback==m->mac_address_validate_mode && m->mac_address_active && !m->mac_header_active && !m->mac_address_gate.failed && wifi_mac_private_owned(d) &&
        s->attempted && s->complete && s->valid && s->blocks==1 && s->writes==1 && s->polls>=1 && s->polls<=1024 && s->reads==11+s->polls &&
        g->phase==15 && g->block==1 && !g->pending && !g->failed && g->submitted==s->reads+1 && g->polls==s->polls && mt76_mac_address_header_valid(m->mac_header_private,16);
}
static bool wifi_mac_address_transfer(void *context,unsigned op,bool write,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_address_gate *g=&m->mac_address_gate;
    if(!v || !wifi_mac_address_owned(d) || !m->mac_address.attempted || m->mac_address.complete || m->mac_address.operations!=op ||
       !mt76_mac_address_begin(g,m->mac_header_private,op,write,a,*v))return false;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    /* Snapshot this transfer before MAC-disable cleanup replaces firmware's
     * shared receipt. No data/setup payload is copied. */
    m->mac_address_usb.attempted=true;m->mac_address_usb.operation=op;
    m->mac_address_usb.ok=ok;m->mac_address_usb.actual=d->firmware.control_actual;
    m->mac_address_usb.cc=d->firmware.control_cc;m->mac_address_usb.residue=d->firmware.control_residue;
    m->mac_address_usb.stage=d->firmware.control_stage;m->mac_address_usb.timeout=d->firmware.control_timeout;
    m->mac_address_usb.quarantined=d->ep0_quarantined;
    return mt76_mac_address_finish(g,m->mac_header_private,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_mac_address(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_header_result *h=&m->mac_header;struct mt76_efuse_map_gate *hg=&m->mac_header_gate;
    struct mt76_mac_address_result *s=&m->mac_address;struct mt76_mac_address_gate *g=&m->mac_address_gate;
    g->captured_readback=m->mac_address_validate_mode;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_header_active=true;
    const struct mt76_efuse_map_io header_io={{d,wifi_mac_header_read,wifi_mac_header_write,wifi_mac_header_wait},wifi_mac_header_owned};
    enum mt76_capture_status header_status=mt76_capture_mac_header(&header_io,&d->wifi_identity,h,m->mac_header_private);
    m->mac_header_active=false;m->mac_address_active=true;
    enum mt76_capture_status status=MT76_CAPTURE_REFUSED;
    if(header_status==MT76_CAPTURE_OK && wifi_mac_address_owned(d)){
        const struct mt76_mac_address_io io={d,wifi_mac_address_owned,wifi_mac_address_transfer};
        status=mt76_mac_address_apply_profile(s,&io,&m->efuse_map,m->mac_header_private,16,m->mac_address_validate_mode);
    }
    for(unsigned i=0;i<16;i++)((volatile uint8_t *)m->mac_header_private)[i]=0;
    g->previous=0;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_address_active=false;
    console_printf("wifi-mac-header: status=%u attempted=%u complete=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u private_cleared=1\n",(uint64_t)header_status,(uint64_t)h->attempted,(uint64_t)h->complete,(uint64_t)h->valid,(uint64_t)h->reads,(uint64_t)h->writes,(uint64_t)h->polls,(uint64_t)h->blocks,(uint64_t)hg->submitted,(uint64_t)hg->phase,(uint64_t)hg->failed);
    for(unsigned i=0;i<2;i++)console_printf("wifi-mac-header-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)hg->mac[i],(uint64_t)hg->core[i]);
    console_printf("wifi-mac-header-dma: value=%x\n",(uint64_t)hg->dma[0]);
    console_printf("wifi-mac-address: status=%u attempted=%u complete=%u operations=%u submitted=%u gate_operations=%u reads=%u writes=%u verified=%u failed=%u private_cleared=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)g->submitted,(uint64_t)g->operations,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified,(uint64_t)g->failed);
    if(m->mac_address_validate_mode)console_printf("wifi-mac-address-profile: captured=1 control_written=0x00ff0000 control_expected=0x00000000 exact=1\n");
    console_printf("wifi-mac-address-diagnostic: failure=%u compared=%u address_match=%u control_match=%u expected_control=%x observed_control=%x\n",
        (uint64_t)g->failure,(uint64_t)g->compared,(uint64_t)g->address_match,(uint64_t)g->control_match,(uint64_t)g->expected_control,(uint64_t)g->observed_control);
    console_printf("wifi-mac-address-usb: attempted=%u operation=%u ok=%u actual=%d cc=%u residue=%u stage=%u timeout=%u quarantined=%u\n",
        (uint64_t)m->mac_address_usb.attempted,(uint64_t)m->mac_address_usb.operation,(uint64_t)m->mac_address_usb.ok,(int64_t)m->mac_address_usb.actual,
        (uint64_t)m->mac_address_usb.cc,(uint64_t)m->mac_address_usb.residue,(uint64_t)m->mac_address_usb.stage,(uint64_t)m->mac_address_usb.timeout,(uint64_t)m->mac_address_usb.quarantined);
    for(unsigned i=0;i<20;i++)console_printf("wifi-mac-address-check: index=%u mac=%u/%u core=%x/%x dma=%x\n",(uint64_t)i,(uint64_t)g->mac[2*i],(uint64_t)g->mac[2*i+1],(uint64_t)g->core[2*i],(uint64_t)g->core[2*i+1],(uint64_t)g->dma[i]);
    return status==MT76_CAPTURE_OK && g->operations==160 && g->submitted==160 && !g->pending && !g->failed?true:wifi_mcu_fail(d);
}

static bool wifi_clock_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->clock_mode && m->clock_active && m->compact_mode && !m->compact_active &&
        (m->compact_limit==11 || (m->clock_pair_mode && m->compact_limit==12) || (m->table_mode && m->compact_limit==2)) && m->compact_next==2 && wifi_compact_prefix(m,2) &&
        !m->compact[2].attempted && !m->compact_submitted[2] && !m->bbp.attempted &&
        wifi_mcu_owned(d) && m->configured && !m->pending && m->identity_validated &&
        m->write_validated && m->timing.complete && m->timing.ack_validated && d->firmware.radio_reset.prepared;
}
static bool wifi_clock_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_clock_owned(d) || (address!=0x1004 && address!=0x1200 && address!=0x400))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_clock_write(void *context,uint32_t address,uint32_t value) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_clock_probe *s=&m->clock;
    if(!wifi_clock_owned(d) || address!=0x400 || value!=0x80c00 || m->clock_submitted ||
        !s->attempted || !s->write_attempted || s->written || s->complete || s->reads!=3 ||
        s->mac_before || (s->status_before&3) || s->pbf_before!=0x80000)return false;
    m->clock_submitted=true;
    for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=(uint8_t)(value>>(8*i));
    struct usb_setup_packet setup={.request_type=0x40,.request=6,.index=0x400,.length=4};
    int actual=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,false,&actual) || actual!=4)return wifi_mcu_fail(d);
    return true;
}
static bool wifi_mcu_clock_enable(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->clock_active=true;
    const struct mt76_clock_io io={{d,wifi_clock_owned,wifi_clock_read},wifi_clock_write};
    enum mt76_radio_status status=mt76_clock_enable(&m->clock,&io);
    m->clock_active=false;struct mt76_clock_probe *s=&m->clock;
    console_printf("wifi-clock: status=%u attempted=%u write_attempted=%u submitted=%u written=%u complete=%u reads=%u pbf_before=%x pbf_after=%x mac_before=%x mac_after=%x status_before=%x status_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->write_attempted,(uint64_t)m->clock_submitted,(uint64_t)s->written,(uint64_t)s->complete,(uint64_t)s->reads,
        (uint64_t)s->pbf_before,(uint64_t)s->pbf_after,(uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->status_before,(uint64_t)s->status_after);
    if(status!=MT76_RADIO_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_bbp_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_mcu_owned(d) && m->configured && !m->pending && m->bbp_active &&
        (m->compact_mode ? (m->compact_next==2 && wifi_compact_prefix(m,2)) :
        (m->format.complete && m->format.ack_mask==3 && m->format.batches_completed==2 && m->mac.complete)) &&
        m->timing.complete && m->identity_validated &&
        m->write_validated && d->firmware.radio_reset.prepared;
}
static bool wifi_bbp_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;
    if(!value || !wifi_bbp_owned(d) || (address!=0x1004 && address!=0x1200 &&
        address!=0x0400 && address!=0x2000))return false;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};
    int actual=0;*value=0;
    if(!wifi_fw_control(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true; /* Pure engine rejects all-ones and bounds zero-core polling. */
}
static bool wifi_mcu_probe_bbp(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;m->bbp_active=true;
    const struct mt76_bbp_io io={d,wifi_bbp_owned,wifi_bbp_read};
    enum mt76_radio_status status=mt76_bbp_probe_ready(&m->bbp,&io);
    m->bbp_active=false;struct mt76_bbp_probe *s=&m->bbp;
    console_printf("wifi-bbp-ready: status=%u attempted=%u complete=%u reads=%u core_reads=%u core=%x pbf=%x mac_before=%x mac_after=%x status_before=%x status_after=%x\n",
        (uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,
        (uint64_t)s->core_reads,(uint64_t)s->core,(uint64_t)s->pbf,
        (uint64_t)s->mac_before,(uint64_t)s->mac_after,(uint64_t)s->status_before,(uint64_t)s->status_after);
    if(status!=MT76_RADIO_OK)return wifi_mcu_fail(d);
    return true;
}

static bool wifi_mcu_initialize_compact(struct xhci_device *d,unsigned limit,bool clocks) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(m->compact_mode || m->compact_next || ((limit<5 && !(m->table_mode && limit==2)) || limit>12))return wifi_mcu_fail(d);
    m->compact_mode=true;m->compact_limit=limit;m->clock_mode=clocks;
    for(unsigned phase=0;phase<limit;phase++) {
        if(phase==2 && clocks && !wifi_mcu_clock_enable(d))return false;
        if(phase==2 && !wifi_mcu_probe_bbp(d))return false;
        if(phase==2 && clocks && m->bbp.pbf!=0x80c00)return wifi_mcu_fail(d);
        if(!wifi_mcu_compact_step(d,phase))return false;
    }
    return true;
}

static bool wifi_mac_board_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_mac_address_result *s=&m->mac_address;struct mt76_mac_address_gate *g=&m->mac_address_gate;
    if(!m->mac_board_mode || !m->mac_board_active || !m->mac_address_validate_mode ||
       m->mac_header_active || m->mac_address_active || !wifi_mac_private_owned(d) ||
       !s->attempted || !s->complete || s->operations!=160 || s->reads!=140 || s->writes!=20 || s->verified!=20 ||
       !g->captured_readback || g->operations!=160 || g->submitted!=160 || g->failed || g->pending ||
       !m->mac_header.complete || !m->mac_header.valid || m->mac_header.blocks!=1 || m->mac_header.writes!=1 ||
       m->mac_header_gate.phase!=15 || m->mac_header_gate.failed || m->mac_header_gate.pending)return false;
    for(unsigned i=0;i<40;i++)if(g->mac[i] || g->core[i]!=0xf000f200)return false;
    for(unsigned i=0;i<20;i++)if(g->dma[i]&15)return false;
    for(unsigned i=0;i<16;i++)if(m->mac_header_private[i])return false;
    const unsigned indexes[4]={4,5,20,21};
    for(unsigned i=0;i<4;i++)if(m->mac_tail.values[indexes[i]]!=mt76_mac_board_initial(i))return false;
    return true;
}
static bool wifi_mac_board_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_mac_board_owned(d) || m->mac_board_submitted!=op || !mt76_mac_board_transfer_valid(&m->mac_board,op,write,a,data,n))return false;
    m->mac_board_submitted++;
    for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=write?data[i]:0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=(uint16_t)n};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,n,!write,&actual) && actual==(int)n;
    if(ok && !write)for(unsigned i=0;i<n;i++)data[i]=d->descriptor_buffer[i];
    /* Never log or retain verification bytes in the shared descriptor buffer. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=0;
    return ok?true:wifi_mcu_fail(d);
}

static bool wifi_mcu_mac_board(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_board *s=&m->mac_board;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=true;
    const struct mt76_mac_board_io io={d,wifi_mac_board_owned,wifi_mac_board_transfer};
    enum mt76_mcu_timing_status status=mt76_mac_board_apply(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=false;
    console_printf("wifi-mac-board: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->mac_board_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<8;i++)console_printf("wifi-mac-board-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i]);
    for(unsigned i=0;i<4;i++)console_printf("wifi-mac-board-value: index=%u address=%x before=%x after=%x dma=%x\n",(uint64_t)i,(uint64_t)mt76_mac_board_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i],(uint64_t)s->dma[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_ant_snapshot_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_mac_board *b=&m->mac_board;
    if(!m->ant_snapshot_mode || !m->ant_snapshot_active || !wifi_mac_board_owned(d) ||
       !b->attempted || !b->complete || b->operations!=32 || m->mac_board_submitted!=32 || b->reads!=28 || b->writes!=4 || b->verified!=4)return false;
    for(unsigned i=0;i<4;i++)if(b->before[i]!=mt76_mac_board_initial(i) || b->after[i]!=mt76_mac_board_value(i) || (b->dma[i]&15))return false;
    for(unsigned i=0;i<8;i++)if(b->mac[i] || b->core[i]!=0xf000f200)return false;
    return true;
}
static bool wifi_ant_snapshot_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_snapshot *s=&m->ant_snapshot;
    if(!value || !wifi_ant_snapshot_owned(d) || !s->attempted || s->complete || s->reads>=11 ||
       m->ant_snapshot_submitted!=s->reads || address!=mt76_ant_snapshot_request(s->reads))return false;
    m->ant_snapshot_submitted++;*value=0;
    struct usb_setup_packet setup={.request_type=0xc0,.request=7,.index=(uint16_t)address,.length=4};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    if(!wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,4,true,&actual) || actual!=4)return wifi_mcu_fail(d);
    for(unsigned i=0;i<4;i++)*value|=(uint32_t)d->descriptor_buffer[i]<<(8*i);
    return true;
}
static bool wifi_mcu_ant_snapshot(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_snapshot *s=&m->ant_snapshot;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=true;
    const struct mt76_mcu_timing_io io={.context=d,.authorize=wifi_ant_snapshot_owned,.read=wifi_ant_snapshot_read};
    enum mt76_mcu_timing_status status=mt76_ant_snapshot_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=false;
    console_printf("wifi-ant-snapshot: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->ant_snapshot_submitted);
    for(unsigned i=0;i<2;i++)console_printf("wifi-ant-snapshot-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i],(uint64_t)s->dma[i]);
    for(unsigned i=0;i<5;i++)console_printf("wifi-ant-snapshot-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_ant_snapshot_address(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_ant_ready(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_snapshot *s=&m->ant_snapshot;
    if(!m->ant_init_mode || !wifi_ant_snapshot_owned(d) || !s->attempted || !s->complete || s->reads!=11 || m->ant_snapshot_submitted!=11)return false;
    for(unsigned i=0;i<5;i++)if(s->values[i]!=mt76_ant_init_initial(i))return false;
    for(unsigned i=0;i<2;i++)if(s->mac[i] || s->core[i]!=0xf000f200 || (s->dma[i]&15))return false;
    return true;
}
static bool wifi_ant_fields_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->ant_fields_active && !m->ant_init_active && !m->ant_fields.complete && !m->ant_fields_gate.failed && wifi_ant_ready(d);
}
static bool wifi_ant_fields_transfer(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
    struct mt76_efuse_map_gate *g=&d->firmware.mcu.ant_fields_gate;
    if(!v || !d->firmware.mcu.ant_fields.attempted || !wifi_ant_fields_owned(d) || !mt76_ant_fields_begin(g,write,a,*v))return false;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    return mt76_ant_fields_finish(g,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_ant_fields_read(void *context,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_ant_fields_transfer(context,false,a,v);}
static bool wifi_ant_fields_write(void *context,uint32_t a,uint32_t v){return wifi_ant_fields_transfer(context,true,a,&v);}
static bool wifi_ant_fields_wait(void *context,uint32_t us){
    struct xhci_device *d=context;struct mt76_efuse_map_gate *g=&d->firmware.mcu.ant_fields_gate;
    if(!wifi_ant_fields_owned(d) || !mt76_efuse_map_delay(g,us))return false;
    if(timer_delay_us(us))return true;
    g->failed=true;return wifi_mcu_fail(d);
}
static bool wifi_ant_init_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_fields *f=&m->ant_fields;struct mt76_efuse_map_gate *g=&m->ant_fields_gate;
    if(!m->ant_init_active || m->ant_fields_active || !wifi_ant_ready(d) || !f->valid || !mt76_ant_fields_valid(f) ||
       g->phase!=15 || g->block!=2 || g->pending || g->failed || g->polls!=f->polls || g->submitted!=f->reads+f->writes)return false;
    for(unsigned i=0;i<4;i++)if(g->mac[i] || g->core[i]!=0xf000f200)return false;
    for(unsigned i=0;i<2;i++)if(g->dma[i]&15)return false;
    return true;
}
static bool wifi_ant_init_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned n){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_ant_init_owned(d) || m->ant_init_submitted!=op || !mt76_ant_init_transfer_valid(&m->ant_init,op,write,a,data,n))return false;
    m->ant_init_submitted++;
    for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=write?data[i]:0;
    struct usb_setup_packet setup={.request_type=write?0x40:0xc0,.request=write?6:7,.index=(uint16_t)a,.length=(uint16_t)n};int actual=0;
    d->firmware.stop_attempted=d->firmware.stop_ok=false;d->firmware.stop_event_cc=0;
    bool ok=wifi_fw_control_once(d,&setup,d->descriptor_buffer_phys,n,!write,&actual) && actual==(int)n;
    if(ok && !write)for(unsigned i=0;i<n;i++)data[i]=d->descriptor_buffer[i];
    /* Never log or retain verification bytes in the shared descriptor buffer. */
    if(!d->ep0_quarantined)for(unsigned i=0;i<n;i++)d->descriptor_buffer[i]=0;
    return ok?true:wifi_mcu_fail(d);
}

static bool wifi_mcu_ant_init(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_fields *f=&m->ant_fields;struct mt76_efuse_map_gate *g=&m->ant_fields_gate;struct mt76_ant_init *s=&m->ant_init;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_fields_active=true;
    const struct mt76_efuse_map_io fields_io={{d,wifi_ant_fields_read,wifi_ant_fields_write,wifi_ant_fields_wait},wifi_ant_fields_owned};
    enum mt76_capture_status fields_status=mt76_capture_ant_fields(&fields_io,&d->wifi_identity,f);
    m->ant_fields_active=false;m->ant_init_active=true;
    enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    if(fields_status==MT76_CAPTURE_OK && wifi_ant_init_owned(d)){
        const struct mt76_ant_init_io io={d,wifi_ant_init_owned,wifi_ant_init_transfer};status=mt76_ant_init_apply(s,&io);
    }
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=false;
    unsigned fg=0,ig=0;
    for(unsigned i=0;i<4;i++)if(g->mac[i] || g->core[i]!=0xf000f200)fg++;
    for(unsigned i=0;i<2;i++)if(g->dma[i]&15)fg++;
    for(unsigned i=0;i<10;i++)if(s->mac[i] || s->core[i]!=0xf000f200)ig++;
    for(unsigned i=0;i<5;i++)if(s->dma[i]&15)ig++;
    console_printf("wifi-ant-fields: status=%u complete=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u guard_errors=%u\n",(uint64_t)fields_status,(uint64_t)f->complete,(uint64_t)f->valid,(uint64_t)f->reads,(uint64_t)f->writes,(uint64_t)f->polls,(uint64_t)f->blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed,(uint64_t)fg);
    console_printf("wifi-ant-init: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u guard_errors=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->ant_init_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified,(uint64_t)ig);
    for(unsigned i=0;i<5;i++)console_printf("wifi-ant-init-value: index=%u before=%x after=%x\n",(uint64_t)i,(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_rf_probe_owned(void *context) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_ant_init *a=&m->ant_init;
    if(!m->rf_probe_mode || !m->rf_probe_active || !wifi_ant_init_owned(d) ||
       !a->attempted || !a->complete || a->operations!=40 || a->reads!=35 || a->writes!=5 || a->verified!=5 || m->ant_init_submitted!=40)return false;
    for(unsigned i=0;i<5;i++)if(a->before[i]!=mt76_ant_init_initial(i) || a->after[i]!=mt76_ant_init_value(i) || (a->dma[i]&15))return false;
    for(unsigned i=0;i<10;i++)if(a->mac[i] || a->core[i]!=0xf000f200)return false;
    return true;
}
static bool wifi_rf_probe_read(void *context,uint32_t address,uint32_t *value) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_probe *s=&m->rf_probe;
    unsigned n=m->rf_probe_reads;
    if(!value || !wifi_rf_probe_owned(d) || !s->attempted || s->complete || n>=6 || s->reads!=n+1 ||
       address!=mt76_rf_probe_read_address(n) || (n<3?(m->rf_probe_submitted || s->request_attempted):(!m->rf_probe_submitted || !s->reply_valid)))return false;
    m->rf_probe_reads++;
    *value=0;return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_probe_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_probe *s=&m->rf_probe;uint8_t allowed[16];
    if(!wifi_rf_probe_owned(d) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX || m->rf_probe_submitted ||
       !s->attempted || !s->request_attempted || s->reply_valid || s->complete || s->reads!=3 || m->rf_probe_reads!=3 ||
       s->mac[0] || s->core[0]!=0xf000f200 || (s->dma[0]&15) || length!=mt76_rf_probe_packet(allowed,sizeof allowed))return -1;
    for(unsigned i=0;i<length;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_probe_submitted=true;
    for(unsigned i=0;i<length;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,length),received=-1;
    if(sent==(int)length)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-rf-probe-command: sequence=8 sent=%d received=%d cc=%u residue=%u\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_probe(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_probe *s=&m->rf_probe;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_probe_owned,wifi_rf_probe_read,wifi_rf_probe_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_probe_apply(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=false;
    console_printf("wifi-rf-probe: status=%u attempted=%u complete=%u reads=%u submitted_reads=%u request=%u submitted=%u reply_valid=%u received=%d bank=0 reg=22 value=%u writes=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->rf_probe_reads,(uint64_t)s->request_attempted,(uint64_t)m->rf_probe_submitted,(uint64_t)s->reply_valid,(int64_t)s->received,(uint64_t)s->value);
    for(unsigned i=0;i<2;i++)console_printf("wifi-rf-probe-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->mac[i],(uint64_t)s->core[i],(uint64_t)s->dma[i]);
    for(unsigned i=0;i<4;i++)console_printf("wifi-rf-probe-word: index=%u value=%x\n",(uint64_t)i,(uint64_t)s->words[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_rf_table_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_probe *p=&m->rf_probe;
    if(!m->rf_table_mode || !m->rf_table_active || !wifi_rf_probe_owned(d) ||
       !p->attempted || !p->request_attempted || !p->reply_valid || !p->complete || p->reads!=6 ||
       m->rf_probe_reads!=6 || !m->rf_probe_submitted || p->received!=16 || p->value!=0 ||
       p->words[0]!=0x4a080008 || p->words[1]!=0x80000016 || p->words[2] || p->words[3]!=0x4a080008)return false;
    for(unsigned i=0;i<2;i++)if(p->mac[i] || p->core[i]!=0xf000f200 || p->dma[i]==UINT32_MAX || (p->dma[i]&15))return false;
    return true;
}
static bool wifi_rf_table_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_table_submitted;
    if(!value || !wifi_rf_table_owned(d) || !mt76_rf_table_admit(&m->rf_table,op) || address==UINT32_MAX || address!=mt76_rf_table_guard_address(op))return false;
    m->rf_table_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_table_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_table_submitted;uint8_t allowed[16];
    if(!wifi_rf_table_owned(d) || !mt76_rf_table_admit(&m->rf_table,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_table_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_table_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-table-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_table_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_table(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_table;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_table_owned,wifi_rf_table_read,wifi_rf_table_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_table_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=false;
    console_printf("wifi-rf-table: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_table_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask);
    for(unsigned i=0;i<MT76_RF_TABLE_COMMANDS*2;i++)console_printf("wifi-rf-table-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_CENTRAL_COUNT;i++)console_printf("wifi-rf-table-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_central_address(i),(uint64_t)mt76_rf_central_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_bank5_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_bank5_mode && m->rf_bank5_active && wifi_rf_table_owned(d) &&
        m->rf_table_submitted==MT76_RF_TABLE_OPS && mt76_rf_table_zero_profile_valid(&m->rf_table);
}
static bool wifi_rf_bank5_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_bank5_submitted;
    if(!value || !wifi_rf_bank5_owned(d) || !mt76_rf_bank5_admit(&m->rf_bank5,op) || address==UINT32_MAX || address!=mt76_rf_bank5_guard_address(op))return false;
    m->rf_bank5_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_bank5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_bank5_submitted;uint8_t allowed[16];
    if(!wifi_rf_bank5_owned(d) || !mt76_rf_bank5_admit(&m->rf_bank5,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_bank5_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_bank5_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-bank5-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_bank5_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_bank5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_bank5;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_bank5_owned,wifi_rf_bank5_read,wifi_rf_bank5_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_bank5_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=false;
    console_printf("wifi-rf-bank5: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_bank5_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_BANK5_COMMANDS*2;i++)console_printf("wifi-rf-bank5-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_BANK5_COUNT;i++)console_printf("wifi-rf-bank5-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_bank5_address(i),(uint64_t)mt76_rf_bank5_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_bank6_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_bank6_mode && m->rf_bank6_active && wifi_rf_bank5_owned(d) &&
        m->rf_bank5_submitted==MT76_RF_BANK5_OPS && mt76_rf_bank5_exact_profile_valid(&m->rf_bank5);
}
static bool wifi_rf_bank6_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_bank6_submitted;
    if(!value || !wifi_rf_bank6_owned(d) || !mt76_rf_bank6_admit(&m->rf_bank6,op) || address==UINT32_MAX || address!=mt76_rf_bank6_guard_address(op))return false;
    m->rf_bank6_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_bank6_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_bank6_submitted;uint8_t allowed[16];
    if(!wifi_rf_bank6_owned(d) || !mt76_rf_bank6_admit(&m->rf_bank6,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_bank6_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_bank6_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-bank6-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_bank6_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_bank6(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_bank6;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_bank6_owned,wifi_rf_bank6_read,wifi_rf_bank6_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_bank6_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=false;
    console_printf("wifi-rf-bank6: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_bank6_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_BANK6_COMMANDS*2;i++)console_printf("wifi-rf-bank6-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_BANK6_COUNT;i++)console_printf("wifi-rf-bank6-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_bank6_address(i),(uint64_t)mt76_rf_bank6_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_vga_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_vga_mode && m->rf_vga_active && wifi_rf_bank6_owned(d) &&
        m->rf_bank6_submitted==MT76_RF_BANK6_OPS && mt76_rf_bank6_exact_profile_valid(&m->rf_bank6);
}
static bool wifi_rf_vga_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_vga_submitted;
    if(!value || !wifi_rf_vga_owned(d) || !mt76_rf_vga_admit(&m->rf_vga,op) || address==UINT32_MAX || address!=mt76_rf_vga_guard_address(op))return false;
    m->rf_vga_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_vga_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_vga_submitted;uint8_t allowed[16];
    if(!wifi_rf_vga_owned(d) || !mt76_rf_vga_admit(&m->rf_vga,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_vga_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_vga_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-vga-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_vga_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_vga(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_vga;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_vga_owned,wifi_rf_vga_read,wifi_rf_vga_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_vga_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    console_printf("wifi-rf-vga: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_vga_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_VGA_COMMANDS*2;i++)console_printf("wifi-rf-vga-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_VGA_COUNT;i++)console_printf("wifi-rf-vga-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_vga_address(i),(uint64_t)mt76_rf_vga_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
struct wifi_5ghz_prereq_context {struct xhci_device *d;struct mt76_5ghz_prereq *s;unsigned submitted;bool active;};
static bool wifi_5ghz_prereq_owned(void *context){
    struct wifi_5ghz_prereq_context *c=context;struct xhci_wifi_mcu *m=&c->d->firmware.mcu;
    return wifi_5ghz_prereq_requested && c->active && c->submitted==c->s->operations &&
        wifi_rf_vga_owned(c->d) && m->rf_vga_submitted==MT76_RF_VGA_OPS && mt76_rf_vga_exact_profile_valid(&m->rf_vga);
}
static bool wifi_5ghz_prereq_read(void *context,uint32_t address,uint32_t *value){
    struct wifi_5ghz_prereq_context *c=context;unsigned op=c->submitted;
    if(!value || !wifi_5ghz_prereq_owned(c) || !mt76_5ghz_prereq_admit(c->s,op) || mt76_5ghz_prereq_rf(op) || address!=mt76_5ghz_prereq_address(op))return false;
    c->submitted++;*value=0;bool ok=wifi_mac_private_usb(c->d,false,address,value);
    console_printf("wifi-5ghz-prereq-usb: op=%u address=%x value=%x ok=%u writes=0\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(c->d);
}
static int wifi_5ghz_prereq_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct wifi_5ghz_prereq_context *c=context;struct xhci_device *d=c->d;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=c->submitted;uint8_t allowed[16];
    if(!wifi_5ghz_prereq_owned(c) || !mt76_5ghz_prereq_admit(c->s,op) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_5ghz_prereq_packet(op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    c->submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-5ghz-prereq-command: op=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x writes=0\n",(uint64_t)op,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_5ghz_prereq(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_5ghz_prereq s={0};struct wifi_5ghz_prereq_context c={d,&s,0,true};
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    const struct mt76_mcu_timing_io io={&c,wifi_5ghz_prereq_owned,wifi_5ghz_prereq_read,wifi_5ghz_prereq_exchange};
    enum mt76_mcu_timing_status status=mt76_5ghz_prereq_capture(&s,&io);c.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && c.submitted==49 && mt76_5ghz_prereq_valid(&s);
    console_printf("wifi-5ghz-prereq: status=%u complete=%u valid=%u operations=%u submitted=%u reads=%u exchanges=%u observed=%u writes=0 network_out=0\n",(uint64_t)status,(uint64_t)s.complete,(uint64_t)valid,(uint64_t)s.operations,(uint64_t)c.submitted,(uint64_t)s.reads,(uint64_t)s.exchanges,(uint64_t)s.observed);
    for(unsigned i=0;i<7;i++)console_printf("wifi-5ghz-prereq-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_5ghz_prereq_target(i),(uint64_t)s.values[i]);
    return valid?true:wifi_mcu_fail(d);
}
/* One boot-owned transaction, separate fixed state for every forward/reverse
 * step; never put the proof/DMA owner on the small kernel stack. */
static struct {
    struct xhci_device *d;
    struct mt76_5ghz_band state;
    unsigned submitted[10],last_phase;
    bool active;
} wifi_5ghz_band;
static const char *wifi_5ghz_band_prefix(unsigned phase){
    static const char *prefix[10]={"refused","wifi-5ghz-prereq","wifi-5ghz-select-rf","wifi-5ghz-select-mac","wifi-5ghz-selected","wifi-5ghz-restore-mac","wifi-5ghz-restore-rf","wifi-5ghz-original","wifi-5ghz-synth","wifi-a20-cycle"};
    return phase<10?prefix[phase]:"refused";
}
static bool wifi_5ghz_band_owned(void *context){
    if(context!=&wifi_5ghz_band)return false;
    struct xhci_device *d=wifi_5ghz_band.d;struct mt76_5ghz_band *s=&wifi_5ghz_band.state;
    if(!d || !wifi_5ghz_band_requested || !wifi_5ghz_prereq_requested || !wifi_5ghz_band.active || !mt76_5ghz_band_owned(s) || s->channel161!=wifi_5ghz_channel_requested || s->a20_cycle!=wifi_a20_cycle_requested)return false;
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_rf_vga_owned(d) && m->rf_vga_submitted==MT76_RF_VGA_OPS && mt76_rf_vga_exact_profile_valid(&m->rf_vga) &&
        wifi_5ghz_band.submitted[mt76_5ghz_band_wire_phase(s)]==mt76_5ghz_band_operations(s);
}
static bool wifi_5ghz_band_authorize(void *context){
    if(!wifi_5ghz_band_owned(context))return false;
    unsigned phase=mt76_5ghz_band_wire_phase(&wifi_5ghz_band.state);
    if(wifi_5ghz_band.last_phase!=phase){
        wifi_5ghz_band.last_phase=phase;
        if(phase==9)console_write("wifi-a20-cycle-progress: fixed24 high-band/20MHz words,verify and reverse restoration;no VCO/RX/network OUT\n");
        console_printf("wifi-5ghz-band-phase: index=%u start=1\n",(uint64_t)phase);
    }
    return true;
}
static bool wifi_5ghz_band_read(void *context,uint32_t address,uint32_t *value){
    struct mt76_5ghz_band *s=&wifi_5ghz_band.state;unsigned phase=mt76_5ghz_band_wire_phase(s),op=phase<10?wifi_5ghz_band.submitted[phase]:UINT32_MAX;
    if(!value || !wifi_5ghz_band_owned(context) || !mt76_5ghz_band_read_valid(s,op,address))return false;
    wifi_5ghz_band.submitted[phase]++;*value=0;bool ok=wifi_mac_private_usb(wifi_5ghz_band.d,false,address,value);
    console_printf("%s-usb: op=%u address=%x value=%x ok=%u writes=0\n",wifi_5ghz_band_prefix(phase),(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_5ghz_band.d);
}
static int wifi_5ghz_band_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct mt76_5ghz_band *s=&wifi_5ghz_band.state;unsigned phase=mt76_5ghz_band_wire_phase(s),op=phase<10?wifi_5ghz_band.submitted[phase]:UINT32_MAX;uint8_t allowed[16];
    if(!wifi_5ghz_band_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_5ghz_band_packet(s,op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    struct xhci_device *d=wifi_5ghz_band.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    wifi_5ghz_band.submitted[phase]++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    bool write=(phase==2 && op/7<40) || (phase==6 && op/7<2) || (phase==8 && mt76_synth5_write(op/7)) || (phase==9 && mt76_a20_cycle_write(op/7));
    console_printf("%s-command: op=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x writes=%u\n",wifi_5ghz_band_prefix(phase),(uint64_t)op,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3],(uint64_t)write);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_5ghz_band_transfer(void *context,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
    struct mt76_5ghz_band *s=&wifi_5ghz_band.state;unsigned phase=mt76_5ghz_band_wire_phase(s);
    if(!wifi_5ghz_band_owned(context) || !mt76_5ghz_band_transfer_valid(s,op,write,address,data,length))return false;
    uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)data[b]<<(8*b);
    wifi_5ghz_band.submitted[phase]++;bool ok=wifi_mac_private_usb(wifi_5ghz_band.d,write,address,&v);
    if(!write && ok)for(unsigned b=0;b<4;b++)data[b]=(uint8_t)(v>>(8*b));
    console_printf("%s-usb: op=%u address=%x value=%x ok=%u writes=%u\n",wifi_5ghz_band_prefix(phase),(uint64_t)op,(uint64_t)address,(uint64_t)v,(uint64_t)ok,(uint64_t)write);
    return ok?true:wifi_mcu_fail(wifi_5ghz_band.d);
}
static void wifi_5ghz_inventory_report(const char *prefix,const struct mt76_5ghz_prereq *s,unsigned submitted){
    bool valid=mt76_5ghz_prereq_valid(s);
    console_printf("%s: status=%u complete=%u valid=%u operations=%u submitted=%u reads=%u exchanges=%u observed=%u writes=0 network_out=0\n",prefix,(uint64_t)(valid?0:MT76_TIMING_STATE),(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->operations,(uint64_t)submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->observed);
    for(unsigned i=0;i<7;i++)console_printf("%s-value: index=%u address=%x value=%x\n",prefix,(uint64_t)i,(uint64_t)mt76_5ghz_prereq_target(i),(uint64_t)s->values[i]);
}
static bool wifi_mcu_a20_inventory(struct xhci_device *d);
static bool wifi_mcu_frontend(struct xhci_device *d);
static bool wifi_mcu_pa5(struct xhci_device *d);
static bool wifi_mcu_bbp5(struct xhci_device *d);
static bool wifi_mcu_5ghz_band(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_5ghz_band *s=&wifi_5ghz_band.state;
    if(s->attempted || wifi_5ghz_band.active)return false;
    wifi_5ghz_band.d=d;wifi_5ghz_band.active=true;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    if(wifi_5ghz_channel_requested)console_write("wifi-5ghz-channel-progress: fixed161 masked fields,13-word verification and reverse restoration;no VCO/RX/network OUT\n");
    else console_write("wifi-5ghz-band-progress: fixed5GHz band selection,verify,four-word restoration;no frequency/VCO/RX/network OUT\n");
    const struct mt76_mcu_timing_io rf={&wifi_5ghz_band,wifi_5ghz_band_authorize,wifi_5ghz_band_read,wifi_5ghz_band_exchange};
    const struct mt76_band_mac_io mac={&wifi_5ghz_band,wifi_5ghz_band_authorize,wifi_5ghz_band_transfer};
    enum mt76_mcu_timing_status status=wifi_a20_cycle_requested?mt76_5ghz_a20_capture(s,&rf,&mac):wifi_5ghz_channel_requested?mt76_5ghz_channel_capture(s,&rf,&mac):mt76_5ghz_band_capture(s,&rf,&mac);wifi_5ghz_band.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    wifi_5ghz_inventory_report("wifi-5ghz-prereq",&s->before,wifi_5ghz_band.submitted[1]);
    wifi_5ghz_inventory_report("wifi-5ghz-selected",&s->selected,wifi_5ghz_band.submitted[4]);
    wifi_5ghz_inventory_report("wifi-5ghz-original",&s->original,wifi_5ghz_band.submitted[7]);
    for(unsigned phase=2;phase<=6;phase++){
        if(phase==4)continue;
        unsigned operations=phase==2?s->rf.operations:phase==3?s->mac.operations:phase==5?s->mac_restore.operations:s->rf_restore.operations;
        bool valid=phase==2?mt76_rf_band5g_exact_profile_valid(&s->rf):phase==3?mt76_band5g_mac_exact_profile_valid(&s->mac):phase==5?mt76_restore_mac_exact_profile_valid(&s->mac_restore):mt76_rf_restore_exact_profile_valid(&s->rf_restore);
        console_printf("%s: complete=%u valid=%u operations=%u submitted=%u\n",wifi_5ghz_band_prefix(phase),(uint64_t)valid,(uint64_t)valid,(uint64_t)operations,(uint64_t)wifi_5ghz_band.submitted[phase]);
    }
    if(wifi_5ghz_channel_requested)console_printf("wifi-5ghz-synth: complete=%u valid=%u operations=%u submitted=%u writes=%u\n",(uint64_t)s->synth.complete,(uint64_t)mt76_synth5_valid(&s->synth),(uint64_t)s->synth.operations,(uint64_t)wifi_5ghz_band.submitted[8],(uint64_t)s->synth.writes);
    if(wifi_a20_cycle_requested)console_printf("wifi-a20-cycle: complete=%u valid=%u operations=%u submitted=%u writes=%u\n",(uint64_t)s->a20.complete,(uint64_t)mt76_a20_cycle_valid(&s->a20),(uint64_t)s->a20.operations,(uint64_t)wifi_5ghz_band.submitted[9],(uint64_t)s->a20.acks);
    bool valid=status==MT76_TIMING_OK && mt76_5ghz_band_valid(s);unsigned operations=0;
    for(unsigned phase=1;phase<=9;phase++)operations+=wifi_5ghz_band.submitted[phase];
    console_printf("wifi-5ghz-band: status=%u complete=%u restored=%u phase=%u operations=%u network_out=0\n",(uint64_t)status,(uint64_t)valid,(uint64_t)s->restored,(uint64_t)s->phase,(uint64_t)operations);
    if(wifi_5ghz_channel_requested)console_printf("wifi-5ghz-channel: status=%u complete=%u restored=%u channel=161 operations=%u network_out=0\n",(uint64_t)status,(uint64_t)valid,(uint64_t)(s->restored && mt76_synth5_valid(&s->synth)),(uint64_t)operations);
    if(valid && wifi_a20_inventory_requested)valid=wifi_mcu_a20_inventory(d);
    if(valid && wifi_frontend_requested)valid=wifi_mcu_frontend(d);
    return valid?true:wifi_mcu_fail(d);
}
static struct {struct xhci_device *d;struct mt76_rf_table state;unsigned submitted;bool active;} wifi_a20_inventory;
static bool wifi_a20_inventory_owned(void *context){
    if(context!=&wifi_a20_inventory || !wifi_a20_inventory_requested || !wifi_a20_inventory.active)return false;
    struct xhci_device *d=wifi_a20_inventory.d;struct mt76_rf_table *s=&wifi_a20_inventory.state;
    return d && d==wifi_5ghz_band.d && wifi_5ghz_channel_requested && wifi_5ghz_band_requested && wifi_5ghz_prereq_requested &&
        wifi_5ghz_band.state.channel161 && mt76_5ghz_band_valid(&wifi_5ghz_band.state) && wifi_rf_vga_owned(d) &&
        mt76_rf_vga_exact_profile_valid(&d->firmware.mcu.rf_vga) && d->firmware.mcu.rf_vga_submitted==MT76_RF_VGA_OPS &&
        s->attempted && !s->complete && s->operations<=168 && wifi_a20_inventory.submitted==s->operations;
}
static bool wifi_a20_inventory_read(void *context,uint32_t address,uint32_t *value){
    unsigned op=wifi_a20_inventory.submitted;
    if(!value || !wifi_a20_inventory_owned(context) || !mt76_a20_inventory_admit(&wifi_a20_inventory.state,op) || address==UINT32_MAX || address!=mt76_a20_inventory_guard_address(op))return false;
    wifi_a20_inventory.submitted++;*value=0;bool ok=wifi_mac_private_usb(wifi_a20_inventory.d,false,address,value);
    console_printf("wifi-a20-inventory-usb: op=%u address=%x value=%x ok=%u writes=0\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_a20_inventory.d);
}
static int wifi_a20_inventory_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    unsigned op=wifi_a20_inventory.submitted;uint8_t allowed[16];
    if(!wifi_a20_inventory_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_a20_inventory_packet(&wifi_a20_inventory.state,op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    struct xhci_device *d=wifi_a20_inventory.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    wifi_a20_inventory.submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-a20-inventory-command: op=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x writes=0\n",(uint64_t)op,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_a20_inventory(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&wifi_a20_inventory.state;
    if(s->attempted || wifi_a20_inventory.active || !mt76_5ghz_band_valid(&wifi_5ghz_band.state))return false;
    wifi_a20_inventory.d=d;wifi_a20_inventory.active=true;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    console_write("wifi-a20-inventory-progress: read24 fixed RF originals after channel161 restoration;no new RF writes/network OUT\n");
    const struct mt76_mcu_timing_io io={&wifi_a20_inventory,wifi_a20_inventory_owned,wifi_a20_inventory_read,wifi_a20_inventory_exchange};
    enum mt76_mcu_timing_status status=mt76_a20_inventory_capture(s,&io);wifi_a20_inventory.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_a20_inventory_valid(s);
    for(unsigned i=0;i<24;i++)console_printf("wifi-a20-inventory-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_rf_a20_address(i),(uint64_t)s->values[i]);
    console_printf("wifi-a20-inventory: status=%u complete=%u valid=%u operations=%u submitted=%u reads=%u exchanges=%u writes=0 network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->operations,(uint64_t)wifi_a20_inventory.submitted,(uint64_t)s->reads,(uint64_t)s->exchanges);
    return valid?true:wifi_mcu_fail(d);
}
static struct {struct xhci_device *d;struct mt76_5ghz_prereq state;unsigned submitted;bool active;} wifi_frontend;
static bool wifi_frontend_owned(void *context){
    if(context!=&wifi_frontend || !wifi_frontend_requested || !wifi_frontend.active)return false;
    struct xhci_device *d=wifi_frontend.d;struct mt76_5ghz_prereq *s=&wifi_frontend.state;
    return d && d==wifi_5ghz_band.d && wifi_5ghz_channel_requested && wifi_5ghz_band_requested && wifi_5ghz_prereq_requested &&
        wifi_a20_cycle_requested && wifi_5ghz_band.state.a20_cycle && wifi_5ghz_band.state.channel161 && mt76_ant_fields_valid(&d->firmware.mcu.ant_fields) && d->firmware.mcu.ant_fields.nic0==0xfd11 && d->firmware.mcu.ant_fields.nic1==0x1004 && mt76_5ghz_band_valid(&wifi_5ghz_band.state) && wifi_rf_vga_owned(d) &&
        mt76_rf_vga_exact_profile_valid(&d->firmware.mcu.rf_vga) && d->firmware.mcu.rf_vga_submitted==MT76_RF_VGA_OPS &&
        s->attempted && !s->complete && s->operations<=308 && wifi_frontend.submitted==s->operations;
}
static bool wifi_frontend_read(void *context,uint32_t address,uint32_t *value){
    unsigned op=wifi_frontend.submitted;
    if(!value || !wifi_frontend_owned(context) || !mt76_frontend_admit(&wifi_frontend.state,op) || mt76_frontend_rf(op) || address==UINT32_MAX || address!=mt76_frontend_address(op))return false;
    wifi_frontend.submitted++;*value=0;bool ok=wifi_mac_private_usb(wifi_frontend.d,false,address,value);
    console_printf("wifi-frontend-usb: op=%u address=%x value=%x ok=%u writes=0\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_frontend.d);
}
static int wifi_frontend_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    unsigned op=wifi_frontend.submitted;uint8_t allowed[16];
    if(!wifi_frontend_owned(context) || !mt76_frontend_admit(&wifi_frontend.state,op) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_frontend_packet(op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    struct xhci_device *d=wifi_frontend.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    wifi_frontend.submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-frontend-command: op=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x writes=0\n",(uint64_t)op,(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_frontend(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_5ghz_prereq *s=&wifi_frontend.state;
    if(s->attempted || wifi_frontend.active || !mt76_5ghz_band_valid(&wifi_5ghz_band.state))return false;
    console_printf("wifi-frontend-board: nic0=%x nic1=%x external5g=%u\n",(uint64_t)m->ant_fields.nic0,(uint64_t)m->ant_fields.nic1,(uint64_t)((m->ant_fields.nic0&0x200)==0));
    wifi_frontend.d=d;wifi_frontend.active=true;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    console_write("wifi-frontend-progress: read44 fixed PA/BBP/control originals after41-word restoration;no new register writes/network OUT\n");
    const struct mt76_mcu_timing_io io={&wifi_frontend,wifi_frontend_owned,wifi_frontend_read,wifi_frontend_exchange};
    enum mt76_mcu_timing_status status=mt76_frontend_capture(s,&io);wifi_frontend.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_frontend_valid(s);
    for(unsigned i=0;i<44;i++)console_printf("wifi-frontend-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_frontend_target(i),(uint64_t)s->values[i]);
    console_printf("wifi-frontend: status=%u complete=%u valid=%u operations=%u submitted=%u reads=%u exchanges=%u writes=0 network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->operations,(uint64_t)wifi_frontend.submitted,(uint64_t)s->reads,(uint64_t)s->exchanges);
    return valid?(!wifi_pa5_requested || wifi_mcu_pa5(d)):wifi_mcu_fail(d);
}
static struct {struct xhci_device *d;struct mt76_pa5 state;unsigned submitted;bool active;} wifi_pa5;
static bool wifi_pa5_parent(struct xhci_device *d){
    if(!d || d!=wifi_frontend.d || d!=wifi_5ghz_band.d || !wifi_pa5_requested || !wifi_frontend_requested ||
       !wifi_a20_cycle_requested || !wifi_5ghz_channel_requested || !wifi_5ghz_band_requested || !wifi_5ghz_prereq_requested ||
       !mt76_frontend_valid(&wifi_frontend.state) || wifi_frontend.submitted!=308 ||
       !mt76_5ghz_band_valid(&wifi_5ghz_band.state) || !wifi_5ghz_band.state.a20_cycle || !wifi_5ghz_band.state.channel161 ||
       !mt76_ant_fields_valid(&d->firmware.mcu.ant_fields) || d->firmware.mcu.ant_fields.nic0!=0xfd11 || d->firmware.mcu.ant_fields.nic1!=0x1004 ||
       !wifi_rf_vga_owned(d) || !mt76_rf_vga_exact_profile_valid(&d->firmware.mcu.rf_vga) || d->firmware.mcu.rf_vga_submitted!=MT76_RF_VGA_OPS)return false;
    return mt76_pa5_parent_valid(&wifi_frontend.state);
}
static bool wifi_pa5_owned(void *context){
    struct mt76_pa5 *s=&wifi_pa5.state;
    return context==&wifi_pa5 && wifi_pa5.active && wifi_pa5_parent(wifi_pa5.d) &&
        s->attempted && !s->complete && !s->restored && s->operations<=490 && wifi_pa5.submitted==s->operations;
}
static bool wifi_pa5_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    unsigned op=wifi_pa5.submitted;
    if(!v || !wifi_pa5_owned(context) || !mt76_pa5_transfer_valid(&wifi_pa5.state,op,write,address,*v))return false;
    wifi_pa5.submitted++;bool ok=wifi_mac_private_usb(wifi_pa5.d,write,address,v);
    console_printf("wifi-pa5-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_pa5.d);
}
static int wifi_pa5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    unsigned op=wifi_pa5.submitted;uint8_t allowed[16];
    if(!wifi_pa5_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_pa5_packet(&wifi_pa5.state,op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    struct xhci_device *d=wifi_pa5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    wifi_pa5.submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-pa5-command: op=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_pa5_write(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_pa5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_pa5 *s=&wifi_pa5.state;
    if(s->attempted || wifi_pa5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_pa5_parent(d);enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-pa5-progress: read14 originals;select external5GHz PA;read14;reverse14;read14;MAC disabled;no BBP/VCO/RX/network OUT\n");
    if(parent){wifi_pa5.d=d;wifi_pa5.active=true;const struct mt76_pa5_io io={&wifi_pa5,wifi_pa5_owned,wifi_pa5_transfer,wifi_pa5_exchange};status=mt76_pa5_capture(s,m->ant_fields.nic0,m->ant_fields.nic1,&io);}
    wifi_pa5.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_pa5_valid(s);
    console_printf("wifi-pa5: status=%u complete=%u valid=%u restored=%u operations=%u submitted=%u reads=%u exchanges=%u writes=%u parent=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->restored,(uint64_t)s->operations,(uint64_t)wifi_pa5.submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->writes,(uint64_t)parent);
    return valid?(!wifi_bbp5_requested || wifi_mcu_bbp5(d)):wifi_mcu_fail(d);
}
static struct {struct xhci_device *d;struct mt76_gain5_fields gain;struct mt76_efuse_map_gate gate;struct mt76_bbp5 state;unsigned submitted;bool active,fields_active;} wifi_bbp5;
static bool wifi_bbp5_parent(struct xhci_device *d){
    return wifi_bbp5_requested && d && d==wifi_pa5.d && wifi_pa5_parent(d) &&
        mt76_pa5_valid(&wifi_pa5.state) && wifi_pa5.submitted==490 && mt76_bbp5_parent_valid(&wifi_frontend.state);
}
static bool wifi_gain5_owned(void *context){
    struct mt76_rx_fields *f=&wifi_bbp5.gain.transport;struct mt76_efuse_map_gate *g=&wifi_bbp5.gate;
    return context==&wifi_bbp5 && wifi_bbp5.active && wifi_bbp5.fields_active && wifi_bbp5_parent(wifi_bbp5.d) &&
        f->attempted && !f->complete && !g->failed && !g->pending && !wifi_bbp5.state.attempted && g->submitted==f->reads+f->writes;
}
static bool wifi_gain5_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    struct mt76_efuse_map_gate *g=&wifi_bbp5.gate;unsigned phase=g->phase,op=g->submitted;
    if(!v || !wifi_gain5_owned(context) || !mt76_rx_fields_begin(g,write,address,*v))return false;
    bool ok=wifi_mac_private_usb(wifi_bbp5.d,write,address,v);bool valid=mt76_rx_fields_finish(g,ok,*v);
    console_printf("wifi-bbp5-field-io: op=%u phase=%u write=%u address=%x ok=%u raw_block_logged=0\n",(uint64_t)op,(uint64_t)phase,(uint64_t)write,(uint64_t)address,(uint64_t)(ok && valid));
    return ok && valid?true:wifi_mcu_fail(wifi_bbp5.d);
}
static bool wifi_gain5_read(void *context,uint32_t address,uint32_t *v){if(!v)return false;*v=0;return wifi_gain5_transfer(context,false,address,v);}
static bool wifi_gain5_write(void *context,uint32_t address,uint32_t v){return wifi_gain5_transfer(context,true,address,&v);}
static bool wifi_gain5_wait(void *context,uint32_t us){
    unsigned phase=wifi_bbp5.gate.phase;
    if(!wifi_gain5_owned(context) || !mt76_efuse_map_delay(&wifi_bbp5.gate,us))return false;
    bool ok=timer_delay_us(us);if(!ok){wifi_bbp5.gate.failed=true;wifi_mcu_fail(wifi_bbp5.d);}
    console_printf("wifi-bbp5-field-delay: phase=%u us=%u ok=%u\n",(uint64_t)phase,(uint64_t)us,(uint64_t)ok);return ok;
}
static bool wifi_bbp5_owned(void *context){
    struct mt76_gain5_fields *f=&wifi_bbp5.gain;struct mt76_efuse_map_gate *g=&wifi_bbp5.gate;struct mt76_bbp5 *s=&wifi_bbp5.state;
    return context==&wifi_bbp5 && wifi_bbp5.active && !wifi_bbp5.fields_active && wifi_bbp5_parent(wifi_bbp5.d) &&
        mt76_gain5_valid(f) && g->phase==15 && g->block==1 && !g->failed && !g->pending && g->polls==f->transport.polls && g->submitted==f->transport.reads+f->transport.writes &&
        s->attempted && !s->complete && !s->restored && s->lna==f->lna && s->operations<=735 && wifi_bbp5.submitted==s->operations;
}
static bool wifi_bbp5_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    unsigned op=wifi_bbp5.submitted;
    if(!v || !wifi_bbp5_owned(context) || !mt76_bbp5_transfer_valid(&wifi_bbp5.state,op,write,address,*v))return false;
    wifi_bbp5.submitted++;bool ok=wifi_mac_private_usb(wifi_bbp5.d,write,address,v);
    console_printf("wifi-bbp5-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_bbp5.d);
}
static bool wifi_mcu_bbp5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp5 *s=&wifi_bbp5.state;struct mt76_gain5_fields *f=&wifi_bbp5.gain;struct mt76_efuse_map_gate *g=&wifi_bbp5.gate;
    if(s->attempted || f->transport.attempted || wifi_bbp5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_bbp5_parent(d);enum mt76_capture_status fs=MT76_CAPTURE_REFUSED;enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-bbp5-progress: fresh logical40 high-band gain;read21 originals;select A-band20MHz;read21;reverse21;read21;no VCO/RX/network OUT\n");
    if(parent){wifi_bbp5.d=d;wifi_bbp5.active=wifi_bbp5.fields_active=true;const struct mt76_efuse_map_io io={{&wifi_bbp5,wifi_gain5_read,wifi_gain5_write,wifi_gain5_wait},wifi_gain5_owned};fs=mt76_capture_gain5(&io,&d->wifi_identity,f);wifi_bbp5.fields_active=false;}
    console_printf("wifi-bbp5-fields: status=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u raw_block_logged=0\n",(uint64_t)fs,(uint64_t)mt76_gain5_valid(f),(uint64_t)f->transport.reads,(uint64_t)f->transport.writes,(uint64_t)f->transport.polls,(uint64_t)f->transport.blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed);
    console_printf("wifi-bbp5-gain: lna=%u fallback=%u clamped=%u agc8=%x channel=161 bandwidth=20 raw_fields_logged=0\n",(uint64_t)f->lna,(uint64_t)f->fallback,(uint64_t)f->clamped,(uint64_t)f->agc8);
    for(unsigned i=0;i<2;i++)console_printf("wifi-bbp5-field-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)g->mac[i],(uint64_t)g->core[i]);
    console_printf("wifi-bbp5-field-dma: value=%x\n",(uint64_t)g->dma[0]);
    if(fs==MT76_CAPTURE_OK && mt76_gain5_valid(f)){const struct mt76_bbp5_io io={&wifi_bbp5,wifi_bbp5_owned,wifi_bbp5_transfer};status=mt76_bbp5_capture(s,f->lna,&io);}
    wifi_bbp5.active=wifi_bbp5.fields_active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_bbp5_valid(s);
    console_printf("wifi-bbp5: status=%u complete=%u valid=%u restored=%u operations=%u submitted=%u reads=%u writes=%u parent=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->restored,(uint64_t)s->operations,(uint64_t)wifi_bbp5.submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)parent);
    return valid?(!wifi_held_requested || wifi_mcu_held(d)):wifi_mcu_fail(d);
}
/* One static device owner; all child plans use its context and authorizer. */
static struct {struct xhci_device *d;struct mt76_channel5 state;unsigned submitted;bool active;} wifi_held;
static bool wifi_held_parent(struct xhci_device *d){
    struct mt76_gain5_fields *f=&wifi_bbp5.gain;struct mt76_efuse_map_gate *g=&wifi_bbp5.gate;
    return wifi_held_requested && d && d==wifi_bbp5.d && wifi_bbp5_parent(d) &&
        !wifi_bbp5.active && !wifi_bbp5.fields_active && mt76_bbp5_valid(&wifi_bbp5.state) && wifi_bbp5.submitted==735 &&
        mt76_gain5_valid(f) && wifi_bbp5.state.lna==f->lna && g->phase==15 && g->block==1 && !g->failed && !g->pending &&
        g->polls==f->transport.polls && g->submitted==f->transport.reads+f->transport.writes;
}
static bool wifi_held_owned(void *context){
    return context==&wifi_held && wifi_held.active && wifi_held_parent(wifi_held.d) &&
        mt76_channel5_owned(&wifi_held.state) && wifi_held.state.lna==wifi_bbp5.gain.lna &&
        wifi_held.submitted==mt76_channel5_operations(&wifi_held.state);
}
static bool wifi_held_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    if(!v || !wifi_held_owned(context) || !mt76_channel5_transfer_valid(&wifi_held.state,write,address,*v))return false;
    unsigned op=wifi_held.submitted++,phase=mt76_channel5_phase(&wifi_held.state),local=mt76_channel5_local(&wifi_held.state);
    bool ok=wifi_mac_private_usb(wifi_held.d,write,address,v);
    console_printf("wifi-held-usb: op=%u phase=%u local=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)write,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_held.d);
}
static bool wifi_held_read(void *context,uint32_t address,uint32_t *v){if(!v)return false;*v=0;return wifi_held_transfer(context,false,address,v);}
static bool wifi_held_mac(void *context,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
    if(!data || length!=4 || !wifi_held_owned(context) || wifi_held.state.phase!=1 || op!=mt76_channel5_local(&wifi_held.state))return false;
    uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)data[i]<<(8*i);
    if(!wifi_held_transfer(context,write,address,&v))return false;
    for(unsigned i=0;i<4;i++)data[i]=(uint8_t)(v>>(8*i));
    return true;
}
static int wifi_held_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    uint8_t allowed[16];
    if(!wifi_held_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_channel5_packet(&wifi_held.state,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    unsigned op=wifi_held.submitted++,phase=mt76_channel5_phase(&wifi_held.state),local=mt76_channel5_local(&wifi_held.state);
    struct xhci_device *d=wifi_held.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-held-command: op=%u phase=%u local=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)((packet[2]>>4)==12),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_held(struct xhci_device *d){
    struct mt76_channel5 *s=&wifi_held.state;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(s->attempted || wifi_held.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_held_parent(d);enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-held-progress: select band/frequency/A20/PA/BBP together;independent reads;reverse restoration;no VCO/RX/network OUT\n");
    if(parent){wifi_held.d=d;wifi_held.active=true;
        const struct mt76_mcu_timing_io rf={&wifi_held,wifi_held_owned,wifi_held_read,wifi_held_exchange};
        const struct mt76_band_mac_io mac={&wifi_held,wifi_held_owned,wifi_held_mac};
        const struct mt76_pa5_io pa={&wifi_held,wifi_held_owned,wifi_held_transfer,wifi_held_exchange};
        const struct mt76_bbp5_io bbp={&wifi_held,wifi_held_owned,wifi_held_transfer};
        status=mt76_channel5_capture(s,m->ant_fields.nic0,m->ant_fields.nic1,wifi_bbp5.gain.lna,&rf,&mac,&pa,&bbp);
    }
    wifi_held.active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_channel5_valid(s) && wifi_held.submitted==3732;
    console_printf("wifi-held: status=%u complete=%u valid=%u selected=%u restored=%u operations=%u submitted=%u lna=%u parent=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->selected,(uint64_t)(mt76_5ghz_band_valid(&s->band) && mt76_pa5_valid(&s->pa) && mt76_bbp5_valid(&s->bbp)),(uint64_t)mt76_channel5_operations(s),(uint64_t)wifi_held.submitted,(uint64_t)s->lna,(uint64_t)parent);
    return valid?(!wifi_trim5_requested || wifi_mcu_trim5(d)):wifi_mcu_fail(d);
}
/* Separate fresh-field and RF ownership; no old G-band trim-mode admission. */
static struct {struct xhci_device *d;struct mt76_trim5 state;struct mt76_trim_fields fields;struct mt76_efuse_map_gate gate;unsigned submitted;bool active,fields_active;} wifi_trim5;
static bool wifi_trim5_parent(struct xhci_device *d){
    return wifi_trim5_requested && d && d==wifi_held.d && !wifi_held.active && wifi_held_parent(d) &&
        mt76_channel5_valid(&wifi_held.state) && wifi_held.submitted==3732;
}
static bool wifi_trim5_fields_owned(void *context){
    struct mt76_trim_fields *f=&wifi_trim5.fields;struct mt76_efuse_map_gate *g=&wifi_trim5.gate;
    return context==&wifi_trim5 && wifi_trim5.active && wifi_trim5.fields_active && wifi_trim5_parent(wifi_trim5.d) &&
        f->attempted && !f->complete && !wifi_trim5.state.attempted && !g->failed && !g->pending && g->submitted==f->reads+f->writes;
}
static bool wifi_trim5_fields_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    struct mt76_efuse_map_gate *g=&wifi_trim5.gate;unsigned phase=g->phase,block=g->block,op=g->submitted;
    if(!v || !wifi_trim5_fields_owned(context) || !mt76_trim_fields_begin(g,write,address,*v))return false;
    bool ok=wifi_mac_private_usb(wifi_trim5.d,write,address,v);bool valid=mt76_trim_fields_finish(g,ok,*v);
    console_printf("wifi-trim5-field-io: op=%u block=%u phase=%u write=%u address=%x ok=%u raw_block_logged=0\n",(uint64_t)op,(uint64_t)block,(uint64_t)phase,(uint64_t)write,(uint64_t)address,(uint64_t)(ok && valid));
    return ok && valid?true:wifi_mcu_fail(wifi_trim5.d);
}
static bool wifi_trim5_fields_read(void *context,uint32_t address,uint32_t *v){if(!v)return false;*v=0;return wifi_trim5_fields_transfer(context,false,address,v);}
static bool wifi_trim5_fields_write(void *context,uint32_t address,uint32_t v){return wifi_trim5_fields_transfer(context,true,address,&v);}
static bool wifi_trim5_fields_wait(void *context,uint32_t us){
    struct mt76_efuse_map_gate *g=&wifi_trim5.gate;unsigned phase=g->phase,block=g->block;
    if(!wifi_trim5_fields_owned(context) || !mt76_efuse_map_delay(g,us))return false;
    bool ok=timer_delay_us(us);if(!ok){g->failed=true;wifi_mcu_fail(wifi_trim5.d);}
    console_printf("wifi-trim5-field-delay: block=%u phase=%u us=%u ok=%u\n",(uint64_t)block,(uint64_t)phase,(uint64_t)us,(uint64_t)ok);return ok;
}
static bool wifi_trim5_owned(void *context){
    struct mt76_trim_fields *f=&wifi_trim5.fields;struct mt76_efuse_map_gate *g=&wifi_trim5.gate;struct mt76_trim5 *s=&wifi_trim5.state;
    return context==&wifi_trim5 && wifi_trim5.active && !wifi_trim5.fields_active && wifi_trim5_parent(wifi_trim5.d) &&
        mt76_trim_fields_valid(f) && g->phase==15 && g->block==2 && !g->failed && !g->pending &&
        g->polls==f->polls && g->submitted==f->reads+f->writes && s->trim==f->trim && s->lna==wifi_bbp5.gain.lna && s->with_channel &&
        mt76_trim5_owned(s) && wifi_trim5.submitted==mt76_trim5_operations(s);
}
static bool wifi_trim5_transfer(void *context,bool write,uint32_t address,uint32_t *v){
    if(!v || !wifi_trim5_owned(context) || !mt76_trim5_transfer_valid(&wifi_trim5.state,write,address,*v))return false;
    unsigned op=wifi_trim5.submitted++,phase=mt76_trim5_phase(&wifi_trim5.state),local=mt76_trim5_local(&wifi_trim5.state);
    bool ok=wifi_mac_private_usb(wifi_trim5.d,write,address,v);
    console_printf("wifi-trim5-usb: op=%u phase=%u local=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)write,(uint64_t)address,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(wifi_trim5.d);
}
static bool wifi_trim5_read(void *context,uint32_t address,uint32_t *v){if(!v)return false;*v=0;return wifi_trim5_transfer(context,false,address,v);}
static bool wifi_trim5_mac(void *context,unsigned op,bool write,uint32_t address,uint8_t *data,unsigned length){
    if(!data || length!=4 || !wifi_trim5_owned(context) || wifi_trim5.state.phase!=2 || wifi_trim5.state.channel.phase!=1 || op!=mt76_trim5_local(&wifi_trim5.state))return false;
    uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)data[i]<<(8*i);
    if(!wifi_trim5_transfer(context,write,address,&v))return false;
    for(unsigned i=0;i<4;i++)data[i]=(uint8_t)(v>>(8*i));
    return true;
}
static int wifi_trim5_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    uint8_t allowed[16];
    if(!wifi_trim5_owned(context) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_trim5_packet(&wifi_trim5.state,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    unsigned op=wifi_trim5.submitted++,phase=mt76_trim5_phase(&wifi_trim5.state),local=mt76_trim5_local(&wifi_trim5.state);
    struct xhci_device *d=wifi_trim5.d;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t w[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-trim5-command: op=%u phase=%u local=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)phase,(uint64_t)local,(uint64_t)((packet[2]>>4)==12),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_trim5(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_trim5 *s=&wifi_trim5.state;struct mt76_trim_fields *f=&wifi_trim5.fields;struct mt76_efuse_map_gate *g=&wifi_trim5.gate;
    if(s->attempted || f->attempted || wifi_trim5.active)return false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=true;
    bool parent=wifi_trim5_parent(d);enum mt76_capture_status fs=MT76_CAPTURE_REFUSED;enum mt76_mcu_timing_status status=MT76_TIMING_REFUSED;
    console_write("wifi-trim5-progress: fresh frequency fields;select trim around band/frequency/A20/PA/BBP;independent reads;reverse originals;no DAC/VCO/RX/network OUT\n");
    if(parent){wifi_trim5.d=d;wifi_trim5.active=wifi_trim5.fields_active=true;const struct mt76_efuse_map_io io={{&wifi_trim5,wifi_trim5_fields_read,wifi_trim5_fields_write,wifi_trim5_fields_wait},wifi_trim5_fields_owned};fs=mt76_capture_trim_fields(&io,&d->wifi_identity,f);wifi_trim5.fields_active=false;}
    console_printf("wifi-trim5-fields: status=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u raw_block_logged=0\n",(uint64_t)fs,(uint64_t)mt76_trim_fields_valid(f),(uint64_t)f->reads,(uint64_t)f->writes,(uint64_t)f->polls,(uint64_t)f->blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed);
    console_printf("wifi-trim5-value: trim=%u lna=%u channel=161 bandwidth=20 raw_fields_logged=0\n",(uint64_t)f->trim,(uint64_t)wifi_bbp5.gain.lna);
    for(unsigned i=0;i<4;i++)console_printf("wifi-trim5-field-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)g->mac[i],(uint64_t)g->core[i]);
    for(unsigned i=0;i<2;i++)console_printf("wifi-trim5-field-dma: index=%u value=%x\n",(uint64_t)i,(uint64_t)g->dma[i]);
    if(fs==MT76_CAPTURE_OK && mt76_trim_fields_valid(f)){const struct mt76_mcu_timing_io rf={&wifi_trim5,wifi_trim5_owned,wifi_trim5_read,wifi_trim5_exchange};const struct mt76_band_mac_io mac={&wifi_trim5,wifi_trim5_owned,wifi_trim5_mac};const struct mt76_pa5_io pa={&wifi_trim5,wifi_trim5_owned,wifi_trim5_transfer,wifi_trim5_exchange};const struct mt76_bbp5_io bbp={&wifi_trim5,wifi_trim5_owned,wifi_trim5_transfer};status=mt76_trim5_channel_capture(s,f->nic0,f->nic1,f->trim,wifi_bbp5.gain.lna,&rf,&mac,&pa,&bbp);}
    wifi_trim5.active=wifi_trim5.fields_active=false;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=false;
    bool valid=status==MT76_TIMING_OK && mt76_trim5_valid(s) && s->with_channel && wifi_trim5.submitted==3767;
    console_printf("wifi-trim5: status=%u complete=%u valid=%u restored=%u operations=%u submitted=%u trim=%u lna=%u parent=%u network_out=0\n",(uint64_t)status,(uint64_t)s->complete,(uint64_t)valid,(uint64_t)s->restored,(uint64_t)mt76_trim5_operations(s),(uint64_t)wifi_trim5.submitted,(uint64_t)s->trim,(uint64_t)s->lna,(uint64_t)parent);
    return valid?(!wifi_dac5_requested || wifi_mcu_dac5(d)):wifi_mcu_fail(d);
}
static bool wifi_rf_g20_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_g20_mode && m->rf_g20_active && wifi_rf_vga_owned(d) &&
        m->rf_vga_submitted==MT76_RF_VGA_OPS && mt76_rf_vga_exact_profile_valid(&m->rf_vga);
}
static bool wifi_rf_g20_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_g20_submitted;
    if(!value || !wifi_rf_g20_owned(d) || !mt76_rf_g20_admit(&m->rf_g20,op) || address==UINT32_MAX || address!=mt76_rf_g20_guard_address(op))return false;
    m->rf_g20_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_g20_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_g20_submitted;uint8_t allowed[16];
    if(!wifi_rf_g20_owned(d) || !mt76_rf_g20_admit(&m->rf_g20,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_g20_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_g20_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-g20-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_g20_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_g20(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_g20;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_g20_owned,wifi_rf_g20_read,wifi_rf_g20_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_g20_capture(s,&io);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=false;
    console_printf("wifi-rf-g20: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_g20_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_G20_COMMANDS*2;i++)console_printf("wifi-rf-g20-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_G20_COUNT;i++)console_printf("wifi-rf-g20-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_g20_address(i),(uint64_t)mt76_rf_g20_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_trim_ready(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_trim_mode && wifi_rf_g20_owned(d) && m->rf_g20_submitted==MT76_RF_G20_OPS && mt76_rf_g20_exact_profile_valid(&m->rf_g20);
}
static bool wifi_trim_fields_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->trim_fields_active && !m->rf_trim_active && !m->trim_fields.complete && !m->trim_fields_gate.failed && wifi_rf_trim_ready(d);
}
static bool wifi_trim_fields_transfer(struct xhci_device *d,bool write,uint32_t a,uint32_t *v){
    struct mt76_efuse_map_gate *g=&d->firmware.mcu.trim_fields_gate;
    if(!v || !d->firmware.mcu.trim_fields.attempted || !wifi_trim_fields_owned(d) || !mt76_trim_fields_begin(g,write,a,*v))return false;
    bool ok=wifi_mac_private_usb(d,write,a,v);
    return mt76_trim_fields_finish(g,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_trim_fields_read(void *context,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_trim_fields_transfer(context,false,a,v);}
static bool wifi_trim_fields_write(void *context,uint32_t a,uint32_t v){return wifi_trim_fields_transfer(context,true,a,&v);}
static bool wifi_trim_fields_wait(void *context,uint32_t us){
    struct xhci_device *d=context;struct mt76_efuse_map_gate *g=&d->firmware.mcu.trim_fields_gate;
    if(!wifi_trim_fields_owned(d) || !mt76_efuse_map_delay(g,us))return false;
    if(timer_delay_us(us))return true;
    g->failed=true;return wifi_mcu_fail(d);
}
static bool wifi_rf_trim_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct mt76_efuse_map_gate *g=&m->trim_fields_gate;struct mt76_trim_fields *f=&m->trim_fields;
    if(!m->rf_trim_active || m->trim_fields_active || !wifi_rf_trim_ready(d) || !f->valid || !mt76_trim_fields_valid(f) ||
       g->phase!=15 || g->block!=2 || g->pending || g->failed || g->polls!=f->polls || g->submitted!=f->reads+f->writes)return false;
    for(unsigned i=0;i<4;i++)if(g->mac[i] || g->core[i]!=0xf000f200)return false;
    for(unsigned i=0;i<2;i++)if(g->dma[i]&15)return false;
    return true;
}
static bool wifi_rf_trim_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_trim_submitted;
    if(!value || !wifi_rf_trim_owned(d) || !mt76_rf_trim_admit(&m->rf_trim,op) || address==UINT32_MAX || address!=mt76_rf_trim_guard_address(op))return false;
    m->rf_trim_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_trim_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_trim_submitted;uint8_t allowed[16];
    if(!wifi_rf_trim_owned(d) || !mt76_rf_trim_admit(&m->rf_trim,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_trim_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_trim_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-trim-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_trim_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_trim(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->rf_trim;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->trim_fields_active=true;
    const struct mt76_efuse_map_io fields_io={{d,wifi_trim_fields_read,wifi_trim_fields_write,wifi_trim_fields_wait},wifi_trim_fields_owned};
    enum mt76_capture_status fields_status=mt76_capture_trim_fields(&fields_io,&d->wifi_identity,&m->trim_fields);
    m->trim_fields_active=false;m->rf_trim_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_trim_owned,wifi_rf_trim_read,wifi_rf_trim_exchange};
    enum mt76_mcu_timing_status status=fields_status==MT76_CAPTURE_OK && wifi_rf_trim_owned(d)?mt76_rf_trim_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->trim_fields_active=m->rf_trim_active=false;
    struct mt76_trim_fields *f=&m->trim_fields;struct mt76_efuse_map_gate *g=&m->trim_fields_gate;unsigned guard_errors=0;
    for(unsigned i=0;i<4;i++)if(g->mac[i] || g->core[i]!=0xf000f200)guard_errors++;
    for(unsigned i=0;i<2;i++)if(g->dma[i]&15)guard_errors++;
    console_printf("wifi-trim-fields: status=%u attempted=%u complete=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u guard_errors=%u raw_logged=0\n",(uint64_t)fields_status,(uint64_t)f->attempted,(uint64_t)f->complete,(uint64_t)f->valid,(uint64_t)f->reads,(uint64_t)f->writes,(uint64_t)f->polls,(uint64_t)f->blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed,(uint64_t)guard_errors);
    console_printf("wifi-rf-trim: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_trim_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_TRIM_COMMANDS*2;i++)console_printf("wifi-rf-trim-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_TRIM_COUNT;i++)console_printf("wifi-rf-trim-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_trim_address(i),(uint64_t)mt76_rf_trim_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_cal_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_cal_mode && m->rf_cal_active && wifi_rf_trim_owned(d) &&
        m->rf_trim_submitted==MT76_RF_TRIM_OPS && mt76_rf_trim_exact_profile_valid(&m->rf_trim);
}
static bool wifi_rf_cal_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_cal_submitted;
    if(!value || !wifi_rf_cal_owned(d) || !mt76_rf_cal_admit(&m->rf_cal,op) || address==UINT32_MAX || address!=mt76_rf_cal_guard_address(op))return false;
    m->rf_cal_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_cal_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_cal_submitted;uint8_t allowed[16];
    if(!wifi_rf_cal_owned(d) || !mt76_rf_cal_admit(&m->rf_cal,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_cal_packet(&m->rf_cal,op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_cal_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-cal-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_cal_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_cal(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_cal *s=&m->rf_cal;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_cal_owned,wifi_rf_cal_read,wifi_rf_cal_exchange};
    enum mt76_mcu_timing_status status=wifi_rf_cal_owned(d)?mt76_rf_cal_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=false;
    console_printf("wifi-rf-cal: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u dac_final=%u vco_final=%u vco_clear_observed=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_cal_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->words[8][2],(uint64_t)s->words[9][2],(uint64_t)(s->complete && !(s->words[9][2]&0x80)));
    for(unsigned i=0;i<20;i++)console_printf("wifi-rf-cal-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<10;i++)console_printf("wifi-rf-cal-value: index=%u address=%x write=%u value=%u\n",(uint64_t)i,(uint64_t)mt76_rf_cal_address(i),(uint64_t)mt76_rf_cal_write(i),(uint64_t)s->words[i][2]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_wait_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_wait_mode && m->rf_wait_active && wifi_rf_cal_owned(d) &&
        m->rf_cal_submitted==MT76_RF_CAL_OPS && (mt76_rf_cal_pending_profile_valid(&m->rf_cal) ||
        (m->vco_ready_mode && m->channel_observe_mode && m->channel_state_mode && mt76_rf_cal_cleared_profile_valid(&m->rf_cal)));
}
static bool wifi_rf_wait_delay(void *context,uint32_t us){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_wait *s=&m->rf_wait;
    if(!wifi_rf_wait_owned(d) || m->rf_wait_submitted!=s->operations || m->rf_wait_delays!=s->waits ||
       !mt76_rf_wait_delay_admit(s) || !us || us!=mt76_rf_wait_delay(s->waits))return false;
    m->rf_wait_delays++;
    bool ok=timer_delay_us(us);
    console_printf("wifi-rf-wait-delay: index=%u requested_us=%u ok=%u\n",(uint64_t)s->waits,(uint64_t)us,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_rf_wait_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_wait_submitted;
    if(!value || !wifi_rf_wait_owned(d) || !mt76_rf_wait_admit(&m->rf_wait,op) || m->rf_wait_delays!=m->rf_wait.waits || address==UINT32_MAX || address!=mt76_rf_wait_guard_address(op))return false;
    m->rf_wait_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_wait_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_wait_submitted;uint8_t allowed[16];
    if(!wifi_rf_wait_owned(d) || !mt76_rf_wait_admit(&m->rf_wait,op) || m->rf_wait_delays!=m->rf_wait.waits || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_wait_packet(&m->rf_wait,op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_wait_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-rf-wait-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_wait_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_rf_wait(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_wait *s=&m->rf_wait;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=true;
    if(m->vco_ready_mode)console_printf("wifi-vco-prerequisite: pending=%u early_clear=%u observation_only=1\n",(uint64_t)mt76_rf_cal_pending_profile_valid(&m->rf_cal),(uint64_t)mt76_rf_cal_cleared_profile_valid(&m->rf_cal));
    const struct mt76_rf_wait_io io={{d,wifi_rf_wait_owned,wifi_rf_wait_read,wifi_rf_wait_exchange},wifi_rf_wait_delay};
    enum mt76_mcu_timing_status status=wifi_rf_wait_owned(d)?mt76_rf_wait_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=false;
    console_printf("wifi-rf-wait: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u waits=%u delays_submitted=%u requested_us=%u first_clear=%u final=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_wait_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->waits,(uint64_t)m->rf_wait_delays,(uint64_t)s->delay_us,(uint64_t)s->first_clear,(uint64_t)s->words[5][2]);
    for(unsigned i=0;i<12;i++)console_printf("wifi-rf-wait-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    unsigned cumulative=0;
    for(unsigned i=0;i<6;i++){cumulative+=mt76_rf_wait_delay(i);console_printf("wifi-rf-wait-value: index=%u cumulative_requested_us=%u address=%x value=%u\n",(uint64_t)i,(uint64_t)cumulative,(uint64_t)0x80000004,(uint64_t)s->words[i][2]);}
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_path_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->path_mode && m->path_active && wifi_rf_wait_owned(d) && m->rf_wait_submitted==42 &&
        m->rf_wait_delays==6 && mt76_rf_wait_clear_profile_valid(&m->rf_wait);
}
static bool wifi_path_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_path_owned(d) || m->path_submitted!=op || !mt76_path_transfer_valid(&m->path,op,write,a,data,length))return false;
    m->path_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-path-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_path(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_path *s=&m->path;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=true;
    const struct mt76_path_io io={d,wifi_path_owned,wifi_path_transfer};
    enum mt76_mcu_timing_status status=wifi_path_owned(d)?mt76_path_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=false;
    console_printf("wifi-path: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->path_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<4;i++)console_printf("wifi-path-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<2;i++)console_printf("wifi-path-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_path_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_channel_state_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->channel_state_mode && m->channel_state_active && wifi_path_owned(d) && m->path_submitted==18 && mt76_path_exact_profile_valid(&m->path);
}
static bool wifi_channel_state_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->channel_state_submitted;
    if(!v || !wifi_channel_state_owned(d) || m->channel_state.reads!=op || !(m->channel_observe_mode?mt76_channel_observe_admit(&m->channel_state):mt76_channel_state_admit(&m->channel_state)) || a==UINT32_MAX || a!=mt76_channel_state_request(op))return false;
    m->channel_state_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("%s-usb: op=%u address=%x value=%x ok=%u\n",m->channel_observe_mode?"wifi-channel-observation":"wifi-channel-state",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_channel_state(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_channel_state *s=&m->channel_state;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=true;
    const struct mt76_mcu_timing_io io={.context=d,.authorize=wifi_channel_state_owned,.read=wifi_channel_state_read};
    enum mt76_mcu_timing_status status=wifi_channel_state_owned(d)?(m->channel_observe_mode?mt76_channel_observe_capture(s,&io):mt76_channel_state_capture(s,&io)):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=false;
    if(m->channel_observe_mode){
        console_printf("wifi-channel-observation: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0 observation_only=1 all_ones_mask=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->channel_state_submitted,(uint64_t)mt76_channel_observe_all_ones(s));
    }else{
        console_printf("wifi-channel-state: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0 observation_only=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->channel_state_submitted);
    }
    const char *prefix=m->channel_observe_mode?"wifi-channel-observation":"wifi-channel-state";
    for(unsigned i=0;i<2;i++)console_printf("%s-guard: index=%u mac=%x core=%x dma=%x\n",prefix,(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<12;i++)console_printf("%s-value: index=%u address=%x value=%x\n",prefix,(uint64_t)i,(uint64_t)mt76_channel_state_address(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_rf_central_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_probe *p=&m->rf_probe;
    if(!m->rf_central_mode || !m->rf_central_active || !wifi_rf_probe_owned(d) ||
       !p->attempted || !p->request_attempted || !p->reply_valid || !p->complete || p->reads!=6 ||
       m->rf_probe_reads!=6 || !m->rf_probe_submitted || p->received!=16 || p->value!=0 ||
       p->words[0]!=0x4a080008 || p->words[1]!=0x80000016 || p->words[2] || p->words[3]!=0x4a080008)return false;
    for(unsigned i=0;i<2;i++)if(p->mac[i] || p->core[i]!=0xf000f200 || p->dma[i]==UINT32_MAX || (p->dma[i]&15))return false;
    return true;
}
static bool wifi_rf_central_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_central_submitted;
    if(!value || !wifi_rf_central_owned(d) || (m->rf_six_mode && op>=45) || !mt76_rf_central_admit(&m->rf_central,op) ||
       address==UINT32_MAX || address!=mt76_rf_central_guard_address(op))return false;
    m->rf_central_submitted++;
    *value=0;return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_central_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_central_submitted;uint8_t allowed[16];
    if(!wifi_rf_central_owned(d) || (m->rf_six_mode && op>=45) || !mt76_rf_central_admit(&m->rf_central,op) || !packet || !reply ||
       capacity<MT76_MCU_RESPONSE_MAX || length!=16 || mt76_rf_central_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_central_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    /* Preserve each transport receipt; no raw calibration, keys or traffic. */
    console_printf("wifi-rf-central-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",(uint64_t)op,(uint64_t)mt76_rf_central_sequence(op/8,op%8==3),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_rf_six_prefix_authorize(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return wifi_rf_central_owned(d) && (!m->rf_six_mode || m->rf_central_submitted<45);
}
static bool wifi_rf_six_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_six_mode && m->rf_six_active && wifi_rf_central_owned(d) && m->rf_central_submitted==45 &&
        mt76_rf_six_prefix_valid(&m->rf_central,m->rf_central_status);
}
static bool wifi_rf_six_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_six_submitted;
    if(!value || !wifi_rf_six_owned(d) || !mt76_rf_six_admit(&m->rf_six,op) || address==UINT32_MAX || address!=mt76_rf_six_guard_address(op))return false;
    m->rf_six_submitted++;*value=0;return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static int wifi_rf_six_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->rf_six_submitted;uint8_t allowed[16];
    if(!wifi_rf_six_owned(d) || !mt76_rf_six_admit(&m->rf_six,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_six_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->rf_six_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    console_printf("wifi-rf-six-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u\n",(uint64_t)op,(uint64_t)(6+(op-3)/4),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_rf_csr_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_csr_mode && m->rf_csr_active && wifi_rf_six_owned(d) &&
        m->rf_six_submitted==23 && mt76_rf_six_complete_valid(&m->rf_six);
}
static bool wifi_rf_csr_transfer(void *context,unsigned op,bool write,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    bool expected_write;uint32_t expected_address,expected_value;
    if(!value || !wifi_rf_csr_owned(d) || op!=m->rf_csr_submitted || op!=m->rf_csr.operations ||
       !mt76_rf_csr_request(&m->rf_csr,&expected_write,&expected_address,&expected_value) ||
       write!=expected_write || address!=expected_address || (write && *value!=expected_value))return false;
    m->rf_csr_submitted++;
    bool ok=wifi_mac_private_usb(d,write,address,value);
    /* Only selectors and fixed public guard/RF observations; no private fields. */
    console_printf("wifi-rf-csr-usb: op=%u write=%u address=%x value=%x ok=%u actual=%u cc=%u residue=%u timeout=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)address,(uint64_t)*value,(uint64_t)ok,(uint64_t)d->firmware.control_actual,(uint64_t)d->firmware.control_cc,(uint64_t)d->firmware.control_residue,(uint64_t)d->firmware.control_timeout);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_rf_write_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rf_write_mode && m->rf_write_active && wifi_rf_csr_owned(d) &&
        m->rf_csr_submitted==m->rf_csr.operations && mt76_rf_csr_zero_valid(&m->rf_csr) &&
        m->rf_central_status==MT76_TIMING_STATE && !m->rf_central.words[2] &&
        !m->rf_six.values[0] && !m->rf_six.values[3];
}
static bool wifi_rf_write_transfer(void *context,unsigned op,bool write,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    bool expected_write;uint32_t expected_address,expected_value;
    if(!value || !wifi_rf_write_owned(d) || op!=m->rf_write_submitted || op!=m->rf_write.operations ||
       !mt76_rf_write_request(&m->rf_write,&expected_write,&expected_address,&expected_value) ||
       write!=expected_write || address!=expected_address || (write && *value!=expected_value))return false;
    m->rf_write_submitted++;
    bool ok=wifi_mac_private_usb(d,write,address,value);
    console_printf("wifi-rf-write-usb: op=%u write=%u address=%x value=%x ok=%u actual=%u cc=%u residue=%u timeout=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)address,(uint64_t)*value,(uint64_t)ok,(uint64_t)d->firmware.control_actual,(uint64_t)d->firmware.control_cc,(uint64_t)d->firmware.control_residue,(uint64_t)d->firmware.control_timeout);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_rf_state_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_write *s=&m->rf_write;
    if(!m->rf_state_mode || !m->rf_state_active || !wifi_rf_write_owned(d) || !s->complete ||
       s->phase!=14 || s->operations!=m->rf_write_submitted || s->writes!=2 || s->reads+2!=s->operations || s->words[2]!=0x600)return false;
    /* Reconstruct the final authorized read to validate every prior guard/poll. */
    struct mt76_rf_write before=*s;before.complete=false;before.phase=13;before.operations--;before.reads--;
    bool w;uint32_t a,v;
    return s->guards[2][2]!=UINT32_MAX && !(s->guards[2][2]&15) && mt76_rf_write_request(&before,&w,&a,&v);
}
static bool wifi_rf_state_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!value || !wifi_rf_state_owned(d) || !mt76_rf_state_admit(&m->rf_state) ||
       m->rf_state_submitted!=m->rf_state.reads || address!=mt76_rf_state_request(m->rf_state.reads))return false;
    m->rf_state_submitted++;*value=0;
    return wifi_mac_private_usb(d,false,address,value)?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_rf_state(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_state *s=&m->rf_state;
    m->rf_six_active=m->rf_csr_active=m->rf_write_active=m->rf_state_active=true;
    const struct mt76_mcu_timing_io io={.context=d,.authorize=wifi_rf_state_owned,.read=wifi_rf_state_read};
    enum mt76_mcu_timing_status status=mt76_rf_state_capture(s,&io);
    m->rf_six_active=m->rf_csr_active=m->rf_write_active=m->rf_state_active=false;
    console_printf("wifi-rf-state: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0 observation_only=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->rf_state_submitted);
    for(unsigned i=0;i<2;i++)console_printf("wifi-rf-state-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_STATE_COUNT;i++)console_printf("wifi-rf-state-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_rf_state_address(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK;
}
static bool wifi_mcu_rf_write(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_write *s=&m->rf_write;
    m->rf_six_active=m->rf_csr_active=m->rf_write_active=true;
    const struct mt76_rf_csr_io io={d,wifi_rf_write_owned,wifi_rf_write_transfer};
    enum mt76_mcu_timing_status status=mt76_rf_write_apply(s,&io);
    m->rf_six_active=m->rf_csr_active=m->rf_write_active=false;
    console_printf("wifi-rf-write: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u selectors=%u phase=%u before=0 after=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_write_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->phase,(uint64_t)(s->words[2]&255));
    for(unsigned i=0;i<3;i++)console_printf("wifi-rf-write-poll: index=%u count=%u word=%x\n",(uint64_t)i,(uint64_t)s->polls[i],(uint64_t)s->words[i]);
    for(unsigned i=0;i<3;i++)console_printf("wifi-rf-write-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    return status==MT76_TIMING_OK;
}
static bool wifi_mcu_rf_csr(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_csr *s=&m->rf_csr;
    m->rf_six_active=m->rf_csr_active=true;
    const struct mt76_rf_csr_io io={d,wifi_rf_csr_owned,wifi_rf_csr_transfer};
    enum mt76_mcu_timing_status status=mt76_rf_csr_apply(s,&io);
    m->rf_six_active=m->rf_csr_active=false;
    console_printf("wifi-rf-csr: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u selectors=%u targets=%u phase=%u rf_writes=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_csr_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->index,(uint64_t)s->phase);
    for(unsigned i=0;i<5;i++)console_printf("wifi-rf-csr-value: index=%u selector=%x idle_polls=%u complete_polls=%u idle=%x result=%x\n",(uint64_t)i,(uint64_t)mt76_rf_csr_selector(i),(uint64_t)s->polls[i][0],(uint64_t)s->polls[i][1],(uint64_t)s->idle[i],(uint64_t)s->result[i]);
    for(unsigned i=0;i<10;i++)console_printf("wifi-rf-csr-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    return status==MT76_TIMING_OK;
}
static bool wifi_mcu_rf_six(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_six *s=&m->rf_six;
    m->rf_six_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_six_owned,wifi_rf_six_read,wifi_rf_six_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_six_apply(s,&io);m->rf_six_active=false;
    console_printf("wifi-rf-six: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u requests=%u central_complete=%u central_status=%u first_value=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_six_submitted,(uint64_t)s->reads,(uint64_t)s->requests,(uint64_t)m->rf_central.complete,(uint64_t)m->rf_central_status,(uint64_t)m->rf_central.words[2]);
    for(unsigned i=0;i<6;i++)console_printf("wifi-rf-six-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<5;i++)console_printf("wifi-rf-six-reply: index=%u header=%x address=%x value=%x trailer=%x\n",(uint64_t)i,(uint64_t)s->words[i][0],(uint64_t)s->words[i][1],(uint64_t)s->words[i][2],(uint64_t)s->words[i][3]);
    return status==MT76_TIMING_OK;
}
static bool wifi_mcu_rf_central(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_central *s=&m->rf_central;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_central_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_rf_six_prefix_authorize,wifi_rf_central_read,wifi_rf_central_exchange};
    enum mt76_mcu_timing_status status=mt76_rf_central_apply(s,&io);m->rf_central_status=status;
    unsigned errors=0;
    for(unsigned i=0;i<s->reads && i<264;i++){
        uint32_t v=s->guards[i/3][i%3];
        if(i%3==0?v!=0:i%3==1?v!=0xf000f200:v==UINT32_MAX || (v&15))errors++;
    }
    console_printf("wifi-rf-central-guards: reads_checked=%u boundaries_complete=%u boundaries_unexecuted=%u\n",(uint64_t)s->reads,(uint64_t)(s->reads/3),(uint64_t)(88-s->reads/3));
    console_printf("wifi-rf-central: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u exchanges=%u acks=%u verified=%u guard_errors=%u received=%d\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->rf_central_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->verified,(uint64_t)errors,(int64_t)s->received);
    for(unsigned i=0;i<(m->rf_six_mode?6:MT76_RF_CENTRAL_COUNT);i++){
        uint32_t observed=s->values[i];
        if(m->rf_six_mode && i==5 && s->received==16 && s->words[1]==0x80000006)observed=s->words[2];
        console_printf("wifi-rf-central-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_central_address(i),(uint64_t)mt76_rf_central_value(i),(uint64_t)observed);
    }
    for(unsigned i=0;i<4;i++)console_printf("wifi-rf-central-word: index=%u value=%x\n",(uint64_t)i,(uint64_t)s->words[i]);
    if(status!=MT76_TIMING_OK){
        unsigned i=s->operations/8;if(i>=MT76_RF_CENTRAL_COUNT)i=MT76_RF_CENTRAL_COUNT-1;
        for(unsigned b=0;b<2;b++)console_printf("wifi-rf-central-failure-guard: index=%u boundary=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)b,(uint64_t)s->guards[i*2+b][0],(uint64_t)s->guards[i*2+b][1],(uint64_t)s->guards[i*2+b][2]);
    }
    bool ok=m->rf_six_mode?wifi_mcu_rf_six(d):status==MT76_TIMING_OK;
    if(ok && m->rf_csr_mode)ok=wifi_mcu_rf_csr(d);
    if(ok && m->rf_write_mode)ok=wifi_mcu_rf_write(d);
    if(ok && m->rf_state_mode)ok=wifi_mcu_rf_state(d);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_central_active=false;
    return ok?true:wifi_mcu_fail(d);
}

static bool wifi_usb_cycle_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->usb_cycle_mode && m->usb_cycle_active && m->vco_ready_mode && m->channel_observe_mode &&
        wifi_channel_state_owned(d) && m->channel_state_submitted==18 && mt76_channel_observe_exact_profile_valid(&m->channel_state);
}
static bool wifi_usb_cycle_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_usb_cycle_owned(d) || m->usb_cycle_submitted!=op || !mt76_usb_cycle_transfer_valid(&m->usb_cycle,op,write,a,data,length))return false;
    if(op>=4 && m->usb_cycle.before[0]!=0)return false;
    m->usb_cycle_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-usb-cycle-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_usb_cycle(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_usb_cycle *s=&m->usb_cycle;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=true;
    const struct mt76_usb_cycle_io io={d,wifi_usb_cycle_owned,wifi_usb_cycle_transfer};
    enum mt76_mcu_timing_status status=wifi_usb_cycle_owned(d)?mt76_usb_cycle_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=false;
    console_printf("wifi-usb-cycle: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->usb_cycle_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<2;i++)console_printf("wifi-usb-cycle-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<1;i++)console_printf("wifi-usb-cycle-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_usb_cycle_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_bw_observe_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->bw_observe_mode && m->bw_observe_active && wifi_usb_cycle_owned(d) && m->usb_cycle_submitted==9 && mt76_usb_cycle_exact_profile_valid(&m->usb_cycle);
}
static bool wifi_bw_observe_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->bw_observe_submitted;
    if(!v || !wifi_bw_observe_owned(d) || !mt76_bw_observe_admit(&m->bw_observe,op) || a==UINT32_MAX || a!=mt76_bw_observe_guard_address(op))return false;
    m->bw_observe_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-bw-observe-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static int wifi_bw_observe_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;uint8_t allowed[16];
    if(!wifi_bw_observe_owned(d) || m->bw_observe_submitted!=3 || !mt76_bw_observe_admit(&m->bw_observe,3) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_bw_observe_packet(allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->bw_observe_submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};for(unsigned j=0;j<4;j++)if(received>=(int)(4*j+4))for(unsigned b=0;b<4;b++)words[j]|=(uint32_t)m->buffer[4*j+b]<<(8*b);
    console_printf("wifi-bw-observe-command: sequence=15 sent=%d received=%d cc=%u residue=%u h=%x p0=%x p1=%x p2=%x\n",(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_bw_observe(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bw_observe *s=&m->bw_observe;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_bw_observe_owned,wifi_bw_observe_read,wifi_bw_observe_exchange};
    enum mt76_mcu_timing_status status=wifi_bw_observe_owned(d)?mt76_bw_observe_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=false;
    console_printf("wifi-bw-observe: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u exchanges=%u received=%d observation_only=1 ack_validated=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->bw_observe_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(int64_t)s->received);
    for(unsigned i=0;i<2;i++)console_printf("wifi-bw-observe-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<4;i++)console_printf("wifi-bw-observe-word: index=%u value=%x\n",(uint64_t)i,(uint64_t)s->words[i]);
    if(m->bbp20_mode)console_printf("wifi-bw-validated: exact_profile=%u\n",(uint64_t)mt76_bw_observe_exact_profile_valid(s));
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_bbp20_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->bbp20_mode && m->bbp20_active && wifi_bw_observe_owned(d) && m->bw_observe_submitted==7 && mt76_bw_observe_exact_profile_valid(&m->bw_observe);
}
static bool wifi_bbp20_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_bbp20_owned(d) || m->bbp20_submitted!=op || !mt76_bbp20_transfer_valid(&m->bbp20,op,write,a,data,length))return false;
    if(op%9>=4 && m->bbp20.before[op/9]!=mt76_bbp20_initial(op/9))return false;
    m->bbp20_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-bbp20-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_bbp20(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_bbp20 *s=&m->bbp20;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=true;
    const struct mt76_bbp20_io io={d,wifi_bbp20_owned,wifi_bbp20_transfer};
    enum mt76_mcu_timing_status status=wifi_bbp20_owned(d)?mt76_bbp20_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=false;
    console_printf("wifi-bbp20: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->bbp20_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<10;i++)console_printf("wifi-bbp20-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<5;i++)console_printf("wifi-bbp20-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_bbp20_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_channel_probe_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->channel_probe_mode && m->channel_probe_active && wifi_bbp20_owned(d) && m->bbp20_submitted==45 && mt76_bbp20_exact_profile_valid(&m->bbp20);
}
static bool wifi_channel_probe_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->channel_probe_submitted;bool command;uint32_t allowed;
    if(!v || !wifi_channel_probe_owned(d) || !mt76_channel_probe_admit(&m->channel_probe,op) || !mt76_channel_probe_request(op,&command,&allowed) || command || a!=allowed)return false;
    m->channel_probe_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-channel-probe-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static int wifi_channel_probe_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->channel_probe_submitted;uint8_t allowed[16];
    if(!wifi_channel_probe_owned(d) || !packet || !reply || length!=16 || capacity<MT76_MCU_RESPONSE_MAX || mt76_channel_probe_packet(&m->channel_probe,op,allowed,16)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->channel_probe_submitted++;for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};for(unsigned j=0;j<4;j++)if(received>=(int)(4*j+4))for(unsigned b=0;b<4;b++)words[j]|=(uint32_t)m->buffer[4*j+b]<<(8*b);
    console_printf("wifi-channel-probe-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)((op-11)/7+1),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_channel_probe(struct xhci_device *d){
    struct xhci_device *m_d=d;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_channel_probe *s=&m->channel_probe;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=true;
    const struct mt76_mcu_timing_io io={m_d,wifi_channel_probe_owned,wifi_channel_probe_read,wifi_channel_probe_exchange};
    enum mt76_mcu_timing_status status=wifi_channel_probe_owned(d)?mt76_channel_probe_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=false;
    console_printf("wifi-channel-probe: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u exchanges=%u writes=0 observation_only=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->channel_probe_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges);
    for(unsigned i=0;i<32;i++)console_printf("wifi-channel-probe-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<5;i++)console_printf("wifi-channel-probe-mac: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_channel_probe_mac_address(i),(uint64_t)s->mac[i]);
    for(unsigned i=0;i<15;i++)console_printf("wifi-channel-probe-rf: index=%u address=%x value=%u\n",(uint64_t)i,(uint64_t)mt76_channel_probe_rf_address(i),(uint64_t)s->words[i][2]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_band2g_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->band2g_mode && m->band2g_active && wifi_channel_probe_owned(d) && m->channel_probe_submitted==116 && mt76_channel_probe_exact_profile_valid(&m->channel_probe);
}
static bool wifi_band2g_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->band2g_submitted;
    if(!value || !wifi_band2g_owned(d) || !mt76_rf_band2g_admit(&m->band2g,op) || address==UINT32_MAX || address!=mt76_rf_band2g_guard_address(op))return false;
    m->band2g_submitted++;*value=0;
    bool ok=wifi_mac_private_usb(d,false,address,value);
    console_printf("wifi-band2g-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static int wifi_band2g_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->band2g_submitted;uint8_t allowed[16];
    if(!wifi_band2g_owned(d) || !mt76_rf_band2g_admit(&m->band2g,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_rf_band2g_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->band2g_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-band2g-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_rf_band2g_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_band2g(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->band2g;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_band2g_owned,wifi_band2g_read,wifi_band2g_exchange};
    enum mt76_mcu_timing_status status=wifi_band2g_owned(d)?mt76_rf_band2g_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=false;
    console_printf("wifi-band2g: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->band2g_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_RF_BAND2G_COMMANDS*2;i++)console_printf("wifi-band2g-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_RF_BAND2G_COUNT;i++)console_printf("wifi-band2g-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_rf_band2g_address(i),(uint64_t)mt76_rf_band2g_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_band_mac_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->band2g_mode && m->band_mac_active && wifi_band2g_owned(d) && m->band2g_submitted==980 && mt76_rf_band2g_exact_profile_valid(&m->band2g);
}
static bool wifi_band_mac_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_band_mac_owned(d) || m->band_mac_submitted!=op || !mt76_band_mac_transfer_valid(&m->band_mac,op,write,a,data,length))return false;
    if(op%9>=4 && m->band_mac.before[op/9]!=mt76_band_mac_initial(op/9))return false;
    m->band_mac_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-band-mac-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_band_mac(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_band_mac *s=&m->band_mac;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=true;
    const struct mt76_band_mac_io io={d,wifi_band_mac_owned,wifi_band_mac_transfer};
    enum mt76_mcu_timing_status status=wifi_band_mac_owned(d)?mt76_band_mac_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=false;
    console_printf("wifi-band-mac: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->band_mac_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<4;i++)console_printf("wifi-band-mac-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<2;i++)console_printf("wifi-band-mac-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_band_mac_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_synth1_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->synth1_mode && m->synth1_active && wifi_band_mac_owned(d) && m->band_mac_submitted==18 && mt76_band_mac_exact_profile_valid(&m->band_mac);
}
static bool wifi_synth1_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->synth1_submitted;
    if(!value || !wifi_synth1_owned(d) || !mt76_synth1_admit(&m->synth1,op) || address==UINT32_MAX || address!=mt76_synth1_guard_address(op))return false;
    m->synth1_submitted++;*value=0;
    bool ok=wifi_mac_private_usb(d,false,address,value);
    console_printf("wifi-synth1-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static int wifi_synth1_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->synth1_submitted;uint8_t allowed[16];
    if(!wifi_synth1_owned(d) || !mt76_synth1_admit(&m->synth1,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_synth1_packet(&m->synth1,op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->synth1_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-synth1-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_synth1_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_synth1(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_synth1 *s=&m->synth1;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=true;
    const struct mt76_mcu_timing_io io={d,wifi_synth1_owned,wifi_synth1_read,wifi_synth1_exchange};
    enum mt76_mcu_timing_status status=wifi_synth1_owned(d)?mt76_synth1_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=false;
    console_printf("wifi-synth1: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u channel=1 writes=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->synth1_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->acks);
    for(unsigned i=0;i<126;i++)console_printf("wifi-synth1-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<21;i++)console_printf("wifi-synth1-value: index=%u address=%x mask=%u bits=%u before=%u after=%u\n",(uint64_t)i,(uint64_t)mt76_synth1_address(i),(uint64_t)mt76_synth1_mask(i),(uint64_t)mt76_synth1_bits(i),(uint64_t)s->words[3*i][2],(uint64_t)s->words[3*i+2][2]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_post_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->post_mode && m->post_active && wifi_synth1_owned(d) && m->synth1_submitted==441 && mt76_synth1_exact_profile_valid(&m->synth1) && mt76_trim_fields_valid(&m->trim_fields) && (m->trim_fields.nic0&0x100);
}
static bool wifi_post_read(void *context,uint32_t address,uint32_t *value){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->post_submitted;
    if(!value || !wifi_post_owned(d) || !mt76_post_admit(&m->post,op) || address==UINT32_MAX || address!=mt76_post_guard_address(op))return false;
    m->post_submitted++;*value=0;
    bool ok=wifi_mac_private_usb(d,false,address,value);
    console_printf("wifi-post-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)address,(uint64_t)*value,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static int wifi_post_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->post_submitted;uint8_t allowed[16];
    if(!wifi_post_owned(d) || !mt76_post_admit(&m->post,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_post_packet(op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned i=0;i<16;i++)if(packet[i]!=allowed[i])return -1;
    m->post_submitted++;
    for(unsigned i=0;i<16;i++)d->firmware.buffer[i]=packet[i];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;
    if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};
    for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned b=0;b<4;b++)words[n]|=(uint32_t)m->buffer[4*n+b]<<(8*b);
    console_printf("wifi-post-command: op=%u sequence=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_post_sequence(op/7),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int i=0;i<received;i++)reply[i]=m->buffer[i];
    return received;
}
static bool wifi_mcu_post(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rf_table *s=&m->post;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=true;
    if(wifi_post_owned(d))console_printf("wifi-post-pa: internal_2g=1 same_boot_mapped=1 raw_logged=0\n");
    const struct mt76_mcu_timing_io io={d,wifi_post_owned,wifi_post_read,wifi_post_exchange};
    enum mt76_mcu_timing_status status=wifi_post_owned(d)?mt76_post_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=false;
    console_printf("wifi-post: status=%u attempted=%u complete=%u observation_only=1 operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u equal=%u mismatch_mask_lo=%x mismatch_mask_hi=%x\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->post_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->equal,s->mismatch_mask,s->mismatch_hi);
    for(unsigned i=0;i<MT76_POST_COMMANDS*2;i++)console_printf("wifi-post-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<MT76_POST_COUNT;i++)console_printf("wifi-post-value: index=%u address=%x expected=%u observed=%u\n",(uint64_t)i,(uint64_t)mt76_post_address(i),(uint64_t)mt76_post_value(i),(uint64_t)s->values[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
static bool wifi_post_mac_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->post_mode && m->post_mac_active && wifi_post_owned(d) && m->post_submitted==266 && mt76_post_exact_profile_valid(&m->post);
}
static bool wifi_post_mac_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_post_mac_owned(d) || m->post_mac_submitted!=op || !mt76_post_mac_transfer_valid(&m->post_mac,op,write,a,data,length))return false;
    if(op%9>=4 && m->post_mac.before[op/9]!=mt76_post_mac_initial(op/9))return false;
    m->post_mac_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-post-mac-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_post_mac(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_post_mac *s=&m->post_mac;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=true;
    const struct mt76_post_mac_io io={d,wifi_post_mac_owned,wifi_post_mac_transfer};
    enum mt76_mcu_timing_status status=wifi_post_mac_owned(d)?mt76_post_mac_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=false;
    console_printf("wifi-post-mac: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->post_mac_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<6;i++)console_printf("wifi-post-mac-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<3;i++)console_printf("wifi-post-mac-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_post_mac_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}


static bool wifi_gain_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->gain_mode && m->gain_active && wifi_post_mac_owned(d) && m->post_mac_submitted==27 && mt76_post_mac_exact_profile_valid(&m->post_mac);
}
static bool wifi_gain_fields_transfer(void *context,bool write,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_efuse_map_gate *g=&m->gain_gate;
    if(!v || m->gain_inventory_active || !m->gain_fields.attempted || !wifi_gain_owned(d) || !mt76_rx_fields_begin(g,write,a,*v))return false;
    unsigned phase=g->phase,op=g->submitted-1;bool ok=wifi_mac_private_usb(d,write,a,v);
    console_printf("wifi-gain-fields-usb: op=%u phase=%u write=%u address=%x ok=%u raw_block_logged=0\n",(uint64_t)op,(uint64_t)phase,(uint64_t)write,(uint64_t)a,(uint64_t)ok);
    return mt76_rx_fields_finish(g,ok,*v)?true:wifi_mcu_fail(d);
}
static bool wifi_gain_fields_read(void *context,uint32_t a,uint32_t *v){if(!v)return false;*v=0;return wifi_gain_fields_transfer(context,false,a,v);}
static bool wifi_gain_fields_write(void *context,uint32_t a,uint32_t v){return wifi_gain_fields_transfer(context,true,a,&v);}
static bool wifi_gain_fields_wait(void *context,uint32_t us){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned phase=m->gain_gate.phase;
    if(!wifi_gain_owned(d) || m->gain_inventory_active || !mt76_efuse_map_delay(&m->gain_gate,us))return false;
    bool ok=timer_delay_us(us);if(!ok){m->gain_gate.failed=true;wifi_mcu_fail(d);}console_printf("wifi-gain-fields-delay: phase=%u us=%u ok=%u\n",(uint64_t)phase,(uint64_t)us,(uint64_t)ok);return ok;
}
static bool wifi_gain_inventory_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_efuse_map_gate *g=&m->gain_gate;struct mt76_rx_fields *f=&m->gain_fields;
    if(!(wifi_gain_owned(d) && m->gain_inventory_active && f->valid && mt76_rx_fields_valid(f) && g->phase==15 && g->block==1 && !g->failed && !g->pending && g->polls==f->polls && g->submitted==f->reads+f->writes))return false;
    for(unsigned i=0;i<2;i++)if(g->mac[i] || g->core[i]!=0xf000f200)return false;
    return g->dma[0]==0x40000050;
}
static bool wifi_gain_inventory_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->gain_inventory_submitted;
    if(!v || !wifi_gain_inventory_owned(d) || !mt76_gain_inventory_admit(&m->gain_inventory,op) || a==UINT32_MAX || a!=mt76_gain_inventory_address(op))return false;
    m->gain_inventory_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-gain-inventory-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_gain(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rx_fields *f=&m->gain_fields;struct mt76_efuse_map_gate *g=&m->gain_gate;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=true;
    const struct mt76_efuse_map_io io={{d,wifi_gain_fields_read,wifi_gain_fields_write,wifi_gain_fields_wait},wifi_gain_owned};
    enum mt76_capture_status status=wifi_gain_owned(d)?mt76_capture_rx_fields(&io,&d->wifi_identity,f):MT76_CAPTURE_REFUSED;
    console_printf("wifi-gain-fields: status=%u attempted=%u complete=%u valid=%u reads=%u writes=%u polls=%u blocks=%u submitted=%u phase=%u failed=%u logical=64 mode=0 raw_block_logged=0\n",(uint64_t)status,(uint64_t)f->attempted,(uint64_t)f->complete,(uint64_t)f->valid,(uint64_t)f->reads,(uint64_t)f->writes,(uint64_t)f->polls,(uint64_t)f->blocks,(uint64_t)g->submitted,(uint64_t)g->phase,(uint64_t)g->failed);
    console_printf("wifi-gain-decode: lna_raw=%u lna=%u rssi0=%d rssi1=%d clamped=%u agc8=%x observation_only=1\n",(uint64_t)f->lna_raw,(uint64_t)f->lna,(int64_t)f->rssi[0],(int64_t)f->rssi[1],(uint64_t)f->rssi_clamped,(uint64_t)f->agc8);
    for(unsigned i=0;i<2;i++)console_printf("wifi-gain-fields-guard: index=%u mac=%x core=%x\n",(uint64_t)i,(uint64_t)g->mac[i],(uint64_t)g->core[i]);
    console_printf("wifi-gain-fields-dma: value=%x\n",(uint64_t)g->dma[0]);
    bool ok=status==MT76_CAPTURE_OK;
    if(ok){m->gain_inventory_active=true;const struct mt76_mcu_timing_io inventory={d,wifi_gain_inventory_owned,wifi_gain_inventory_read,NULL};struct mt76_gain_inventory *v=&m->gain_inventory;
        enum mt76_mcu_timing_status st=wifi_gain_inventory_owned(d)?mt76_gain_inventory_capture(v,&inventory):MT76_TIMING_REFUSED;
        console_printf("wifi-gain-inventory: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0 observation_only=1\n",(uint64_t)st,(uint64_t)v->attempted,(uint64_t)v->complete,(uint64_t)v->reads,(uint64_t)m->gain_inventory_submitted);
        for(unsigned i=0;i<2;i++)console_printf("wifi-gain-inventory-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)v->guards[i][0],(uint64_t)v->guards[i][1],(uint64_t)v->guards[i][2]);
        for(unsigned i=0;i<21;i++)console_printf("wifi-gain-inventory-value: index=%u address=%x value=%x\n",(uint64_t)i,(uint64_t)mt76_gain_inventory_target(i),(uint64_t)v->values[i]);
        ok=st==MT76_TIMING_OK;
    }
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=false;m->gain_inventory_active=false;
    return ok?true:wifi_mcu_fail(d);
}

static bool wifi_channel_bbp_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_rx_fields *f=&m->gain_fields;
    return m->channel_bbp_mode && m->channel_bbp_active && wifi_gain_inventory_owned(d) &&
        m->gain_inventory_submitted==27 && mt76_gain_inventory_exact_profile_valid(&m->gain_inventory) &&
        f->lna_raw==0 && f->lna==0 && f->rssi[0]==1 && f->rssi[1]==1 && !f->rssi_clamped && f->agc8==0x16344ef0;
}
static bool wifi_channel_bbp_transfer(void *context,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    if(!wifi_channel_bbp_owned(d) || m->channel_bbp_submitted!=op || !mt76_channel_bbp_transfer_valid(&m->channel_bbp,op,write,a,data,length))return false;
    m->channel_bbp_submitted++;uint32_t v=0;
    if(write)for(unsigned j=0;j<4;j++)v|=(uint32_t)data[j]<<(8*j);
    bool ok=wifi_mac_private_usb(d,write,a,&v);
    if(ok && !write)for(unsigned j=0;j<4;j++)data[j]=(uint8_t)(v>>(8*j));
    console_printf("wifi-channel-bbp-usb: op=%u write=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)write,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_channel_bbp(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_channel_bbp *s=&m->channel_bbp;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=true;
    const struct mt76_post_mac_io io={d,wifi_channel_bbp_owned,wifi_channel_bbp_transfer};
    enum mt76_mcu_timing_status status=wifi_channel_bbp_owned(d)?mt76_channel_bbp_apply(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=false;
    console_printf("wifi-channel-bbp: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u writes=%u verified=%u channel=1 bandwidth=20 lna=0\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->channel_bbp_submitted,(uint64_t)s->reads,(uint64_t)s->writes,(uint64_t)s->verified);
    for(unsigned i=0;i<42;i++)console_printf("wifi-channel-bbp-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<21;i++)console_printf("wifi-channel-bbp-value: index=%u address=%x before=%x after=%x\n",(uint64_t)i,(uint64_t)mt76_channel_bbp_address(i),(uint64_t)s->before[i],(uint64_t)s->after[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_post_vco_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->post_vco_mode && m->post_vco_active && m->post_vco_delays==m->post_vco.waits && wifi_channel_bbp_owned(d) &&
        m->channel_bbp_submitted==189 && mt76_channel_bbp_exact_profile_valid(&m->channel_bbp);
}
static bool wifi_post_vco_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->post_vco_submitted;
    if(!v || !wifi_post_vco_owned(d) || !mt76_post_vco_admit(&m->post_vco,op) || a==UINT32_MAX || a!=mt76_post_vco_guard_address(op))return false;
    m->post_vco_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-post-vco-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);return ok?true:wifi_mcu_fail(d);
}
static bool wifi_post_vco_delay(void *context,uint32_t us){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_post_vco *s=&m->post_vco;
    if(!wifi_post_vco_owned(d) || m->post_vco_submitted!=s->operations || m->post_vco_delays!=s->waits || !mt76_post_vco_delay_admit(s) || us!=mt76_post_vco_delay(s->waits))return false;
    m->post_vco_delays++;bool ok=timer_delay_us(us);console_printf("wifi-post-vco-delay: sample=%u us=%u ok=%u\n",(uint64_t)s->waits,(uint64_t)us,(uint64_t)ok);return ok?true:wifi_mcu_fail(d);
}
static int wifi_post_vco_exchange(void *context,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->post_vco_submitted;uint8_t allowed[16];
    if(!wifi_post_vco_owned(d) || !mt76_post_vco_admit(&m->post_vco,op) || !packet || !reply || capacity<MT76_MCU_RESPONSE_MAX ||
       length!=16 || mt76_post_vco_packet(&m->post_vco,op,allowed,sizeof allowed)!=16)return -1;
    for(unsigned j=0;j<16;j++)if(packet[j]!=allowed[j])return -1;
    m->post_vco_submitted++;for(unsigned j=0;j<16;j++)d->firmware.buffer[j]=packet[j];
    int sent=wifi_mcu_bulk(d,false,16),received=-1;if(sent==16)received=wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
    uint32_t words[4]={0};for(unsigned n=0;n<4;n++)if(received>=(int)(4*n+4))for(unsigned j=0;j<4;j++)words[n]|=(uint32_t)m->buffer[4*n+j]<<(8*j);
    console_printf("wifi-post-vco-command: op=%u sequence=%u write=%u sent=%d received=%d cc=%u residue=%u h=%x address=%x value=%x t=%x\n",(uint64_t)op,(uint64_t)mt76_post_vco_sequence(op/7),(uint64_t)(op/7==1),(int64_t)sent,(int64_t)received,(uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)words[0],(uint64_t)words[1],(uint64_t)words[2],(uint64_t)words[3]);
    if(received<0 || (size_t)received>capacity)return -1;
    for(int j=0;j<received;j++)reply[j]=m->buffer[j];
    return received;
}
static bool wifi_mcu_post_vco(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_post_vco *s=&m->post_vco;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=true;
    const struct mt76_rf_wait_io io={{d,wifi_post_vco_owned,wifi_post_vco_read,wifi_post_vco_exchange},wifi_post_vco_delay};
    enum mt76_mcu_timing_status status=wifi_post_vco_owned(d)?mt76_post_vco_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=false;
    console_printf("wifi-post-vco: status=%u attempted=%u complete=%u operations=%u submitted=%u reads=%u exchanges=%u acks=%u observed=%u waits=%u delay_us=%u first_clear=%u observation_only=1\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->operations,(uint64_t)m->post_vco_submitted,(uint64_t)s->reads,(uint64_t)s->exchanges,(uint64_t)s->acks,(uint64_t)s->observed,(uint64_t)s->waits,(uint64_t)s->delay_us,(uint64_t)s->first_clear);
    for(unsigned j=0;j<18;j++)console_printf("wifi-post-vco-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)j,(uint64_t)s->guards[j][0],(uint64_t)s->guards[j][1],(uint64_t)s->guards[j][2]);
    for(unsigned j=0;j<9;j++)console_printf("wifi-post-vco-value: index=%u value=%u\n",(uint64_t)j,(uint64_t)s->words[j][2]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}

static bool wifi_agc_state_owned(void *context){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->agc_state_mode && m->agc_state_active && wifi_post_vco_owned(d) && m->post_vco_submitted==63 &&
        (m->scan_mode?mt76_post_vco_settled_profile_valid(&m->post_vco):
                      mt76_post_vco_captured_profile_valid(&m->post_vco));
}
static bool wifi_agc_state_read(void *context,uint32_t a,uint32_t *v){
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;unsigned op=m->agc_state_submitted;
    if(!v || !wifi_agc_state_owned(d) || !mt76_agc_state_admit(&m->agc_state,op) || a==UINT32_MAX || a!=mt76_agc_state_address(op))return false;
    m->agc_state_submitted++;*v=0;bool ok=wifi_mac_private_usb(d,false,a,v);
    console_printf("wifi-agc-state-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)*v,(uint64_t)ok);return ok?true:wifi_mcu_fail(d);
}
static bool wifi_mcu_agc_state(struct xhci_device *d){
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct mt76_agc_state *s=&m->agc_state;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=m->agc_state_active=true;
    console_write("wifi-agc-state-progress: reading fixed AGC8/9; no new radio writes\n");
    const struct mt76_mcu_timing_io io={d,wifi_agc_state_owned,wifi_agc_state_read,NULL};
    enum mt76_mcu_timing_status status=wifi_agc_state_owned(d)?mt76_agc_state_capture(s,&io):MT76_TIMING_REFUSED;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=m->agc_state_active=false;
    console_printf("wifi-agc-state: status=%u attempted=%u complete=%u reads=%u submitted=%u writes=0 init_done=%u low_gain=%d\n",(uint64_t)status,(uint64_t)s->attempted,(uint64_t)s->complete,(uint64_t)s->reads,(uint64_t)m->agc_state_submitted,(uint64_t)s->init_done,(int64_t)s->low_gain);
    for(unsigned i=0;i<2;i++)console_printf("wifi-agc-state-guard: index=%u mac=%x core=%x dma=%x\n",(uint64_t)i,(uint64_t)s->guards[i][0],(uint64_t)s->guards[i][1],(uint64_t)s->guards[i][2]);
    for(unsigned i=0;i<2;i++)console_printf("wifi-agc-state-value: index=%u address=%x read=%u raw=%x initial=%u current=%u\n",(uint64_t)i,(uint64_t)(0x2320+4*i),(uint64_t)(s->reads>=4+i),(uint64_t)s->raw[i],(uint64_t)s->initial[i],(uint64_t)s->current[i]);
    return status==MT76_TIMING_OK?true:wifi_mcu_fail(d);
}
#include "xhci_wifi_rx.inc"

static void wifi_mcu_select_mode(struct xhci_wifi_mcu *m,unsigned observe) {
m->clock_pair_mode=observe==8;m->table_mode=observe>=9 && observe<=80;m->mac_route_mode=observe>=12 && observe<=80;m->mac_policy_mode=observe>=13 && observe<=80;m->mac_policy_validate=observe>=15 && observe<=80;m->mac_protect_mode=observe>=16 && observe<=80;m->mac_contention_mode=observe>=17 && observe<=80;m->mac_txsw_mode=observe>=18 && observe<=80;m->mac_beacon_mode=observe>=19 && observe<=80;m->mac_vht_mode=observe>=20 && observe<=80;m->mac_buffer_mode=observe>=21 && observe<=80;m->mac_fce_mode=observe>=22 && observe<=80;m->mac_fce_validate=observe>=23 && observe<=80;m->mac_ampdu_mode=observe>=24 && observe<=80;m->bbp_after_mac_mode=observe>=25 && observe<=80;m->bbp_switch_mode=observe>=26 && observe<=80;m->dcoc_mode=observe>=27 && observe<=80;m->skey_mode=observe>=28 && observe<=80;m->wcid_mode=observe>=29 && observe<=80;m->mac_tail_mode=observe>=30 && observe<=80;m->mac_fixed_mode=observe>=31 && observe<=80;m->efuse_map_mode=observe>=32 && observe<=80;m->mac_address_mode=observe>=33 && observe<=80;m->mac_address_validate_mode=observe>=34 && observe<=80;m->mac_board_mode=observe>=35 && observe<=80;m->ant_snapshot_mode=observe>=36 && observe<=80;m->ant_init_mode=observe>=37 && observe<=80;m->rf_probe_mode=observe>=38 && observe<=80;m->rf_central_mode=observe>=39 && observe<=43;m->rf_six_mode=observe>=40 && observe<=43;m->rf_csr_mode=observe>=41 && observe<=43;m->rf_write_mode=observe==42 || observe==43;m->rf_state_mode=observe==43;m->mac_policy_observe=observe==14;m->mac_snapshot_mode=observe==10 || observe==11;m->mac_raw_mode=observe==11;m->rf_table_mode=observe>=44 && observe<=80;m->rf_bank5_mode=observe>=45 && observe<=80;m->rf_bank6_mode=observe>=46 && observe<=80;m->rf_vga_mode=observe>=47 && observe<=80;m->rf_g20_mode=observe>=48 && observe<=80;m->rf_trim_mode=observe>=49 && observe<=80;m->rf_cal_mode=observe>=50 && observe<=80;m->rf_wait_mode=observe>=51 && observe<=80;m->path_mode=observe>=52 && observe<=80;m->channel_state_mode=observe>=53 && observe<=80;m->channel_observe_mode=observe>=54 && observe<=80;m->vco_ready_mode=observe>=55 && observe<=80;m->usb_cycle_mode=observe>=56 && observe<=80;m->bw_observe_mode=observe>=57 && observe<=80;m->bbp20_mode=observe>=58 && observe<=80;m->channel_probe_mode=observe>=59 && observe<=80;m->band2g_mode=observe>=60 && observe<=80;m->synth1_mode=observe>=61 && observe<=80;m->post_mode=observe>=62 && observe<=80;m->gain_mode=observe>=63 && observe<=80;m->channel_bbp_mode=observe>=64 && observe<=80;m->post_vco_mode=observe>=65 && observe<=80;m->agc_state_mode=observe>=66 && observe<=80;m->rx_lifecycle_mode=observe>=67 && observe<=80;m->mac_cycle_mode=observe>=68 && observe<=80;m->cal_probe_mode=observe>=69 && observe<=80;m->poweron_mode=observe>=70 && observe<=80;m->passive_mode=observe==71;m->window_mode=observe>=72 && observe<=80;m->window_long_mode=observe>=73 && observe<=80;m->discovery_mode=observe>=74 && observe<=80;m->channel_mode=observe==75;m->tune_mode=observe>=76 && observe<=80;m->receive_mode=observe>=77 && observe<=80;m->scan_mode=observe>=78 && observe<=80;m->scan_retry_mode=observe==79;m->scan_batch_mode=observe==80;
}

static bool wifi_capture_mcu(bool reset,bool write_probe,bool pair_probe,bool timing,bool mac_basics,bool format,bool bbp,bool bbp_core,bool bbp_front,bool bbp_agc,bool bbp_batch,bool compact,unsigned agc_steps,unsigned observe) {
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
            else if(compact) {
                wifi_mcu_select_mode(m,observe);
                if(!wifi_mcu_initialize_compact(d,m->table_mode?2:5+agc_steps,(observe==6 || observe==7 || observe==8 || m->table_mode)))saved=false;
                else if(m->table_mode){if(!wifi_mcu_clock_enable(d) || !wifi_mcu_probe_bbp(d) || m->bbp.pbf!=0x80c00 || (m->mac_snapshot_mode && !wifi_mcu_mac_snapshot(d)) || (m->mac_route_mode && !wifi_mcu_mac_route(d)) || (m->mac_policy_mode && !wifi_mcu_mac_policy(d)) || (m->mac_protect_mode && !wifi_mcu_mac_protect(d)) || (m->mac_contention_mode && !wifi_mcu_mac_contention(d)) || (m->mac_txsw_mode && !wifi_mcu_mac_txsw(d)) || (m->mac_beacon_mode && !wifi_mcu_mac_beacon(d)) || (m->mac_vht_mode && !wifi_mcu_mac_vht(d)) || (m->mac_buffer_mode && !wifi_mcu_mac_buffer(d)) || (m->mac_fce_mode && !wifi_mcu_mac_fce(d)) || (m->mac_ampdu_mode && !wifi_mcu_mac_ampdu(d)) || ((m->bbp_after_mac_mode || (!m->mac_route_mode && !m->mac_raw_mode)) && !wifi_mcu_table_capture(d)) || (m->bbp_switch_mode && !wifi_mcu_bbp_switch(d)) || (m->dcoc_mode && !wifi_mcu_dcoc(d)) || (m->skey_mode && !wifi_mcu_skey(d)) || (m->wcid_mode && !wifi_mcu_wcid(d)) || (m->mac_tail_mode && !wifi_mcu_mac_tail(d)) || (m->mac_fixed_mode && !wifi_mcu_mac_fixed(d)) || (m->efuse_map_mode && !wifi_mcu_efuse_map(d)) || (m->mac_address_mode && !wifi_mcu_mac_address(d)) || (m->mac_board_mode && !wifi_mcu_mac_board(d)) || (m->ant_snapshot_mode && !wifi_mcu_ant_snapshot(d)) || (m->ant_init_mode && !wifi_mcu_ant_init(d)) || (m->rf_probe_mode && !wifi_mcu_rf_probe(d)) || (m->rf_central_mode && !wifi_mcu_rf_central(d)) || (m->rf_table_mode && !wifi_mcu_rf_table(d)) || (m->rf_bank5_mode && !wifi_mcu_rf_bank5(d)) || (m->rf_bank6_mode && !wifi_mcu_rf_bank6(d)) || (m->rf_vga_mode && !wifi_mcu_rf_vga(d)) || (wifi_5ghz_prereq_requested && !(wifi_5ghz_band_requested?wifi_mcu_5ghz_band(d):wifi_mcu_5ghz_prereq(d))) || (m->rf_g20_mode && !wifi_mcu_rf_g20(d)) || (m->rf_trim_mode && !wifi_mcu_rf_trim(d)) || (m->rf_cal_mode && !wifi_mcu_rf_cal(d)) || (m->rf_wait_mode && !wifi_mcu_rf_wait(d)) || (m->path_mode && !wifi_mcu_path(d)) || (m->channel_state_mode && !wifi_mcu_channel_state(d)) || (m->usb_cycle_mode && !wifi_mcu_usb_cycle(d)) || (m->bw_observe_mode && !wifi_mcu_bw_observe(d)) || (m->bbp20_mode && !wifi_mcu_bbp20(d)) || (m->channel_probe_mode && !wifi_mcu_channel_probe(d)) || (m->band2g_mode && (!wifi_mcu_band2g(d) || !wifi_mcu_band_mac(d))) || (m->synth1_mode && !wifi_mcu_synth1(d)) || (m->post_mode && (!wifi_mcu_post(d) || !wifi_mcu_post_mac(d))) || (m->gain_mode && !wifi_mcu_gain(d)) || (m->channel_bbp_mode && !wifi_mcu_channel_bbp(d)) || (m->post_vco_mode && !wifi_mcu_post_vco(d)) || (m->agc_state_mode && !wifi_mcu_agc_state(d)) || (m->rx_lifecycle_mode && !wifi_mcu_rx_lifecycle(d)))saved=false;}
                else if(observe==4 || observe==5 || observe==6 || observe==7) { if(!wifi_mcu_txbe_access(d,observe==5 || observe==7))saved=false; }
                else if(observe==2 || observe==3) { if(!wifi_mcu_txbe_first(d,observe==3))saved=false; }
                else if(observe==1 && !wifi_mcu_observe_txbe(d))saved=false;
            }
            else if(mac_basics && !wifi_mcu_initialize_mac(d))saved=false;
            else if(format && !wifi_mcu_initialize_format(d))saved=false;
            else if(bbp && !wifi_mcu_probe_bbp(d))saved=false;
            else if(bbp_core && !wifi_mcu_initialize_bbp_core(d))saved=false;
            else if(bbp_front && !wifi_mcu_initialize_bbp_front(d))saved=false;
            else if(bbp_agc && !bbp_batch && !wifi_mcu_initialize_bbp_agc(d))saved=false;
            else if(bbp_batch && !wifi_mcu_initialize_bbp_batch(d))saved=false;
        }
        f->active=false;
    }
    return found && saved;
}

bool xhci_wifi_capture_mcu(void) { return wifi_capture_mcu(false,false,false,false,false,false,false,false,false,false,false,false,false,false); }
bool xhci_wifi_capture_radio_reset(void) { return wifi_capture_mcu(true,false,false,false,false,false,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_write_probe(void) { return wifi_capture_mcu(true,true,false,false,false,false,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_pair_probe(void) { return wifi_capture_mcu(true,true,true,false,false,false,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_timing_probe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_mac_basics(void) { return wifi_capture_mcu(true,true,true,true,true,false,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_mac_format(void) { return wifi_capture_mcu(true,true,true,true,true,true,false,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_bbp_ready(void) { return wifi_capture_mcu(true,true,true,true,true,true,true,false,false,false,false,false,false,false); }

bool xhci_wifi_capture_bbp_core(void) { return wifi_capture_mcu(true,true,true,true,true,true,true,true,false,false,false,false,false,false); }

bool xhci_wifi_capture_bbp_front(void) { return wifi_capture_mcu(true,true,true,true,true,true,true,true,true,false,false,false,false,false); }

bool xhci_wifi_capture_bbp_agc(void) { return wifi_capture_mcu(true,true,true,true,true,true,true,true,true,true,false,false,false,false); }

bool xhci_wifi_capture_bbp_batch(void) { return wifi_capture_mcu(true,true,true,true,true,true,true,true,true,true,true,false,false,false); }

bool xhci_wifi_capture_compact(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,false,false); }

bool xhci_wifi_capture_agc_more(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,true,false); }

bool xhci_wifi_capture_agc_next(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,2,false); }

bool xhci_wifi_capture_agc_tail(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,3,false); }

bool xhci_wifi_capture_rxc_start(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,4,false); }

bool xhci_wifi_capture_rxc_tail(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,5,false); }

bool xhci_wifi_capture_txbe_start(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,false); }

bool xhci_wifi_capture_txbe_next(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,7,false); }

bool xhci_wifi_capture_txbe_observe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,true); }

bool xhci_wifi_capture_txbe_first(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,2); }

bool xhci_wifi_capture_txbe_second(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,3); }

bool xhci_wifi_capture_txbe_access(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,4); }

bool xhci_wifi_capture_txbe_direct(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,5); }

bool xhci_wifi_capture_clock(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,6); }

bool xhci_wifi_capture_clock_direct(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,6,7); }

bool xhci_wifi_capture_clock_pair(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,7,8); }

bool xhci_wifi_capture_bbp_table(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,9); }

bool xhci_wifi_capture_mac_snapshot(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,10); }

bool xhci_wifi_capture_mac_raw(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,11); }

bool xhci_wifi_capture_mac_route(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,12); }

bool xhci_wifi_capture_mac_policy(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,13); }

bool xhci_wifi_capture_mac_policy_observe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,14); }

bool xhci_wifi_capture_mac_policy_validate(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,15); }

bool xhci_wifi_capture_mac_protect(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,16); }

bool xhci_wifi_capture_mac_contention(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,17); }

bool xhci_wifi_capture_mac_txsw(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,18); }

bool xhci_wifi_capture_mac_beacon(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,19); }

bool xhci_wifi_capture_mac_vht(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,20); }

bool xhci_wifi_capture_mac_buffer(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,21); }

bool xhci_wifi_capture_mac_fce(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,22); }

bool xhci_wifi_capture_mac_fce_validate(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,23); }

bool xhci_wifi_capture_mac_ampdu(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,24); }

bool xhci_wifi_capture_bbp_after_mac(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,25); }

bool xhci_wifi_capture_bbp_switch(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,26); }

bool xhci_wifi_capture_dcoc(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,27); }

bool xhci_wifi_capture_skey(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,28); }

bool xhci_wifi_capture_wcid(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,29); }

bool xhci_wifi_capture_mac_tail(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,30); }

bool xhci_wifi_capture_mac_fixed(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,31); }

bool xhci_wifi_capture_efuse_map(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,32); }

bool xhci_wifi_capture_mac_address(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,33); }

bool xhci_wifi_capture_mac_address_validate(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,34); }

bool xhci_wifi_capture_mac_board(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,35); }

bool xhci_wifi_capture_ant_snapshot(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,36); }

bool xhci_wifi_capture_ant_init(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,37); }

bool xhci_wifi_capture_rf_probe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,38); }

bool xhci_wifi_capture_rf_central(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,39); }

bool xhci_wifi_capture_rf_six(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,40); }

bool xhci_wifi_capture_rf_csr(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,41); }

bool xhci_wifi_capture_rf_write(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,42); }

bool xhci_wifi_capture_rf_state(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,43); }

bool xhci_wifi_capture_rf_table(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,44); }

bool xhci_wifi_capture_rf_bank5(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,45); }
bool xhci_wifi_capture_rf_bank6(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,46); }
bool xhci_wifi_capture_rf_vga(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,47); }
bool xhci_wifi_capture_rf_g20(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,48); }
bool xhci_wifi_capture_rf_trim(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,49); }
bool xhci_wifi_capture_rf_cal(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,50); }
bool xhci_wifi_capture_rf_wait(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,51); }
bool xhci_wifi_capture_path(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,52); }
bool xhci_wifi_capture_channel_state(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,53); }
bool xhci_wifi_capture_channel_observe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,54); }
bool xhci_wifi_capture_vco_ready(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,55); }
bool xhci_wifi_capture_usb_cycle(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,56); }
bool xhci_wifi_capture_bw_observe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,57); }
bool xhci_wifi_capture_bbp20(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,58); }
bool xhci_wifi_capture_channel_probe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,59); }
bool xhci_wifi_capture_band2g(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,60); }
bool xhci_wifi_capture_synth1(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,61); }
bool xhci_wifi_capture_post(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,62); }
bool xhci_wifi_capture_gain(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,63); }
bool xhci_wifi_capture_channel_bbp(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,64); }
bool xhci_wifi_capture_post_vco(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,65); }
bool xhci_wifi_capture_agc_state(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,66); }

bool xhci_wifi_capture_rx_lifecycle(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,67); }

bool xhci_wifi_capture_mac_cycle(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,68); }

bool xhci_wifi_capture_cal_probe(void) { return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,69); }

bool xhci_wifi_capture_poweron(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,70);}

bool xhci_wifi_capture_passive(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,71);}

bool xhci_wifi_capture_window(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,72);}

bool xhci_wifi_capture_window_long(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,73);}

bool xhci_wifi_capture_discovery(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,74);}

bool xhci_wifi_capture_channel_preview(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,75);}

bool xhci_wifi_capture_channel_tune(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,76);}

bool xhci_wifi_capture_channel_receive(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,77);}

bool xhci_wifi_capture_scan(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,78);}

bool xhci_wifi_capture_scan_retry(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,79);}

bool xhci_wifi_capture_scan_batch(void){return wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,80);}

/* Same serialized mode80 operations; only inspect already stopped/retained DMA. */
bool xhci_wifi_capture_scan_channel_diagnosis(void){
    if(wifi_scan_diagnosis_requested)return false;
    wifi_scan_diagnosis_requested=true;
    bool ok=xhci_wifi_capture_scan_batch();
    wifi_scan_diagnosis_requested=false;
    return ok;
}

bool xhci_wifi_capture_5ghz_prereq(void){
    if(wifi_5ghz_prereq_requested)return false;
    wifi_5ghz_prereq_requested=true;
    bool ok=wifi_capture_mcu(true,true,true,true,false,false,false,false,false,false,false,true,0,47);
    wifi_5ghz_prereq_requested=false;return ok;
}

bool xhci_wifi_capture_5ghz_band(void){
    if(wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_5ghz_band_requested=true;
    bool ok=xhci_wifi_capture_5ghz_prereq();
    wifi_5ghz_band_requested=false;return ok;
}

bool xhci_wifi_capture_5ghz_channel(void){
    if(wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_5ghz_channel_requested=true;
    bool ok=xhci_wifi_capture_5ghz_band();
    wifi_5ghz_channel_requested=false;return ok;
}

bool xhci_wifi_capture_a20_inventory(void){
    if(wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_a20_inventory_requested=true;bool ok=xhci_wifi_capture_5ghz_channel();wifi_a20_inventory_requested=false;return ok;
}

bool xhci_wifi_capture_a20_cycle(void){
    if(wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_a20_cycle_requested=true;bool ok=xhci_wifi_capture_5ghz_channel();wifi_a20_cycle_requested=false;return ok;
}

bool xhci_wifi_capture_frontend(void){
    if(wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_frontend_requested=true;bool ok=xhci_wifi_capture_a20_cycle();wifi_frontend_requested=false;return ok;
}

bool xhci_wifi_capture_pa5_cycle(void){
    if(wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_pa5_requested=true;bool ok=xhci_wifi_capture_frontend();wifi_pa5_requested=false;return ok;
}

bool xhci_wifi_capture_bbp5_cycle(void){
    if(wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_bbp5_requested=true;bool ok=xhci_wifi_capture_pa5_cycle();wifi_bbp5_requested=false;return ok;
}

bool xhci_wifi_capture_held_channel(void){
    if(wifi_held_requested || wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_held_requested=true;bool ok=xhci_wifi_capture_bbp5_cycle();wifi_held_requested=false;return ok;
}

bool xhci_wifi_capture_trim5_channel(void){
    if(wifi_trim5_requested || wifi_held_requested || wifi_bbp5_requested || wifi_pa5_requested || wifi_frontend_requested || wifi_a20_cycle_requested || wifi_a20_inventory_requested || wifi_5ghz_channel_requested || wifi_5ghz_band_requested || wifi_5ghz_prereq_requested)return false;
    wifi_trim5_requested=true;bool ok=xhci_wifi_capture_held_channel();wifi_trim5_requested=false;return ok;
}

#include "xhci_wifi_dac5.inc"

#include "xhci_wifi_vco5.inc"

#include "xhci_wifi_channel_vco5.inc"
