#include <srvros/console.h>
#include <srvros/lpss_spi.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stdint.h>

#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x0002
#define PCI_COMMAND_BUS_MASTER 0x0004

#define INTEL_VENDOR_ID 0x8086
#define BROADWELL_LPSS_SPI1_DEVICE_ID 0x9cba
#define BROADWELL_LPSS_DMA_DEVICE_ID 0x9ce0

#define LPSS_SPI_MMIO_VIRTUAL 0xffffc001f4000000ull
#define LPSS_SPI_MMIO_PAGES 2
#define LPSS_SPI_PAGE_SIZE 4096ull
#define LPSS_SPI_PAGE_MASK (LPSS_SPI_PAGE_SIZE - 1)

#define PXA_SSCR0 0x00
#define PXA_SSCR1 0x04
#define PXA_SSSR 0x08
#define PXA_SSITR 0x0c
#define PXA_SSTO 0x28
#define PXA_SSPSP 0x2c

#define LPSS_PRIV_BASE 0x800
#define LPSS_PRIV_RESETS 0x04
#define LPSS_PRIV_GENERAL 0x08
#define LPSS_PRIV_SSP 0x0c
#define LPSS_PRIV_CS_CONTROL 0x18
#define LPSS_PRIV_CLOCK_GATE 0x38

#define PXA_SSCR0_SSE (1u << 7)
#define PXA_SSSR_TNF (1u << 2)
#define PXA_SSSR_RNE (1u << 3)
#define PXA_SSSR_BSY (1u << 4)

struct lpss_spi_device {
    const struct pci_device *spi;
    const struct pci_device *dma;
    uint16_t command;
    uint64_t mmio_physical;
    uint64_t mmio_mapped_physical;
    uint64_t mmio_virtual;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t mmio_sample[4];
    uint32_t regs[6];
    uint32_t priv_regs[5];
    bool bar_is_64;
    bool mapped;
    bool mmio_sample_valid;
    bool regs_valid;
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

static void sample_named_registers(void) {
    if (!lpss_spi.mapped) {
        return;
    }

    lpss_spi.regs[0] = lpss_mmio_read32(PXA_SSCR0);
    lpss_spi.regs[1] = lpss_mmio_read32(PXA_SSCR1);
    lpss_spi.regs[2] = lpss_mmio_read32(PXA_SSSR);
    lpss_spi.regs[3] = lpss_mmio_read32(PXA_SSITR);
    lpss_spi.regs[4] = lpss_mmio_read32(PXA_SSTO);
    lpss_spi.regs[5] = lpss_mmio_read32(PXA_SSPSP);
    lpss_spi.priv_regs[0] = lpss_mmio_read32(LPSS_PRIV_BASE + LPSS_PRIV_RESETS);
    lpss_spi.priv_regs[1] = lpss_mmio_read32(LPSS_PRIV_BASE + LPSS_PRIV_GENERAL);
    lpss_spi.priv_regs[2] = lpss_mmio_read32(LPSS_PRIV_BASE + LPSS_PRIV_SSP);
    lpss_spi.priv_regs[3] = lpss_mmio_read32(LPSS_PRIV_BASE + LPSS_PRIV_CS_CONTROL);
    lpss_spi.priv_regs[4] = lpss_mmio_read32(LPSS_PRIV_BASE + LPSS_PRIV_CLOCK_GATE);
    lpss_spi.regs_valid = true;
}

bool lpss_spi_init(void) {
    lpss_spi = (struct lpss_spi_device) { 0 };
    lpss_spi.spi = find_intel_device(BROADWELL_LPSS_SPI1_DEVICE_ID);
    lpss_spi.dma = find_intel_device(BROADWELL_LPSS_DMA_DEVICE_ID);
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
            sample_named_registers();
        }
    }

    console_printf("lpss-spi: spi1 bus=%u dev=%u fn=%u id=%x class=%x:%x:%x irq=%u pin=%u cmd=%x bar0=%x bar1=%x mmio=%x mapped=%u\n",
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
        lpss_spi.mapped ? 1ull : 0ull);
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
    console_printf("lpss-spi-regs: lpss reset=%x general=%x ssp=%x cs=%x clock_gate=%x\n",
        (uint64_t)lpss_spi.priv_regs[0],
        (uint64_t)lpss_spi.priv_regs[1],
        (uint64_t)lpss_spi.priv_regs[2],
        (uint64_t)lpss_spi.priv_regs[3],
        (uint64_t)lpss_spi.priv_regs[4]);
}
