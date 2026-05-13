#include <srvros/console.h>
#include <srvros/e1000.h>
#include <srvros/net.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stdint.h>

#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x0002
#define PCI_COMMAND_BUS_MASTER 0x0004

#define E1000_VENDOR_INTEL 0x8086
#define E1000_DEVICE_82540EM 0x100e
#define E1000_MMIO_VIRTUAL 0xffffc001f1000000ull
#define E1000_MMIO_PAGES 32

#define E1000_REG_CTRL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_ICR 0x00c0
#define E1000_REG_IMS 0x00d0
#define E1000_REG_IMC 0x00d8
#define E1000_REG_RCTL 0x0100
#define E1000_REG_TCTL 0x0400
#define E1000_REG_TIPG 0x0410
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH 0x2810
#define E1000_REG_RDT 0x2818
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH 0x3810
#define E1000_REG_TDT 0x3818
#define E1000_REG_RAL0 0x5400
#define E1000_REG_RAH0 0x5404

#define E1000_CTRL_SLU (1u << 6)
#define E1000_RCTL_EN (1u << 1)
#define E1000_RCTL_UPE (1u << 3)
#define E1000_RCTL_MPE (1u << 4)
#define E1000_RCTL_BAM (1u << 15)
#define E1000_RCTL_SECRC (1u << 26)
#define E1000_RAH_AV (1u << 31)
#define E1000_TCTL_EN (1u << 1)
#define E1000_TCTL_PSP (1u << 3)

#define E1000_INT_LSC (1u << 2)
#define E1000_INT_RXDMT0 (1u << 4)
#define E1000_INT_RXO (1u << 6)
#define E1000_INT_RXT0 (1u << 7)

#define E1000_RX_DESC_STATUS_DD 0x01
#define E1000_RX_DESC_STATUS_EOP 0x02
#define E1000_TX_DESC_STATUS_DD 0x01
#define E1000_TX_CMD_EOP 0x01
#define E1000_TX_CMD_IFCS 0x02
#define E1000_TX_CMD_RS 0x08

#define E1000_RX_DESC_COUNT 32
#define E1000_TX_DESC_COUNT 16
#define E1000_PACKET_BUFFER_SIZE 2048

struct e1000_rx_desc {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    uint64_t address;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

static volatile uint32_t *mmio;
static uint64_t mmio_physical;
static const struct pci_device *nic;
static uint8_t mac[6];
static uint32_t status_reg;
static volatile struct e1000_rx_desc *rx_desc;
static volatile struct e1000_tx_desc *tx_desc;
static uint8_t *rx_buffers[E1000_RX_DESC_COUNT];
static uint8_t *tx_buffers[E1000_TX_DESC_COUNT];
static uint32_t rx_tail;
static uint32_t tx_tail;
static uint64_t rx_packets;
static uint64_t tx_packets;
static uint64_t interrupts;
static uint64_t spurious_interrupts;
static uint64_t rx_interrupts;
static uint64_t link_interrupts;
static uint64_t overflow_interrupts;
static uint64_t rx_low_water_interrupts;
static uint32_t last_interrupt_cause;
static bool interrupts_enabled;
static bool rings_ready;

static uint32_t reg_read(uint32_t offset) {
    return mmio[offset / sizeof(uint32_t)];
}

static void reg_write(uint32_t offset, uint32_t value) {
    mmio[offset / sizeof(uint32_t)] = value;
}

static void write_barrier(void) {
    __asm__ volatile ("" : : : "memory");
}

static void zero_memory(void *address, uint64_t size) {
    uint8_t *bytes = address;
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static void print_hex_byte(uint8_t value) {
    static const char digits[] = "0123456789abcdef";
    console_putc(digits[(value >> 4) & 0xf]);
    console_putc(digits[value & 0xf]);
}

static void print_mac(void) {
    for (uint64_t i = 0; i < 6; i++) {
        if (i != 0) {
            console_putc(':');
        }
        print_hex_byte(mac[i]);
    }
}

static bool map_bar(uint32_t bar0) {
    if ((bar0 & 0x1) != 0) {
        console_write("e1000: bar0 is io-space, expected mmio\n");
        return false;
    }

    uint64_t physical = bar0 & 0xfffffff0u;
    mmio_physical = physical;
    for (uint64_t i = 0; i < E1000_MMIO_PAGES; i++) {
        if (!vmm_map_page(E1000_MMIO_VIRTUAL + i * 4096,
                physical + i * 4096,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
            console_write("e1000: mmio map failed\n");
            return false;
        }
    }

    mmio = (volatile uint32_t *)E1000_MMIO_VIRTUAL;
    return true;
}

static bool ensure_mmio_mapping(void) {
    if (mmio_physical == 0) {
        return false;
    }

    for (uint64_t i = 0; i < E1000_MMIO_PAGES; i++) {
        uint64_t virtual_address = E1000_MMIO_VIRTUAL + i * 4096;
        uint64_t translated = 0;
        if (vmm_virt_to_phys(virtual_address, &translated)) {
            continue;
        }

        if (!vmm_map_page(virtual_address,
                mmio_physical + i * 4096,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
            console_write("e1000: mmio remap failed\n");
            return false;
        }
    }

    return true;
}

static bool allocate_rings(void) {
    uint64_t rx_ring_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    uint64_t tx_ring_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (rx_ring_phys == 0 || tx_ring_phys == 0) {
        console_write("e1000: ring allocation failed\n");
        return false;
    }

    rx_desc = pmm_phys_to_virt(rx_ring_phys);
    tx_desc = pmm_phys_to_virt(tx_ring_phys);
    zero_memory((void *)rx_desc, 4096);
    zero_memory((void *)tx_desc, 4096);

    for (uint64_t i = 0; i < E1000_RX_DESC_COUNT; i++) {
        uint64_t buffer_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if (buffer_phys == 0) {
            console_write("e1000: rx buffer allocation failed\n");
            return false;
        }
        rx_buffers[i] = pmm_phys_to_virt(buffer_phys);
        zero_memory(rx_buffers[i], E1000_PACKET_BUFFER_SIZE);
        rx_desc[i].address = buffer_phys;
    }

    for (uint64_t i = 0; i < E1000_TX_DESC_COUNT; i++) {
        uint64_t buffer_phys = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if (buffer_phys == 0) {
            console_write("e1000: tx buffer allocation failed\n");
            return false;
        }
        tx_buffers[i] = pmm_phys_to_virt(buffer_phys);
        zero_memory(tx_buffers[i], E1000_PACKET_BUFFER_SIZE);
        tx_desc[i].address = buffer_phys;
        tx_desc[i].status = E1000_TX_DESC_STATUS_DD;
    }

    reg_write(E1000_REG_RDBAL, (uint32_t)(rx_ring_phys & 0xffffffff));
    reg_write(E1000_REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    reg_write(E1000_REG_RDLEN, E1000_RX_DESC_COUNT * sizeof(struct e1000_rx_desc));
    reg_write(E1000_REG_RDH, 0);
    reg_write(E1000_REG_RDT, E1000_RX_DESC_COUNT - 1);
    rx_tail = 0;

    reg_write(E1000_REG_TDBAL, (uint32_t)(tx_ring_phys & 0xffffffff));
    reg_write(E1000_REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    reg_write(E1000_REG_TDLEN, E1000_TX_DESC_COUNT * sizeof(struct e1000_tx_desc));
    reg_write(E1000_REG_TDH, 0);
    reg_write(E1000_REG_TDT, 0);
    tx_tail = 0;

    return true;
}

static void configure_rxtx(void) {
    reg_write(E1000_REG_IMC, 0xffffffffu);
    (void)reg_read(E1000_REG_ICR);
    reg_write(E1000_REG_CTRL, reg_read(E1000_REG_CTRL) | E1000_CTRL_SLU);
    reg_write(E1000_REG_RAL0,
        ((uint32_t)mac[0]) |
        ((uint32_t)mac[1] << 8) |
        ((uint32_t)mac[2] << 16) |
        ((uint32_t)mac[3] << 24));
    reg_write(E1000_REG_RAH0,
        ((uint32_t)mac[4]) |
        ((uint32_t)mac[5] << 8) |
        E1000_RAH_AV);

    reg_write(E1000_REG_RCTL,
        E1000_RCTL_EN |
        E1000_RCTL_UPE |
        E1000_RCTL_MPE |
        E1000_RCTL_BAM |
        E1000_RCTL_SECRC);
    reg_write(E1000_REG_TIPG, 0x0060200au);
    reg_write(E1000_REG_TCTL,
        E1000_TCTL_EN |
        E1000_TCTL_PSP |
        (0x10u << 4) |
        (0x40u << 12));

    rings_ready = true;
}

bool e1000_init(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device->vendor_id == E1000_VENDOR_INTEL &&
            device->device_id == E1000_DEVICE_82540EM) {
            nic = device;
            break;
        }
    }

    if (nic == 0) {
        console_write("e1000: no supported device\n");
        return false;
    }

    uint16_t command = pci_read_config16(nic, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    pci_write_config16(nic, PCI_COMMAND, command);

    if (!map_bar(nic->bar[0])) {
        nic = 0;
        return false;
    }

    status_reg = reg_read(E1000_REG_STATUS);
    uint32_t ral = reg_read(E1000_REG_RAL0);
    uint32_t rah = reg_read(E1000_REG_RAH0);

    mac[0] = ral & 0xff;
    mac[1] = (ral >> 8) & 0xff;
    mac[2] = (ral >> 16) & 0xff;
    mac[3] = (ral >> 24) & 0xff;
    mac[4] = rah & 0xff;
    mac[5] = (rah >> 8) & 0xff;

    console_printf("e1000: mmio=%x status=%x mac=",
        E1000_MMIO_VIRTUAL,
        (uint64_t)status_reg);
    print_mac();
    console_printf(" irq=%u\n", (uint64_t)nic->interrupt_line);

    if (!allocate_rings()) {
        nic = 0;
        mmio = 0;
        return false;
    }
    configure_rxtx();
    console_printf("e1000: rx=%u tx=%u irq-ready\n",
        (uint64_t)E1000_RX_DESC_COUNT,
        (uint64_t)E1000_TX_DESC_COUNT);
    return true;
}

bool e1000_available(void) {
    return nic != 0 && mmio != 0;
}

const uint8_t *e1000_mac(void) {
    if (!e1000_available()) {
        return 0;
    }

    return mac;
}

uint8_t e1000_irq_line(void) {
    if (!e1000_available()) {
        return 0xff;
    }
    return nic->interrupt_line;
}

bool e1000_enable_interrupts(void) {
    if (!e1000_available() || !rings_ready) {
        return false;
    }
    if (!ensure_mmio_mapping()) {
        return false;
    }

    reg_write(E1000_REG_IMC, 0xffffffffu);
    (void)reg_read(E1000_REG_ICR);
    reg_write(E1000_REG_IMS, E1000_INT_LSC | E1000_INT_RXDMT0 | E1000_INT_RXO | E1000_INT_RXT0);
    (void)reg_read(E1000_REG_STATUS);
    interrupts_enabled = true;
    console_printf("e1000: interrupts enabled irq=%u mask=%x\n",
        (uint64_t)nic->interrupt_line,
        (uint64_t)(E1000_INT_LSC | E1000_INT_RXDMT0 | E1000_INT_RXO | E1000_INT_RXT0));
    return true;
}

bool e1000_handle_interrupt(void) {
    if (!e1000_available() || !rings_ready || mmio == 0) {
        return false;
    }

    uint32_t cause = reg_read(E1000_REG_ICR);
    if (cause == 0) {
        spurious_interrupts++;
        return true;
    }

    interrupts++;
    last_interrupt_cause = cause;
    if ((cause & E1000_INT_RXT0) != 0) {
        rx_interrupts++;
    }
    if ((cause & E1000_INT_LSC) != 0) {
        link_interrupts++;
    }
    if ((cause & E1000_INT_RXO) != 0) {
        overflow_interrupts++;
    }
    if ((cause & E1000_INT_RXDMT0) != 0) {
        rx_low_water_interrupts++;
    }
    if ((cause & (E1000_INT_RXT0 | E1000_INT_RXDMT0 | E1000_INT_RXO)) != 0) {
        net_notify_rx();
    }
    return true;
}

void e1000_print_status(void) {
    if (!e1000_available()) {
        console_write("net: e1000 unavailable\n");
        return;
    }
    if (!ensure_mmio_mapping()) {
        return;
    }

    status_reg = reg_read(E1000_REG_STATUS);
    console_printf("net: e1000 vendor=%x device=%x status=%x mac=",
        (uint64_t)nic->vendor_id,
        (uint64_t)nic->device_id,
        (uint64_t)status_reg);
    print_mac();
    console_printf(" irq=%u bar0=%x interrupts=%s count=%u spurious=%u cause=%x\n",
        (uint64_t)nic->interrupt_line,
        (uint64_t)nic->bar[0],
        interrupts_enabled ? "on" : "off",
        interrupts,
        spurious_interrupts,
        (uint64_t)last_interrupt_cause);
    console_printf("net: irq_causes rx=%u low=%u overflow=%u link=%u\n",
        rx_interrupts,
        rx_low_water_interrupts,
        overflow_interrupts,
        link_interrupts);
    console_printf("net: rings=%s rx_packets=%u tx_packets=%u rdh=%u rdt=%u tdh=%u tdt=%u\n",
        rings_ready ? "ready" : "down",
        rx_packets,
        tx_packets,
        (uint64_t)reg_read(E1000_REG_RDH),
        (uint64_t)reg_read(E1000_REG_RDT),
        (uint64_t)reg_read(E1000_REG_TDH),
        (uint64_t)reg_read(E1000_REG_TDT));
}

static uint16_t read_be16(const uint8_t *data) {
    return ((uint16_t)data[0] << 8) | data[1];
}

uint64_t e1000_poll(uint64_t max_frames, bool verbose) {
    if (!e1000_available() || !rings_ready) {
        console_write("net: e1000 unavailable\n");
        return 0;
    }
    if (!ensure_mmio_mapping()) {
        return 0;
    }

    uint64_t received = 0;
    while (received < max_frames &&
        (rx_desc[rx_tail].status & E1000_RX_DESC_STATUS_DD) != 0) {
        uint16_t length = rx_desc[rx_tail].length;
        uint8_t status = rx_desc[rx_tail].status;
        uint8_t *packet = rx_buffers[rx_tail];

        rx_packets++;
        received++;

        if (verbose && length >= 14) {
            console_printf("net: rx len=%u status=%x ethertype=%x src=",
                (uint64_t)length,
                (uint64_t)status,
                (uint64_t)read_be16(packet + 12));
            for (uint64_t i = 6; i < 12; i++) {
                if (i != 6) {
                    console_putc(':');
                }
                print_hex_byte(packet[i]);
            }
            console_write(" dst=");
            for (uint64_t i = 0; i < 6; i++) {
                if (i != 0) {
                    console_putc(':');
                }
                print_hex_byte(packet[i]);
            }
            console_putc('\n');
        } else if (verbose) {
            console_printf("net: rx len=%u status=%x\n", (uint64_t)length, (uint64_t)status);
        }

        net_handle_ethernet_frame(packet, length);

        rx_desc[rx_tail].status = 0;
        rx_desc[rx_tail].errors = 0;
        write_barrier();
        reg_write(E1000_REG_RDT, rx_tail);
        rx_tail = (rx_tail + 1) % E1000_RX_DESC_COUNT;
    }

    if (verbose && received == 0) {
        console_write("net: rx none\n");
    }
    return received;
}

bool e1000_send_frame(const uint8_t *frame, uint16_t length) {
    if (!e1000_available() || !rings_ready) {
        console_write("net: e1000 unavailable\n");
        return false;
    }
    if (!ensure_mmio_mapping()) {
        return false;
    }

    if (frame == 0 || length == 0 || length > E1000_PACKET_BUFFER_SIZE) {
        return false;
    }

    volatile struct e1000_tx_desc *desc = &tx_desc[tx_tail];
    if ((desc->status & E1000_TX_DESC_STATUS_DD) == 0) {
        console_write("net: tx ring busy\n");
        return false;
    }

    uint8_t *packet = tx_buffers[tx_tail];
    for (uint64_t i = 0; i < length; i++) {
        packet[i] = frame[i];
    }

    if (length < 60) {
        for (uint64_t i = length; i < 60; i++) {
            packet[i] = 0;
        }
        length = 60;
    }

    desc->length = length;
    desc->cso = 0;
    desc->cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
    desc->status = 0;
    desc->css = 0;
    desc->special = 0;
    write_barrier();

    tx_tail = (tx_tail + 1) % E1000_TX_DESC_COUNT;
    reg_write(E1000_REG_TDT, tx_tail);
    tx_packets++;
    return true;
}

bool e1000_send_test_frame(void) {
    uint8_t packet[60];
    for (uint64_t i = 0; i < sizeof(packet); i++) {
        packet[i] = 0;
    }

    for (uint64_t i = 0; i < 6; i++) {
        packet[i] = 0xff;
        packet[6 + i] = mac[i];
    }
    packet[12] = 0x88;
    packet[13] = 0xb5;
    const char payload[] = "srvros e1000 test frame";
    uint64_t payload_len = sizeof(payload) - 1;
    for (uint64_t i = 0; i < payload_len; i++) {
        packet[14 + i] = (uint8_t)payload[i];
    }

    uint16_t length = 14 + payload_len;
    bool sent = e1000_send_frame(packet, length);
    if (sent) {
        console_printf("net: tx test len=%u\n", (uint64_t)length);
    }
    return sent;
}
