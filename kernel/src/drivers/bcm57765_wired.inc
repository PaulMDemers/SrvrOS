/* Normal startup keeps one provider and one receive worker for the whole boot.
 * This service owns PHY sampling and DHCP, and never waits in the desktop. */
static struct {
    struct bcm_packet nic;
    struct net_device_ops provider;
    struct scheduler_wait_queue wait;
    enum bcm57765_wired_trial trial;
    uint64_t sample_due, dhcp_due, traffic_due, traffic_start;
    unsigned leases, losses, windows, attempts, replies;
    uint16_t sequence;
    bool initial_carrier, leased, unsafe, running, failed;
    volatile bool stop, stopped;
} bcm_wired;

static bool wired_sample(bool *up) {
    return cable_sample(false, &bcm_wired.unsafe, up);
}

static bool wired_dhcp_current(void *context) {
    const struct bcm_packet *nic = context;
    return !bcm_wired.stop && nic->active && nic->carrier && !nic->fault;
}

static bool wired_dhcp_retry(void *context) {
    const struct bcm_packet *nic = context;
    packet_barrier();
    return wired_dhcp_current(context) && nic->memory->status.tx_consumer == nic->tx_post;
}

static bool wired_stopping(void *context) {
    (void)context;
    return bcm_wired.stop;
}

static bool wired_stopped(void *context) {
    (void)context;
    return bcm_wired.stopped;
}

static void wired_link_down(void) {
    bcm_wired.nic.carrier = false;
    bool clear = net_managed_link_down(&bcm_wired.provider, false);
    struct srv_net_status_info status;
    clear = clear && net_status_info(&status) == 0 && status.worker_started &&
        !status.dhcp_configured && !status.local_ip && !status.router_ip;
    bcm_wired.losses++;
    console_printf("bcm57765-wired: link-down loss=%u deconfigured=%u worker=%u\n",
        (uint64_t)bcm_wired.losses, (uint64_t)clear, (uint64_t)(clear && status.worker_started));
    bcm_wired.leased = false;
    if (!clear) bcm_wired.failed = true;
}

static void wired_dhcp(void) {
    struct net_dhcp_retry_guard guard = {
        .context = &bcm_wired.nic, .current = wired_dhcp_current, .retry_ready = wired_dhcp_retry,
    };
    int64_t address = net_dhcp_request_discover_retry(&guard);
    if (bcm_wired.stop) return;
    if (address <= 0) {
        if (!net_managed_link_down(&bcm_wired.provider, false)) bcm_wired.failed = true;
        bcm_wired.dhcp_due = timer_monotonic_us() + 3000000;
        console_write("bcm57765-wired: DHCP retry pending\n");
        return;
    }
    bcm_wired.leased = true;
    bcm_wired.leases++;
    bcm_wired.attempts = bcm_wired.replies = 0;
    bcm_wired.traffic_due = bcm_wired.traffic_start = timer_monotonic_us();
    console_printf("bcm57765-wired: lease=%u worker=1\n", (uint64_t)bcm_wired.leases);
}

static void wired_trial_traffic(uint64_t now) {
    struct srv_net_status_info status;
    if (net_status_info(&status) != 0 || !status.router_ip) return;
    bcm_wired.traffic_due = now + 500000;
    bcm_wired.attempts++;
    if (net_ping(status.router_ip, ++bcm_wired.sequence, 25, NULL) == 0) bcm_wired.replies++;
    /* Record one sustained window per lease while probes continue until loss.
     * Leave margin before the desktop's 20-second unplug prompt. */
    if (bcm_wired.attempts == 35) {
        uint64_t elapsed = timer_monotonic_us() - bcm_wired.traffic_start;
        bool passed = elapsed >= 17000000 && bcm_wired.replies >= 30;
        if (passed) bcm_wired.windows++;
        console_printf("bcm57765-wired: traffic lease=%u elapsed_us=%u probes=%u replies=%u result=%s\n",
            (uint64_t)bcm_wired.leases, elapsed, (uint64_t)bcm_wired.attempts,
            (uint64_t)bcm_wired.replies, passed ? "PASS" : "INCOMPLETE");
    }
}

static void wired_worker(void *context) {
    (void)context;
    unsigned differing = 0;
    while (!bcm_wired.stop && !bcm_wired.failed) {
        uint64_t now = timer_monotonic_us();
        if (now >= bcm_wired.sample_due) {
            bcm_wired.sample_due = now + 100000;
            bool up;
            if (!wired_sample(&up) || bcm_wired.nic.fault) {
                bcm_wired.failed = true;
                break;
            }
            differing = up == bcm_wired.nic.carrier ? 0 : differing + 1;
            if (differing == 3) {
                differing = 0;
                if (up) {
                    bcm_wired.nic.carrier = true;
                    bcm_wired.dhcp_due = now + 1000000;
                    console_write("bcm57765-wired: link-up speed_mbps=1000 duplex=full\n");
                } else wired_link_down();
            }
        }
        if (bcm_wired.nic.carrier && !differing && !bcm_wired.failed) {
            if (!bcm_wired.leased && now >= bcm_wired.dhcp_due) wired_dhcp();
            else if (bcm_wired.leased && bcm_wired.trial && now >= bcm_wired.traffic_due) wired_trial_traffic(now);
        }
        scheduler_wait_timeout(&bcm_wired.wait, wired_stopping, NULL, timer_ticks() + 5);
    }
    bcm_wired.nic.carrier = false;
    if (!net_managed_link_down(&bcm_wired.provider, true)) bcm_wired.unsafe = true;
    if (bcm_wired.failed) console_write("bcm57765-wired: link service stopped after fault\n");
    bcm_wired.stopped = true;
    scheduler_wake_all(&bcm_wired.wait);
}

bool bcm57765_wired_start(enum bcm57765_wired_trial trial) {
    if (!trial && !display_profile_is_pro81()) return false;
    struct bcm_packet *nic = &bcm_wired.nic;
    enum bcm57765_probe_result result = packet_open(nic, false);
    if (result != BCM57765_LINK_DOWN && result != BCM57765_LINK_UP) {
        bcm_wired.unsafe = result == BCM57765_UNSAFE;
        console_write("bcm57765-wired: provider unavailable\n");
        return false;
    }
    bcm_wired.trial = trial;
    bcm_wired.provider = packet_provider(nic);
    bool sampled = wired_sample(&bcm_wired.initial_carrier);
    nic->carrier = bcm_wired.initial_carrier;
    bcm_wired.dhcp_due = timer_monotonic_us() + 1000000;
    bool registered = sampled && net_device_register(&bcm_wired.provider);
    bool managed = registered && net_start_managed_worker(&bcm_wired.provider);
    if (managed && scheduler_spawn("bcm57765-link", wired_worker, NULL)) {
        bcm_wired.running = true;
        console_printf("bcm57765-wired: startup carrier=%u worker=1 trial=%u\n",
            (uint64_t)bcm_wired.initial_carrier, (uint64_t)trial);
        return true;
    }
    if (managed) (void)net_managed_link_down(&bcm_wired.provider, true);
    bool detached = !registered || net_device_retire(&bcm_wired.provider), fenced;
    if (!packet_stop(nic, detached, &fenced)) bcm_wired.unsafe = true;
    console_write("bcm57765-wired: provider unavailable after controller start\n");
    return false;
}

bool bcm57765_wired_finish(bool *retired) {
    *retired = !bcm_wired.unsafe;
    if (!bcm_wired.running) return false;
    bcm_wired.stop = true;
    scheduler_wake_all(&bcm_wired.wait);
    if (!scheduler_wait_timeout(&bcm_wired.wait, wired_stopped, NULL, timer_ticks() + 700) || !bcm_wired.stopped) {
        *retired = false;
        return false;
    }
    __asm__ volatile("cli" ::: "memory");
    struct bcm_packet *nic = &bcm_wired.nic;
    packet_tx_reap(nic);
    bool detached = net_device_retire(&bcm_wired.provider), fenced;
    bool clean = packet_stop(nic, detached, &fenced);
    if (fenced) bcm_wired.running = false;
    *retired = !bcm_wired.unsafe && clean;
    __asm__ volatile("sti" ::: "memory");
    console_printf("bcm57765-wired: final leases=%u losses=%u windows=%u tx_completed=%u rx_completed=%u dropped=%u dma=%s bus_master_off=%u\n",
        (uint64_t)bcm_wired.leases, (uint64_t)bcm_wired.losses, (uint64_t)bcm_wired.windows,
        (uint64_t)nic->sent, (uint64_t)nic->received, (uint64_t)nic->dropped,
        clean ? "released" : "retained", (uint64_t)fenced);
    bool connected = bcm_wired.trial == BCM57765_WIRED_CONNECTED;
    unsigned leases = connected ? 1 : 4;
    return *retired && !bcm_wired.failed && !nic->fault && connected == bcm_wired.initial_carrier &&
        bcm_wired.leases == leases && bcm_wired.losses == leases - 1 && bcm_wired.windows == leases;
}

/* Reboot can originate inside a user address space. Fence DMA here, keeping
 * those mappings pinned until the reset. The trial uses full retirement. */
bool bcm57765_wired_fence(void) {
    if (!bcm_wired.running) return !bcm_wired.unsafe;
    bcm_wired.stop = true;
    bcm_wired.nic.carrier = false;
    bool fenced;
    (void)packet_stop(&bcm_wired.nic, false, &fenced);
    if (fenced) bcm_wired.running = false;
    return fenced;
}
