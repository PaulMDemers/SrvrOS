/* BCM4331 package 9, HT PHY rev 1, D11 rev 29, 2.4 GHz channel 10 only. */
enum {
    TX_DMA_CONTROL = 0x200, TX_DMA_STATUS = 0x210, DMA_CHANNEL_STRIDE = 0x40,
    RX_DMA_CONTROL = 0x220, RX_DMA_INDEX = 0x224, RX_DMA_LOW = 0x228, RX_DMA_HIGH = 0x22c,
    RX_DMA_STATUS = 0x230, RX_DMA_ERROR = 0x234, DMA_STATE = 0xf0000000, RX_DMA_POINTER = 0x1fff,
    DMA_ENABLE = 1, RX_DIRECT_FIFO = 0x100, RX_DMA_NO_PARITY = 0x800, DMA64_PCIE = 0x80000000u,
    D11_DMA0_REASON = 0x20, D11_DMA0_MASK = 0x24, D11_DMA0_EVENTS = 0x1fc00, D11_RX_DONE = 0x10000,
    D11_IRQ_DMA = 0x8000,
    RADIO_SELECTOR = 0x3d8, RADIO_DATA = 0x3da, PHY_SELECTOR = 0x3fc, PHY_DATA = 0x3fe,
    PHY_RF_COMMAND = 0x810, PHY_RF_POWER = 2,
    MAC_BEACON_PROMISCUOUS = 0x100000, MAC_PROMISCUOUS = 0x1000000,
    MAC_AWAKE = 0x4000000, MAC_DISCARD_PMQ = 0x40000000,
    RX_FRAME_OFFSET = 38, RX_BUFFER_BYTES = RX_FRAME_OFFSET + 2352, RX_SLOTS = 128,
    RX_RING_ALIGNMENT = 8192, RX_DMA_PAGES = RX_SLOTS + 3, BEACON_CHANNEL = 10,
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

struct bcm4331_rx_descriptor {
    uint32_t control, size, low, high;
};

static unsigned listen_for_beacons(volatile uint32_t *regs, const uint8_t *buffers) {
    unsigned frames = 0, valid = 0, target = 0, timeouts = 0, last_rx_channel = 0;
    unsigned beacon_headers = 0, beacon_checksums = 0, rx_events = 0;
    unsigned first_length = 0, first_header = 0;
    uint32_t status = 0, error = 0;
    uint64_t start = timer_monotonic_us();
    for (unsigned poll = 0; poll < 150000 && frames < RX_SLOTS - 1 && target < 3; poll++) {
        if (timer_monotonic_us() - start >= 15000000) break;
        uint32_t reason = regs[D11_IRQ_REASON / 4], dma = regs[D11_DMA0_REASON / 4];
        status = regs[RX_DMA_STATUS / 4];
        error = regs[RX_DMA_ERROR / 4];
        if (reason == UINT32_MAX || dma == UINT32_MAX || status == UINT32_MAX || (error & DMA_STATE)) break;
        if (reason & D11_IRQ_DMA) regs[D11_IRQ_REASON / 4] = D11_IRQ_DMA;
        if (dma & D11_DMA0_EVENTS) regs[D11_DMA0_REASON / 4] = dma & D11_DMA0_EVENTS;
        if (dma & D11_RX_DONE) rx_events++;
        unsigned pointer = status & RX_DMA_POINTER;
        unsigned completed = pointer / sizeof(struct bcm4331_rx_descriptor);
        if (pointer % sizeof(struct bcm4331_rx_descriptor) || completed < frames || completed >= RX_SLOTS) break;
        if (completed == frames) {
            if (!timer_delay_us(100)) break;
            continue;
        }
        const uint8_t *buffer = buffers + frames * PAGE_BYTES;
        const volatile uint16_t *frame_length = (const volatile uint16_t *)buffer;
        unsigned length = *frame_length;
        for (unsigned wait = 0; !length && wait < 5; wait++) {
            if (!timer_delay_us(2)) break;
            length = *frame_length;
        }
        if (!length) {
            timeouts++;
            break;
        }
        __asm__ volatile ("lfence" ::: "memory");
        frames++;
        uint32_t mac_status = radio_le32(buffer + 16);
        unsigned rx_channel = radio_le16(buffer + 22);
        last_rx_channel = rx_channel;
        unsigned prefix = 6 + (mac_status & 4 ? 2 : 0);
        if (length < prefix + 2 || length > RX_BUFFER_BYTES - RX_FRAME_OFFSET || (mac_status & 1)) continue;
        const uint8_t *frame = buffer + RX_FRAME_OFFSET + prefix;
        length -= prefix;
        valid++;
        if (valid == 1) {
            first_length = length;
            first_header = radio_le16(frame);
        }
        if (length >= 40 && frame[0] == 0x80) {
            beacon_headers++;
            if (beacon_fcs(frame, length - 4) == radio_le32(frame + length - 4)) beacon_checksums++;
        }
        if (!(rx_channel & 0x1800) && ((rx_channel >> 3) & 0xff) == BEACON_CHANNEL &&
            target_beacon(frame, length)) target++;
    }
    console_printf("bcm4331-radio: channel=10 dma_frames=%u valid_frames=%u target_beacons=%u timeouts=%u rx_channel=%x\n",
        (uint64_t)frames, (uint64_t)valid, (uint64_t)target, (uint64_t)timeouts, (uint64_t)last_rx_channel);
    console_printf("bcm4331-radio: beacon_headers=%u beacon_checksums=%u rx_events=%u first_length=%u first_fc=%x\n",
        (uint64_t)beacon_headers, (uint64_t)beacon_checksums, (uint64_t)rx_events, (uint64_t)first_length, (uint64_t)first_header);
    console_printf("bcm4331-radio: dma_status=%x dma_error=%x\n", (uint64_t)status, (uint64_t)error);
    return target;
}

static unsigned receive_beacons(const struct pci_device *p, volatile uint32_t *regs, uint32_t mac,
        bool *retired, unsigned *dma_pages) {
    uint16_t command = pci_read_config16(p, PCI_COMMAND);
    if (command == UINT16_MAX || (command & (PCI_MEMORY | PCI_BUS_MASTER)) != PCI_MEMORY) return 0;
    for (unsigned channel = 0; channel < 6; channel++) {
        volatile uint32_t *engine = regs + channel * DMA_CHANNEL_STRIDE / 4;
        uint32_t tx = engine[TX_DMA_CONTROL / 4], tx_status = engine[TX_DMA_STATUS / 4];
        uint32_t rx = engine[RX_DMA_CONTROL / 4], rx_status = engine[RX_DMA_STATUS / 4];
        /* On this D11 stepping only channel 0 exposes receive registers. */
        bool rx_idle = channel == 0 ? !(rx & (DMA_ENABLE | RX_DIRECT_FIFO)) && !(rx_status & DMA_STATE) :
            rx == UINT32_MAX && rx_status == UINT32_MAX;
        if ((tx & DMA_ENABLE) || (tx_status & DMA_STATE) || !rx_idle) {
            console_printf("bcm4331-radio: refused DMA channel %u tx=%x tx_status=%x rx=%x rx_status=%x\n",
                (uint64_t)channel, (uint64_t)tx, (uint64_t)tx_status, (uint64_t)rx, (uint64_t)rx_status);
            return 0;
        }
    }
    uint64_t memory = pmm_alloc_frames_tagged(RX_DMA_PAGES, PMM_FRAME_DMA);
    if (!memory) return 0;
    *dma_pages = RX_DMA_PAGES;
    uint64_t ring = (memory + RX_RING_ALIGNMENT - 1) & ~(uint64_t)(RX_RING_ALIGNMENT - 1);
    uint64_t buffers = memory + 3 * PAGE_BYTES;
    struct bcm4331_rx_descriptor *descriptors = pmm_phys_to_virt(ring);
    uint8_t *data = pmm_phys_to_virt(buffers);
    for (unsigned slot = 0; slot < RX_SLOTS; slot++) {
        uint64_t address = buffers + slot * PAGE_BYTES;
        for (unsigned byte = 0; byte < RX_BUFFER_BYTES; byte++) data[slot * PAGE_BYTES + byte] = 0;
        descriptors[slot] = (struct bcm4331_rx_descriptor){
            .control = slot == RX_SLOTS - 1 ? 0x10000000 : 0,
            .size = RX_BUFFER_BYTES, .low = (uint32_t)address, .high = DMA64_PCIE | (uint32_t)(address >> 32),
        };
    }
    __asm__ volatile ("mfence" ::: "memory");
    uint32_t control = DMA_ENABLE | RX_DMA_NO_PARITY | (RX_FRAME_OFFSET << 1);
    uint32_t low = (uint32_t)ring, high = DMA64_PCIE | (uint32_t)(ring >> 32);
    unsigned end = (RX_SLOTS - 1) * sizeof *descriptors;
    regs[RX_DMA_CONTROL / 4] = control;
    regs[RX_DMA_LOW / 4] = low;
    regs[RX_DMA_HIGH / 4] = high;
    regs[RX_DMA_INDEX / 4] = end;
    bool configured = regs[RX_DMA_CONTROL / 4] == control && regs[RX_DMA_LOW / 4] == low &&
        regs[RX_DMA_HIGH / 4] == high && regs[RX_DMA_INDEX / 4] == end;
    if (configured) pci_write_config16(p, PCI_COMMAND, command | PCI_BUS_MASTER);
    bool mastering = configured && pci_read_config16(p, PCI_COMMAND) == (command | PCI_BUS_MASTER);
    console_printf("bcm4331-radio: dma_configured=%u dma_started=%u slots=%u\n",
        (uint64_t)configured, (uint64_t)mastering, (uint64_t)(RX_SLOTS - 1));

    unsigned matches = 0;
    if (mastering) {
        regs[D11_DMA0_MASK / 4] = D11_RX_DONE;
        regs[D11_MAC_CONTROL / 4] = mac | MAC_BEACON_PROMISCUOUS | MAC_PROMISCUOUS |
            MAC_AWAKE | MAC_DISCARD_PMQ | D11_MAC_ENABLED;
        regs[D11_IRQ_REASON / 4] = D11_MAC_SUSPENDED;
        if (regs[D11_MAC_CONTROL / 4] & D11_MAC_ENABLED) matches = listen_for_beacons(regs, data);
    }
    regs[D11_MAC_CONTROL / 4] = mac;
    bool stopped = wait_d11_register(regs, D11_MAC_CONTROL, D11_MAC_ENABLED, 0);
    bool suspended = stopped && (!mastering || wait_d11_register(regs, D11_IRQ_REASON, D11_MAC_SUSPENDED, D11_MAC_SUSPENDED));
    regs[D11_DMA0_MASK / 4] = 0;
    regs[RX_DMA_CONTROL / 4] = 0;
    bool disabled = wait_d11_register(regs, RX_DMA_STATUS, DMA_STATE, 0) &&
        wait_d11_register(regs, RX_DMA_CONTROL, DMA_ENABLE | RX_DIRECT_FIFO, 0);
    pci_write_config16(p, PCI_COMMAND, command);
    bool fenced = pci_read_config16(p, PCI_COMMAND) == command && pci_transactions_drained(p);
    bool cleared = false;
    if (disabled && fenced) {
        regs[RX_DMA_INDEX / 4] = 0;
        regs[RX_DMA_LOW / 4] = 0;
        regs[RX_DMA_HIGH / 4] = 0;
        cleared = !regs[RX_DMA_LOW / 4] && !regs[RX_DMA_HIGH / 4] && !regs[RX_DMA_INDEX / 4];
        if (cleared) {
            pmm_free_frame_range(memory, RX_DMA_PAGES);
            *dma_pages = 0;
        }
    }
    *retired = suspended && cleared && !regs[D11_DMA0_MASK / 4];
    console_printf("bcm4331-radio: mac_suspended_event=%u dma_disabled=%u dma_fenced=%u ring_cleared=%u\n",
        (uint64_t)suspended, (uint64_t)disabled, (uint64_t)fenced, (uint64_t)cleared);
    return matches;
}

static bool passive_radio_trial(const struct pci_device *p, volatile uint32_t *regs,
        const struct radio_assets *assets, bool *retired, unsigned *dma_pages) {
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
    uint32_t rx_control = regs[RX_DMA_CONTROL / 4];
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
        matches = receive_beacons(p, regs, mac, retired, dma_pages);
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
