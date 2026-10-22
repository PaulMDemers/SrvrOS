/* One diagnostic only: MCU random read of MAC_CSR0, never RF or register writes.
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
    const uint8_t payload[8]={0x00,0x10,0x41,0,0,0,0,0};
    bytes=mt76_mcu_encode(10,1,payload,sizeof payload,f->buffer,MT76_MCU_PACKET_MAX);
    if(!bytes)return -1;
    *read_sent=wifi_mcu_bulk(d,false,bytes);
    if(*read_sent!=(int)bytes)return -1;
    return wifi_mcu_bulk(d,true,MT76_MCU_RESPONSE_MAX);
}

bool xhci_wifi_capture_mcu(void) {
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
        disabled=disabled && wifi_mcu_owned(d) && wifi_fw_read(d,0x1004,&mac) && mac==0x20;
        /* Evidence only; no speculative body-length/event assumptions promoted. */
        console_printf("wifi-mcu-capture: slot=%u sent=%d received=%d decoded=%u sequence=%u event=%u reported_length=%u cc=%u residue=%u mac_disabled=%u failed=%u validated=0\n",
            (uint64_t)d->slot_id,(int64_t)sent,(int64_t)received,(uint64_t)decoded,
            (uint64_t)r.sequence,(uint64_t)r.event,(uint64_t)r.reported_length,
            (uint64_t)m->completion,(uint64_t)m->remaining,(uint64_t)disabled,(uint64_t)m->failed);
        /* A response is not MCU acceptance, even if sequence matches. */
        if(!decoded || r.sequence!=1 || !disabled){wifi_mcu_fail(d);saved=false;}
        f->active=false;
    }
    return found && saved;
}
