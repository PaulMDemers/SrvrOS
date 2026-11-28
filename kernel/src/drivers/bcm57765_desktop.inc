/* The desktop uses the ordinary polled receive worker while connected.
 * The cable observer owns MDIO and pauses that worker between DHCP leases. */
static struct {
    struct bcm_packet nic;
    struct net_device_ops provider;
    struct net_bounded_probe_io io;
    struct net_bounded_probe scopes[2];
    struct scheduler_wait_queue wait;
    unsigned leases, disconnects;
    bool attempted, unsafe, running, resident, failed;
    volatile bool stop, stopped;
} bcm_desktop;

static bool cable_owner(void *context, uint64_t epoch) {
    const struct bcm_packet *nic = context;
    return (epoch == 1 || epoch == 2) && !bcm_desktop.stop && nic->active && nic->carrier && !nic->fault;
}

static bool cable_retry(void *context, uint64_t epoch) {
    const struct bcm_packet *nic = context;
    return cable_owner(context, epoch) && nic->memory->status.tx_consumer == nic->tx_post;
}

static bool cable_sample(bool *up) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    uint16_t status, aux;
    bool pending = false;
    bool valid = mdio_read(MII_STATUS, &status, &pending) && mdio_read(MII_STATUS, &status, &pending) &&
        mdio_read(MII_AUX_STATUS, &aux, &pending) && status != UINT16_MAX && aux != UINT16_MAX;
    if (valid) {
        /* BMSR establishes loss. During negotiation keep the previous state
         * until both status sources establish the supported link mode. */
        *up = (status & MII_LINK) &&
            ((aux & 0x704) == 0x704 || bcm_desktop.nic.carrier);
    }
    if (pending && (mac_read(MDIO_COMMAND) & MDIO_BUSY)) bcm_desktop.unsafe = true;
    if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
    return valid;
}

static bool cable_connect(void) {
    unsigned epoch = bcm_desktop.leases + 1;
    struct net_bounded_probe *scope = &bcm_desktop.scopes[epoch - 1];
    bool admitted = net_bounded_probe_init_connection(scope, &bcm_desktop.io, &bcm_desktop.provider,
        epoch, timer_monotonic_us() + 5000000);
    int64_t address = admitted ? net_dhcp_request_bounded(scope) : -1;
    struct srv_net_status_info status;
    bool route = address > 0 && net_status_info(&status) == 0 &&
        status.dhcp_configured && status.local_ip == (uint32_t)address && status.router_ip;
    uint64_t elapsed = 0;
    bool gateway = route && net_ping_bounded(scope, status.router_ip, (uint16_t)epoch, &elapsed) == 0;
    bool worker = gateway && net_bounded_connection_start_worker(scope, NULL);
    if (!worker && admitted && !scope->closed) (void)net_bounded_probe_finish(scope);
    const struct net_dhcp_observation *dhcp = &scope->dhcp_observation;
    console_printf("bcm57765-desktop: connected epoch=%u lease=%u offer=%u ack=%u gateway_echo=%u worker=%u dhcp_us=%u\n",
        (uint64_t)epoch, (uint64_t)(address > 0), dhcp->accepted_offer, dhcp->accepted_ack,
        (uint64_t)gateway, (uint64_t)worker, dhcp->elapsed_us);
    if (worker) {
        bcm_desktop.leases++;
        bcm_desktop.resident = true;
    }
    return worker;
}

static bool cable_disconnect(bool link_lost) {
    if (!bcm_desktop.resident) return true;
    bool stopped = net_bounded_connection_stop_worker(&bcm_desktop.scopes[bcm_desktop.leases - 1]);
    if (stopped) bcm_desktop.resident = false;
    struct srv_net_status_info status;
    bool clear = stopped && net_status_info(&status) == 0 && !status.dhcp_configured &&
        !status.local_ip && !status.router_ip && !status.worker_started;
    console_printf("bcm57765-desktop: %s epoch=%u link=%u deconfigured=%u worker_stopped=%u\n",
        link_lost ? "unplug" : "exit", (uint64_t)bcm_desktop.leases,
        (uint64_t)bcm_desktop.nic.carrier, (uint64_t)clear, (uint64_t)stopped);
    if (clear && link_lost) bcm_desktop.disconnects++;
    return clear;
}

static bool cable_stop_requested(void *context) {
    (void)context;
    return bcm_desktop.stop;
}

static bool cable_stopped(void *context) {
    (void)context;
    return bcm_desktop.stopped;
}

static void cable_worker(void *context) {
    (void)context;
    struct bcm_packet *nic = &bcm_desktop.nic;
    unsigned differing = 0;
    if (!cable_connect()) bcm_desktop.failed = true;
    while (!bcm_desktop.stop && !bcm_desktop.failed) {
        bool up;
        if (!cable_sample(&up) || nic->fault) {
            bcm_desktop.failed = true;
            break;
        }
        differing = up == nic->carrier ? 0 : differing + 1;
        if (differing == 3) {
            nic->carrier = up;
            differing = 0;
            if (!up) {
                if (!cable_disconnect(true)) bcm_desktop.failed = true;
            } else if (bcm_desktop.leases < 2 && !cable_connect()) {
                bcm_desktop.failed = true;
            }
        }
        scheduler_wait_timeout(&bcm_desktop.wait, cable_stop_requested, NULL, timer_ticks() + 10);
    }
    if (!cable_disconnect(false)) bcm_desktop.failed = true;
    bcm_desktop.stopped = true;
    scheduler_wake_all(&bcm_desktop.wait);
}

bool bcm57765_desktop_start(void) {
    if (bcm_desktop.attempted) return false;
    bcm_desktop.attempted = true;
    struct bcm_packet *nic = &bcm_desktop.nic;
    enum bcm57765_probe_result state = packet_open(nic);
    if (state != BCM57765_LINK_UP) {
        bcm_desktop.unsafe = state == BCM57765_UNSAFE;
        console_write("bcm57765-desktop: provider unavailable\n");
        return false;
    }
    bool pending = false;
    bool wire = packet_loopback(nic) && packet_wire(&pending);
    bcm_desktop.provider = packet_provider(nic);
    bcm_desktop.io = (struct net_bounded_probe_io){
        .context = nic, .current = cable_owner, .retry_ready = cable_retry,
    };
    bool registered = wire && net_device_register(&bcm_desktop.provider);
    if (registered && scheduler_spawn("bcm57765-link", cable_worker, NULL)) {
        bcm_desktop.running = true;
        return true;
    }
    bool can_reboot;
    bool detached = !registered || net_device_retire(&bcm_desktop.provider);
    bool clean = packet_stop(nic, detached, &can_reboot);
    bcm_desktop.unsafe = !clean || pending;
    console_write("bcm57765-desktop: provider unavailable after controller start\n");
    return false;
}

bool bcm57765_desktop_finish(bool *retired) {
    *retired = !bcm_desktop.unsafe;
    if (!bcm_desktop.running) return false;
    bcm_desktop.stop = true;
    scheduler_wake_all(&bcm_desktop.wait);
    if (!scheduler_wait_timeout(&bcm_desktop.wait, cable_stopped, NULL, timer_ticks() + 700) ||
        !bcm_desktop.stopped) {
        *retired = false;
        console_write("bcm57765-desktop: cable worker retirement unverified\n");
        return false;
    }
    __asm__ volatile("cli" ::: "memory");
    struct bcm_packet *nic = &bcm_desktop.nic;
    packet_tx_reap(nic);
    bool detached = net_device_retire(&bcm_desktop.provider), can_reboot;
    bool clean = packet_stop(nic, detached && !bcm_desktop.resident, &can_reboot);
    *retired = *retired && !bcm_desktop.unsafe && clean;
    __asm__ volatile("sti" ::: "memory");
    console_printf("bcm57765-desktop: final leases=%u disconnects=%u tx_completed=%u rx_completed=%u dropped=%u dma=%s bus_master_off=%u\n",
        (uint64_t)bcm_desktop.leases, (uint64_t)bcm_desktop.disconnects, (uint64_t)nic->sent,
        (uint64_t)nic->received, (uint64_t)nic->dropped, clean ? "released" : "retained", (uint64_t)can_reboot);
    return *retired && !bcm_desktop.failed && !nic->fault && bcm_desktop.leases == 2 && bcm_desktop.disconnects == 1;
}
