/* BCM57765 single-queue poll mode. The trial accepts only the observed
 * copper gigabit profile and never installs a resident interrupt handler. */
enum {
    HOST_CONTROL = 0x68, DMA_CONTROL = 0x6c, SRAM_WINDOW = 0x7c, SRAM_DATA = 0x84,
    PCI_INT_DISABLE = 0x400, PCI_EXP_STATUS = 0xb6, PCI_EXP_PENDING = 0x20,
    INT_MAILBOX = 0x204, RX_POST_MAILBOX = 0x26c, RX_JUMBO_MAILBOX = 0x274,
    RX_RETURN_MAILBOX = 0x284, TX_POST_MAILBOX = 0x304,
    MAC_ADDRESS_HIGH = 0x410, MAC_ADDRESS_LOW = 0x414,
    MAC_MTU = 0x43c, MAC_TX = 0x45c, MAC_TX_LENGTHS = 0x464, MAC_RX = 0x468,
    MAC_RULE_FIRST = 0x480, MAC_RULE_CLASS = 0x500, MAC_RX_WATERMARK = 0x504,
    MAC_LOOPBACK = 0x10, MAC_DMA_PATH = 0x00e00000, MAC_GMII = 0x08,
    TX_DATA_INIT = 0xc00, TX_DATA_COMPLETE = 0x1000, TX_DESC_SELECT = 0x1400,
    TX_DESC_INIT = 0x1800, TX_DESC_COMPLETE = 0x1c00, RX_LIST = 0x2000,
    RX_DESC_INIT = 0x2400, RX_JUMBO_RCB = 0x2440, RX_STANDARD_RCB = 0x2450,
    RX_DATA_COMPLETE = 0x2800, RX_BD_INIT = 0x2c00,
    RX_COMPLETION = 0x3000, HOST_COALESCE = 0x3c00,
    HOST_RX_TICKS = 0x3c08, HOST_TX_TICKS = 0x3c0c,
    HOST_RX_FRAMES = 0x3c10, HOST_TX_FRAMES = 0x3c14,
    HOST_STATUS_HIGH = 0x3c38, HOST_STATUS_LOW = 0x3c3c,
    HOST_RX_RETURN_INDEX = 0x3c80, HOST_TX_CONSUMER_INDEX = 0x3cc0,
    MEMORY_ARBITER = 0x4000, BUFFER_MANAGER = 0x4400,
    READ_DMA = 0x4800, WRITE_DMA = 0x4c00,
    READ_DMA_RESERVE = 0x4900, GENERAL_MODE = 0x6800, GENERAL_CONFIG = 0x6804,
    SRAM_TX_RCB = 0x100, SRAM_RETURN_RCB = 0x200, SRAM_RX_BD_CACHE = 0x6000,
    SRAM_DATA_SIGNATURE = 0xb54, SRAM_DATA_CONFIG = 0xb58,
    MODE_ENABLE = 2, RCB_DISABLED = 2, HOST_STACK_UP = 0x10000,
    IRQ_MASK = 2, DMA_ERROR_BITS = 0x3fc,
    RING_SIZE = 512, RING_MASK = RING_SIZE - 1, RX_POSTED = 64,
    BUFFER_BYTES = 2048, RX_DMA_BYTES = 1536, RX_COOKIE = 0x10000,
    RX_INVALID_FLAGS = 0xc60, MIN_FRAME_BYTES = 60, FCS_BYTES = 4,
    REGISTER_PAGES = 7,
};

struct bcm_tx_bd {
    uint32_t high, low, length_flags, vlan;
};
struct bcm_rx_bd {
    uint32_t high, low, index_length, type_flags, checksum, error_vlan, reserved, cookie;
};
struct bcm_status {
    uint32_t word, tag, rx_consumers, mini_consumer;
    volatile uint16_t rx_producer, tx_consumer;
};
struct bcm_packet_memory {
    _Alignas(64) struct bcm_tx_bd tx[RING_SIZE];
    _Alignas(64) struct bcm_rx_bd rx[RING_SIZE];
    _Alignas(64) volatile struct bcm_rx_bd returned[RING_SIZE];
    _Alignas(64) volatile struct bcm_status status;
    _Alignas(64) uint8_t rx_data[RING_SIZE][BUFFER_BYTES];
    _Alignas(64) uint8_t tx_data[RING_SIZE][BUFFER_BYTES];
};
_Static_assert(sizeof(struct bcm_tx_bd) == 16 && sizeof(struct bcm_rx_bd) == 32, "BCM descriptor layout");
_Static_assert(offsetof(struct bcm_status, rx_producer) == 16 &&
    offsetof(struct bcm_status, tx_consumer) == 18, "BCM status ring zero layout");
#define BCM_DMA_PAGES ((sizeof(struct bcm_packet_memory) + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE)

struct bcm_packet {
    const struct pci_device *pci;
    struct bcm_packet_memory *memory;
    uint64_t root, bar, dma;
    uint16_t original_command;
    unsigned mapped, tx_post, tx_seen, rx_post, rx_seen;
    unsigned queued, sent, received, dropped;
    uint8_t mac[6];
    bool programmed, active, fault;
};

struct bcm_register_setting { unsigned offset; uint32_t value; };

/* Downstream consumers before producers. Each mode is also checked before
 * ownership and disabled in reverse order before reclaiming the allocation. */
static const struct bcm_register_setting packet_modes[] = {
    { BUFFER_MANAGER, 0x6 },
    { HOST_COALESCE, 0x302 }, /* 32-byte host status, clear receive tick on BD. */
    { RX_COMPLETION, 0x6 }, { RX_LIST, 0x2 },
    { WRITE_DMA, 0x200003fe }, { READ_DMA, 0x303fe },
    { RX_DATA_COMPLETE, 0x6 }, { TX_DATA_COMPLETE, 0x2 }, { TX_DESC_COMPLETE, 0x6 },
    { RX_BD_INIT, 0x6 }, { RX_DESC_INIT, 0x12 },
    { TX_DATA_INIT, 0x2 }, { TX_DESC_INIT, 0x6 }, { TX_DESC_SELECT, 0x6 },
};

static void mac_write(unsigned offset, uint32_t value) {
    *(volatile uint32_t *)(register_window + offset) = value;
}

static void packet_barrier(void) {
    __asm__ volatile("mfence" ::: "memory");
}

static bool wait_register(unsigned offset, uint32_t bits, uint32_t expected) {
    for (unsigned i = 0; i < 1000; i++) {
        uint32_t value = mac_read(offset);
        if (value != UINT32_MAX && (value & bits) == expected) return true;
        if (!timer_delay_us(10)) break;
    }
    console_printf("bcm57765-packet: mode wait failed reg=%x raw=%x expected=%x\n",
        (uint64_t)offset, (uint64_t)mac_read(offset), (uint64_t)expected);
    return false;
}

static bool packet_pci_idle(const struct pci_device *p) {
    for (unsigned i = 0; i < 1000; i++) {
        uint16_t status = pci_read_config16(p, PCI_EXP_STATUS);
        if (status != UINT16_MAX && !(status & PCI_EXP_PENDING)) return true;
        if (!timer_delay_us(10)) return false;
    }
    return false;
}

static uint32_t sram_read(const struct bcm_packet *nic, unsigned address) {
    pci_write_config32(nic->pci, SRAM_WINDOW, address);
    uint32_t value = pci_read_config32(nic->pci, SRAM_DATA);
    pci_write_config32(nic->pci, SRAM_WINDOW, 0);
    return value;
}

static void sram_write(const struct bcm_packet *nic, unsigned address, uint32_t value) {
    pci_write_config32(nic->pci, SRAM_WINDOW, address);
    pci_write_config32(nic->pci, SRAM_DATA, value);
    pci_write_config32(nic->pci, SRAM_WINDOW, 0);
}

static bool packet_map(struct bcm_packet *nic) {
    nic->root = vmm_kernel_address_space();
    if (!vmm_diagnostic_sole_kernel_root(nic->root)) return false;
    for (unsigned i = 0; i < REGISTER_PAGES; i++) {
        uint64_t occupied, off = i * PMM_FRAME_SIZE;
        if (vmm_virt_to_phys_in_space(nic->root, register_window + off, &occupied) ||
            !vmm_map_page_in_space(nic->root, register_window + off, nic->bar + off,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) return false;
        nic->mapped++;
    }
    return true;
}

static bool packet_unmap(struct bcm_packet *nic) {
    bool ok = true;
    while (nic->mapped) {
        uint64_t removed, off = --nic->mapped * PMM_FRAME_SIZE;
        if (!vmm_unmap_page_in_space(nic->root, register_window + off, &removed) ||
            removed != nic->bar + off) ok = false;
    }
    return ok;
}

static bool packet_firmware_idle(const struct bcm_packet *nic) {
    for (unsigned i = 0; i < sizeof(packet_modes) / sizeof(packet_modes[0]); i++) {
        uint32_t offset = packet_modes[i].offset, value = mac_read(offset);
        if (offset == BUFFER_MANAGER) continue; /* On-chip MBUF pool. */
        if (value & MODE_ENABLE) {
            console_printf("bcm57765-packet: firmware owns reg=%x raw=%x\n", (uint64_t)offset, (uint64_t)value);
            return false;
        }
    }
    if ((mac_read(MAC_MODE) & (MAC_DMA_PATH | MAC_LOOPBACK)) ||
        ((mac_read(MAC_TX) | mac_read(MAC_RX)) & MODE_ENABLE) ||
        (mac_read(GENERAL_MODE) & HOST_STACK_UP) || pci_read_config32(nic->pci, SRAM_WINDOW))
        return false;
    return true;
}

static bool packet_mac(struct bcm_packet *nic) {
    uint32_t high = mac_read(MAC_ADDRESS_HIGH), low = mac_read(MAC_ADDRESS_LOW);
    nic->mac[0] = high >> 8;
    nic->mac[1] = high;
    nic->mac[2] = low >> 24;
    nic->mac[3] = low >> 16;
    nic->mac[4] = low >> 8;
    nic->mac[5] = low;
    return !(nic->mac[0] & 1) && ((high & 0xffff) || low);
}

static uint64_t packet_buffer(uint64_t dma, size_t area, unsigned slot) {
    return dma + area + slot * BUFFER_BYTES;
}

static unsigned ring_distance(unsigned newer, unsigned older) {
    return (newer - older) & RING_MASK;
}

static bool packet_alloc(struct bcm_packet *nic) {
    nic->dma = pmm_alloc_frames_tagged(BCM_DMA_PAGES, PMM_FRAME_DMA);
    if (!nic->dma || nic->dma > UINT64_C(0x100000000) - BCM_DMA_PAGES * PMM_FRAME_SIZE) return false;
    nic->memory = pmm_phys_to_virt(nic->dma);
    memset(nic->memory, 0, BCM_DMA_PAGES * PMM_FRAME_SIZE);
    for (unsigned i = 0; i < RING_SIZE; i++) {
        uint64_t address = packet_buffer(nic->dma, offsetof(struct bcm_packet_memory, rx_data), i);
        nic->memory->rx[i] = (struct bcm_rx_bd) {
            .low = (uint32_t)address, .index_length = RX_DMA_BYTES, .type_flags = 4, .cookie = RX_COOKIE | i,
        };
    }
    return true;
}

static bool program_rings(struct bcm_packet *nic) {
    mac_write(MEMORY_ARBITER, MODE_ENABLE);
    if (!wait_register(MEMORY_ARBITER, MODE_ENABLE, MODE_ENABLE)) return false;
    uint32_t signature = sram_read(nic, SRAM_DATA_SIGNATURE), config = sram_read(nic, SRAM_DATA_CONFIG);
    console_printf("bcm57765-packet: SRAM signature=%x config=%x\n", (uint64_t)signature, (uint64_t)config);
    if (signature == UINT32_MAX || config == UINT32_MAX ||
        (signature == 0x4b657654 && (config & 0x80))) return false; /* Active ASF firmware. */
    nic->programmed = true;
    uint32_t host = pci_read_config32(nic->pci, HOST_CONTROL);
    pci_write_config32(nic->pci, HOST_CONTROL, (host & ~1u) | IRQ_MASK);
    if (!(pci_read_config32(nic->pci, HOST_CONTROL) & IRQ_MASK)) return false;
    mac_write(INT_MAILBOX, 1);
    pci_write_config32(nic->pci, DMA_CONTROL, pci_read_config32(nic->pci, DMA_CONTROL) | 1u);

    /* Host descriptors use non-frame word swap, while wire payloads use
     * both frame swaps on little-endian x86. No checksum/segmentation offload. */
    mac_write(GENERAL_MODE, 0x00130034);
    mac_write(GENERAL_CONFIG, (mac_read(GENERAL_CONFIG) & ~0xffu) | 0x82u);
    mac_write(0x4410, 0);     /* Buffer manager read-DMA low water. */
    mac_write(0x4414, 0x2a);  /* MAC receive low water on this ASIC. */
    mac_write(0x4418, 0xa0);  /* Receive buffer high water. */
    mac_write(0x2c18, RX_POSTED / 8);
    mac_write(0x2010, 0x181); /* Receive list placement, ring zero. */
    mac_write(MAC_MTU, 1522);
    mac_write(MAC_TX_LENGTHS, 0x2620);
    for (unsigned i = 0; i < 16; i++) mac_write(MAC_RULE_FIRST + i * 4, 0);
    mac_write(MAC_RULE_CLASS, 8);
    mac_write(MAC_RX_WATERMARK, 1);
    mac_write(READ_DMA_RESERVE, mac_read(READ_DMA_RESERVE) | 4u);

    for (unsigned i = 1; i < 2; i++) sram_write(nic, SRAM_TX_RCB + i * 16 + 8, RCB_DISABLED);
    for (unsigned i = 1; i < 4; i++) sram_write(nic, SRAM_RETURN_RCB + i * 16 + 8, RCB_DISABLED);
    sram_write(nic, SRAM_TX_RCB, 0);
    sram_write(nic, SRAM_TX_RCB + 4, nic->dma + offsetof(struct bcm_packet_memory, tx));
    sram_write(nic, SRAM_TX_RCB + 8, RING_SIZE << 16);
    sram_write(nic, SRAM_RETURN_RCB, 0);
    sram_write(nic, SRAM_RETURN_RCB + 4, nic->dma + offsetof(struct bcm_packet_memory, returned));
    sram_write(nic, SRAM_RETURN_RCB + 8, RING_SIZE << 16);
    mac_write(RX_STANDARD_RCB, 0);
    mac_write(RX_STANDARD_RCB + 4, nic->dma + offsetof(struct bcm_packet_memory, rx));
    mac_write(RX_STANDARD_RCB + 8, (RING_SIZE << 16) | (RX_DMA_BYTES << 2));
    mac_write(RX_STANDARD_RCB + 12, SRAM_RX_BD_CACHE);
    mac_write(RX_JUMBO_RCB + 8, RCB_DISABLED);
    mac_write(HOST_STATUS_HIGH, 0);
    mac_write(HOST_STATUS_LOW, nic->dma + offsetof(struct bcm_packet_memory, status));
    mac_write(HOST_RX_TICKS, 20);
    mac_write(HOST_TX_TICKS, 20);
    mac_write(HOST_RX_FRAMES, 1);
    mac_write(HOST_TX_FRAMES, 1);
    mac_write(TX_POST_MAILBOX, 0);
    mac_write(RX_RETURN_MAILBOX, 0);
    mac_write(RX_POST_MAILBOX, 0);
    mac_write(RX_JUMBO_MAILBOX, 0);
    return sram_read(nic, SRAM_TX_RCB + 4) == nic->dma + offsetof(struct bcm_packet_memory, tx) &&
        sram_read(nic, SRAM_RETURN_RCB + 4) == nic->dma + offsetof(struct bcm_packet_memory, returned) &&
        mac_read(HOST_STATUS_LOW) == nic->dma + offsetof(struct bcm_packet_memory, status);
}

static bool packet_start(struct bcm_packet *nic) {
    if (!program_rings(nic)) return false;
    packet_barrier();
    uint16_t command = nic->original_command | PCI_INT_DISABLE | PCI_BUS_MASTER;
    pci_write_config16(nic->pci, PCI_COMMAND, command);
    if (pci_read_config16(nic->pci, PCI_COMMAND) != command) return false;
    for (unsigned i = 0; i < sizeof(packet_modes) / sizeof(packet_modes[0]); i++) {
        const struct bcm_register_setting *setting = &packet_modes[i];
        mac_write(setting->offset, setting->value);
        if (!wait_register(setting->offset, MODE_ENABLE, MODE_ENABLE)) return false;
    }
    mac_write(MAC_MODE, MAC_DMA_PATH | MAC_GMII | MAC_LOOPBACK);
    if (!wait_register(MAC_MODE, MAC_DMA_PATH | MAC_LOOPBACK, MAC_DMA_PATH | MAC_LOOPBACK) ||
        !timer_delay_us(40)) return false;
    mac_write(MAC_TX, 0x102); /* Enable and prevent MBUF lockup. */
    if (!wait_register(MAC_TX, MODE_ENABLE, MODE_ENABLE) || !timer_delay_us(100)) return false;
    mac_write(MAC_RX, MODE_ENABLE);
    if (!wait_register(MAC_RX, MODE_ENABLE, MODE_ENABLE) || !timer_delay_us(10))
        return false;
    nic->rx_post = RX_POSTED;
    mac_write(RX_POST_MAILBOX, nic->rx_post);
    nic->active = mac_read(RX_POST_MAILBOX) == RX_POSTED;
    return nic->active;
}

static void packet_tx_reap(struct bcm_packet *nic) {
    unsigned observed = nic->memory->status.tx_consumer;
    unsigned progress = ring_distance(observed, nic->tx_seen);
    if (observed >= RING_SIZE || progress > ring_distance(nic->tx_post, nic->tx_seen)) {
        nic->fault = true;
        return;
    }
    nic->sent += progress;
    nic->tx_seen = observed;
}

static bool packet_send(void *context, const uint8_t *frame, uint16_t size) {
    struct bcm_packet *nic = context;
    if (!nic->active || nic->fault || size < 14 || size > NET_DEVICE_FRAME_MAX) return false;
    packet_tx_reap(nic);
    unsigned slot = nic->tx_post, next = (slot + 1) & RING_MASK;
    if (nic->fault || next == nic->tx_seen) return false;
    unsigned wire_bytes = size < MIN_FRAME_BYTES ? MIN_FRAME_BYTES : size;
    for (unsigned i = 0; i < wire_bytes; i++) nic->memory->tx_data[slot][i] = i < size ? frame[i] : 0;
    uint64_t address = packet_buffer(nic->dma, offsetof(struct bcm_packet_memory, tx_data), slot);
    nic->memory->tx[slot] = (struct bcm_tx_bd) {
        .low = (uint32_t)address, .length_flags = (wire_bytes << 16) | 4u,
    };
    packet_barrier();
    nic->tx_post = next;
    nic->queued++;
    mac_write(TX_POST_MAILBOX, next);
    return true;
}

/* Check the returned descriptor before addressing the owned receive buffer.
 * A single standard ring has no out-of-order or jumbo completions. */
static bool packet_receive(struct bcm_packet *nic, const uint8_t **frame, uint16_t *size) {
    unsigned producer = nic->memory->status.rx_producer;
    if (producer >= RING_SIZE || ring_distance(producer, nic->rx_seen) > RX_POSTED) {
        nic->fault = true;
        return false;
    }
    if (nic->rx_seen == producer) return false;
    packet_barrier();
    struct bcm_rx_bd desc = nic->memory->returned[nic->rx_seen];
    unsigned slot = ring_distance(nic->rx_post, RX_POSTED);
    if (desc.cookie != (RX_COOKIE | slot)) {
        console_printf("bcm57765-packet: unexpected receive cookie=%x expected=%x\n",
            (uint64_t)desc.cookie, (uint64_t)(RX_COOKIE | slot));
        nic->fault = true;
        return false;
    }
    unsigned with_fcs = desc.index_length & 0xffff;
    bool valid = with_fcs >= 14 + FCS_BYTES && with_fcs <= NET_DEVICE_FRAME_MAX + FCS_BYTES &&
        !(desc.type_flags & RX_INVALID_FLAGS) && !(desc.error_vlan >> 16);
    *frame = valid ? nic->memory->rx_data[slot] : NULL;
    *size = valid ? with_fcs - FCS_BYTES : 0;
    if (valid) nic->received++;
    else nic->dropped++;
    return true;
}

static void packet_return(struct bcm_packet *nic) {
    nic->rx_seen = (nic->rx_seen + 1) & RING_MASK;
    nic->rx_post = (nic->rx_post + 1) & RING_MASK;
    packet_barrier();
    mac_write(RX_RETURN_MAILBOX, nic->rx_seen);
    mac_write(RX_POST_MAILBOX, nic->rx_post);
}

static uint64_t packet_poll(void *context, uint64_t budget, bool verbose) {
    (void)verbose;
    struct bcm_packet *nic = context;
    if (!nic->active || nic->fault) return 0;
    packet_tx_reap(nic);
    uint64_t delivered = 0;
    for (uint64_t i = 0; i < budget && !nic->fault; i++) {
        const uint8_t *frame;
        uint16_t size;
        if (!packet_receive(nic, &frame, &size)) break;
        if (frame) {
            net_handle_ethernet_frame(frame, size);
            delivered++;
        }
        packet_return(nic);
    }
    return delivered;
}

static bool packet_loopback(struct bcm_packet *nic) {
    uint8_t pattern[MIN_FRAME_BYTES];
    for (unsigned i = 0; i < 6; i++) pattern[i] = pattern[i + 6] = nic->mac[i];
    pattern[12] = 0x88; pattern[13] = 0xb5;
    for (unsigned i = 14; i < sizeof(pattern); i++) pattern[i] = (uint8_t)(i * 73u + 9u);
    if (!packet_send(nic, pattern, sizeof(pattern))) return false;
    bool match = false;
    for (unsigned i = 0; i < 2000 && !nic->fault; i++) {
        const uint8_t *received;
        uint16_t size;
        packet_tx_reap(nic);
        if (packet_receive(nic, &received, &size)) {
            if (size == sizeof(pattern)) {
                unsigned j = 0;
                while (j < sizeof(pattern) && pattern[j] == received[j]) j++;
                if (j == sizeof(pattern)) match = true;
            }
            packet_return(nic);
        }
        if ((match && nic->sent == 1) || !timer_delay_us(50)) break;
    }
    console_printf("bcm57765-packet: loopback tx_completed=%u rx_completed=%u exact=%u\n",
        (uint64_t)nic->sent, (uint64_t)nic->received, (uint64_t)match);
    return match && nic->sent == 1 && !nic->fault;
}

static bool packet_link(void *context, struct net_device_link *link) {
    const struct bcm_packet *nic = context;
    *link = (struct net_device_link){ .up = nic->active && !nic->fault,
        .speed_mbps = 1000, .full_duplex = true, .duplex_known = true };
    return true;
}

static bool packet_dhcp_current(void *context, uint64_t epoch) {
    const struct bcm_packet *nic = context;
    return epoch == 1 && nic->active && !nic->fault;
}

static bool packet_dhcp_retry(void *context, uint64_t epoch) {
    const struct bcm_packet *nic = context;
    return packet_dhcp_current(context, epoch) &&
        nic->memory->status.tx_consumer == nic->tx_post;
}

static bool packet_dhcp(struct bcm_packet *nic, const struct net_device_ops *provider) {
    if (!net_init_bounded_probe() || !net_device_selected_is(provider)) return false;
    const struct net_bounded_probe_io io = { .context = nic, .current = packet_dhcp_current,
        .retry_ready = packet_dhcp_retry };
    struct net_bounded_probe probe = {0};
    __asm__ volatile("sti" ::: "memory");
    bool timer = timer_wait_ticks_bounded(2, 100000000);
    bool admitted = timer && net_bounded_probe_init(&probe, &io, 1, timer_monotonic_us() + 4000000);
    int64_t lease = admitted ? net_dhcp_request_bounded(&probe) : -1;
    bool finished = admitted && net_bounded_probe_finish(&probe);
    struct srv_net_status_info status;
    bool configured = finished && lease > 0 && net_status_info(&status) == 0 &&
        status.dhcp_configured && status.local_ip == (uint32_t)lease;
    __asm__ volatile("cli" ::: "memory");
    packet_tx_reap(nic);
    const struct net_dhcp_observation *dhcp = &probe.dhcp_observation;
    console_printf("bcm57765-packet: dhcp admitted=%u configured=%u discover=%u offer=%u request=%u ack=%u router=%u end=%u polls=%u elapsed_us=%u\n",
        (uint64_t)admitted, (uint64_t)configured, dhcp->discover_queued, dhcp->accepted_offer,
        dhcp->request_queued, dhcp->accepted_ack, (uint64_t)(configured && status.router_ip != 0),
        dhcp->end_reason, dhcp->polls, dhcp->elapsed_us);
    return configured && dhcp->accepted_offer && dhcp->accepted_ack && !nic->fault;
}

/* A failed retirement must keep the whole allocation pinned. Clearing BME
 * and proving PCIe idle also bounds failure recovery before the warm reboot. */
static bool packet_stop(struct bcm_packet *nic, bool provider_retired, bool *can_reboot) {
    nic->active = false;
    bool engines = true;
    if (nic->programmed) {
        mac_write(MAC_RX, 0);
        mac_write(MAC_TX, 0);
        engines = wait_register(MAC_RX, MODE_ENABLE, 0);
        engines = wait_register(MAC_TX, MODE_ENABLE, 0) && engines;
        mac_write(MAC_MODE, MAC_GMII);
        for (unsigned i = sizeof(packet_modes) / sizeof(packet_modes[0]); i > 0; i--) {
            unsigned offset = packet_modes[i - 1].offset;
            mac_write(offset, 0);
            engines = wait_register(offset, MODE_ENABLE, 0) && engines;
        }
    }

    uint16_t stopped_command = (nic->original_command | PCI_INT_DISABLE) & ~PCI_BUS_MASTER;
    pci_write_config16(nic->pci, PCI_COMMAND, stopped_command);
    *can_reboot = pci_read_config16(nic->pci, PCI_COMMAND) == stopped_command &&
        packet_pci_idle(nic->pci);
    if (!*can_reboot || !engines || !provider_retired) return false;
    if (nic->programmed) {
        mac_write(GENERAL_MODE, mac_read(GENERAL_MODE) & ~HOST_STACK_UP);
        sram_write(nic, SRAM_TX_RCB + 8, RCB_DISABLED);
        sram_write(nic, SRAM_TX_RCB + 4, 0);
        sram_write(nic, SRAM_RETURN_RCB + 8, RCB_DISABLED);
        sram_write(nic, SRAM_RETURN_RCB + 4, 0);
        mac_write(RX_STANDARD_RCB + 8, RCB_DISABLED);
        mac_write(RX_STANDARD_RCB + 4, 0);
        mac_write(HOST_STATUS_LOW, 0);
        if (mac_read(HOST_STATUS_LOW) || mac_read(RX_STANDARD_RCB + 4) ||
            sram_read(nic, SRAM_TX_RCB + 4) || sram_read(nic, SRAM_RETURN_RCB + 4)) return false;
    }
    packet_barrier();
    if (!packet_unmap(nic)) return false;
    if (nic->dma) pmm_free_frame_range(nic->dma, BCM_DMA_PAGES);
    return true;
}

enum bcm57765_probe_result bcm57765_packet_trial(void) {
    enum bcm57765_probe_result link = bcm57765_probe();
    if (link != BCM57765_LINK_UP) return link;
    struct bcm_packet nic = { .pci = controller() };
    if (!nic.pci || !net_device_registry_empty()) return BCM57765_REFUSED;
    nic.original_command = pci_read_config16(nic.pci, PCI_COMMAND);
    nic.bar = nic.pci->bar[0] & ~15ull;
    if (!(nic.original_command & PCI_MEMORY) || (pci_read_config16(nic.pci, PCI_PM_CONTROL) & PM_STATE) ||
        (pci_read_config16(nic.pci, 0x5a) & 1u) || (pci_read_config16(nic.pci, 0xa2) & 0x8000u) ||
        (pci_read_config16(nic.pci, 0xac) & 0xff) != 0x10) {
        console_write("bcm57765-packet: refused PCI decode, power or interrupt state\n");
        return BCM57765_REFUSED;
    }
    bool pending = false;
    uint16_t aux;
    if (!packet_map(&nic) || !packet_firmware_idle(&nic) || !packet_mac(&nic) ||
        !mdio_read(MII_AUX_STATUS, &aux, &pending) || (aux & 0x704) != 0x704) {
        console_write("bcm57765-packet: refused firmware state, station address or link mode\n");
        return !packet_unmap(&nic) || pending ? BCM57765_UNSAFE : BCM57765_REFUSED;
    }
    uint16_t disabled = (nic.original_command | PCI_INT_DISABLE) & ~PCI_BUS_MASTER;
    pci_write_config16(nic.pci, PCI_COMMAND, disabled);
    if (pci_read_config16(nic.pci, PCI_COMMAND) != disabled || !packet_pci_idle(nic.pci)) {
        console_write("bcm57765-packet: could not fence prior PCI bus mastering\n");
        return BCM57765_UNSAFE;
    }
    bool started = packet_alloc(&nic) && packet_start(&nic);
    bool loopback = started && packet_loopback(&nic);
    bool configured = false, provider_retired = true;
    struct net_device_ops provider = { .name = "bcm57765", .context = &nic, .priority = 100,
        .send = packet_send, .poll = packet_poll, .link = packet_link };
    for (unsigned i = 0; i < 6; i++) provider.mac[i] = nic.mac[i];
    if (loopback) {
        mac_write(MAC_MODE, MAC_DMA_PATH | MAC_GMII);
        if (wait_register(MAC_MODE, MAC_LOOPBACK, 0) && timer_delay_us(40) &&
            read_link(&pending) == BCM57765_LINK_UP &&
            net_device_register(&provider)) {
            configured = packet_dhcp(&nic, &provider);
            provider_retired = net_device_retire(&provider);
        }
    }
    console_printf("bcm57765-packet: started=%u queued=%u tx_completed=%u rx_completed=%u dropped=%u fault=%u tx_index=%u rx_index=%u rdma=%x wdma=%x\n",
        (uint64_t)started, (uint64_t)nic.queued, (uint64_t)nic.sent, (uint64_t)nic.received,
        (uint64_t)nic.dropped, (uint64_t)nic.fault,
        (uint64_t)mac_read(HOST_TX_CONSUMER_INDEX), (uint64_t)mac_read(HOST_RX_RETURN_INDEX),
        (uint64_t)(mac_read(READ_DMA + 4) & DMA_ERROR_BITS), (uint64_t)(mac_read(WRITE_DMA + 4) & DMA_ERROR_BITS));
    bool can_reboot;
    bool retired = packet_stop(&nic, provider_retired, &can_reboot);
    console_printf("bcm57765-packet: retirement dma=%s bus_master_off=%u provider=%u\n",
        retired ? "released" : "retained", (uint64_t)can_reboot, (uint64_t)provider_retired);
    if (!can_reboot || !provider_retired || pending) return BCM57765_UNSAFE;
    return configured && retired ? BCM57765_DHCP_OK : BCM57765_INCOMPLETE;
}
