/* Software WPA2/CCMP over the station's single D11 transmit queue. */
static struct {
    struct wpa2_handshake handshake;
    struct wpa2_ccmp keys;
    uint8_t air[RX_BUFFER_BYTES], plaintext[NET_DEVICE_FRAME_MAX], output[STATION_TX_STRIDE - STATION_TX_HEADER];
} bcm4331_security;

static const uint8_t station_snap[] = {0xaa, 0xaa, 3, 0, 0, 0};

static const uint8_t *station_key(void) {
    const struct initramfs_file *file = initramfs_find("/cfg/bcm4331-station.key");
    static const uint8_t magic[] = {'S', 'R', 'W', 'P', 'A', '5', 0, 1};
    if (!file || !file->data || file->size != 105 || file->mode != 0600) return 0;
    const uint8_t *p = file->data;
    if (!radio_equal(p, magic, sizeof magic) || p[8] != sizeof station_ssid - 1 ||
        !radio_equal(p + 9, station_ssid, p[8])) return 0;
    for (unsigned i = 9 + p[8]; i < 41; i++) if (p[i]) return 0;
    return p;
}

static void station_scrub(void *memory, unsigned bytes) {
    volatile uint8_t *p = memory;
    while (bytes--) *p++ = 0;
}

static const uint8_t *station_eapol(struct bcm4331_station *s, unsigned type, unsigned *length) {
    uint8_t *f = bcm4331_security.air;
    uint64_t start = timer_monotonic_us();
    for (unsigned poll = 0; poll < 30000 && !s->fault; poll++) {
        uint64_t now = timer_monotonic_us();
        if (now < start || now >= s->deadline || now - start >= 3000000) break;
        unsigned n = station_rx(s, f, sizeof bcm4331_security.air);
        if (n >= 24 + 8 + 99 + 4 && (f[1] & ~0x38u) == 2 && (f[0] == 0x08 || f[0] == 0x88) &&
            !(f[22] & 15) && radio_equal(f + 4, s->address, 6) && radio_equal(f + 10, s->bssid, 6) &&
            radio_equal(f + 16, s->bssid, 6)) {
            unsigned offset = f[0] == 0x88 ? 26 : 24;
            const uint8_t *llc = f + offset;
            const uint8_t *packet = llc + 8;
            if (n >= offset + 8 + 99 + 4 && radio_equal(llc, station_snap, sizeof station_snap) &&
                llc[6] == 0x88 && llc[7] == 0x8e && packet[1] == 3 &&
                ((unsigned)packet[5] << 8 | packet[6]) == type) {
                *length = n - offset - 8 - 4;
                return packet;
            }
        }
        if (!n && !timer_delay_us(100)) break;
    }
    return 0;
}

static bool station_wpa_send(struct bcm4331_station *s) {
    uint8_t f[24 + 8 + 121];
    station_header(s, f, 0x08);
    f[1] = 1;
    for (unsigned i = 0; i < sizeof station_snap; i++) f[24 + i] = station_snap[i];
    f[30] = 0x88;
    f[31] = 0x8e;
    unsigned length = wpa2_hs_emit(&bcm4331_security.handshake, 1, f + 32, 121);
    bool delivered = length && station_tx(s, f, 32 + length);
    station_scrub(f, sizeof f);
    return length && wpa2_hs_tx_result(&bcm4331_security.handshake, 1,
        delivered ? WPA2_HS_CONFIRMED : WPA2_HS_UNCERTAIN);
}

static bool station_wpa(struct bcm4331_station *s, const uint8_t *key) {
    struct wpa2_handshake *hs = &bcm4331_security.handshake;
    struct wpa2_ccmp *ccmp = &bcm4331_security.keys;
    unsigned length = 0;
    const uint8_t *m1 = station_eapol(s, 0x008a, &length);
    bool first = false, second = false, third = false, installed = false, fourth = false;
    if (m1) {
        /* This Sandy Bridge CPU has no RDRAND. Bind a fresh Ubuntu-generated
         * per-trial seed to the actual authenticator nonce and local time. */
        uint8_t material[72], snonce[32];
        uint64_t now = timer_monotonic_us();
        for (unsigned i = 0; i < 32; i++) {
            material[i] = key[73 + i];
            material[32 + i] = m1[17 + i];
        }
        for (unsigned i = 0; i < 8; i++) material[64 + i] = (uint8_t)(now >> (8 * i));
        sha256_digest(material, sizeof material, snonce);
        bool started = wpa2_hs_start(hs, &s->peer, key + 41, snonce, 1);
        station_scrub(material, sizeof material);
        station_scrub(snonce, sizeof snonce);
        first = started && wpa2_hs_receive(hs, 1, s->bssid, s->address, m1, length);
        second = first && station_wpa_send(s);
    }
    const uint8_t *m3 = second ? station_eapol(s, 0x13ca, &length) : 0;
    if (m3) {
        struct wpa2_hs_keys export = {0};
        third = wpa2_hs_receive(hs, 1, s->bssid, s->address, m3, length);
        installed = third && wpa2_hs_take_keys(hs, 1, &export) && wpa2_ccmp_install(ccmp, hs, &export);
        station_scrub(&export, sizeof export);
        if (installed) {
            fourth = wpa2_hs_install_result(hs, 1, WPA2_HS_CONFIRMED) && station_wpa_send(s);
            s->secured = fourth && wpa2_ccmp_activate(ccmp, hs);
        }
    }
    console_printf("bcm4331-wpa2: m1_seen=%u m1=%u m2=%u m3_seen=%u m3=%u keys=%u m4=%u secured=%u rx_result=%u\n",
        (uint64_t)(m1 != 0), (uint64_t)first, (uint64_t)second, (uint64_t)(m3 != 0), (uint64_t)third,
        (uint64_t)installed, (uint64_t)fourth, (uint64_t)s->secured, (uint64_t)hs->last_rx);
    return s->secured;
}

static bool station_net_send(void *context, const uint8_t *frame, uint16_t length) {
    struct bcm4331_station *s = context;
    station_tx_reap(s);
    if (!s->provider_live || !s->secured || s->fault || s->tx_pending || length < 14 || length > NET_DEVICE_FRAME_MAX ||
        !radio_equal(frame + 6, s->address, 6) ||
        !((frame[12] == 8 && frame[13] == 0) || (frame[12] == 8 && frame[13] == 6))) return false;
    uint8_t header[24], *plain = bcm4331_security.plaintext;
    station_header(s, header, 0x08);
    header[1] = 1;
    for (unsigned i = 0; i < 6; i++) {
        header[16 + i] = frame[i];
        plain[i] = station_snap[i];
    }
    for (unsigned i = 12; i < length; i++) plain[i - 6] = frame[i];
    unsigned n = wpa2_ccmp_seal(&bcm4331_security.keys, 1, header, plain, length - 6,
        bcm4331_security.output, sizeof bcm4331_security.output);
    station_scrub(plain, length - 6);
    return n && station_tx_post(s, bcm4331_security.output, n);
}

static uint64_t station_net_poll(void *context, uint64_t budget, bool verbose) {
    (void)verbose;
    struct bcm4331_station *s = context;
    if (!s->provider_live || !s->secured || s->fault) return 0;
    station_tx_reap(s);
    unsigned received = 0;
    for (unsigned i = 0; i < budget && i < 16 && !s->fault; i++) {
        unsigned previous = s->rx_next;
        uint8_t *air = bcm4331_security.air, *plain = bcm4331_security.plaintext;
        unsigned n = station_rx(s, air, sizeof bcm4331_security.air);
        if (!n && s->rx_next == previous) break;
        if (n < 4) continue;
        n = wpa2_ccmp_open(&bcm4331_security.keys, 1, air, n - 4, plain, sizeof bcm4331_security.plaintext);
        if (n >= 8 && n <= NET_DEVICE_FRAME_MAX - 6 && radio_equal(plain, station_snap, sizeof station_snap) &&
            plain[6] == 8 && (plain[7] == 0 || plain[7] == 6)) {
            uint8_t ethernet[NET_DEVICE_FRAME_MAX];
            for (unsigned j = 0; j < 6; j++) {
                ethernet[j] = air[4 + j];
                ethernet[6 + j] = air[16 + j];
            }
            for (unsigned j = 6; j < n; j++) ethernet[6 + j] = plain[j];
            station_scrub(plain, n);
            net_handle_ethernet_frame(ethernet, n + 6);
            received++;
        } else station_scrub(plain, n);
    }
    return received;
}

static bool station_net_link(void *context, struct net_device_link *out) {
    const struct bcm4331_station *s = context;
    *out = (struct net_device_link){.up = s->provider_live && s->secured && !s->fault};
    return true;
}

static bool station_dhcp_current(void *context, uint64_t epoch) {
    const struct bcm4331_station *s = context;
    return epoch == 1 && s->provider_live && s->secured && !s->fault;
}

static bool station_dhcp_retry(void *context, uint64_t epoch) {
    const struct bcm4331_station *s = context;
    return station_dhcp_current(context, epoch) && !s->tx_pending && s->sent == s->acked;
}

static bool station_dhcp(struct bcm4331_station *s) {
    struct net_device_ops provider = {.name = "bcm4331", .context = s, .priority = 100,
        .send = station_net_send, .poll = station_net_poll, .link = station_net_link};
    for (unsigned i = 0; i < sizeof provider.mac; i++) provider.mac[i] = s->address[i];
    s->provider_live = true;
    bool registered = net_device_registry_empty() && net_device_register(&provider);
    bool selected = registered && net_init_bounded_probe() && net_device_selected_is(&provider);
    struct net_bounded_probe probe = {0};
    const struct net_bounded_probe_io io = {.context = s, .current = station_dhcp_current, .retry_ready = station_dhcp_retry};
    __asm__ volatile ("sti" ::: "memory");
    bool admitted = selected && timer_wait_ticks_bounded(2, 100000000) &&
        net_bounded_probe_init(&probe, &io, 1, timer_monotonic_us() + 5000000);
    int64_t lease = admitted ? net_dhcp_request_bounded(&probe) : -1;
    bool finished = admitted && net_bounded_probe_finish(&probe);
    struct srv_net_status_info info;
    bool configured = finished && lease > 0 && net_status_info(&info) == 0 && info.dhcp_configured &&
        info.local_ip == (uint32_t)lease;
    __asm__ volatile ("cli" ::: "memory");
    station_tx_reap(s);
    s->provider_live = false;
    s->provider_pinned = registered && !net_device_retire(&provider);
    const struct net_dhcp_observation *dhcp = &probe.dhcp_observation;
    console_printf("bcm4331-dhcp: registered=%u selected=%u admitted=%u configured=%u discover=%u offer=%u request=%u ack=%u provider_retired=%u end=%u polls=%u\n",
        (uint64_t)registered, (uint64_t)selected, (uint64_t)admitted, (uint64_t)configured, dhcp->discover_queued,
        dhcp->accepted_offer, dhcp->request_queued, dhcp->accepted_ack, (uint64_t)!s->provider_pinned, dhcp->end_reason, dhcp->polls);
    return configured && dhcp->accepted_offer && dhcp->accepted_ack && !s->provider_pinned &&
        !s->fault && !s->tx_pending && s->sent == s->acked;
}

static bool station_key_retire(struct bcm4331_station *s) {
    struct wpa2_ccmp *ccmp = &bcm4331_security.keys;
    struct wpa2_handshake *hs = &bcm4331_security.handshake;
    s->secured = false;
    bool erased = ccmp->phase == WPA2_CCMP_EMPTY || (wpa2_ccmp_retire(ccmp, 1) && wpa2_ccmp_erased(ccmp, 1));
    bool closed = hs->phase == WPA2_HS_EMPTY || wpa2_hs_retire(hs, 1, erased);
    station_scrub(bcm4331_security.air, sizeof bcm4331_security.air);
    station_scrub(bcm4331_security.plaintext, sizeof bcm4331_security.plaintext);
    station_scrub(bcm4331_security.output, sizeof bcm4331_security.output);
    console_printf("bcm4331-wpa2: keys_erased=%u handshake_closed=%u\n", (uint64_t)erased, (uint64_t)closed);
    return erased && closed;
}
