/* Opt-in retained main-RX endpoint trial. MAC/DMA stay disabled. All endpoint
 * resources survive completion, Stop Endpoint and failures until host reset.
 * No receive payload is logged, delivered, rearmed or interpreted here. */
static bool wifi_rx_fail(struct xhci_device *d) {
    d->firmware.mcu.rx.failed=true;
    return wifi_mcu_fail(d);
}
static bool wifi_rx_owned(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    return m->rx_lifecycle_mode && m->rx.active && wifi_agc_state_owned(d) &&
        m->agc_state_submitted==8 && mt76_agc_state_valid(&m->agc_state) &&
        m->agc_state.raw[1]==0 && !firmware_command_failed;
}
static bool wifi_rx_guards_valid(const struct xhci_wifi_rx *r,unsigned reads) {
    if(r->reads!=reads || reads>6)return false;
    for(unsigned i=0;i<reads;i++)if(r->guards[i/3][i%3]!=(i%3==0?0:i%3==1?0xf000f200u:0x40000050u))return false;
    return true;
}
static bool wifi_rx_guard(struct xhci_device *d,unsigned op) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    if(!wifi_rx_owned(d) || !r->attempted || op>=6 || !wifi_rx_guards_valid(r,op) ||
        (op<3?(r->configured || r->posts):(r->pending || !r->stopped)))return false;
    unsigned edge=op/3,j=op%3;uint32_t a=mt76_rf_probe_read_address(j),v=0;
    bool ok=wifi_mac_private_usb(d,false,a,&v);
    r->guards[edge][j]=v;r->reads++;
    console_printf("wifi-rx-lifecycle-usb: op=%u address=%x value=%x ok=%u\n",(uint64_t)op,(uint64_t)a,(uint64_t)v,(uint64_t)ok);
    return ok && v==(j==0?0:j==1?0xf000f200u:0x40000050u);
}
static bool configure_wifi_rx(struct xhci_device *d) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    if(!wifi_rx_owned(d) || !r->attempted || !wifi_rx_guards_valid(r,3) || r->configured ||
        r->ep.ring || r->buffer || r->posts || !d->output_context ||
        !d->input_context || !xhci.command_ring)return false;
    void *p;
    if(!alloc_frame_zero(&r->ep.ring_phys,&p))return wifi_rx_fail(d);
    r->ep.ring=p;r->ep.cycle=true;r->ep.dci=9;r->ep.packet=512;
    write_link_trb(r->ep.ring,r->ep.ring_phys,true);
    if(!alloc_frame_zero(&r->buffer_phys,&p))return wifi_rx_fail(d);
    r->buffer=p;
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    context_write32(d->input_context,0,1,1u|(1u<<9));
    context_write32(d->input_context,1,0,(context_read32(d->output_context,0,0)&~(31u<<27))|(16u<<27));
    for(unsigned i=1;i<3;i++)context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    context_write32(d->input_context,10,1,(3u<<1)|(6u<<3)|(512u<<16));
    context_write32(d->input_context,10,2,(uint32_t)(r->ep.ring_phys|1));
    context_write32(d->input_context,10,3,(uint32_t)(r->ep.ring_phys>>32));
    context_write32(d->input_context,10,4,4096);
    struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,.parameter_high=d->input_context_phys>>32,
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    uint64_t flags=event_irq_save(),slot=0;firmware_command_exact=true;
    bool ok=submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,&slot) && slot==d->slot_id;
    firmware_command_exact=false;if(!ok)firmware_command_failed=true;
    event_irq_restore(flags);
    if(!ok)return wifi_rx_fail(d);
    r->configured=true;
    console_write("wifi-rx-lifecycle-config: endpoint=132 dci=9 packet=512 configured=1\n");
    return true;
}
static bool wifi_rx_post(struct xhci_device *d) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    if(!wifi_rx_owned(d) || !r->configured || !wifi_rx_guards_valid(r,3) || r->posts ||
        r->pending || r->done || r->failed || r->stop_attempted ||
        !r->ep.ring || !r->buffer || r->ep.dci!=9 || r->ep.packet!=512 ||
        r->ep.enqueue || !r->ep.cycle || !r->buffer_phys || !r->ep.ring_phys ||
        (r->buffer_phys&4095) || (r->ep.ring_phys&4095))return false;
    uint64_t flags=event_irq_save();
    struct xhci_trb trb={.parameter_low=(uint32_t)r->buffer_phys,.parameter_high=r->buffer_phys>>32,
        .status=4096,.control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    r->requested=4096;r->posts=1;r->pending=true;
    r->trb=ring_push(r->ep.ring,r->ep.ring_phys,&r->ep.enqueue,&r->ep.cycle,&trb);
    db_write32(d->slot_id,9);event_irq_restore(flags);
    console_write("wifi-rx-lifecycle-post: count=1 requested=4096\n");return true;
}
static bool wifi_rx_event(struct xhci_device *d,const struct xhci_trb *event) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    uint64_t pointer=event->parameter_low|((uint64_t)event->parameter_high<<32);
    if(!r->configured || r->posts!=1 || ((event->control>>10)&63)!=32 ||
        (event->control>>24)!=d->slot_id || ((event->control>>16)&31)!=9 ||
        pointer!=r->trb)return false;
    if(r->stopped || !r->pending){r->late_events++;return true;}
    if(event->control&4){wifi_rx_fail(d);return true;}
    r->pending=false;r->done=true;r->events++;r->completion=event->status>>24;r->remaining=event->status&0xffffff;
    bool normal=(r->completion==1 || r->completion==13) && r->remaining<=r->requested;
    /* Length-invalid stopped events carry no usable byte count. Only the
     * command completion plus output-context proof can quiesce this buffer. */
    bool stopped=r->stopping && r->completion>=26 && r->completion<=28;
    if(!normal && !stopped)wifi_rx_fail(d);
    return true;
}
static bool wifi_rx_stop(struct xhci_device *d) {
    struct xhci_wifi_rx *r=&d->firmware.mcu.rx;
    /* Teardown deliberately does not require pristine RF/software proof:
     * once posted, an owned endpoint still needs stopping after cancellation. */
    if(!r->attempted || !r->configured || r->posts!=1 || r->stop_attempted ||
        !r->ep.ring || r->ep.dci!=9 || r->trb!=r->ep.ring_phys ||
        !d->present || !d->slot_id || !d->output_context || !xhci.command_ring ||
        firmware_command_failed || strict_control.device)return false;
    uint64_t flags=event_irq_save(),slot=0;
    r->stop_attempted=true;r->stopping=true;
    struct xhci_trb cmd={.control=(15u<<10)|(9u<<16)|((uint32_t)d->slot_id<<24)};
    firmware_command_exact=true;
    bool ok=submit_command_trb(&cmd,15,&slot) && slot==d->slot_id;
    firmware_command_exact=false;r->stopping=false;
    if(!ok)firmware_command_failed=true;
    if(ok) {
        r->stop_state=context_read32(d->output_context,9,0)&7;
        r->stop_dequeue=context_read32(d->output_context,9,2)|((uint64_t)context_read32(d->output_context,9,3)<<32);
        uint64_t p=r->stop_dequeue&~15ull;
        ok=r->stop_state==3 && (r->stop_dequeue&15)==1 && (p==r->trb || p==r->trb+16);
    }
    /* Successful command+stopped context proves cessation even when Stop did
     * not generate a Transfer Event. Never derive actual bytes from CC27. */
    if(ok){r->pending=false;r->stopped=true;}
    event_irq_restore(flags);
    console_printf("wifi-rx-lifecycle-stop: attempted=1 ok=%u state=%u dequeue=%x trb=%x retained=1\n",(uint64_t)ok,(uint64_t)r->stop_state,r->stop_dequeue,r->trb);
    return ok?true:wifi_rx_fail(d);
}
static bool wifi_mcu_rx_lifecycle(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_rx *r=&m->rx;
    if(r->attempted || r->active)return false;
    r->attempted=true;r->active=true;
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=m->agc_state_active=true;
    console_write("wifi-rx-lifecycle-progress: configuring one IN; MAC stays disabled; stop after 10ms\n");
    bool ok=wifi_rx_guard(d,0) && wifi_rx_guard(d,1) && wifi_rx_guard(d,2) && configure_wifi_rx(d) && wifi_rx_post(d);
    if(r->posts) {
        uint64_t start=timer_monotonic_us(),clock=start;unsigned stalled=0;
        while(!r->done) {
            uint64_t flags=event_irq_save();struct xhci_trb event;
            if(poll_event(&event))dispatch_transfer_event(&event);
            event_irq_restore(flags);
            uint64_t now=timer_monotonic_us();
            if(now-start>=10000 || (now==clock && ++stalled>=XHCI_TRANSFER_TIMEOUT))break;
            if(now!=clock){clock=now;stalled=0;}
            __asm__ volatile("pause");
        }
        r->waited_us=timer_monotonic_us()-start;r->wait_ok=r->done || r->waited_us>=10000;
        bool stopped=wifi_rx_stop(d);ok=ok && stopped && !r->failed && r->wait_ok;
    }
    if(ok)ok=wifi_rx_guard(d,3) && wifi_rx_guard(d,4) && wifi_rx_guard(d,5);
    m->table_active=m->dcoc_active=m->skey_active=m->wcid_active=m->mac_tail_active=m->mac_fixed_active=m->efuse_map_active=m->mac_board_active=m->ant_snapshot_active=m->ant_init_active=m->rf_probe_active=m->rf_table_active=m->rf_bank5_active=m->rf_bank6_active=m->rf_vga_active=m->rf_g20_active=m->rf_trim_active=m->rf_cal_active=m->rf_wait_active=m->path_active=m->channel_state_active=m->usb_cycle_active=m->bw_observe_active=m->bbp20_active=m->channel_probe_active=m->band2g_active=m->band_mac_active=m->synth1_active=m->post_active=m->post_mac_active=m->gain_active=m->gain_inventory_active=m->channel_bbp_active=m->post_vco_active=m->agc_state_active=false;
    r->active=false;
    console_printf("wifi-rx-lifecycle: ok=%u attempted=%u configured=%u reads=%u posts=%u requested=%u events=%u cc=%u remaining=%u stop_attempted=%u stopped=%u pending=%u failed=%u wait_ok=%u waited_us=%u retained=1 radio_writes=0\n",(uint64_t)ok,(uint64_t)r->attempted,(uint64_t)r->configured,(uint64_t)r->reads,(uint64_t)r->posts,(uint64_t)r->requested,(uint64_t)r->events,(uint64_t)r->completion,(uint64_t)r->remaining,(uint64_t)r->stop_attempted,(uint64_t)r->stopped,(uint64_t)r->pending,(uint64_t)r->failed,(uint64_t)r->wait_ok,r->waited_us);
    return ok?true:wifi_rx_fail(d);
}
