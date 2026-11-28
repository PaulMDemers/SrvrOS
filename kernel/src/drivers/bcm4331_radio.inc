/* BCM4331 package 9, HT PHY rev 1, D11 rev 29, 2.4 GHz channel 10 only. */
enum {
    PIO_RX_DIRECT = 0x220, PIO_DIRECT_FIFO = 0x100, RX_DMA_ENABLE = 1,
    RX_DMA_STATUS = 0x230,
    PIO_RX_CONTROL = 0x238, PIO_RX_DATA = 0x23c, PIO_FRAME_READY = 1, PIO_DATA_READY = 2,
    RADIO_SELECTOR = 0x3d8, RADIO_DATA = 0x3da, PHY_SELECTOR = 0x3fc, PHY_DATA = 0x3fe,
    PHY_RF_COMMAND = 0x810, PHY_RF_POWER = 2,
    MAC_BEACON_PROMISCUOUS = 0x100000, MAC_PROMISCUOUS = 0x1000000,
    MAC_AWAKE = 0x4000000, MAC_DISCARD_PMQ = 0x40000000,
    RX_HEADER_BYTES = 24, RX_FRAME_LIMIT = 0x700, BEACON_CHANNEL = 10,
    RECIPE_HEADER = 16, RECIPE_OP_BYTES = 8, RECIPE_OPS = 8161,
};

struct radio_assets {
    const uint8_t *initial, *band, *profile;
};

static uint16_t radio_le16(const uint8_t *p) { return (uint16_t)p[1] << 8 | p[0]; }
static uint32_t radio_le32(const uint8_t *p) { return (uint32_t)radio_le16(p + 2) << 16 | radio_le16(p); }

static bool apply_init_registers(volatile uint32_t *regs, const uint8_t *data, unsigned size, unsigned count) {
    unsigned index = 8;
    if (data[0] != 'i' || data[1] != 1 || data[2] || data[3] || data[4] || data[5] ||
        ((unsigned)data[6] << 8 | data[7]) != count) return false;
    for (unsigned op = 0; op < count; op++) {
        if (size - index < 2) return false;
        unsigned selector = (unsigned)data[index] << 8 | data[index + 1];
        unsigned offset = selector & 0x7fff, width = selector & 0x8000 ? 4 : 2;
        index += 2;
        if (offset < 0x100 || offset > 0x700 || offset % width || size - index < width ||
            offset == D11_MAC_CONTROL || (offset >= 0x200 && offset < 0x400)) return false;
        uint32_t value = 0;
        for (unsigned i = 0; i < width; i++) value = value << 8 | data[index++];
        if (width == 4) regs[offset / 4] = value;
        else ((volatile uint16_t *)regs)[offset / 2] = (uint16_t)value;
    }
    return index == size && !regs[D11_IRQ_MASK / 4] && !(regs[D11_MAC_CONTROL / 4] & D11_MAC_ENABLED);
}

static bool run_radio_phase(volatile uint32_t *regs, const uint8_t *ops, unsigned count) {
    volatile uint16_t *half = (volatile uint16_t *)regs;
    for (unsigned i = 0; i < count; i++, ops += RECIPE_OP_BYTES) {
        unsigned operation = radio_le16(ops), offset = operation & 0xfff, delay = radio_le16(ops + 2);
        uint32_t value = radio_le32(ops + 4);
        if (delay > 20000 || (delay && !timer_delay_us(delay))) return false;
        if (operation == (0x4000 | RADIO_DATA) && (value == 1 || value == 2)) {
            unsigned attempt;
            for (attempt = 0; attempt < 300; attempt++) {
                uint16_t observed = half[RADIO_DATA / 2];
                if (observed == UINT16_MAX) return false;
                if (observed & value) break;
                if (!timer_delay_us(100)) return false;
            }
            if (attempt == 300) {
                console_printf("bcm4331: radio calibration timeout selector=%x bit=%x\n",
                    (uint64_t)half[RADIO_SELECTOR / 2], (uint64_t)value);
                return false;
            }
        } else if (operation == (0x8000 | offset) && offset == D11_CLOCK_CONTROL) {
            /* The sampled clock word includes read-only and transient bits.
             * Reproduce the requested HT/PLL state without writing those bits. */
            uint32_t clock = regs[offset / 4];
            if (clock == UINT32_MAX) return false;
            const uint32_t requests = D11_FORCE_HT | D11_PLL_REQUEST;
            regs[offset / 4] = (clock & ~requests) | (value & requests);
        } else if (operation == (0x8000 | D11_SHM_CONTROL) && value == 0x10028) {
            regs[D11_SHM_CONTROL / 4] = value;
        } else if (operation == offset && value <= UINT16_MAX &&
            (offset == PHY_SELECTOR || offset == PHY_DATA || offset == RADIO_SELECTOR || offset == RADIO_DATA ||
             offset == D11_SHM_DATA || offset == 0x492 || offset == 0x62e || offset == 0x630)) {
            half[offset / 2] = (uint16_t)value;
            if (offset == RADIO_SELECTOR) (void)half[offset / 2];
        } else return false;
    }
    return true;
}

static uint32_t beacon_fcs(const uint8_t *p, unsigned size) {
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < size; i++) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return ~crc;
}

static bool target_beacon(const uint8_t *p, unsigned size) {
    static const char target[] = "Demers2.4";
    if (size < 40 || p[0] != 0x80 || (p[1] & 0xc7) || (p[10] & 1) || (p[22] & 15) ||
        !(p[34] & 1) || (p[34] & 2)) return false;
    unsigned station = 0;
    for (unsigned i = 0; i < 6; i++) {
        if (p[4 + i] != 0xff || p[10 + i] != p[16 + i]) return false;
        station |= p[10 + i];
    }
    if (!station) return false;
    unsigned end = size - 4;
    bool ssid = false, channel = false;
    for (unsigned index = 36; index < end;) {
        if (end - index < 2) return false;
        unsigned tag = p[index++], len = p[index++];
        if (len > end - index) return false;
        if (tag == 0) {
            if (ssid || len != sizeof target - 1) return false;
            for (unsigned i = 0; i < len; i++) if (p[index + i] != (uint8_t)target[i]) return false;
            ssid = true;
        }
        if (tag == 3 || tag == 61) {
            if (len != (tag == 3 ? 1u : 22u) || p[index] != BEACON_CHANNEL) return false;
            channel = true;
        }
        index += len;
    }
    return ssid && channel && beacon_fcs(p, end) == radio_le32(p + end);
}

static void fifo_read(volatile uint32_t *reg, uint8_t *out, unsigned size) {
    for (unsigned done = 0; done < size;) {
        uint32_t value = *reg;
        for (unsigned byte = 0; byte < 4 && done < size; byte++, done++) out[done] = (uint8_t)(value >> (byte * 8));
    }
}

static unsigned listen_for_beacons(volatile uint32_t *regs) {
    unsigned frames = 0, valid = 0, target = 0, timeouts = 0, last_rx_channel = 0;
    uint64_t start = timer_monotonic_us();
    for (unsigned poll = 0; poll < 60000 && frames < 1024 && target < 3; poll++) {
        if (timer_monotonic_us() - start >= 6000000) break;
        uint32_t control = regs[PIO_RX_CONTROL / 4];
        if (control == UINT32_MAX) break;
        if (!(control & PIO_FRAME_READY)) {
            if (!timer_delay_us(100)) break;
            continue;
        }
        regs[PIO_RX_CONTROL / 4] = PIO_FRAME_READY;
        if (!wait_d11_register(regs, PIO_RX_CONTROL, PIO_DATA_READY, PIO_DATA_READY)) {
            timeouts++;
            break;
        }
        uint8_t header[RX_HEADER_BYTES], frame[RX_FRAME_LIMIT];
        fifo_read(&regs[PIO_RX_DATA / 4], header, sizeof header);
        unsigned length = radio_le16(header), rx_channel = radio_le16(header + 22);
        frames++;
        last_rx_channel = rx_channel;
        uint32_t status = radio_le32(header + 16);
        if (length < 6 || length > sizeof frame || (status & 1)) {
            regs[PIO_RX_CONTROL / 4] = PIO_DATA_READY;
            continue;
        }
        fifo_read(&regs[PIO_RX_DATA / 4], frame, length);
        valid++;
        /* In PIO the Linux receive path inserts its own alignment padding.
         * On the wire the hardware FIFO begins with the six-byte PLCP. */
        if (!(rx_channel & 0x1800) && ((rx_channel >> 3) & 0xff) == BEACON_CHANNEL &&
            target_beacon(frame + 6, length - 6)) target++;
    }
    console_printf("bcm4331-radio: channel=10 pio_frames=%u valid_frames=%u target_beacons=%u timeouts=%u rx_channel=%x\n",
        (uint64_t)frames, (uint64_t)valid, (uint64_t)target, (uint64_t)timeouts, (uint64_t)last_rx_channel);
    return target;
}

static bool passive_radio_trial(volatile uint32_t *regs, const struct radio_assets *assets, bool *retired) {
    const uint8_t *profile = assets->profile;
    unsigned powerup = radio_le32(profile + 4), phy = radio_le32(profile + 8), tune = radio_le32(profile + 12);
    if (radio_le32(profile) != 0x30315448 || powerup != 318 || phy != 7658 || tune != 185) return false;
    bool initial = apply_init_registers(regs, assets->initial, 2830, 499) &&
        apply_init_registers(regs, assets->band, 178, 35);
    const uint8_t *ops = profile + RECIPE_HEADER;
    bool powered = initial && run_radio_phase(regs, ops, powerup);
    bool phy_ready = powered && run_radio_phase(regs, ops + powerup * RECIPE_OP_BYTES, phy);
    bool tuned = phy_ready && run_radio_phase(regs, ops + (powerup + phy) * RECIPE_OP_BYTES, tune);
    console_printf("bcm4331-radio: initvals=%u radio=%u phy=%u channel10=%u\n",
        (uint64_t)initial, (uint64_t)powered, (uint64_t)phy_ready, (uint64_t)tuned);
    uint32_t mac = regs[D11_MAC_CONTROL / 4];
    uint32_t rx_control = regs[PIO_RX_DIRECT / 4];
    console_printf("bcm4331-radio: receive_preflight mac=%x rxctl=%x rxstatus=%x\n",
        (uint64_t)mac, (uint64_t)rx_control, (uint64_t)regs[RX_DMA_STATUS / 4]);
    unsigned matches = 0;
    if (tuned && mac != UINT32_MAX && !(mac & D11_MAC_ENABLED) && rx_control != UINT32_MAX) {
        volatile uint16_t *half = (volatile uint16_t *)regs;
        regs[D11_SHM_CONTROL / 4] = (D11_SHM_SHARED << 16) | (0x16 / 4);
        half[D11_SHM_DATA_ODD / 2] = 29;
        regs[D11_SHM_CONTROL / 4] = (D11_SHM_SHARED << 16) | (0xa0 / 4);
        half[D11_SHM_DATA / 2] = BEACON_CHANNEL;
        regs[D11_SHM_CONTROL / 4] = (D11_SHM_SHARED << 16) | (0x34 / 4);
        half[D11_SHM_DATA / 2] = 0;
        /* A fresh D11 reset can still expose default receive-control bits.
         * Change only the RX DMA and direct-mode bits with the MAC disabled. */
        uint32_t idle = rx_control & ~(RX_DMA_ENABLE | PIO_DIRECT_FIFO);
        regs[PIO_RX_DIRECT / 4] = idle;
        bool dma_disabled = !(regs[PIO_RX_DIRECT / 4] & RX_DMA_ENABLE) &&
            wait_d11_register(regs, RX_DMA_STATUS, 0xf0000000, 0);
        if (dma_disabled) regs[PIO_RX_DIRECT / 4] = idle | PIO_DIRECT_FIFO;
        bool direct = dma_disabled && (regs[PIO_RX_DIRECT / 4] & (RX_DMA_ENABLE | PIO_DIRECT_FIFO)) == PIO_DIRECT_FIFO;
        console_printf("bcm4331-radio: rx_dma_disabled=%u direct_fifo=%u\n", (uint64_t)dma_disabled, (uint64_t)direct);
        if (direct) {
            regs[D11_IRQ_REASON / 4] = UINT32_MAX;
            regs[D11_MAC_CONTROL / 4] = mac | MAC_BEACON_PROMISCUOUS | MAC_PROMISCUOUS |
                MAC_AWAKE | MAC_DISCARD_PMQ | D11_MAC_ENABLED;
            if (regs[D11_MAC_CONTROL / 4] & D11_MAC_ENABLED) matches = listen_for_beacons(regs);
        }
        regs[D11_MAC_CONTROL / 4] = mac;
        bool mac_stopped = !(regs[D11_MAC_CONTROL / 4] & D11_MAC_ENABLED);
        bool suspended = mac_stopped && wait_d11_register(regs, D11_IRQ_REASON, D11_MAC_SUSPENDED, D11_MAC_SUSPENDED);
        regs[PIO_RX_DIRECT / 4] = idle;
        *retired = mac_stopped && !(regs[PIO_RX_DIRECT / 4] & (RX_DMA_ENABLE | PIO_DIRECT_FIFO));
        console_printf("bcm4331-radio: mac_suspended_event=%u\n", (uint64_t)suspended);
    }
    if (initial) {
        volatile uint16_t *half = (volatile uint16_t *)regs;
        half[PHY_SELECTOR / 2] = PHY_RF_COMMAND;
        uint16_t rf = half[PHY_DATA / 2];
        if (rf != UINT16_MAX) half[PHY_DATA / 2] = rf & ~PHY_RF_POWER;
        *retired = *retired && rf != UINT16_MAX && !(half[PHY_DATA / 2] & PHY_RF_POWER);
    }
    console_printf("bcm4331-radio: receive_retired=%u\n", (uint64_t)*retired);
    return tuned && matches && *retired;
}
