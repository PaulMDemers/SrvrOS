/* System queues commands. Only this owner calls the station routines or
 * permits the resident network worker to poll the endpoints. */
static struct {
    struct xhci_device *device;
    void *context;
    bool (*begin)(void *),(*running)(void *);
    struct scheduler_wait_queue wait;
    uint64_t owner;
    unsigned connected,disconnected;
    bool gate,worker,return_safe,unsafe;
} rtl_desktop;

static bool rtl_cu_desktop_present(void *context) {
    const struct xhci_device *d=rtl_desktop.device;
    return context==&rtl_desktop && d && d->present && !d->removed && !d->retiring &&
        !d->slot_disabled && xhci.operational && !xhci.stopping && wifi_hub_path_present(d);
}

static bool rtl_cu_desktop_driver_bound(const struct xhci_device *d) {
    return d==rtl_desktop.device && rtl_desktop.owner && !rtl_desktop.unsafe &&
        !d->control_failed && !d->ep0_quarantined && rtl_cu_desktop_present(&rtl_desktop);
}

static bool rtl_cu_desktop_due(void *context) { return timer_ticks()>=*(const uint64_t *)context; }

static void rtl_cu_desktop_turn(void) {
    uint64_t next=timer_ticks()+1;
    if(!scheduler_wait_timeout(&rtl_desktop.wait,rtl_cu_desktop_due,&next,next))scheduler_yield();
}

static bool rtl_cu_desktop_gate_begin(void) {
    uint64_t flags=event_irq_save();
    bool ok=!xhci.sync_users && !xhci.poll_users && !xhci.command_pending &&
        !strict_control.device && !scheduler_io_active() && scheduler_io_begin();
    if(ok)rtl_desktop.gate=true;
    event_irq_restore(flags);
    return ok;
}

static void rtl_cu_desktop_gate_end(void) {
    uint64_t flags=event_irq_save();
    if(rtl_desktop.gate){scheduler_io_end();rtl_desktop.gate=false;}
    event_irq_restore(flags);
}

static bool rtl_cu_desktop_gate_wait(void) {
    for(unsigned attempt=0;attempt<200;attempt++) {
        if(rtl_cu_desktop_gate_begin())return true;
        rtl_cu_desktop_turn();
    }
    return false;
}

static bool rtl_cu_desktop_io_safe(void) {
    return !rtl_desktop.gate && !xhci.sync_users && !xhci.poll_users &&
        !xhci.command_pending && !strict_control.device;
}

static bool rtl_cu_desktop_take(struct wifi_service_command *command) {
    uint64_t flags=event_irq_save();
    bool ok=wifi_service_take(rtl_desktop.owner,&rtl_desktop,command);
    event_irq_restore(flags);
    return ok;
}

static bool rtl_cu_desktop_publish_result(const struct wifi_service_command *command,bool ok) {
    uint64_t flags=event_irq_save();
    bool reported=wifi_service_finish(rtl_desktop.owner,&rtl_desktop,command,ok);
    event_irq_restore(flags);
    return reported && ok;
}

static bool rtl_cu_desktop_rx_ready(struct xhci_device *d) {
    if(!rtl_cu_rx_drain() || rtl_station.failed || !rtl_cu_desktop_present(&rtl_desktop))return false;
    if(rtl_passive.pending)return true;
    rtl_cu_rx_inspect();
    return rtl_cu_rx_post(d);
}

static bool rtl_cu_desktop_bootstrap(struct xhci_device *d) {
    if(!rtl_ip.registered && !rtl_cu_ip_register(d))return false;
    rtl_ip.io=(struct net_bounded_probe_io){d,rtl_cu_ip_current,rtl_cu_ip_retry};
    rtl_ip.live=rtl_ip.accepting=true;
    rtl_ip.deadline=timer_monotonic_us()+12000000u;
    rtl_ip.opened=net_bounded_probe_init_connection(&rtl_ip.probe,&rtl_ip.io,&rtl_ip.ops,
        rtl_station.generation,rtl_ip.deadline);
    rtl_ip.dhcp=rtl_ip.opened && net_dhcp_request_bounded(&rtl_ip.probe)>0;
    rtl_desktop.worker=rtl_ip.dhcp && net_bounded_connection_start_worker(&rtl_ip.probe,rtl_cu_desktop_io_safe);
    if(!rtl_desktop.worker && rtl_ip.probe.identity && !rtl_ip.probe.closed)
        (void)net_bounded_probe_finish(&rtl_ip.probe);
    return rtl_desktop.worker;
}

static bool rtl_cu_desktop_verify_data(void) {
    struct srv_net_status_info status;
    uint32_t resolved=0;
    bool dhcp=net_status_info(&status)==0 && status.dhcp_configured && status.local_ip && status.router_ip;
    bool ping=dhcp && net_ping(status.router_ip,(uint16_t)(0x2400u+rtl_station.generation),200,NULL)==0;
    rtl_ip.dns=dhcp && net_dns_resolve("example.com",&resolved)==0 && resolved;
    console_printf("rtl8188cu-desktop-data: generation=%u dhcp=%u ping=%u dns=%u\n",
        rtl_station.generation,(uint64_t)dhcp,(uint64_t)ping,(uint64_t)rtl_ip.dns);
    return ping && rtl_ip.dns;
}

/* Stop issuing data before leaving the AP. A failed proof keeps ownership;
 * pending TRBs and a live network worker are never overwritten. */
static bool rtl_cu_desktop_quiet(struct xhci_device *d) {
    for(unsigned attempt=0;attempt<200;attempt++) {
        if(!rtl_cu_desktop_gate_wait())break;
        uint64_t flags=event_irq_save();
        bool live=!rtl_ip.live || rtl_cu_ip_current(d,rtl_station.generation);
        rtl_ip.accepting=false;
        rtl_ip.drained=live && (!rtl_ip.live || rtl_cu_ip_drain(d,timer_monotonic_us()+250000u));
        bool stopped=rtl_ip.drained && (!rtl_desktop.worker || net_bounded_connection_stop_worker(&rtl_ip.probe));
        if(stopped){rtl_desktop.worker=false;rtl_ip.live=false;}
        if(!rtl_ip.drained)rtl_desktop.unsafe=true;
        event_irq_restore(flags);
        if(stopped || rtl_desktop.unsafe)return stopped;
        rtl_cu_desktop_gate_end();
        rtl_cu_desktop_turn();
    }
    rtl_desktop.unsafe=true;
    return false;
}

static bool rtl_cu_desktop_next(struct xhci_device *d) {
    if(!rtl_cu_station_owned(d) || !rtl_station.leave || !rtl_station.keys_erased ||
       !rtl_ip.probe.disconnected || rtl_station.pending || rtl_station.sends!=rtl_station.completed ||
       !rtl_cu_write_checked(d,RTL_CU_TX_PAUSE,1,0xff))return false;
    rtl_station.enabled=false;
    if(!rtl_cu_desktop_rx_ready(d))return false;
    rtl_cu_erase(&rtl_station.handshake,sizeof rtl_station.handshake);
    rtl_cu_erase(&rtl_station.keys,sizeof rtl_station.keys);
    rtl_station.auth_out=(struct rtl_cu_auth_trace){0};
    rtl_station.sends=rtl_station.completed=0;
    rtl_station.reply_status=rtl_station.aid=rtl_station.last_rx=0;
    rtl_station.auth_sent=rtl_station.auth_ok=rtl_station.assoc_sent=rtl_station.assoc_ok=0;
    rtl_station.m1=rtl_station.m2=rtl_station.m3=rtl_station.m4=rtl_station.leave=0;
    rtl_station.ready=rtl_station.keys_erased=rtl_station.qos=false;
    const struct net_device_ops ops=rtl_ip.ops;
    rtl_cu_erase(&rtl_ip,sizeof rtl_ip);
    rtl_ip.ops=ops;
    rtl_ip.registered=true;
    rtl_station.generation++;
    return true;
}

static bool rtl_cu_desktop_connection(struct xhci_device *d,struct xhci_device *hub,
    const uint8_t *pmk,const struct wifi_service_command *connect,struct wifi_service_command *disconnect) {
    uint64_t flags=event_irq_save();
    bool joined=rtl_cu_desktop_rx_ready(d) && rtl_cu_station_join(d,hub,pmk);
    bool live=joined && rtl_cu_desktop_bootstrap(d);
    event_irq_restore(flags);
    rtl_cu_desktop_gate_end();
    /* Validate one gateway echo and DNS response before reporting Connected. */
    bool data=live && rtl_cu_desktop_verify_data();
    bool published=rtl_cu_desktop_publish_result(connect,live);
    if(published)rtl_desktop.connected++;
    while(published && rtl_cu_desktop_present(&rtl_desktop) && rtl_cu_ip_current(d,rtl_station.generation) &&
          rtl_desktop.running(rtl_desktop.context) && !rtl_cu_desktop_take(disconnect))
        rtl_cu_desktop_turn();
    bool quiet=rtl_cu_desktop_quiet(d);
    flags=event_irq_save();
    if(!quiet)rtl_station.failed=true;
    bool closed=rtl_cu_station_close_link(d,published && quiet);
    if(closed && disconnect->serial)rtl_desktop.disconnected++;
    console_printf("rtl8188cu-desktop-connection: generation=%u joined=%u data=%u disconnected=%u worker_stopped=%u keys_erased=%u\n",
        rtl_station.generation,(uint64_t)joined,(uint64_t)data,(uint64_t)(closed && disconnect->serial),
        (uint64_t)!rtl_desktop.worker,(uint64_t)rtl_station.keys_erased);
    event_irq_restore(flags);
    return closed && data;
}

static bool rtl_cu_desktop_window(struct xhci_device *d,struct xhci_device *hub,const uint8_t *pmk) {
    static const uint8_t ssid[]=RTL_CU_STATION_SSID;
    rtl_desktop.device=d;
    const struct wifi_service_provider ops={&rtl_desktop,"Realtek USB Wi-Fi",rtl_cu_desktop_present};
    uint64_t flags=event_irq_save();
    rtl_desktop.owner=wifi_service_publish_fixed(&ops,ssid,sizeof ssid-1,RTL_CU_STATION_CHANNEL,
        rtl_passive.matches,rtl_station.generation);
    event_irq_restore(flags);
    rtl_cu_desktop_gate_end();
    bool started=rtl_desktop.owner && rtl_desktop.begin(rtl_desktop.context),verified=true;
    bool sealed=false;
    while(started && rtl_desktop.running(rtl_desktop.context) && !rtl_desktop.unsafe &&
          rtl_cu_desktop_present(&rtl_desktop)) {
        struct wifi_service_command connect={0},disconnect={0};
        if(sealed || !rtl_cu_desktop_take(&connect)){rtl_cu_desktop_turn();continue;}
        if(!rtl_cu_desktop_gate_wait()){verified=false;break;}
        bool passed=connect.operation==SRV_WIFI_CONNECT && rtl_cu_desktop_connection(d,hub,pmk,&connect,&disconnect);
        verified=verified && passed;
        flags=event_irq_save();
        bool rearmed=passed && disconnect.serial && rtl_station.generation==1 &&
            rtl_cu_desktop_next(d) && wifi_service_rearm_connection(rtl_desktop.owner,&rtl_desktop,
                &disconnect,rtl_station.generation);
        if(!rearmed) {
            wifi_service_seal_connection(rtl_desktop.owner,&rtl_desktop,passed && disconnect.serial);
            sealed=true;
        }
        event_irq_restore(flags);
        if(!rtl_desktop.unsafe)rtl_cu_desktop_gate_end();
    }
    if(!rtl_desktop.gate && !rtl_cu_desktop_gate_wait())rtl_desktop.unsafe=true;
    flags=event_irq_save();
    wifi_service_remove(rtl_desktop.owner);
    rtl_desktop.owner=0;
    if(rtl_ip.registered && !rtl_desktop.worker && !rtl_ip.live) {
        rtl_ip.retired=net_device_retire(&rtl_ip.ops);
        if(!rtl_ip.retired)rtl_desktop.unsafe=true;
    }
    event_irq_restore(flags);
    return verified && rtl_desktop.connected==2 && rtl_desktop.disconnected==2;
}
