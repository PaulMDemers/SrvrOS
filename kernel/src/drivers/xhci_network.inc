/* Included after the common xHCI ring/control helpers. All runtime data paths
 * are asynchronous and bounded; no synchronous USB transaction is used by the
 * network stack or the input service. Retain DMA resources after any fault. */
static void network_fail(struct xhci_device *d,const char *reason) {
    struct xhci_network *n=d->network;if(n->failed) return;
    n->ready=false;n->failed=true;n->errors++;n->link.known=true;n->link.up=false;
    d->stage="network-quarantined";
    console_printf("xhci-network: slot=%u quarantined reason=%s tx=%u rx=%u\n",(uint64_t)d->slot_id,reason,n->tx_packets,n->rx_packets);
}
static void network_post(struct xhci_device *d,unsigned index,uint32_t length) {
    struct xhci_network_endpoint *e=&d->network->ep[index];
    struct xhci_trb trb={.parameter_low=(uint32_t)e->buffer_phys,.parameter_high=(uint32_t)(e->buffer_phys>>32),
        .status=length,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    e->requested=length;e->remaining=0;e->completion=0;e->done=false;
    e->trb=ring_push(e->hw.ring,e->hw.ring_phys,&e->hw.enqueue,&e->hw.cycle,&trb);
    e->pending=true;db_write32(d->slot_id,e->hw.dci);
}
static bool network_event(struct xhci_device *d,uint8_t dci,uint64_t pointer,uint8_t completion,uint32_t remaining) {
    if(!d->network) return false;
    for(unsigned i=0;i<3;i++) {
        struct xhci_network_endpoint *e=&d->network->ep[i];
        if(e->pending && e->hw.dci==dci && e->trb==pointer) {
            e->pending=false;e->done=true;e->completion=completion;e->remaining=remaining;
            if(i==1) net_notify_rx();
            return true;
        }
    }
    return false;
}
static bool network_complete(struct xhci_device *d,unsigned index) {
    struct xhci_network_endpoint *e=&d->network->ep[index];
    if(!e->done) return false;
    if((e->completion!=1 && e->completion!=13)||e->remaining>e->requested || (!index&&e->remaining)) {
        network_fail(d,"transfer-completion");return false;
    }
    return true;
}
static void network_service(struct xhci_device *d) {
    struct xhci_network *n=d->network;if(!n||!n->ready) return;
    if(!(op_read32(XHCI_OP_PORTS+(d->port_id-1)*16)&XHCI_PORTSC_CCS)) { network_fail(d,"disconnected");return; }
    for(unsigned i=0;i<3;i++) if(n->ep[i].done && !network_complete(d,i)) return;
    struct xhci_network_endpoint *notify=&n->ep[2],*tx=&n->ep[0],*rx=&n->ep[1];
    if(notify->done) {
        unsigned actual=notify->requested-notify->remaining;
        bool valid=usb_ecm_notification(&n->link,notify->buffer,actual,n->config.control_interface);
        /* QEMU 9.2's Ethernet gadget reports the data interface in wIndex. */
        if(!valid && d->vendor_id==0x0525 && d->product_id==0xa4a2)
            valid=usb_ecm_notification(&n->link,notify->buffer,actual,n->config.data_interface);
        if(valid) n->notifications++;else n->drops++;
        notify->done=false;
    }
    if(!notify->pending) network_post(d,2,notify->hw.packet);
    if(!rx->pending && !rx->done) network_post(d,1,4096);
    if(tx->done) {
        tx->done=false;
        if(n->tx_zero) { n->tx_zero=false;network_post(d,0,0); }
        else n->tx_packets++;
    }
    if(tx->pending && timer_monotonic_us()-n->tx_started>=2000000) { network_fail(d,"transmit-deadline");return; }
    if(!tx->pending && n->count) {
        unsigned length=n->length[n->head];copy_bytes(tx->buffer,n->queue[n->head],length);
        n->head=(n->head+1)%XHCI_NET_QUEUE;n->count--;
        n->tx_zero=length%tx->hw.packet==0;n->tx_started=timer_monotonic_us();network_post(d,0,length);
    }
}
static bool network_send(void *context,const uint8_t *frame,uint16_t length) {
    struct xhci_device *d=context;struct xhci_network *n=d->network;
    uint64_t flags=event_irq_save();
    if(!n||!n->ready||!frame||length<14||length>NET_DEVICE_FRAME_MAX||n->count==XHCI_NET_QUEUE||(n->link.known&&!n->link.up)) { if(n) n->drops++;event_irq_restore(flags);return false; }
    unsigned tail=(n->head+n->count)%XHCI_NET_QUEUE;
    copy_bytes(n->queue[tail],frame,length);n->length[tail]=length;n->count++;
    event_irq_restore(flags);return true;
}
static uint64_t network_poll(void *context,uint64_t budget,bool verbose) {
    (void)verbose;struct xhci_device *d=context;struct xhci_network *n=d->network;
    uint64_t flags=event_irq_save(),received=0;
    if(!n||!n->ready||n->polling) { event_irq_restore(flags);return 0; }
    n->polling=true;
    struct xhci_trb event;
    for(unsigned i=0;i<XHCI_EVENT_TRB_COUNT && poll_event(&event);i++) dispatch_transfer_event(&event);
    network_service(d);
    struct xhci_network_endpoint *rx=&n->ep[1];
    if(n->ready && budget && network_complete(d,1)) {
        unsigned length=rx->requested-rx->remaining;
        rx->done=false;
        if(length>=14 && length<=NET_DEVICE_FRAME_MAX) { n->rx_packets++;received++;net_handle_ethernet_frame(rx->buffer,length); }
        else if(length) n->drops++;
        if(n->ready) network_post(d,1,4096);
    }
    n->polling=false;event_irq_restore(flags);return received;
}
static bool network_link(void *context,struct net_device_link *out) {
    struct xhci_network *n=((struct xhci_device *)context)->network;
    if(!n||!n->link.known) return false;
    uint32_t speed=n->link.downstream_bps/1000000;
    *out=(struct net_device_link){.up=n->link.up,.speed_mbps=speed>65535?0:(uint16_t)speed};return true;
}
static bool network_descriptor(struct xhci_device *d,uint8_t type,uint8_t index,uint16_t language,uint16_t length) {
    zero_bytes(d->descriptor_buffer,PMM_FRAME_SIZE);return get_descriptor(d,type,index,language,length);
}
static bool network_configure(struct xhci_device *d,const struct usb_ecm_config *config) {
    struct xhci_network *n=kcalloc(1,sizeof(*n));if(!n) return false;d->network=n;n->config=*config;
    if(!network_descriptor(d,3,0,0,4)||d->descriptor_buffer[0]<4||d->descriptor_buffer[1]!=3) return false;
    uint16_t language=d->descriptor_buffer[2]|(uint16_t)d->descriptor_buffer[3]<<8;
    if(!language||!network_descriptor(d,3,config->mac_string,language,26)||!usb_ecm_mac(d->descriptor_buffer,26,n->mac)) return false;
    const struct usb_ecm_endpoint *descriptors[]={&config->out,&config->in,&config->notify};
    for(unsigned i=0;i<3;i++) {
        struct xhci_network_endpoint *e=&n->ep[i];const struct usb_ecm_endpoint *desc=descriptors[i];void *p;
        e->hw.dci=endpoint_dci(desc->address);e->hw.packet=desc->packet;e->hw.burst=desc->burst;e->hw.cycle=true;
        if(!alloc_frame_zero(&e->hw.ring_phys,&p)) return false;
        e->hw.ring=p;write_link_trb(e->hw.ring,e->hw.ring_phys,true);
        if(!alloc_frame_zero(&e->buffer_phys,&p)) return false;
        e->buffer=p;
    }
    if(!set_configuration(d,config->configuration)) return false;
    struct usb_setup_packet alternate={.request_type=1,.request=11,.value=config->data_alt,.index=config->data_interface};
    if(!control_transfer(d,&alternate,0,0,false)) return false;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);unsigned add=1,max=1;
    for(unsigned i=0;i<3;i++) { unsigned dci=n->ep[i].hw.dci;add|=1u<<dci;if(dci>max) max=dci; }
    context_write32(d->input_context,0,1,add);
    context_write32(d->input_context,1,0,(context_read32(d->output_context,0,0)&~(31u<<27))|(max<<27));
    for(unsigned i=1;i<3;i++) context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    for(unsigned i=0;i<3;i++) {
        struct xhci_network_endpoint *e=&n->ep[i];unsigned ctx=e->hw.dci+1;
        if(i==2) context_write32(d->input_context,ctx,0,(uint32_t)interrupt_interval_exponent(d->speed,config->notify.interval)<<16);
        context_write32(d->input_context,ctx,1,(3u<<1)|((i==0?2u:i==1?6u:7u)<<3)|((uint32_t)e->hw.burst<<8)|((uint32_t)e->hw.packet<<16));
        context_write32(d->input_context,ctx,2,(uint32_t)(e->hw.ring_phys|1));context_write32(d->input_context,ctx,3,(uint32_t)(e->hw.ring_phys>>32));
        context_write32(d->input_context,ctx,4,i==2?(uint32_t)e->hw.packet|((uint32_t)e->hw.packet<<16):1514);
    }
    struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,.parameter_high=(uint32_t)(d->input_context_phys>>32),.control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    if(!submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,0)) return false;
    struct usb_setup_packet filter={.request_type=0x21,.request=0x43,.value=0x0c,.index=config->control_interface};
    if(!control_transfer(d,&filter,0,0,false)) return false;
    struct net_device_ops ops={.name="usb-ecm",.context=d,.priority=50,.send=network_send,.poll=network_poll,.link=network_link};
    copy_bytes(ops.mac,n->mac,6);if(!net_device_register(&ops)) return false;
    d->configuration_value=config->configuration;d->interface_number=config->control_interface;d->configured=true;xhci.configured_devices++;
    n->ready=true;network_count++;d->stage="network-ready";
    network_post(d,1,4096);network_post(d,2,n->ep[2].hw.packet);
    console_printf("xhci-network: ECM ready slot=%u configuration=%u control=%u data=%u alternate=%u\n",(uint64_t)d->slot_id,(uint64_t)config->configuration,(uint64_t)config->control_interface,(uint64_t)config->data_interface,(uint64_t)config->data_alt);
    return true;
}
static bool enumerate_network(struct xhci_device *d,uint16_t length) {
    unsigned configs=d->configuration_count;if(configs>8) configs=8;
    for(unsigned index=0;index<configs;index++) {
        if(index) {
            if(!network_descriptor(d,2,index,0,9)) return false;
            length=d->descriptor_buffer[2]|(uint16_t)d->descriptor_buffer[3]<<8;
            if(length<9||length>4096||!network_descriptor(d,2,index,0,length)) return false;
        }
        struct usb_ecm_config config;
        if(!usb_ecm_parse(d->descriptor_buffer,length,d->speed,&config)) continue;
        if(!network_configure(d,&config)) {
            if(d->network) network_fail(d,"configuration");
            console_printf("xhci-network: ECM configuration failed slot=%u\n",(uint64_t)d->slot_id);
        }
        return true; /* Recognized; do not try a second configuration after mutation. */
    }
    return false;
}
