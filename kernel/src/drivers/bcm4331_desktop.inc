/* A CPU0 worker owns the radio from pre-userspace admission through desktop
 * shutdown. Settings only queues requests. The resident network worker owns
 * packet delivery after DHCP and parks before the station disassociates. */
struct bcm4331_desktop_link {
    struct bcm4331_security security;
    struct net_bounded_probe scope;
    struct net_bounded_probe_io io;
    bool worker;
};

static struct {
    struct scheduler_wait_queue wait;
    const struct pci_device *pci;
    uint64_t owner;
    unsigned connects, disconnects;
    volatile bool stop, done;
    enum bcm4331_desktop_trial trial;
    bool starting, published, passed, unsafe;
} bcm_desktop;

struct bcm4331_signal_trial {
    uint64_t before, lost, returned, next_echo;
    unsigned issued, replies;
};

static void desktop_wifi_observe_signal(struct bcm4331_station *s, struct bcm4331_signal_trial *trial) {
    enum { QUIET_US = 4000000, SETTLE_US = 2000000, ECHO_INTERVAL_US = 1500000, ECHOES = 5 };
    uint64_t now = timer_monotonic_us();
    uint64_t beacon = s->signal_beacon_at;
    if (!beacon) return;
    if (!trial->before) {
        trial->before = beacon;
        console_printf("bcm4331-signal: event=tracking epoch=%u at_us=%u\n", s->epoch, beacon);
    }
    if (!trial->lost && now >= beacon && now - beacon >= QUIET_US) {
        trial->lost = beacon;
        console_printf("bcm4331-signal: event=lost epoch=%u quiet_us=%u beacons=%u\n",
            s->epoch, now - beacon, s->signal_beacons);
    }
    if (trial->lost && !trial->returned && beacon > trial->lost) {
        trial->returned = beacon;
        trial->next_echo = beacon + SETTLE_US;
        console_printf("bcm4331-signal: event=returned epoch=%u gap_us=%u beacons=%u\n",
            s->epoch, beacon - trial->lost, s->signal_beacons);
    }
    if (!trial->returned || now < trial->next_echo || now < beacon || now - beacon >= QUIET_US ||
        trial->issued == ECHOES) return;
    struct srv_net_status_info status;
    bool worker = !net_status_info(&status) && status.worker_started && status.dhcp_configured && status.router_ip;
    trial->issued++;
    bool reply = worker && !net_ping(status.router_ip, (uint16_t)(0x4700 + trial->issued), 50, NULL);
    if (reply) trial->replies++;
    trial->next_echo = now + ECHO_INTERVAL_US;
    console_printf("bcm4331-signal-echo: epoch=%u attempt=%u worker=%u reply=%u\n",
        s->epoch, (uint64_t)trial->issued, (uint64_t)worker, (uint64_t)reply);
}

static bool desktop_wifi_present(void *context) {
    const struct bcm4331_station *s = context;
    return s->desktop && !bcm_desktop.stop;
}

static bool desktop_wifi_due(void *context) { return timer_ticks() >= *(const uint64_t *)context; }

static void desktop_wifi_turn(void) {
    uint64_t tick = timer_ticks() + 1;
    if (!scheduler_wait_timeout(&bcm_desktop.wait, desktop_wifi_due, &tick, tick)) scheduler_yield();
}

static bool desktop_wifi_take(struct bcm4331_station *s, struct wifi_service_command *command) {
    uint64_t flags = station_irq_save();
    bool taken = wifi_service_take(bcm_desktop.owner, s, command);
    station_irq_restore(flags);
    return taken;
}

static bool desktop_wifi_finish_request(struct bcm4331_station *s, const struct wifi_service_command *command,
        bool connected, bool success) {
    uint64_t flags = station_irq_save();
    bool ok = !connected && success ? wifi_service_rearm_connection(bcm_desktop.owner, s, command, s->epoch + 1) :
        wifi_service_finish(bcm_desktop.owner, s, command, success);
    station_irq_restore(flags);
    return ok;
}

static bool desktop_wifi_gateway(struct bcm4331_station *s) {
    enum { ECHOES = 14 };
    struct srv_net_status_info status;
    if (net_status_info(&status) || !status.router_ip || !status.dhcp_configured || !status.worker_started) return false;
    unsigned replies = 0;
    for (unsigned i = 0; i < ECHOES && !bcm_desktop.stop; i++) {
        if (net_ping(status.router_ip, (uint16_t)(0x4200 + s->epoch * 32 + i), 50, NULL)) break;
        replies++;
        desktop_wifi_turn();
    }
    console_printf("bcm4331-desktop-gateway: epoch=%u issued=%u replies=%u\n",
        s->epoch, (uint64_t)ECHOES, (uint64_t)replies);
    return replies == ECHOES;
}

/* Poll data through the normal worker after the lease is installed. */
static bool desktop_wifi_connect(struct bcm4331_station *s, struct bcm4331_desktop_link *link,
        const struct net_device_ops *provider, const uint8_t *key) {
    s->epoch++;
    s->signal_beacon_at = s->signal_beacons = 0;
    s->security = &link->security;
    s->deadline = timer_monotonic_us() + 15000000;
    s->peer = (struct wpa2_hs_peer){0};
    s->beacon = s->authenticated = s->associated = false;
    s->aid = 0;
    s->auth_status = s->assoc_status = UINT16_MAX;
    bool joined = station_join(s);
    bool secured = joined && station_wpa(s, key);
    bool lease = false;
    if (secured) {
        s->provider_live = true;
        link->io = (struct net_bounded_probe_io){s, station_dhcp_current, station_dhcp_retry};
        bool admitted = net_bounded_probe_init_connection(&link->scope, &link->io, provider, s->epoch,
            timer_monotonic_us() + 10000000);
        lease = admitted && net_dhcp_request_bounded_paced(&link->scope) > 0;
        link->worker = lease && net_bounded_connection_start_worker(&link->scope, NULL);
    }
    console_printf("bcm4331-desktop-connect: epoch=%u associated=%u wpa2=%u dhcp=%u worker=%u\n",
        s->epoch, (uint64_t)joined, (uint64_t)secured, (uint64_t)lease, (uint64_t)link->worker);
    return link->worker && (!bcm_desktop.trial || desktop_wifi_gateway(s));
}

static bool desktop_wifi_disconnect(struct bcm4331_station *s, struct bcm4331_desktop_link *link) {
    bool closed = !link->worker;
    uint64_t deadline = timer_ticks() + 300;
    while (!closed) {
        uint64_t flags = station_irq_save();
        station_tx_reap(s);
        closed = !s->tx_pending && net_bounded_connection_stop_worker(&link->scope);
        station_irq_restore(flags);
        if (closed || timer_ticks() >= deadline) break;
        desktop_wifi_turn();
    }
    if (!closed) return false;
    if (link->scope.identity && !link->scope.closed && !net_bounded_probe_disconnect(&link->scope))
        net_bounded_probe_finish(&link->scope);
    s->provider_live = false;
    bool erased = station_key_retire(s);
    uint64_t now = timer_monotonic_us();
    bool absent = s->signal_trial && s->signal_beacon_at && now >= s->signal_beacon_at &&
        now - s->signal_beacon_at >= 4000000;
    bool ack = !s->associated || (!absent && station_disconnect(s));
    if (absent) station_addresses(s, true);
    s->associated = false;
    struct srv_net_status_info status;
    bool cleared = net_status_info(&status) == 0 && !status.dhcp_configured && !status.local_ip &&
        !status.router_ip && !status.worker_started;
    console_printf("bcm4331-desktop-disconnect: epoch=%u stopped=1 ip_cleared=%u keys_erased=%u disassoc_ack=%u\n",
        s->epoch, (uint64_t)cleared, (uint64_t)erased, (uint64_t)ack);
    s->security = NULL;
    kfree(link);
    return cleared && erased && (ack || absent);
}

static bool station_desktop(struct bcm4331_station *s, const uint8_t *key) {
    struct net_device_ops provider = station_provider(s);
    struct bcm4331_signal_trial signal = {0};
    s->signal_trial = bcm_desktop.trial == BCM4331_DESKTOP_SIGNAL_LOSS;
    uint8_t beacon[RX_BUFFER_BYTES];
    s->epoch = 1;
    s->deadline = timer_monotonic_us() + 3000000;
    if (!station_management(s, beacon, sizeof beacon, 0x80) || bcm_desktop.stop) return false;
    for (unsigned i = 0; i < 6; i++) s->bssid[i] = beacon[16 + i];
    s->epoch = 0;
    const struct wifi_service_provider wifi = {s, "Broadcom BCM4331", desktop_wifi_present};
    uint64_t flags = station_irq_save();
    bool registered = net_device_registry_empty() && net_device_register(&provider);
    if (registered) bcm_desktop.owner = wifi_service_publish_fixed(&wifi, 1, BEACON_CHANNEL,
        station_ssid, sizeof station_ssid - 1, 1);
    bcm_desktop.published = bcm_desktop.owner != 0;
    station_irq_restore(flags);
    scheduler_wake_all(&bcm_desktop.wait);
    struct bcm4331_desktop_link *link = NULL;
    bool healthy = bcm_desktop.published;
    while (healthy && !bcm_desktop.stop && !s->fault) {
        struct wifi_service_command command;
        if (!desktop_wifi_take(s, &command)) {
            if (s->signal_trial && link && s->epoch == 1)
                desktop_wifi_observe_signal(s, &signal);
            if (!link) for (unsigned i = 0; i < STATION_RX_SLOTS; i++) {
                unsigned previous = s->rx_next;
                station_rx(s, beacon, sizeof beacon);
                if (s->rx_next == previous) break;
            }
            desktop_wifi_turn();
            continue;
        }
        bool connect = command.operation == SRV_WIFI_CONNECT;
        bool success = false;
        if (connect && !link && s->epoch < UINT64_MAX - 1) {
            link = kcalloc(1, sizeof *link);
            success = link && desktop_wifi_connect(s, link, &provider, key);
            if (success) bcm_desktop.connects++;
        } else if (!connect && link) {
            success = desktop_wifi_disconnect(s, link);
            if (!s->security) link = NULL;
            if (success) bcm_desktop.disconnects++;
        }
        bool published = desktop_wifi_finish_request(s, &command, connect, success);
        healthy = success && published;
        console_printf("bcm4331-desktop-command: serial=%u epoch=%u operation=%u success=%u published=%u\n",
            command.serial, s->epoch, (uint64_t)command.operation, (uint64_t)success, (uint64_t)published);
    }
    if (link) {
        healthy = desktop_wifi_disconnect(s, link) && healthy;
        s->provider_pinned = s->security != NULL;
    }
    flags = station_irq_save();
    wifi_service_remove(bcm_desktop.owner);
    if (registered && !net_device_retire(&provider)) s->provider_pinned = true;
    station_irq_restore(flags);
    console_printf("bcm4331-desktop-final: connect=%u disconnect=%u sent=%u acked=%u provider_retired=%u\n",
        (uint64_t)bcm_desktop.connects, (uint64_t)bcm_desktop.disconnects, (uint64_t)s->sent,
        (uint64_t)s->acked, (uint64_t)!s->provider_pinned);
    if (s->signal_trial)
        console_printf("bcm4331-signal-final: epoch=%u tracked=%u loss=%u restored=%u attempts=%u replies=%u\n",
            s->epoch, (uint64_t)(signal.before != 0), (uint64_t)(signal.lost != 0),
            (uint64_t)(signal.returned != 0), (uint64_t)signal.issued, (uint64_t)signal.replies);
    bool goal = s->signal_trial ? bcm_desktop.connects == 1 && !bcm_desktop.disconnects &&
        signal.issued == 5 && signal.replies == signal.issued :
        bcm_desktop.connects == 2 && bcm_desktop.disconnects == 1;
    bcm_desktop.passed = healthy && !s->fault && !s->provider_pinned && goal;
    return healthy && !s->fault && !s->provider_pinned;
}

static void desktop_wifi_worker(void *unused) {
    (void)unused;
    uint64_t flags = station_irq_save();
    enum bcm4331_probe_result result = bcm4331_probe(BCM4331_STATION_DESKTOP);
    bcm_desktop.unsafe = result == BCM4331_UNSAFE;
    if (!bcm_desktop.unsafe) bcm_desktop.pci = NULL;
    bcm_desktop.done = true;
    station_irq_restore(flags);
    scheduler_wake_all(&bcm_desktop.wait);
}

static bool desktop_wifi_ready(void *unused) {
    (void)unused;
    return bcm_desktop.published || bcm_desktop.done;
}

static bool desktop_wifi_done(void *unused) {
    (void)unused;
    return bcm_desktop.done;
}

bool bcm4331_desktop_configured(void) {
    return display_profile_is_pro81() && station_key() != NULL;
}

bool bcm4331_desktop_start(enum bcm4331_desktop_trial trial) {
    if (bcm_desktop.starting || !bcm4331_desktop_configured()) return false;
    bcm_desktop.trial = trial;
    bcm_desktop.starting = scheduler_spawn("bcm4331-wifi", desktop_wifi_worker, NULL);
    if (!bcm_desktop.starting) return false;
    if (!scheduler_wait_timeout(&bcm_desktop.wait, desktop_wifi_ready, NULL, timer_ticks() + 600) ||
        !desktop_wifi_ready(NULL)) bcm_desktop.stop = true;
    return bcm_desktop.published && !bcm_desktop.done && !bcm_desktop.stop;
}

bool bcm4331_desktop_finish(bool *retired) {
    bcm_desktop.stop = true;
    if (bcm_desktop.starting) {
        scheduler_wake_all(&bcm_desktop.wait);
        scheduler_wait_timeout(&bcm_desktop.wait, desktop_wifi_done, NULL, timer_ticks() + 1400);
    }
    *retired = (!bcm_desktop.starting || bcm_desktop.done) && !bcm_desktop.unsafe;
    return *retired && bcm_desktop.passed;
}

/* Emergency reset may originate in a desktop process. Keep DMA memory
 * pinned and fence PCI through configuration space without using MMIO. */
bool bcm4331_desktop_fence(void) {
    bcm_desktop.stop = true;
    const struct pci_device *p = bcm_desktop.pci;
    if (!p) return !bcm_desktop.unsafe;
    uint16_t command = pci_read_config16(p, PCI_COMMAND);
    if (command == UINT16_MAX) return false;
    command &= ~PCI_BUS_MASTER;
    pci_write_config16(p, PCI_COMMAND, command);
    return pci_read_config16(p, PCI_COMMAND) == command && pci_transactions_drained(p);
}
