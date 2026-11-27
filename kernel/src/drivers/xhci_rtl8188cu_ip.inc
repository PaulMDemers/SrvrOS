/* One boot-only controlled port. No worker or concurrent xHCI consumer:
 * callbacks run under the station's existing IRQ exclusion. */
#define RTL_CU_IP_QUEUE 4u
#define RTL_CU_IP_DRAIN_SPINS 200000u
_Static_assert(RTL_CU_DATA_TX_LIMIT+5u<XHCI_TRB_COUNT-1,"station OUT cannot wrap");
_Static_assert(RTL_CU_DATA_TX_MAX<=PMM_FRAME_SIZE,"station data must fit its pinned DMA page");

static struct {
    struct net_bounded_probe probe;
    struct net_device_ops ops;
    struct { uint16_t bytes; uint8_t frame[RTL_CU_ETH_MAX]; } queue[RTL_CU_IP_QUEUE];
    uint8_t plain[WPA2_CCMP_DATA_MAX],rx[RTL_CU_ETH_MAX];
    unsigned head,queued,posted,completed,received,protected_rx,ignored;
    uint64_t tx_start,deadline;
    bool live,registered,retired,opened,dhcp,dns,drained,failed;
    unsigned http;
} rtl_ip;

static bool rtl_cu_ip_current(void *context,uint64_t epoch) {
    struct xhci_device *d=context;
    return rtl_ip.live && !rtl_ip.failed && rtl_ip.ops.context==d &&
        epoch==RTL_CU_STATION_GENERATION && rtl_station.keys.phase==WPA2_CCMP_ACTIVE &&
        rtl_station.keys.generation==epoch && rtl_cu_station_owned(d);
}

static bool rtl_cu_ip_link(void *context,struct net_device_link *link) {
    link->up=rtl_cu_ip_current(context,RTL_CU_STATION_GENERATION);
    return true;
}

static bool rtl_cu_ip_send(void *context,const uint8_t *frame,uint16_t size) {
    if(!rtl_cu_ip_current(context,RTL_CU_STATION_GENERATION))return false;
    if(size<14 || size>RTL_CU_ETH_MAX || rtl_ip.queued>=RTL_CU_IP_QUEUE ||
       rtl_ip.posted+rtl_ip.queued>=RTL_CU_DATA_TX_LIMIT) {
        rtl_ip.failed=true;
        return false;
    }
    unsigned tail=(rtl_ip.head+rtl_ip.queued)%RTL_CU_IP_QUEUE;
    rtl_ip.queue[tail].bytes=size;
    copy_bytes(rtl_ip.queue[tail].frame,frame,size);
    rtl_ip.queued++;
    return true;
}

static bool rtl_cu_ip_tx(struct xhci_device *d) {
    if(rtl_station.pending)return timer_monotonic_us()-rtl_ip.tx_start<RTL_CU_STATION_TX_US;
    if(rtl_ip.completed!=rtl_ip.posted) {
        rtl_ip.completed=rtl_ip.posted;
        rtl_cu_erase(rtl_station.buffer,RTL_CU_DATA_TX_MAX);
    }
    if(!rtl_ip.queued)return true;
    unsigned head=rtl_ip.head;
    size_t n=rtl_cu_data_tx(&rtl_station.keys,RTL_CU_STATION_GENERATION,rtl_ip.queue[head].frame,
        rtl_ip.queue[head].bytes,rtl_station.sends+1,rtl_ip.plain,rtl_station.buffer);
    rtl_cu_erase(&rtl_ip.queue[head],sizeof rtl_ip.queue[head]);
    if(!n)return false;
    rtl_ip.head=(head+1)%RTL_CU_IP_QUEUE;
    rtl_ip.queued--;
    rtl_ip.posted++;
    rtl_ip.tx_start=timer_monotonic_us();
    rtl_cu_station_post(d,n,false);
    return true;
}

static uint64_t rtl_cu_ip_poll(void *context,uint64_t budget,bool verbose) {
    (void)verbose;
    struct xhci_device *d=context;
    if(!budget || !rtl_cu_ip_current(d,RTL_CU_STATION_GENERATION))return 0;
    for(unsigned i=0;i<32u;i++) {
        struct xhci_trb event;
        if(!poll_event(&event))break;
        dispatch_transfer_event(&event);
    }
    if(!rtl_cu_ip_current(d,RTL_CU_STATION_GENERATION) || !rtl_cu_ip_tx(d))goto failed;
    if(rtl_passive.pending)return 0;

    const uint8_t *frame;
    size_t size,n=0;
    if(rtl_cu_rx_unwrap(rtl_passive.buffer,rtl_passive.actual,&frame,&size)==RTL_CU_RX_DISPOSITIONS &&
       size>=2 && (frame[1]&0x40)) {
        rtl_ip.protected_rx++;
        n=rtl_cu_data_rx(&rtl_station.keys,RTL_CU_STATION_GENERATION,frame,size,rtl_ip.plain,rtl_ip.rx);
    }
    rtl_passive.inspected++;
    if(!rtl_cu_rx_drain() || !rtl_cu_ip_current(d,RTL_CU_STATION_GENERATION) || !rtl_cu_rx_post(d))goto failed;
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

static bool rtl_cu_ip_retry(void *context,uint64_t epoch) {
    return rtl_cu_ip_current(context,epoch) && !rtl_ip.queued && !rtl_station.pending &&
        rtl_ip.posted==1 && rtl_ip.completed==1;
}

static bool rtl_cu_ip_drain(struct xhci_device *d) {
    for(unsigned i=0;i<RTL_CU_IP_DRAIN_SPINS && timer_monotonic_us()<rtl_ip.deadline;i++) {
        if(!rtl_cu_ip_current(d,RTL_CU_STATION_GENERATION))return false;
        if(!rtl_ip.queued && !rtl_station.pending && rtl_ip.posted==rtl_ip.completed)return true;
        /* Drain only OUT ownership. TCP has closed and RX must no longer
         * reach the network stack. OUT remains pinned on any ambiguity. */
        struct xhci_trb event;
        if(poll_event(&event))dispatch_transfer_event(&event);
        if(!rtl_cu_ip_current(d,RTL_CU_STATION_GENERATION) || !rtl_cu_ip_tx(d))return false;
        __asm__ volatile("pause");
    }
    return false;
}

static bool rtl_cu_ip_run(struct xhci_device *d) {
    const struct net_bounded_probe_io io={d,rtl_cu_ip_current,rtl_cu_ip_retry};
    if(!net_device_registry_empty() || !rtl_cu_station_owned(d))return false;
    rtl_ip.ops=(struct net_device_ops){
        .name="rtl8188cu-ccmp",.context=d,.priority=1,.send=rtl_cu_ip_send,.poll=rtl_cu_ip_poll,.link=rtl_cu_ip_link,
    };
    copy_bytes(rtl_ip.ops.mac,rtl_station.peer.station,sizeof rtl_ip.ops.mac);
    rtl_ip.live=true;
    rtl_ip.deadline=timer_monotonic_us()+12000000u;
    rtl_ip.registered=net_device_register(&rtl_ip.ops);
    rtl_ip.opened=rtl_ip.registered && net_init_bounded_probe() &&
        net_bounded_probe_init(&rtl_ip.probe,&io,RTL_CU_STATION_GENERATION,rtl_ip.deadline);
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
    rtl_ip.drained=rtl_ip.registered && rtl_cu_ip_drain(d);
    rtl_ip.live=false;
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
}
