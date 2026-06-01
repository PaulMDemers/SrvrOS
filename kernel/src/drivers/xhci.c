#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>
#include <srvros/xhci.h>

#include <stdbool.h>
#include <stdint.h>

#define XHCI_CLASS_SERIAL_BUS 0x0c
#define XHCI_SUBCLASS_USB 0x03
#define XHCI_PROG_IF_XHCI 0x30
#define XHCI_MMIO_VIRTUAL_BASE 0xffffc00120000000ull
#define XHCI_MMIO_MAP_SIZE (64ull * 1024ull)

struct xhci_state {
    bool present;
    const struct pci_device *pci;
    uint64_t mmio_phys;
    uint64_t mmio_virt;
    uint8_t cap_length;
    uint16_t hci_version;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t db_off;
    uint32_t rts_off;
};

static struct xhci_state xhci;

static uint32_t mmio_read32(uint64_t offset) {
    return *(volatile uint32_t *)(xhci.mmio_virt + offset);
}

static uint8_t mmio_read8(uint64_t offset) {
    return *(volatile uint8_t *)(xhci.mmio_virt + offset);
}

static uint64_t bar_mmio_base(const struct pci_device *device) {
    uint64_t low = device->bar[0] & 0xfffffff0ull;
    if ((device->bar[0] & 0x6) == 0x4) {
        return low | ((uint64_t)device->bar[1] << 32);
    }
    return low;
}

static const struct pci_device *find_xhci_controller(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device != 0 &&
            device->class_code == XHCI_CLASS_SERIAL_BUS &&
            device->subclass == XHCI_SUBCLASS_USB &&
            device->prog_if == XHCI_PROG_IF_XHCI) {
            return device;
        }
    }
    return 0;
}

static bool map_mmio(uint64_t physical) {
    uint64_t page_base = physical & ~0xfffull;
    uint64_t page_offset = physical - page_base;
    uint64_t virtual_base = XHCI_MMIO_VIRTUAL_BASE;

    for (uint64_t offset = 0; offset < XHCI_MMIO_MAP_SIZE; offset += 4096) {
        if (!vmm_map_page(virtual_base + offset,
                page_base + offset,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
            return false;
        }
    }

    xhci.mmio_virt = virtual_base + page_offset;
    return true;
}

void xhci_init(void) {
    xhci = (struct xhci_state) { 0 };
    xhci.pci = find_xhci_controller();
    if (xhci.pci == 0) {
        console_write("xhci: no controller\n");
        return;
    }

    uint16_t command = pci_read_config16(xhci.pci, 0x04);
    command |= 0x0006;
    pci_write_config16(xhci.pci, 0x04, command);

    xhci.mmio_phys = bar_mmio_base(xhci.pci);
    if (xhci.mmio_phys == 0 || !map_mmio(xhci.mmio_phys)) {
        console_printf("xhci: mmio map failed bar0=%x\n", xhci.mmio_phys);
        return;
    }

    xhci.cap_length = mmio_read8(0x00);
    xhci.hci_version = (uint16_t)(mmio_read32(0x00) >> 16);
    xhci.hcs_params1 = mmio_read32(0x04);
    xhci.hcs_params2 = mmio_read32(0x08);
    xhci.hcs_params3 = mmio_read32(0x0c);
    xhci.hcc_params1 = mmio_read32(0x10);
    xhci.db_off = mmio_read32(0x14) & ~0x3u;
    xhci.rts_off = mmio_read32(0x18) & ~0x1fu;
    xhci.present = true;

    console_printf("xhci: bus=%u dev=%u fn=%u vendor=%x device=%x mmio=%x version=%x ports=%u slots=%u\n",
        (uint64_t)xhci.pci->bus,
        (uint64_t)xhci.pci->device,
        (uint64_t)xhci.pci->function,
        (uint64_t)xhci.pci->vendor_id,
        (uint64_t)xhci.pci->device_id,
        xhci.mmio_phys,
        (uint64_t)xhci.hci_version,
        (uint64_t)((xhci.hcs_params1 >> 24) & 0xff),
        (uint64_t)(xhci.hcs_params1 & 0xff));
}

bool xhci_is_present(void) {
    return xhci.present;
}

void xhci_print_status(void) {
    if (!xhci.present) {
        console_write("xhci: no controller detected\n");
        return;
    }

    console_printf("xhci: vendor=%x device=%x bus=%u dev=%u fn=%u irq=%u pin=%u\n",
        (uint64_t)xhci.pci->vendor_id,
        (uint64_t)xhci.pci->device_id,
        (uint64_t)xhci.pci->bus,
        (uint64_t)xhci.pci->device,
        (uint64_t)xhci.pci->function,
        (uint64_t)xhci.pci->interrupt_line,
        (uint64_t)xhci.pci->interrupt_pin);
    console_printf("xhci: mmio_phys=%x mmio_virt=%x caplen=%u version=%x dboff=%x rtsoff=%x\n",
        xhci.mmio_phys,
        xhci.mmio_virt,
        (uint64_t)xhci.cap_length,
        (uint64_t)xhci.hci_version,
        (uint64_t)xhci.db_off,
        (uint64_t)xhci.rts_off);
    console_printf("xhci: hcs1=%x hcs2=%x hcs3=%x hcc1=%x slots=%u ports=%u scratchpads=%u\n",
        (uint64_t)xhci.hcs_params1,
        (uint64_t)xhci.hcs_params2,
        (uint64_t)xhci.hcs_params3,
        (uint64_t)xhci.hcc_params1,
        (uint64_t)(xhci.hcs_params1 & 0xff),
        (uint64_t)((xhci.hcs_params1 >> 24) & 0xff),
        (uint64_t)((xhci.hcs_params2 >> 21) & 0x1f));
}
