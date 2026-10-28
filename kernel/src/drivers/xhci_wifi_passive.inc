/* One retained request. No new transfer, payload delivery or buffer reuse. */
static bool wifi_passive_owned(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_rx *r=&m->rx;
    return m->passive_mode && m->passive.attempted && m->poweron.complete &&
        m->poweron_ack_mask==31 && m->mac_cycle.started && m->mac_cycle.snapshot_complete &&
        !m->mac_cycle.complete && !r->failed && !r->stop_attempted && !r->stopped &&
        r->configured && r->posts==1 && r->requested==4096 && wifi_rx_owned(d);
}
static bool wifi_passive_inspect(void *context,uint32_t tx,uint32_t ibi) {
    struct xhci_device *d=context;struct xhci_wifi_mcu *m=&d->firmware.mcu;
    struct xhci_wifi_passive *s=&m->passive;struct xhci_wifi_rx *r=&m->rx;
    if(!m->passive_mode || s->attempted)return false;
    *s=(struct xhci_wifi_passive){.attempted=true,.view_status=255,.beacon_status=255};
    if(!wifi_poweron_inspect(d,tx,ibi) || !wifi_passive_owned(d))return false;
    console_write("wifi-passive-progress: polling one retained main IN up to1000000us; decode only after MAC disable and exact stop\n");
    uint64_t start=timer_monotonic_us(),previous=start;unsigned stalled=0;s->waiting=true;
    for(unsigned n=0;n<XHCI_TRANSFER_TIMEOUT;n++) {
        uint64_t now=timer_monotonic_us();
        /* Do not repeat the whole ownership proof millions of times when the
         * clock is frozen. Normal captured clocks advance between checks. */
        if(now<previous || (now==previous && ++stalled>=1024))break;
        if(now!=previous)stalled=0;
        if(!wifi_passive_owned(d))break;
        previous=now;
        if(r->done || now-start>=1000000){s->wait_complete=true;break;}
        uint64_t flags=event_irq_save();struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        event_irq_restore(flags);s->polls++;
        __asm__ volatile("pause");
    }
    s->waiting=false;uint64_t end=timer_monotonic_us();
    if(end<previous || end<start || end-start>1100000)s->wait_complete=false;
    s->waited_us=end>=start?end-start:0;
    console_printf("wifi-passive-wait: attempted=1 complete=%u polls=%u waited_us=%u done=%u failed=%u requested=4096 event_during_wait=%u\n",(uint64_t)s->wait_complete,(uint64_t)s->polls,s->waited_us,(uint64_t)r->done,(uint64_t)r->failed,(uint64_t)r->passive_during_wait);
    return s->wait_complete;
}
static bool wifi_passive_capture(struct xhci_device *d) {
    struct xhci_wifi_mcu *m=&d->firmware.mcu;struct xhci_wifi_passive *s=&m->passive;struct xhci_wifi_rx *r=&m->rx;
    if(!m->passive_mode || !s->attempted || s->capture_attempted)return false;
    s->capture_attempted=true;
    uint64_t dequeue=r->stop_dequeue&~15ull;
    if(!s->wait_complete || !m->poweron.complete || m->poweron_ack_mask!=31 ||
       !m->mac_cycle.complete || !m->mac_cycle.mac_disabled || !r->stopped || !r->stop_attempted ||
       r->stop_state!=3 || (r->stop_dequeue&15)!=1 || (dequeue!=r->trb && dequeue!=r->trb+16) ||
       r->pending || r->failed || r->posts!=1 || r->requested!=4096 || r->ep.dci!=9 ||
       !r->buffer || !r->buffer_phys || (r->buffer_phys&4095) || r->trb!=r->ep.ring_phys ||
       !wifi_rx_owned(d))return false;
    /* Only normal completions carry a byte count. CC26/27/28 and absence of a
     * transfer event are observations with zero usable bytes, even if residue
     * would numerically imply data. Never inspect cancellation-owned bytes. */
    bool normal=r->done && (r->completion==1 || r->completion==13);
    if(!normal && r->done && (r->events!=1 || r->completion<26 || r->completion>28 ||
       (r->completion!=27 && r->remaining>r->requested)))return false;
    if(!r->done && (r->events || r->completion || r->remaining))return false;
    if(normal) {
        if(r->events!=1 || r->remaining>r->requested)return false;
        s->actual=r->requested-r->remaining;
    }
    if(s->actual) {
        __asm__ volatile("" ::: "memory");
        struct mt76_rx_view v;struct mt76_rx_beacon b={0};s->inspected=true;
        s->view_status=mt76_rx_usb_view(r->buffer,s->actual,&v);
        if(s->view_status==MT76_RX_OK) {
            s->consumed=(unsigned)v.consumed;s->mpdu=(unsigned)v.mpdu_size;s->frame_control=v.frame_control;
            s->beacon_status=mt76_rx_beacon(r->buffer,s->actual,1,&b);
            if(s->beacon_status==MT76_RX_OK) {
                s->beacon=true;s->ssid_size=b.ssid_size;s->channel=b.channel;s->interval=b.interval;
                s->privacy=b.privacy;s->rsn=b.rsn_present;s->wpa=b.wpa_present;
                s->probe_response=b.probe_response;s->ds_present=b.ds_present;
            }
        }
        /* Never retain/log SSID, addresses or raw payload in the observation. */
        for(unsigned i=0;i<sizeof b;i++)((volatile uint8_t *)&b)[i]=0;
    }
    s->complete=true;return true;
}
static void wifi_passive_log(struct xhci_device *d) {
    struct xhci_wifi_passive *s=&d->firmware.mcu.passive;
    console_printf("wifi-passive: attempted=%u wait_complete=%u capture_attempted=%u complete=%u inspected=%u actual=%u view_status=%u beacon_status=%u consumed=%u mpdu=%u frame_control=%x beacon=%u event_during_wait=%u posts=%u retained=1 rearmed=0 data_out=0 first_entry_only=1\n",(uint64_t)s->attempted,(uint64_t)s->wait_complete,(uint64_t)s->capture_attempted,(uint64_t)s->complete,(uint64_t)s->inspected,(uint64_t)s->actual,(uint64_t)s->view_status,(uint64_t)s->beacon_status,(uint64_t)s->consumed,(uint64_t)s->mpdu,(uint64_t)s->frame_control,(uint64_t)s->beacon,(uint64_t)d->firmware.mcu.rx.passive_during_wait,(uint64_t)d->firmware.mcu.rx.posts);
    if(s->beacon)console_printf("wifi-passive-beacon: channel=%u interval=%u ssid_size=%u privacy=%u rsn=%u wpa=%u probe_response=%u ds_present=%u identifiers_logged=0\n",(uint64_t)s->channel,(uint64_t)s->interval,(uint64_t)s->ssid_size,(uint64_t)s->privacy,(uint64_t)s->rsn,(uint64_t)s->wpa,(uint64_t)s->probe_response,(uint64_t)s->ds_present);
}
