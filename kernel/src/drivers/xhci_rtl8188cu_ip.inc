/* One boot-only controlled port. No worker or concurrent xHCI consumer:
 * callbacks run under the station's existing IRQ exclusion. */
#define RTL_CU_IP_QUEUE 4u
#define RTL_CU_IP_DRAIN_SPINS 200000u
#define RTL_CU_C2H_EVENT 0x01a0u
#define RTL_CU_C2H_OWNER 0x01afu
#define RTL_CU_C2H_CCX 3u
#define RTL_CU_CCX_ACK 0x40u
#define RTL_CU_CCX_POLL_US 5000u
#define RTL_CU_CCX_MAX_POLLS 192u
_Static_assert(RTL_CU_DATA_TX_LIMIT+5u<XHCI_TRB_COUNT-1,"station OUT cannot wrap");
_Static_assert(RTL_CU_DATA_TX_MAX<=PMM_FRAME_SIZE,"station data must fit its pinned DMA page");

enum rtl_cu_ccx_result {
    RTL_CU_CCX_NOT_RUN, RTL_CU_CCX_UNAVAILABLE, RTL_CU_CCX_WAITING, RTL_CU_CCX_ACKED,
    RTL_CU_CCX_NO_ACK, RTL_CU_CCX_MISSING, RTL_CU_CCX_IO_ERROR, RTL_CU_CCX_OTHER_EVENT,
};

static struct {
    struct net_bounded_probe probe;
    struct net_bounded_probe_io io;
    struct net_device_ops ops;
    struct { uint16_t bytes; uint8_t frame[RTL_CU_ETH_MAX]; } queue[RTL_CU_IP_QUEUE];
    uint8_t plain[WPA2_CCMP_DATA_MAX],rx[RTL_CU_ETH_MAX];
    unsigned head,queued,posted,completed,received,protected_rx,ignored;
    uint64_t tx_start,deadline;
    struct {
        uint64_t started,next,elapsed_us,fingerprint;
        unsigned mpdu,sequence,polls,initial_owner,event,retries,status;
        enum rtl_cu_ccx_result result;
        bool requested;
    } first_tx;
    bool live,accepting,registered,retired,opened,dhcp,dns,drained,failed;
    unsigned http;
} rtl_ip;

/* Only the first protected MPDU (the initial Discover) requests a report.
 * This firmware mailbox has no frame identifier. Refuse a nonempty mailbox
 * and leave unsolicited events alone. The checksum covers the report bit. */
static void rtl_cu_ip_trace_first(struct xhci_device *d,uint8_t *usb) {
    size_t mpdu=usb[0]|(unsigned)usb[1]<<8;
    const uint8_t *frame=usb+32+8u*(usb[7]>>2);
    uint8_t digest[32];
    sha256_digest(frame+32,mpdu-32,digest); /* Ciphertext and MIC. */
    rtl_ip.first_tx.mpdu=(unsigned)mpdu;
    rtl_ip.first_tx.sequence=(frame[22]|(unsigned)frame[23]<<8)>>4;
    for(unsigned i=0;i<8;i++)
        rtl_ip.first_tx.fingerprint=rtl_ip.first_tx.fingerprint<<8|digest[i];
    uint32_t owner=0;
    if(!rtl8188cu_read(d,RTL_CU_C2H_OWNER,1,&owner)) {
        rtl_ip.first_tx.result=RTL_CU_CCX_IO_ERROR;
        return;
    }
    rtl_ip.first_tx.initial_owner=owner;
    if(owner) {
        rtl_ip.first_tx.result=RTL_CU_CCX_UNAVAILABLE;
        return;
    }
    usb[10]|=8; /* TX descriptor DWORD 2 bit 19, CCX report. */
    usb[28]^=8; /* Update the corresponding bit in the XOR checksum. */
    rtl_ip.first_tx.result=RTL_CU_CCX_WAITING;
    rtl_ip.first_tx.requested=true;
}

static void rtl_cu_ip_trace_poll(struct xhci_device *d) {
    if(rtl_ip.first_tx.result!=RTL_CU_CCX_WAITING || !rtl_ip.completed)return;
    uint64_t now=timer_monotonic_us();
    if(now<rtl_ip.first_tx.next)return;
    rtl_ip.first_tx.next=now+RTL_CU_CCX_POLL_US;
    rtl_ip.first_tx.polls++;
    uint32_t owner=0,event=0,retries=0,status=0;
    if(!rtl8188cu_read(d,RTL_CU_C2H_OWNER,1,&owner))goto io_error;
    if(owner==0) {
        if(rtl_ip.first_tx.polls==RTL_CU_CCX_MAX_POLLS)
            rtl_ip.first_tx.result=RTL_CU_CCX_MISSING;
        return;
    }
    if(owner!=0xff || !rtl8188cu_read(d,RTL_CU_C2H_EVENT,1,&event))goto io_error;
    rtl_ip.first_tx.event=event;
    if((event&15)!=RTL_CU_C2H_CCX) {
        rtl_ip.first_tx.result=RTL_CU_CCX_OTHER_EVENT;
        return;
    }
    if(!rtl8188cu_read(d,RTL_CU_C2H_EVENT+2,1,&retries) ||
       !rtl8188cu_read(d,RTL_CU_C2H_EVENT+9,1,&status) ||
       !rtl8188cu_write_value(d,RTL_CU_C2H_OWNER,1,0))goto io_error;
    rtl_ip.first_tx.retries=retries&63;
    rtl_ip.first_tx.status=status;
    rtl_ip.first_tx.elapsed_us=now-rtl_ip.first_tx.started;
    rtl_ip.first_tx.result=status&RTL_CU_CCX_ACK?RTL_CU_CCX_ACKED:RTL_CU_CCX_NO_ACK;
    return;
io_error:
    rtl_ip.first_tx.result=RTL_CU_CCX_IO_ERROR;
}

static bool rtl_cu_ip_current(void *context,uint64_t epoch) {
    struct xhci_device *d=context;
    return rtl_ip.live && !rtl_ip.failed && rtl_ip.ops.context==d &&
        epoch==rtl_station.generation && rtl_station.keys.phase==WPA2_CCMP_ACTIVE &&
        rtl_station.keys.generation==epoch && rtl_cu_station_owned(d);
}

static bool rtl_cu_ip_link(void *context,struct net_device_link *link) {
    link->up=rtl_cu_ip_current(context,rtl_station.generation);
    return true;
}

static bool rtl_cu_ip_enqueue(void *context,const uint8_t *frame,uint16_t size) {
    if(!rtl_ip.accepting || !rtl_cu_ip_current(context,rtl_station.generation))return false;
    if(size<14 || size>RTL_CU_ETH_MAX || rtl_ip.queued>=RTL_CU_IP_QUEUE ||
       (!rtl_station.desktop && rtl_ip.posted+rtl_ip.queued>=RTL_CU_DATA_TX_LIMIT)) {
        if(!rtl_station.desktop)rtl_ip.failed=true;
        return false;
    }
    unsigned tail=(rtl_ip.head+rtl_ip.queued)%RTL_CU_IP_QUEUE;
    rtl_ip.queue[tail].bytes=size;
    copy_bytes(rtl_ip.queue[tail].frame,frame,size);
    rtl_ip.queued++;
    return true;
}

static bool rtl_cu_ip_send(void *context,const uint8_t *frame,uint16_t size) {
    uint64_t flags=event_irq_save();
    bool queued=rtl_cu_ip_enqueue(context,frame,size);
    event_irq_restore(flags);
    return queued;
}

static bool rtl_cu_ip_tx(struct xhci_device *d) {
    if(rtl_station.pending)return timer_monotonic_us()-rtl_ip.tx_start<RTL_CU_STATION_TX_US;
    if(rtl_ip.completed!=rtl_ip.posted) {
        rtl_ip.completed=rtl_ip.posted;
        rtl_cu_erase(rtl_station.buffer,RTL_CU_DATA_TX_MAX);
    }
    rtl_cu_ip_trace_poll(d);
    if(!rtl_cu_ip_current(d,rtl_station.generation))return false;
    if(!rtl_ip.queued)return true;
    if(!rtl_cu_station_can_post())return false;
    unsigned head=rtl_ip.head;
    size_t n=rtl_cu_data_tx(&rtl_station.keys,rtl_station.generation,rtl_ip.queue[head].frame,
        rtl_ip.queue[head].bytes,rtl_cu_station_next_sequence(),rtl_ip.plain,rtl_station.buffer);
    rtl_cu_erase(&rtl_ip.queue[head],sizeof rtl_ip.queue[head]);
    if(!n)return false;
    if(!rtl_ip.posted) {
        rtl_cu_ip_trace_first(d,rtl_station.buffer);
        if(!rtl_cu_ip_current(d,rtl_station.generation))return false;
    }
    rtl_ip.head=(head+1)%RTL_CU_IP_QUEUE;
    rtl_ip.queued--;
    rtl_ip.posted++;
    rtl_ip.tx_start=timer_monotonic_us();
    if(rtl_ip.posted==1)rtl_ip.first_tx.started=rtl_ip.first_tx.next=rtl_ip.tx_start;
    rtl_cu_station_post(d,n,false);
    return true;
}

static uint64_t rtl_cu_ip_service(void *context,uint64_t budget) {
    struct xhci_device *d=context;
    if(!budget || !rtl_cu_ip_current(d,rtl_station.generation))return 0;
    for(unsigned i=0;i<32u;i++) {
        struct xhci_trb event;
        if(!poll_event(&event))break;
        dispatch_transfer_event(&event);
    }
    if(!rtl_cu_ip_current(d,rtl_station.generation) || !rtl_cu_ip_tx(d))goto failed;
    if(rtl_passive.pending)return 0;

    const uint8_t *frame;
    size_t size,n=0;
    if(rtl_cu_rx_unwrap(rtl_passive.buffer,rtl_passive.actual,&frame,&size)==RTL_CU_RX_DISPOSITIONS &&
       size>=2 && (frame[1]&0x40)) {
        rtl_ip.protected_rx++;
        n=rtl_cu_data_rx(&rtl_station.keys,rtl_station.generation,frame,size,rtl_ip.plain,rtl_ip.rx);
    }
    rtl_passive.inspected++;
    if(!rtl_cu_rx_drain() || !rtl_cu_ip_current(d,rtl_station.generation) || !rtl_cu_rx_post(d))goto failed;
    if(n) {
        rtl_ip.received++;
        net_handle_ethernet_frame(rtl_ip.rx,(uint16_t)n);
        rtl_cu_erase(rtl_ip.rx,n);
        return 1;
    }
    rtl_ip.ignored++;
    return 0;
failed:
    rtl_ip.failed=true;
    return 0;
}

static uint64_t rtl_cu_ip_poll(void *context,uint64_t budget,bool verbose) {
    (void)verbose;
    uint64_t flags=event_irq_save();
    uint64_t count=rtl_station.desktop && (xhci.sync_users || xhci.command_pending || strict_control.device)?
        0:rtl_cu_ip_service(context,budget);
    event_irq_restore(flags);
    return count;
}

static bool rtl_cu_ip_retry(void *context,uint64_t epoch) {
    return rtl_cu_ip_current(context,epoch) && !rtl_ip.queued && !rtl_station.pending &&
        rtl_ip.posted==1 && rtl_ip.completed==1;
}

static bool rtl_cu_ip_drain(struct xhci_device *d,uint64_t deadline) {
    for(unsigned i=0;i<RTL_CU_IP_DRAIN_SPINS && timer_monotonic_us()<deadline;i++) {
        if(!rtl_cu_ip_current(d,rtl_station.generation))return false;
        if(!rtl_ip.queued && !rtl_station.pending && rtl_ip.posted==rtl_ip.completed)return true;
        /* Drain only OUT ownership. TCP has closed and RX must no longer
         * reach the network stack. OUT remains pinned on any ambiguity. */
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(!rtl_cu_ip_current(d,rtl_station.generation) || !rtl_cu_ip_tx(d))return false;
        __asm__ volatile("pause");
    }
    return false;
}

static bool rtl_cu_ip_register(struct xhci_device *d) {
    if(!net_device_registry_empty() || !rtl_cu_station_owned(d))return false;
    rtl_ip.ops=(struct net_device_ops){
        .name="rtl8188cu-ccmp",.context=d,.priority=1,.send=rtl_cu_ip_send,.poll=rtl_cu_ip_poll,.link=rtl_cu_ip_link,
    };
    copy_bytes(rtl_ip.ops.mac,rtl_station.peer.station,sizeof rtl_ip.ops.mac);
    rtl_ip.registered=net_device_register(&rtl_ip.ops);
    return rtl_ip.registered;
}

static bool rtl_cu_ip_run(struct xhci_device *d) {
    if(!rtl_cu_ip_register(d))return false;
    rtl_ip.io=(struct net_bounded_probe_io){d,rtl_cu_ip_current,rtl_cu_ip_retry};
    rtl_ip.live=rtl_ip.accepting=true;
    rtl_ip.deadline=timer_monotonic_us()+12000000u;
    rtl_ip.opened=rtl_ip.registered && net_init_bounded_probe() &&
        net_bounded_probe_init(&rtl_ip.probe,&rtl_ip.io,rtl_station.generation,rtl_ip.deadline);
    if(rtl_ip.opened) {
        uint32_t remote=0;
        rtl_ip.dhcp=net_dhcp_request_bounded(&rtl_ip.probe)>0;
        rtl_ip.dns=rtl_ip.dhcp && net_dns_resolve_bounded(&rtl_ip.probe,"example.com",&remote)==0;
        if(rtl_ip.dns) {
            int64_t status=net_http_status_bounded(&rtl_ip.probe,remote);
            if(status>=200 && status<=599)rtl_ip.http=(unsigned)status;
        }
    }
    bool finished=net_bounded_probe_finish(&rtl_ip.probe);
    rtl_ip.drained=rtl_ip.registered && rtl_cu_ip_drain(d,rtl_ip.deadline);
    if(rtl_ip.first_tx.result==RTL_CU_CCX_WAITING)
        rtl_ip.first_tx.result=RTL_CU_CCX_MISSING;
    rtl_ip.live=rtl_ip.accepting=false;
    rtl_ip.retired=!rtl_ip.registered || net_device_retire(&rtl_ip.ops);
    rtl_cu_erase(rtl_ip.queue,sizeof rtl_ip.queue);
    rtl_cu_erase(rtl_ip.rx,sizeof rtl_ip.rx);
    rtl_cu_erase(rtl_ip.plain,sizeof rtl_ip.plain);
    if(!rtl_ip.drained || !rtl_ip.retired)rtl_station.failed=true;
    return rtl_ip.http && finished && rtl_ip.drained && rtl_ip.retired && rtl_ip.received && rtl_ip.completed;
}

static void rtl_cu_ip_log(void) {
    console_printf("rtl8188cu-ip: opened=%u tx=%u completed=%u protected_rx=%u received=%u ignored=%u dhcp=%u dns=%u http=%u drained=%u retired=%u\n",
        (uint64_t)rtl_ip.opened,(uint64_t)rtl_ip.posted,(uint64_t)rtl_ip.completed,(uint64_t)rtl_ip.protected_rx,
        (uint64_t)rtl_ip.received,(uint64_t)rtl_ip.ignored,(uint64_t)rtl_ip.dhcp,(uint64_t)rtl_ip.dns,
        (uint64_t)rtl_ip.http,(uint64_t)rtl_ip.drained,(uint64_t)rtl_ip.retired);
    const struct net_dhcp_observation *d=&rtl_ip.probe.dhcp_observation;
    console_printf("rtl8188cu-dhcp: version=%u started=%u finished=%u end=%u discover=%u queued=%u request=%u request_queued=%u elapsed_us=%u\n",
        d->version,d->started,d->finished,d->end_reason,d->discovers,d->discover_queued,
        d->requests,d->request_queued,d->elapsed_us);
    console_printf("rtl8188cu-dhcp-rx: frames=%u ipv4=%u udp=%u dhcp=%u offer=%u accepted_offer=%u ack=%u accepted_ack=%u nak=%u rejected=%u first=%u last=%u mask=%u\n",
        d->frames,d->ipv4,d->udp,d->dhcp,d->offers,d->accepted_offer,d->acks,d->accepted_ack,
        d->naks,d->rejected,d->first_reject,d->last_reject,d->reject_mask);
    console_printf("rtl8188cu-radio-tx: requested=%u usb=%u mpdu=%u sequence=%u fingerprint=%x result=%u owner=%u event=%u status=%u retries=%u polls=%u elapsed_us=%u\n",
        (uint64_t)rtl_ip.first_tx.requested,(uint64_t)(rtl_ip.completed!=0),(uint64_t)rtl_ip.first_tx.mpdu,
        (uint64_t)rtl_ip.first_tx.sequence,rtl_ip.first_tx.fingerprint,(uint64_t)rtl_ip.first_tx.result,
        (uint64_t)rtl_ip.first_tx.initial_owner,(uint64_t)rtl_ip.first_tx.event,(uint64_t)rtl_ip.first_tx.status,
        (uint64_t)rtl_ip.first_tx.retries,(uint64_t)rtl_ip.first_tx.polls,rtl_ip.first_tx.elapsed_us);
}
