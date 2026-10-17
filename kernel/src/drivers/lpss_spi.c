#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/lpss_spi.h>
#include <srvros/pci.h>
#include <srvros/scheduler.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stdint.h>

#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x0002
#define PCI_COMMAND_BUS_MASTER 0x0004

#define INTEL_VENDOR_ID 0x8086
/* 0x9cba is MEI, not SPI. Linux identifies this GSPI controller at 00:15.4. */
#define BROADWELL_LPSS_SPI1_DEVICE_ID 0x9ce6
#define BROADWELL_LPSS_DMA_DEVICE_ID 0x9ce0

#define LPSS_SPI_MMIO_VIRTUAL 0xffffc001f4000000ull
#define LPSS_SPI_MMIO_PAGES 2
#define LPSS_SPI_PAGE_SIZE 4096ull
#define LPSS_SPI_PAGE_MASK (LPSS_SPI_PAGE_SIZE - 1)

#define PXA_SSCR0 0x00
#define PXA_SSCR1 0x04
#define PXA_SSSR 0x08
#define PXA_SSITR 0x0c
#define PXA_SSDR 0x10
#define PXA_SSTO 0x28
#define PXA_SSPSP 0x2c

#define LPSS_PRIV_RAW_COUNT 8
#define LPSS_PRIV_RAW_STRIDE 4
#define LPSS_PRIV_SSP_REG_DIS_DMA_FIN (1u << 0)
#define LPSS_PRIV_CS_CONTROL_SW_MODE (1u << 0)
#define LPSS_PRIV_CS_CONTROL_CS_HIGH (1u << 1)
#define LPSS_PRIV_GENERAL_RXTO_HOLDOFF_DISABLE (1u << 24)
#define PXA_SSCR0_SSE (1u << 7)
#define PXA_SSCR0_DSS_8BIT 7u
#define PXA_SSCR0_SCR(value) ((value) << 8)
#define PXA_SSSR_TNF (1u << 2)
#define PXA_SSSR_RNE (1u << 3)
#define PXA_SSSR_BSY (1u << 4)

#define APPLE_SPI_PACKET_SIZE 256
#define APPLE_SPI_PACKET_DATA_SIZE 246
#define APPLE_SPI_READ_PACKET 0x20
#define APPLE_SPI_DEVICE_KEYBOARD 0x01
#define APPLE_SPI_MESSAGE_KEYBOARD 0x0110
#define APPLE_SPI_WAIT_LIMIT 100000

struct lpss_spi_profile {
    const char *name;
    uint32_t private_base;
    uint32_t general_offset;
    uint32_t ssp_offset;
    uint32_t cs_control_offset;
    bool has_general;
    bool has_ssp;
    bool has_cs_control;
    bool has_capabilities;
};

static const struct lpss_spi_profile lpss_profile_broadwell_lpt = {
    .name = "lpt",
    .private_base = 0x800,
    .general_offset = 0x08,
    .ssp_offset = 0x0c,
    .cs_control_offset = 0x18,
    .has_general = true,
    .has_ssp = true,
    .has_cs_control = true,
    .has_capabilities = false,
};

struct lpss_spi_device {
    const struct pci_device *spi;
    const struct pci_device *dma;
    const struct lpss_spi_profile *profile;
    uint16_t command;
    uint64_t mmio_physical;
    uint64_t mmio_mapped_physical;
    uint64_t mmio_virtual;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t mmio_sample[4];
    uint32_t regs[6];
    uint32_t pre_regs[6];
    uint32_t pre_priv_regs[8];
    uint32_t priv_regs[8];
    bool bar_is_64;
    bool mapped;
    bool mmio_sample_valid;
    bool regs_valid;
    bool pre_regs_valid;
    bool prepared;
    bool prepare_skipped_no_profile;
    bool prepare_skipped_unmapped;
    bool worker_started;
    uint8_t previous_keyboard_report[8];
    uint64_t polls;
    uint64_t packets;
    uint64_t keyboard_reports;
    uint64_t transfer_errors;
    uint64_t packet_errors;
    uint64_t packet_crc_errors;
    uint64_t message_crc_errors;
};

static struct lpss_spi_device lpss_spi;

static const struct pci_device *find_intel_device(uint16_t device_id) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device != 0 &&
            device->vendor_id == INTEL_VENDOR_ID &&
            device->device_id == device_id) {
            return device;
        }
    }
    return 0;
}

static bool decode_mmio_bar(const struct pci_device *device,
    uint64_t *physical_out,
    bool *bar_is_64_out) {
    if (device == 0 || physical_out == 0 || bar_is_64_out == 0) {
        return false;
    }

    uint32_t bar0 = device->bar[0];
    if (bar0 == 0 || (bar0 & 0x1) != 0) {
        return false;
    }

    bool is_64 = ((bar0 >> 1) & 0x3) == 0x2;
    uint64_t physical = bar0 & 0xfffffff0u;
    if (is_64) {
        physical |= (uint64_t)device->bar[1] << 32;
    }
    if (physical == 0) {
        return false;
    }

    *physical_out = physical;
    *bar_is_64_out = is_64;
    return true;
}

static bool map_spi_mmio(void) {
    if ((lpss_spi.command & PCI_COMMAND_MEMORY_SPACE) == 0) {
        return false;
    }

    uint64_t page_offset = lpss_spi.mmio_physical & LPSS_SPI_PAGE_MASK;
    lpss_spi.mmio_mapped_physical = lpss_spi.mmio_physical & ~LPSS_SPI_PAGE_MASK;
    for (uint64_t i = 0; i < LPSS_SPI_MMIO_PAGES; i++) {
        if (!vmm_map_page(LPSS_SPI_MMIO_VIRTUAL + i * LPSS_SPI_PAGE_SIZE,
                lpss_spi.mmio_mapped_physical + i * LPSS_SPI_PAGE_SIZE,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
            return false;
        }
    }

    lpss_spi.mmio_virtual = LPSS_SPI_MMIO_VIRTUAL + page_offset;
    lpss_spi.mapped = true;
    return true;
}

static void sample_mmio(void) {
    if (!lpss_spi.mapped) {
        return;
    }

    volatile uint32_t *regs = (volatile uint32_t *)lpss_spi.mmio_virtual;
    for (uint64_t i = 0; i < sizeof(lpss_spi.mmio_sample) / sizeof(lpss_spi.mmio_sample[0]); i++) {
        lpss_spi.mmio_sample[i] = regs[i];
    }
    lpss_spi.mmio_sample_valid = true;
}

static uint32_t lpss_mmio_read32(uint32_t offset) {
    volatile uint32_t *reg = (volatile uint32_t *)(lpss_spi.mmio_virtual + offset);
    return *reg;
}

static void lpss_mmio_write32(uint32_t offset, uint32_t value) {
    volatile uint32_t *reg = (volatile uint32_t *)(lpss_spi.mmio_virtual + offset);
    *reg = value;
}

static void sample_register_bank(uint32_t regs[6], uint32_t priv_regs[8]) {
    regs[0] = lpss_mmio_read32(PXA_SSCR0);
    regs[1] = lpss_mmio_read32(PXA_SSCR1);
    regs[2] = lpss_mmio_read32(PXA_SSSR);
    regs[3] = lpss_mmio_read32(PXA_SSITR);
    regs[4] = lpss_mmio_read32(PXA_SSTO);
    regs[5] = lpss_mmio_read32(PXA_SSPSP);

    uint32_t private_base = lpss_spi.profile != 0 ? lpss_spi.profile->private_base : 0;
    for (uint64_t i = 0; i < LPSS_PRIV_RAW_COUNT; i++) {
        priv_regs[i] = lpss_mmio_read32(private_base + (uint32_t)i * LPSS_PRIV_RAW_STRIDE);
    }
}

static void sample_named_registers(void) {
    if (!lpss_spi.mapped) {
        return;
    }

    sample_register_bank(lpss_spi.regs, lpss_spi.priv_regs);
    lpss_spi.regs_valid = true;
}

static void lpss_spi_write_private(uint32_t offset, uint32_t value) {
    if (lpss_spi.profile == 0) {
        return;
    }
    lpss_mmio_write32(lpss_spi.profile->private_base + offset, value);
}

static uint32_t lpss_spi_read_private(uint32_t offset) {
    if (lpss_spi.profile == 0) {
        return 0xffffffffu;
    }
    return lpss_mmio_read32(lpss_spi.profile->private_base + offset);
}

static void lpss_spi_apply_profile_setup(void) {
    const struct lpss_spi_profile *profile = lpss_spi.profile;
    if (profile == 0) {
        lpss_spi.prepare_skipped_no_profile = true;
        return;
    }

    if (profile->has_cs_control) {
        uint32_t cs_control = lpss_spi_read_private(profile->cs_control_offset);
        cs_control |= LPSS_PRIV_CS_CONTROL_SW_MODE | LPSS_PRIV_CS_CONTROL_CS_HIGH;
        lpss_spi_write_private(profile->cs_control_offset, cs_control);
    }

    if (profile->has_ssp) {
        uint32_t ssp = lpss_spi_read_private(profile->ssp_offset);
        ssp |= LPSS_PRIV_SSP_REG_DIS_DMA_FIN;
        lpss_spi_write_private(profile->ssp_offset, ssp);
    }

    if (profile->has_general) {
        uint32_t general = lpss_spi_read_private(profile->general_offset);
        general |= LPSS_PRIV_GENERAL_RXTO_HOLDOFF_DISABLE;
        lpss_spi_write_private(profile->general_offset, general);
    }

    lpss_spi.prepared = true;
}

static void lpss_spi_prepare_controller(void) {
    if (!lpss_spi.mapped) {
        lpss_spi.prepare_skipped_unmapped = true;
        return;
    }

    sample_register_bank(lpss_spi.pre_regs, lpss_spi.pre_priv_regs);
    lpss_spi.pre_regs_valid = true;

    lpss_spi_apply_profile_setup();
    sample_named_registers();
}

static uint16_t read_le16(const uint8_t *value) {
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint16_t apple_crc16(const uint8_t *data, uint64_t length) {
    uint16_t crc = 0;
    for (uint64_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1) != 0 ? (uint16_t)((crc >> 1) ^ 0xa001u) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static bool wait_status(uint32_t mask) {
    for (uint64_t i = 0; i < APPLE_SPI_WAIT_LIMIT; i++) {
        if ((lpss_mmio_read32(PXA_SSSR) & mask) == mask) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    return false;
}

static void set_chip_select(bool selected) {
    const struct lpss_spi_profile *profile = lpss_spi.profile;
    if (profile == 0 || !profile->has_cs_control) {
        return;
    }
    uint32_t control = lpss_spi_read_private(profile->cs_control_offset);
    control |= LPSS_PRIV_CS_CONTROL_SW_MODE;
    if (selected) {
        control &= ~LPSS_PRIV_CS_CONTROL_CS_HIGH;
    } else {
        control |= LPSS_PRIV_CS_CONTROL_CS_HIGH;
    }
    lpss_spi_write_private(profile->cs_control_offset, control);
}

static bool read_packet(uint8_t packet[APPLE_SPI_PACKET_SIZE]) {
    uint32_t original_sscr0 = lpss_mmio_read32(PXA_SSCR0);
    lpss_mmio_write32(PXA_SSCR0, original_sscr0 & ~PXA_SSCR0_SSE);
    lpss_mmio_write32(PXA_SSCR1, 0);
    lpss_mmio_write32(PXA_SSCR0,
        PXA_SSCR0_DSS_8BIT | PXA_SSCR0_SCR(4) | PXA_SSCR0_SSE);
    set_chip_select(true);

    bool ok = true;
    for (uint64_t i = 0; i < APPLE_SPI_PACKET_SIZE; i++) {
        if (!wait_status(PXA_SSSR_TNF)) {
            ok = false;
            break;
        }
        lpss_mmio_write32(PXA_SSDR, 0);
        if (!wait_status(PXA_SSSR_RNE)) {
            ok = false;
            break;
        }
        packet[i] = (uint8_t)lpss_mmio_read32(PXA_SSDR);
    }

    set_chip_select(false);
    lpss_mmio_write32(PXA_SSCR0, original_sscr0 & ~PXA_SSCR0_SSE);
    lpss_mmio_write32(PXA_SSCR0, original_sscr0);
    return ok;
}

static bool parse_keyboard_packet(const uint8_t packet[APPLE_SPI_PACKET_SIZE]) {
    if (apple_crc16(packet, APPLE_SPI_PACKET_SIZE) != 0) {
        lpss_spi.packet_crc_errors++;
        return false;
    }
    if (packet[0] != APPLE_SPI_READ_PACKET ||
        packet[1] != APPLE_SPI_DEVICE_KEYBOARD ||
        read_le16(&packet[2]) != 0 || read_le16(&packet[4]) != 0) {
        lpss_spi.packet_errors++;
        return false;
    }

    uint16_t packet_length = read_le16(&packet[6]);
    if (packet_length < 20 || packet_length > APPLE_SPI_PACKET_DATA_SIZE) {
        lpss_spi.packet_errors++;
        return false;
    }
    const uint8_t *message = &packet[8];
    if (read_le16(message) != APPLE_SPI_MESSAGE_KEYBOARD || message[2] != 0) {
        lpss_spi.packet_errors++;
        return false;
    }
    uint16_t message_length = read_le16(&message[6]);
    if (message_length < 12 || (uint32_t)message_length + 8u > packet_length) {
        lpss_spi.packet_errors++;
        return false;
    }
    const uint8_t *keyboard = &message[8];
    if (apple_crc16(keyboard, message_length) != 0) {
        lpss_spi.message_crc_errors++;
        return false;
    }

    uint8_t report[8] = { 0 };
    report[0] = keyboard[1];
    for (uint64_t i = 0; i < 6; i++) {
        report[i + 2] = keyboard[i + 3];
    }
    keyboard_inject_hid_report(report, lpss_spi.previous_keyboard_report);
    lpss_spi.keyboard_reports++;
    return true;
}

bool lpss_spi_poll_once(void) {
    if (!lpss_spi.mapped || !lpss_spi.prepared) {
        return false;
    }
    lpss_spi.polls++;
    if ((lpss_mmio_read32(PXA_SSSR) & PXA_SSSR_RNE) == 0) {
        return false;
    }

    uint8_t packet[APPLE_SPI_PACKET_SIZE];
    if (!read_packet(packet)) {
        lpss_spi.transfer_errors++;
        return false;
    }
    lpss_spi.packets++;
    return parse_keyboard_packet(packet);
}

static void lpss_spi_worker(void *arg) {
    (void)arg;
    for (;;) {
        (void)lpss_spi_poll_once();
        scheduler_yield();
    }
}

bool lpss_spi_start_worker(void) {
    if (!lpss_spi.mapped || !lpss_spi.prepared) {
        return false;
    }
    if (lpss_spi.worker_started) {
        return true;
    }
    lpss_spi.worker_started = scheduler_spawn("lpss-spi", lpss_spi_worker, 0);
    if (lpss_spi.worker_started) {
        console_write("lpss-spi: polling Apple topcase worker started\n");
    }
    return lpss_spi.worker_started;
}

bool lpss_spi_init(void) {
    lpss_spi = (struct lpss_spi_device) { 0 };
    lpss_spi.spi = find_intel_device(BROADWELL_LPSS_SPI1_DEVICE_ID);
    lpss_spi.dma = find_intel_device(BROADWELL_LPSS_DMA_DEVICE_ID);
    lpss_spi.profile = &lpss_profile_broadwell_lpt;
    if (lpss_spi.spi != 0 &&
        (lpss_spi.spi->class_code != 0x0c || lpss_spi.spi->subclass != 0x80)) {
        console_write("lpss-spi: refusing controller with unexpected PCI class\n");
        lpss_spi.spi = 0;
    }
    if (lpss_spi.spi == 0) {
        console_write("lpss-spi: no Broadwell SPI1 controller\n");
        return false;
    }

    lpss_spi.command = pci_read_config16(lpss_spi.spi, PCI_COMMAND);
    lpss_spi.bar0 = lpss_spi.spi->bar[0];
    lpss_spi.bar1 = lpss_spi.spi->bar[1];
    if (decode_mmio_bar(lpss_spi.spi, &lpss_spi.mmio_physical, &lpss_spi.bar_is_64)) {
        if (map_spi_mmio()) {
            sample_mmio();
            lpss_spi_prepare_controller();
        }
    }

    console_printf("lpss-spi: spi1 bus=%u dev=%u fn=%u id=%x class=%x:%x:%x irq=%u pin=%u cmd=%x bar0=%x bar1=%x mmio=%x mapped=%u profile=%s base=%x\n",
        (uint64_t)lpss_spi.spi->bus,
        (uint64_t)lpss_spi.spi->device,
        (uint64_t)lpss_spi.spi->function,
        ((uint64_t)lpss_spi.spi->device_id << 16) | lpss_spi.spi->vendor_id,
        (uint64_t)lpss_spi.spi->class_code,
        (uint64_t)lpss_spi.spi->subclass,
        (uint64_t)lpss_spi.spi->prog_if,
        (uint64_t)lpss_spi.spi->interrupt_line,
        (uint64_t)lpss_spi.spi->interrupt_pin,
        (uint64_t)lpss_spi.command,
        (uint64_t)lpss_spi.bar0,
        (uint64_t)lpss_spi.bar1,
        lpss_spi.mmio_physical,
        lpss_spi.mapped ? 1ull : 0ull,
        lpss_spi.profile != 0 ? lpss_spi.profile->name : "none",
        lpss_spi.profile != 0 ? (uint64_t)lpss_spi.profile->private_base : 0ull);
    if (lpss_spi.dma != 0) {
        console_printf("lpss-spi: dma bus=%u dev=%u fn=%u id=%x class=%x:%x:%x irq=%u pin=%u bar0=%x\n",
            (uint64_t)lpss_spi.dma->bus,
            (uint64_t)lpss_spi.dma->device,
            (uint64_t)lpss_spi.dma->function,
            ((uint64_t)lpss_spi.dma->device_id << 16) | lpss_spi.dma->vendor_id,
            (uint64_t)lpss_spi.dma->class_code,
            (uint64_t)lpss_spi.dma->subclass,
            (uint64_t)lpss_spi.dma->prog_if,
            (uint64_t)lpss_spi.dma->interrupt_line,
            (uint64_t)lpss_spi.dma->interrupt_pin,
            (uint64_t)lpss_spi.dma->bar[0]);
    } else {
        console_write("lpss-spi: paired lpss dma not found\n");
    }

    return true;
}

bool lpss_spi_available(void) {
    return lpss_spi.spi != 0;
}

void lpss_spi_print_status(void) {
    if (!lpss_spi_available()) {
        console_write("lpss-spi: no Broadwell SPI1 controller detected\n");
        return;
    }

    console_printf("lpss-spi: spi1 present vendor=%x device=%x bus=%u dev=%u fn=%u command=%x memory=%u busmaster=%u\n",
        (uint64_t)lpss_spi.spi->vendor_id,
        (uint64_t)lpss_spi.spi->device_id,
        (uint64_t)lpss_spi.spi->bus,
        (uint64_t)lpss_spi.spi->device,
        (uint64_t)lpss_spi.spi->function,
        (uint64_t)lpss_spi.command,
        (lpss_spi.command & PCI_COMMAND_MEMORY_SPACE) != 0 ? 1ull : 0ull,
        (lpss_spi.command & PCI_COMMAND_BUS_MASTER) != 0 ? 1ull : 0ull);
    console_printf("lpss-spi: bar0=%x bar1=%x bar64=%u mmio_phys=%x mapped_phys=%x mmio_virt=%x mapped=%u\n",
        (uint64_t)lpss_spi.bar0,
        (uint64_t)lpss_spi.bar1,
        lpss_spi.bar_is_64 ? 1ull : 0ull,
        lpss_spi.mmio_physical,
        lpss_spi.mmio_mapped_physical,
        lpss_spi.mmio_virtual,
        lpss_spi.mapped ? 1ull : 0ull);
    if (lpss_spi.mmio_sample_valid) {
        console_printf("lpss-spi: mmio[0..3]=%x %x %x %x\n",
            (uint64_t)lpss_spi.mmio_sample[0],
            (uint64_t)lpss_spi.mmio_sample[1],
            (uint64_t)lpss_spi.mmio_sample[2],
            (uint64_t)lpss_spi.mmio_sample[3]);
    }
    console_printf("lpss-spi: profile=%s private_base=%x prepared=%u skip_no_profile=%u skip_unmapped=%u\n",
        lpss_spi.profile != 0 ? lpss_spi.profile->name : "none",
        lpss_spi.profile != 0 ? (uint64_t)lpss_spi.profile->private_base : 0ull,
        lpss_spi.prepared ? 1ull : 0ull,
        lpss_spi.prepare_skipped_no_profile ? 1ull : 0ull,
        lpss_spi.prepare_skipped_unmapped ? 1ull : 0ull);
    console_printf("lpss-spi: worker=%u polls=%u packets=%u keyboard_reports=%u transfer_errors=%u packet_errors=%u packet_crc_errors=%u message_crc_errors=%u\n",
        lpss_spi.worker_started ? 1ull : 0ull,
        lpss_spi.polls,
        lpss_spi.packets,
        lpss_spi.keyboard_reports,
        lpss_spi.transfer_errors,
        lpss_spi.packet_errors,
        lpss_spi.packet_crc_errors,
        lpss_spi.message_crc_errors);
    lpss_spi_print_registers();
    if (lpss_spi.dma != 0) {
        console_printf("lpss-spi: dma present vendor=%x device=%x bus=%u dev=%u fn=%u class=%x:%x:%x irq=%u\n",
            (uint64_t)lpss_spi.dma->vendor_id,
            (uint64_t)lpss_spi.dma->device_id,
            (uint64_t)lpss_spi.dma->bus,
            (uint64_t)lpss_spi.dma->device,
            (uint64_t)lpss_spi.dma->function,
            (uint64_t)lpss_spi.dma->class_code,
            (uint64_t)lpss_spi.dma->subclass,
            (uint64_t)lpss_spi.dma->prog_if,
            (uint64_t)lpss_spi.dma->interrupt_line);
    }
}

void lpss_spi_print_registers(void) {
    if (!lpss_spi_available()) {
        console_write("lpss-spi-regs: controller unavailable\n");
        return;
    }
    if (!lpss_spi.regs_valid) {
        console_write("lpss-spi-regs: unavailable; mmio not mapped\n");
        return;
    }

    sample_named_registers();
    if (lpss_spi.pre_regs_valid) {
        console_printf("lpss-spi-regs-pre: sscr0=%x sscr1=%x sssr=%x ssitr=%x ssto=%x sspsp=%x\n",
            (uint64_t)lpss_spi.pre_regs[0],
            (uint64_t)lpss_spi.pre_regs[1],
            (uint64_t)lpss_spi.pre_regs[2],
            (uint64_t)lpss_spi.pre_regs[3],
            (uint64_t)lpss_spi.pre_regs[4],
            (uint64_t)lpss_spi.pre_regs[5]);
        console_printf("lpss-spi-priv-pre: profile=%s base=%x raw00=%x raw04=%x raw08=%x raw0c=%x raw10=%x raw14=%x raw18=%x raw1c=%x\n",
            lpss_spi.profile != 0 ? lpss_spi.profile->name : "none",
            lpss_spi.profile != 0 ? (uint64_t)lpss_spi.profile->private_base : 0ull,
            (uint64_t)lpss_spi.pre_priv_regs[0],
            (uint64_t)lpss_spi.pre_priv_regs[1],
            (uint64_t)lpss_spi.pre_priv_regs[2],
            (uint64_t)lpss_spi.pre_priv_regs[3],
            (uint64_t)lpss_spi.pre_priv_regs[4],
            (uint64_t)lpss_spi.pre_priv_regs[5],
            (uint64_t)lpss_spi.pre_priv_regs[6],
            (uint64_t)lpss_spi.pre_priv_regs[7]);
    }
    console_printf("lpss-spi-regs: sscr0=%x sscr1=%x sssr=%x ssitr=%x ssto=%x sspsp=%x\n",
        (uint64_t)lpss_spi.regs[0],
        (uint64_t)lpss_spi.regs[1],
        (uint64_t)lpss_spi.regs[2],
        (uint64_t)lpss_spi.regs[3],
        (uint64_t)lpss_spi.regs[4],
        (uint64_t)lpss_spi.regs[5]);
    console_printf("lpss-spi-regs: enabled=%u busy=%u tx_not_full=%u rx_not_empty=%u\n",
        (lpss_spi.regs[0] & PXA_SSCR0_SSE) != 0 ? 1ull : 0ull,
        (lpss_spi.regs[2] & PXA_SSSR_BSY) != 0 ? 1ull : 0ull,
        (lpss_spi.regs[2] & PXA_SSSR_TNF) != 0 ? 1ull : 0ull,
        (lpss_spi.regs[2] & PXA_SSSR_RNE) != 0 ? 1ull : 0ull);
    console_printf("lpss-spi-priv: profile=%s base=%x raw00=%x raw04=%x raw08=%x raw0c=%x raw10=%x raw14=%x raw18=%x raw1c=%x\n",
        lpss_spi.profile != 0 ? lpss_spi.profile->name : "none",
        lpss_spi.profile != 0 ? (uint64_t)lpss_spi.profile->private_base : 0ull,
        (uint64_t)lpss_spi.priv_regs[0],
        (uint64_t)lpss_spi.priv_regs[1],
        (uint64_t)lpss_spi.priv_regs[2],
        (uint64_t)lpss_spi.priv_regs[3],
        (uint64_t)lpss_spi.priv_regs[4],
        (uint64_t)lpss_spi.priv_regs[5],
        (uint64_t)lpss_spi.priv_regs[6],
        (uint64_t)lpss_spi.priv_regs[7]);
}
