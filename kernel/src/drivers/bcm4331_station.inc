/* Firmware 784.2 station management on the qualified HT PHY and channel. */
enum {
    STATION_RX_SLOTS = 64, STATION_TX_LIMIT = 32, STATION_TX_HEADER = 118,
    STATION_TX_CHANNEL = 1, STATION_TX_STRIDE = 2048, STATION_TX_BUFFER = 67 * PAGE_BYTES,
    STATION_TX_RING = 64 * PAGE_BYTES, DMA_TABLE_END = 0x10000000, DMA_TX_INTERRUPT = 0x20000000,
    DMA_FRAME_END = 0x40000000, DMA_FRAME_START = 0x80000000u,
    TX_DMA_INDEX = 0x204, TX_DMA_LOW = 0x208, TX_DMA_HIGH = 0x20c, TX_DMA_ERROR = 0x214,
    D11_TX_STATUS = 0x170, D11_TX_STATUS_NEXT = 0x174,
    D11_FILTER_SELECT = 0x420, D11_FILTER_DATA = 0x422, D11_TEMPLATE_SELECT = 0x130,
    D11_TEMPLATE_DATA = 0x134,
};

struct bcm4331_security {
    struct wpa2_handshake handshake;
    struct wpa2_ccmp keys;
    uint8_t air[RX_BUFFER_BYTES], plaintext[NET_DEVICE_FRAME_MAX], output[STATION_TX_STRIDE - STATION_TX_HEADER];
};

static const uint8_t station_ssid[] = "Demers2.4";
static const uint8_t station_rsn[] = {48, 20, 1, 0, 0, 15, 172, 4, 1, 0, 0, 15, 172, 4,
    1, 0, 0, 15, 172, 2, 0, 0};

struct bcm4331_station {
    volatile uint32_t *regs;
    uint8_t *rx;
    struct bcm4331_security *security;
    uint64_t tx_ring, tx_buffers;
    uint64_t deadline, tx_deadline, epoch;
    unsigned rx_next, sent, acked;
    uint32_t tx_status, tx_dma_status, tx_dma_error;
    bool fault, tx_pending, secured, provider_live, provider_pinned;
    uint8_t address[6], bssid[6], last_anonce_digest[32];
    struct wpa2_hs_peer peer;
    uint16_t aid, auth_status, assoc_status;
    bool beacon, authenticated, associated;
};

static void radio_put16(uint8_t *out, unsigned value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void radio_put32(uint8_t *out, uint32_t value) {
    radio_put16(out, value);
    radio_put16(out + 2, value >> 16);
}

static bool radio_equal(const uint8_t *a, const uint8_t *b, unsigned length) {
    for (unsigned i = 0; i < length; i++) if (a[i] != b[i]) return false;
    return true;
}

static bool station_peer(const uint8_t *p, unsigned n, struct wpa2_hs_peer *peer) {
    if (!target_beacon(p, n)) return false;
    for (unsigned pos = 36; pos < n - 4;) {
        unsigned len = p[pos + 1];
        if (p[pos] == 48) {
            struct mt76_rsn_profile rsn;
            unsigned size = len + 2;
            if (size > sizeof peer->rsne || mt76_rsn_psk_ccmp(p + pos, size, &rsn) != MT76_RSN_OK) return false;
            peer->rsne_size = size;
            for (unsigned i = 0; i < size; i++) peer->rsne[i] = p[pos + i];
            return true;
        }
        pos += 2 + len;
    }
    return false;
}

static void station_filter(volatile uint32_t *regs, unsigned slot, const uint8_t address[6]) {
    volatile uint16_t *half = (volatile uint16_t *)regs;
    half[D11_FILTER_SELECT / 2] = (uint16_t)(0x20 | slot);
    for (unsigned i = 0; i < 6; i += 2) half[D11_FILTER_DATA / 2] = radio_le16(address + i);
}

static void station_addresses(struct bcm4331_station *station, bool clear) {
    uint8_t addresses[12] = {0};
    if (!clear) for (unsigned i = 0; i < 6; i++) {
        addresses[i] = station->address[i];
        addresses[6 + i] = station->bssid[i];
    }
    station_filter(station->regs, 0, addresses);
    station_filter(station->regs, 3, addresses + 6);
    for (unsigned i = 0; i < sizeof addresses; i += 4) {
        station->regs[D11_TEMPLATE_SELECT / 4] = 0x20 + i;
        station->regs[D11_TEMPLATE_DATA / 4] = radio_le32(addresses + i);
    }
}

static bool station_tx_prepare(struct bcm4331_station *s) {
    volatile uint32_t *r = s->regs + STATION_TX_CHANNEL * DMA_CHANNEL_STRIDE / 4;
    uint32_t low = (uint32_t)s->tx_ring, high = DMA64_PCIE | (uint32_t)(s->tx_ring >> 32);
    for (unsigned i = 0; i < 32; i++) {
        uint32_t status = s->regs[D11_TX_STATUS / 4];
        if (status == UINT32_MAX) return false;
        if (!(status & 1)) break;
        (void)s->regs[D11_TX_STATUS_NEXT / 4];
        if (i == 31) return false;
    }
    r[TX_DMA_LOW / 4] = low;
    r[TX_DMA_HIGH / 4] = high;
    r[TX_DMA_CONTROL / 4] = DMA_ENABLE | RX_DMA_NO_PARITY;
    return r[TX_DMA_LOW / 4] == low && r[TX_DMA_HIGH / 4] == high &&
        r[TX_DMA_INDEX / 4] == 0 && r[TX_DMA_CONTROL / 4] == (DMA_ENABLE | RX_DMA_NO_PARITY);
}

static bool station_tx_post(struct bcm4331_station *s, const uint8_t *frame, unsigned length) {
    if (s->fault || s->tx_pending || s->sent >= STATION_TX_LIMIT || length < 24 || length > STATION_TX_STRIDE - STATION_TX_HEADER) return false;
    volatile uint32_t *r = s->regs + STATION_TX_CHANNEL * DMA_CHANNEL_STRIDE / 4;
    unsigned slot = s->sent * 2;
    unsigned cookie = ((STATION_TX_CHANNEL + 1) << 12) | slot;
    uint64_t phys = s->tx_buffers + s->sent * STATION_TX_STRIDE;
    uint8_t *buffer = pmm_phys_to_virt(phys);
    struct bcm4331_rx_descriptor *desc = pmm_phys_to_virt(s->tx_ring);
    for (unsigned i = 0; i < STATION_TX_HEADER; i++) buffer[i] = 0;
    for (unsigned i = 0; i < length; i++) buffer[STATION_TX_HEADER + i] = frame[i];

    /* Linux b43 uses 1 Mb/s long-preamble CCK for auth and assoc here. */
    unsigned plcp = ((length + 4) * 8 << 16) | 0x040a;
    radio_put32(buffer, 0x1b); /* ACK, long retry, first MSDU and HW sequence. */
    radio_put16(buffer + 4, radio_le16(frame));
    radio_put16(buffer + 8, 0x00c0);
    radio_put16(buffer + 10, 2);
    radio_put16(buffer + 12, 2);
    buffer[18] = 0x0a;
    buffer[21] = BEACON_CHANNEL;
    for (unsigned i = 0; i < 6; i++) buffer[38 + i] = frame[4 + i];
    radio_put32(buffer + 54, plcp);
    radio_put16(buffer + 60, radio_le16(frame + 2));
    radio_put16(buffer + 76, cookie);
    radio_put32(buffer + 112, plcp);
    desc[slot] = (struct bcm4331_rx_descriptor){DMA_FRAME_START, STATION_TX_HEADER,
        (uint32_t)phys, DMA64_PCIE | (uint32_t)(phys >> 32)};
    phys += STATION_TX_HEADER;
    desc[slot + 1] = (struct bcm4331_rx_descriptor){DMA_FRAME_END | DMA_TX_INTERRUPT, length,
        (uint32_t)phys, DMA64_PCIE | (uint32_t)(phys >> 32)};
    __asm__ volatile ("mfence" ::: "memory");
    s->sent++;
    s->tx_pending = true;
    s->tx_deadline = timer_monotonic_us() + 500000;
    r[TX_DMA_INDEX / 4] = (slot + 2) * sizeof *desc;
    return true;
}

static void station_tx_reap(struct bcm4331_station *s) {
    if (!s->tx_pending || s->fault) return;
    volatile uint32_t *r = s->regs + STATION_TX_CHANNEL * DMA_CHANNEL_STRIDE / 4;
    uint32_t status = s->regs[D11_TX_STATUS / 4];
    s->tx_dma_status = r[TX_DMA_STATUS / 4];
    s->tx_dma_error = r[TX_DMA_ERROR / 4];
    if (status == UINT32_MAX || s->tx_dma_status == UINT32_MAX || (s->tx_dma_error & DMA_STATE) ||
        timer_monotonic_us() >= s->tx_deadline) {
        s->fault = true;
        return;
    }
    if (!(status & 1)) return;
    (void)s->regs[D11_TX_STATUS_NEXT / 4];
    s->tx_status = status & 0xffff;
    unsigned cookie = ((STATION_TX_CHANNEL + 1) << 12) | (s->sent * 2 - 2);
    if (status >> 16 != cookie) s->fault = true;
    if (status & 0x40) return;
    unsigned end = s->sent * 2 * sizeof(struct bcm4331_rx_descriptor);
    s->tx_pending = false;
    if (s->fault || (s->tx_dma_status & RX_DMA_POINTER) != end || (status & 0x3e) != 2 || !(status & 0xf000))
        s->fault = true;
    else s->acked++;
}

static bool station_tx(struct bcm4331_station *s, const uint8_t *frame, unsigned length) {
    if (!station_tx_post(s, frame, length)) return false;
    for (unsigned i = 0; s->tx_pending && !s->fault && i < 5000; i++) {
        station_tx_reap(s);
        if (s->tx_pending && !timer_delay_us(100)) break;
    }
    if (s->tx_pending) s->fault = true;
    return !s->fault;
}

/* Copy a completed frame before returning its slot to hardware. */
static unsigned station_rx(struct bcm4331_station *s, uint8_t *out, unsigned capacity) {
    volatile uint32_t *r = s->regs;
    uint32_t status = r[RX_DMA_STATUS / 4], dma_error = r[RX_DMA_ERROR / 4];
    unsigned pointer = status & RX_DMA_POINTER;
    unsigned ready = pointer / sizeof(struct bcm4331_rx_descriptor);
    if (status == UINT32_MAX || (dma_error & DMA_STATE) || ready >= STATION_RX_SLOTS ||
        pointer % sizeof(struct bcm4331_rx_descriptor)) {
        s->fault = true;
        return 0;
    }
    if (ready == s->rx_next) return 0;
    uint8_t *buffer = s->rx + s->rx_next * PAGE_BYTES;
    volatile uint16_t *available = (volatile uint16_t *)buffer;
    if (!*available) return 0;
    __asm__ volatile ("lfence" ::: "memory");
    unsigned n = *available;
    uint32_t mac_status = radio_le32(buffer + 16);
    unsigned channel = radio_le16(buffer + 22);
    unsigned prefix = 6 + (mac_status & 4 ? 2 : 0);
    unsigned length = 0;
    if (!(mac_status & 1) && (channel & 0x1ff8) == BEACON_CHANNEL * 8 && n >= prefix + 4 &&
        n <= RX_BUFFER_BYTES - RX_FRAME_OFFSET && n - prefix <= capacity) {
        const uint8_t *mpdu = buffer + RX_FRAME_OFFSET + prefix;
        n -= prefix;
        if (beacon_fcs(mpdu, n - 4) == radio_le32(mpdu + n - 4)) {
            for (unsigned i = 0; i < n; i++) out[i] = mpdu[i];
            length = n;
        }
    }
    *available = 0;
    __asm__ volatile ("mfence" ::: "memory");
    r[RX_DMA_INDEX / 4] = s->rx_next * sizeof(struct bcm4331_rx_descriptor);
    s->rx_next = (s->rx_next + 1) % STATION_RX_SLOTS;
    uint32_t reason = r[D11_DMA0_REASON / 4];
    if (reason != UINT32_MAX && (reason & D11_DMA0_EVENTS)) r[D11_DMA0_REASON / 4] = reason & D11_DMA0_EVENTS;
    return length;
}

static unsigned station_management(struct bcm4331_station *s, uint8_t *out, unsigned capacity, unsigned subtype) {
    uint64_t start = timer_monotonic_us();
    for (unsigned i = 0; !s->fault && i < 25000; i++) {
        uint64_t now = timer_monotonic_us();
        if (now < start || now >= s->deadline || now - start >= 2500000) break;
        unsigned n = station_rx(s, out, capacity);
        if (subtype == 0x80) {
            if (n >= 24 && (s->epoch == 1 || radio_equal(out + 16, s->bssid, 6)) &&
                station_peer(out, n, &s->peer)) return n;
        } else if (n >= 34 && out[0] == subtype && !(out[1] & 0xc7) && !(out[22] & 15) &&
                radio_equal(out + 4, s->address, 6) && radio_equal(out + 10, s->bssid, 6) &&
                radio_equal(out + 16, s->bssid, 6)) return n;
        if (!n && !timer_delay_us(100)) break;
    }
    return 0;
}

static void station_header(const struct bcm4331_station *s, uint8_t *frame, unsigned subtype) {
    for (unsigned i = 0; i < 24; i++) frame[i] = 0;
    frame[0] = (uint8_t)subtype;
    radio_put16(frame + 2, 314);
    for (unsigned i = 0; i < 6; i++) {
        frame[4 + i] = s->bssid[i];
        frame[10 + i] = s->address[i];
        frame[16 + i] = s->bssid[i];
    }
}

static bool station_join(struct bcm4331_station *s) {
    uint8_t reply[RX_BUFFER_BYTES], request[96] = {0};
    unsigned n = station_management(s, reply, sizeof reply, 0x80);
    if (!n) return false;
    s->beacon = true;
    for (unsigned i = 0; i < 6; i++) {
        s->bssid[i] = s->peer.aa[i] = reply[16 + i];
        s->peer.spa[i] = s->address[i];
    }
    station_addresses(s, false);

    station_header(s, request, 0xb0);
    radio_put16(request + 26, 1); /* Open System transaction one. */
    if (!station_tx(s, request, 30)) return false;
    n = station_management(s, reply, sizeof reply, 0xb0);
    if (!n || radio_le16(reply + 24) != 0 || radio_le16(reply + 26) != 2) return false;
    s->auth_status = radio_le16(reply + 28);
    if (s->auth_status) return false;
    s->authenticated = true;

    station_header(s, request, 0x00);
    radio_put16(request + 24, 0x0411); /* ESS, privacy, short slot. */
    radio_put16(request + 26, 10);
    const uint8_t rates[] = {1, 8, 0x82, 0x84, 0x8b, 0x96, 12, 18, 24, 36, 50, 4, 48, 72, 96, 108};
    unsigned pos = 28;
    request[pos++] = 0;
    request[pos++] = sizeof station_ssid - 1;
    for (unsigned i = 0; i < sizeof station_ssid - 1; i++) request[pos++] = station_ssid[i];
    for (unsigned i = 0; i < sizeof rates; i++) request[pos++] = rates[i];
    for (unsigned i = 0; i < sizeof station_rsn; i++) request[pos++] = station_rsn[i];
    if (!station_tx(s, request, pos)) return false;
    n = station_management(s, reply, sizeof reply, 0x10);
    if (!n) return false;
    s->assoc_status = radio_le16(reply + 26);
    unsigned aid = radio_le16(reply + 28);
    if (s->assoc_status || (aid & 0xc000) != 0xc000 || !(aid & 0x3fff) || (aid & 0x3fff) > 2007) return false;
    s->aid = (uint16_t)(aid & 0x3fff);
    return s->associated = true;
}

#include "bcm4331_wpa.inc"

static bool station_disconnect(struct bcm4331_station *s) {
    uint8_t frame[26];
    station_header(s, frame, 0xa0);
    radio_put16(frame + 24, 3);
    bool ack = station_tx(s, frame, sizeof frame);
    station_addresses(s, true);
    return ack;
}

static bool station_offline(struct bcm4331_station *s, bool wait) {
    struct net_device_link link;
    if (!net_device_link(&link) || link.up) return false;
    if (!wait) return true;
    uint64_t start = timer_monotonic_us();
    for (unsigned i = 0; i < 6000 && !s->fault; i++) {
        uint64_t now = timer_monotonic_us();
        if (now < start) break;
        if (now - start >= 500000) {
            station_scrub(s->security->air, sizeof s->security->air);
            return true;
        }
        station_rx(s, s->security->air, sizeof s->security->air);
        if (!timer_delay_us(100)) break;
    }
    return false;
}

static bool station_reconnect(struct bcm4331_station *s, const uint8_t *key) {
    struct net_device_ops provider = station_provider(s);
    struct net_bounded_probe scopes[2] = {0};
    if (!net_device_registry_empty() || !net_device_register(&provider)) return false;
    bool complete = false;
    for (unsigned i = 0; i < 2; i++) {
        s->epoch = i + 1;
        s->security = &station_security[i];
        s->deadline = timer_monotonic_us() + 15000000;
        s->peer = (struct wpa2_hs_peer){0};
        s->beacon = s->authenticated = s->associated = false;
        s->aid = 0;
        s->auth_status = s->assoc_status = UINT16_MAX;
        bool joined = station_join(s);
        bool secured = joined && station_wpa(s, key);
        bool network = secured && station_gateway(s, &provider, &scopes[i]);
        bool erased = station_key_retire(s);
        bool disassociated = joined && station_disconnect(s);
        bool gap = network && erased && disassociated && station_offline(s, i == 0);
        console_printf("bcm4331-lifetime: epoch=%u beacon=%u authenticated=%u associated=%u auth_status=%u assoc_status=%u secured=%u network=%u keys_erased=%u disassoc_ack=%u offline=%u\n",
            s->epoch, (uint64_t)s->beacon, (uint64_t)s->authenticated, (uint64_t)joined,
            (uint64_t)s->auth_status, (uint64_t)s->assoc_status, (uint64_t)secured, (uint64_t)network,
            (uint64_t)erased, (uint64_t)disassociated, (uint64_t)gap);
        if (!gap) break;
        complete = i == 1;
    }
    s->provider_pinned = !net_device_retire(&provider);
    console_printf("bcm4331-reconnect: complete=%u provider_retired=%u sent=%u acknowledged=%u\n",
        (uint64_t)complete, (uint64_t)!s->provider_pinned, (uint64_t)s->sent, (uint64_t)s->acked);
    return complete && !s->provider_pinned && s->sent == s->acked && !s->tx_pending && !s->fault;
}

static bool station_exchange(struct bcm4331_station *s, const uint8_t *key) {
    s->epoch = 1;
    s->security = &station_security[0];
    s->deadline = timer_monotonic_us() + 15000000;
    bool joined = station_join(s);
    unsigned sent = s->sent, acked = s->acked;
    bool configured = key && joined && station_wpa(s, key) && station_dhcp(s);
    bool keys_closed = !key || station_key_retire(s);
    bool disassociated = joined && station_disconnect(s);
    console_printf("bcm4331-station: beacon=%u authenticated=%u associated=%u auth_status=%u assoc_status=%u tx=%u ack=%u disassociated=%u status=%x dma_status=%x dma_error=%x\n",
        (uint64_t)s->beacon, (uint64_t)s->authenticated, (uint64_t)s->associated, (uint64_t)s->auth_status,
        (uint64_t)s->assoc_status, (uint64_t)sent, (uint64_t)acked, (uint64_t)disassociated,
        (uint64_t)s->tx_status, (uint64_t)s->tx_dma_status, (uint64_t)s->tx_dma_error);
    return key ? configured && keys_closed : joined;
}
