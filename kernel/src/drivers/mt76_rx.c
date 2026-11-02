#include <srvros/mt76_rx.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16;
}

enum mt76_rx_status mt76_rx_usb_view(const uint8_t *p, size_t size,
                                    struct mt76_rx_view *out) {
    if (!out) return MT76_RX_INVALID;
    *out = (struct mt76_rx_view){0};
    /* Linux v7.0: mt76u_get_rx_entry_len, mt76x02_rxwi and
     * mt76x02_mac_process_rx. Use byte loads, including unaligned transfers. */
    if (!p || size < 40) return MT76_RX_INVALID;
    size_t dma = le16(p);
    if (!dma || (dma & 3) || dma > size - 4 || dma < 32)
        return MT76_RX_INVALID;
    const uint8_t *wi = p + 4;
    uint32_t info = le32(wi);
    size_t mpdu = (le32(wi + 4) >> 16) & 0x3fff;
    size_t pad = (info & (1u << 14)) ? 2 : 0;
    if (mpdu < 24 || mpdu > dma - 32 || pad > dma - 32 - mpdu)
        return MT76_RX_INVALID;
    /* Initial passive decoder deliberately has no encrypted/PN, fragmented,
     * aggregate or variable data/control-header processing. Never pass those
     * bytes to a management parser using a guessed header length. */
    if (info & ((7u << 8) | (1u << 3))) return MT76_RX_INVALID;
    if (info & ((1u << 11) | (1u << 15) | (1u << 16) | (7u << 19)))
        return MT76_RX_UNSUPPORTED;
    const uint8_t *h = wi + 32;
    uint16_t fc = le16(h), seq = le16(h + 22);
    if ((fc & 3) || (seq & 15)) return MT76_RX_INVALID;
    if ((fc & 0x000c) || (fc & 0xc700)) return MT76_RX_UNSUPPORTED;
    struct mt76_rx_view v = {
        .header = h, .body = h + 24 + pad, .body_size = mpdu - 24,
        .mpdu_size = mpdu, .consumed = dma + 4, .rxinfo = info,
        .frame_control = fc, .rate = le16(wi + 10), .sequence = seq >> 4
    };
    for (unsigned i = 0; i < 4; i++) v.rssi_raw[i] = wi[12 + i];
    *out = v;
    return MT76_RX_OK;
}

static bool unicast(const uint8_t *p) {
    unsigned any = 0;
    for (unsigned i = 0; i < 6; i++) any |= p[i];
    return any && !(p[0] & 1);
}

enum mt76_rx_status mt76_rx_beacon(const uint8_t *p, size_t size,
                                  unsigned channel, struct mt76_rx_beacon *out) {
    if (!out) return MT76_RX_INVALID;
    *out = (struct mt76_rx_beacon){0};
    if (channel < 1 || channel > 11) return MT76_RX_INVALID;
    struct mt76_rx_view v;
    enum mt76_rx_status status = mt76_rx_usb_view(p, size, &v);
    if (status != MT76_RX_OK) return status;
    unsigned subtype = v.frame_control & 0xf0;
    if (subtype != 0x80 && subtype != 0x50) return MT76_RX_UNSUPPORTED;
    if (v.body_size < 12 || !unicast(v.header + 10) || !unicast(v.header + 16))
        return MT76_RX_INVALID;
    struct mt76_rx_beacon b = {
        .channel = (uint8_t)channel, .probe_response = subtype == 0x50,
        .sequence = v.sequence, .interval = le16(v.body + 8),
        .capabilities = le16(v.body + 10), .hidden = true
    };
    if (!b.interval) return MT76_RX_INVALID;
    /* Initial observation supports infrastructure APs only. */
    if (!(b.capabilities & 1) || (b.capabilities & 2)) return MT76_RX_UNSUPPORTED;
    b.privacy = (b.capabilities & 16) != 0;
    for (unsigned i = 0; i < 8; i++) b.timestamp |= (uint64_t)v.body[i] << (8 * i);
    for (unsigned i = 0; i < 6; i++) {
        b.bssid[i] = v.header[16 + i]; b.transmitter[i] = v.header[10 + i];
    }
    for (unsigned i = 0; i < 4; i++) b.rssi_raw[i] = v.rssi_raw[i];
    bool ssid = false, rates = false;
    /* Entire IE stream must validate before publishing even a partial SSID. */
    for (size_t pos = 12; pos < v.body_size;) {
        if (v.body_size - pos < 2) return MT76_RX_INVALID;
        unsigned id = v.body[pos], n = v.body[pos + 1]; pos += 2;
        if (n > v.body_size - pos) return MT76_RX_INVALID;
        const uint8_t *ie = v.body + pos;
        if (id == 0) {
            if (ssid || n > 32) return MT76_RX_INVALID;
            ssid = true; b.ssid_size = (uint8_t)n;
            for (unsigned i = 0; i < n; i++) {
                b.ssid[i] = ie[i]; if (ie[i]) b.hidden = false;
            }
        } else if (id == 1) {
            if (rates || !n || n > 8) return MT76_RX_INVALID;
            rates = true;
        } else if (id == 3) {
            if (b.ds_present || n != 1 || !ie[0] || ie[0] > 14)
                return MT76_RX_INVALID;
            b.ds_present = true;
            if (ie[0] != channel) return MT76_RX_CHANNEL_MISMATCH;
        } else if (id == 48) {
            if (b.rsn_present || n < 2 || le16(ie) != 1) return MT76_RX_INVALID;
            b.rsn_present = true;
        } else if (id == 221 && n >= 4 && ie[0] == 0 && ie[1] == 0x50 &&
                   ie[2] == 0xf2 && ie[3] == 1) {
            if (b.wpa_present) return MT76_RX_INVALID;
            b.wpa_present = true;
        }
        pos += n;
    }
    if (!ssid || !rates) return MT76_RX_INVALID;
    *out = b;
    return MT76_RX_OK;
}

unsigned mt76_rx_advertised_channel(const uint8_t *p,size_t size) {
    /* The ordinary parser and its tuned-channel rejection remain unchanged.
     * Revalidate the ENTIRE frame: a DS byte before malformed trailing IEs is
     * not a valid observation. Bound the diagnostic to eleven US candidates. */
    for(unsigned channel=1;channel<=11;channel++) {
        struct mt76_rx_beacon b={0};
        enum mt76_rx_status status=mt76_rx_beacon(p,size,channel,&b);
        bool found=status==MT76_RX_OK && b.ds_present;
        for(unsigned i=0;i<sizeof b;i++)((volatile uint8_t *)&b)[i]=0;
        if(found)return channel;
    }
    return 0;
}
