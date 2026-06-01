#include <srvros/console.h>
#include <srvros/intel_gfx.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>

#include <stdbool.h>
#include <stdint.h>

#define PCI_COMMAND 0x04
#define PCI_COMMAND_MEMORY_SPACE 0x0002
#define PCI_COMMAND_BUS_MASTER 0x0004

#define INTEL_VENDOR_ID 0x8086
#define INTEL_GFX_MMIO_VIRTUAL 0xffffc001f3000000ull
#define INTEL_GFX_MMIO_PAGES 4

struct intel_gfx_device {
    const struct pci_device *pci;
    uint64_t mmio_physical;
    uint64_t mmio_virtual;
    uint32_t vendor_device;
    uint32_t revision;
    uint64_t flags;
    uint64_t backend;
};

static struct intel_gfx_device gpu;

static bool is_broadwell_device(uint16_t device_id) {
    switch (device_id) {
    case 0x1602:
    case 0x1606:
    case 0x160a:
    case 0x160b:
    case 0x160d:
    case 0x160e:
    case 0x1612:
    case 0x1616:
    case 0x161a:
    case 0x161b:
    case 0x161d:
    case 0x161e:
    case 0x1622:
    case 0x1626:
    case 0x162a:
    case 0x162b:
    case 0x162d:
        return true;
    default:
        return false;
    }
}

static const char *generation_name(void) {
    if (gpu.pci == 0) {
        return "none";
    }
    if (is_broadwell_device(gpu.pci->device_id)) {
        return "intel-gen8-broadwell";
    }
    return "intel-display";
}

static bool map_mmio(uint32_t bar0) {
    if ((bar0 & 0x1) != 0) {
        console_write("intel-gfx: bar0 is io-space, expected mmio\n");
        return false;
    }

    uint64_t physical = bar0 & 0xfffffff0u;
    if (physical == 0) {
        console_write("intel-gfx: bar0 missing\n");
        return false;
    }

    for (uint64_t i = 0; i < INTEL_GFX_MMIO_PAGES; i++) {
        if (!vmm_map_page(INTEL_GFX_MMIO_VIRTUAL + i * 4096,
                physical + i * 4096,
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) {
            console_write("intel-gfx: mmio map failed\n");
            return false;
        }
    }

    gpu.mmio_physical = physical;
    gpu.mmio_virtual = INTEL_GFX_MMIO_VIRTUAL;
    gpu.flags |= SRV_GFX_FLAG_ACCEL_MMIO_MAPPED;
    return true;
}

static const struct pci_device *find_intel_display(void) {
    for (uint64_t i = 0; i < pci_device_count(); i++) {
        const struct pci_device *device = pci_device_at(i);
        if (device != 0 &&
            device->vendor_id == INTEL_VENDOR_ID &&
            device->class_code == 0x03) {
            return device;
        }
    }
    return 0;
}

bool intel_gfx_init(void) {
    gpu = (struct intel_gfx_device) { 0 };
    gpu.pci = find_intel_display();
    if (gpu.pci == 0) {
        console_write("intel-gfx: no Intel display controller\n");
        return false;
    }

    gpu.flags = SRV_GFX_FLAG_ACCEL_DEVICE_PRESENT |
        SRV_GFX_FLAG_ACCEL_BLITTER_PLANNED |
        SRV_GFX_FLAG_ACCEL_RENDER_PLANNED;
    gpu.backend = is_broadwell_device(gpu.pci->device_id) ?
        SRV_GFX_ACCEL_INTEL_GEN8 :
        SRV_GFX_ACCEL_NONE;

    uint16_t command = pci_read_config16(gpu.pci, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
    pci_write_config16(gpu.pci, PCI_COMMAND, command);

    (void)map_mmio(gpu.pci->bar[0]);
    gpu.vendor_device = ((uint32_t)gpu.pci->device_id << 16) | gpu.pci->vendor_id;
    gpu.revision = gpu.pci->revision;

    console_printf("intel-gfx: %s bus=%u dev=%u fn=%u id=%x rev=%u bar0=%x mmio=%x\n",
        generation_name(),
        (uint64_t)gpu.pci->bus,
        (uint64_t)gpu.pci->device,
        (uint64_t)gpu.pci->function,
        (uint64_t)gpu.vendor_device,
        (uint64_t)gpu.revision,
        (uint64_t)gpu.pci->bar[0],
        gpu.mmio_virtual);
    return true;
}

bool intel_gfx_available(void) {
    return gpu.pci != 0;
}

uint64_t intel_gfx_flags(void) {
    return gpu.flags;
}

uint64_t intel_gfx_backend(void) {
    return gpu.backend;
}

void intel_gfx_print_status(void) {
    if (!intel_gfx_available()) {
        console_write("gpu: no Intel display controller detected\n");
        return;
    }

    console_printf("gpu: %s vendor=%x device=%x rev=%u bus=%u dev=%u fn=%u\n",
        generation_name(),
        (uint64_t)gpu.pci->vendor_id,
        (uint64_t)gpu.pci->device_id,
        (uint64_t)gpu.revision,
        (uint64_t)gpu.pci->bus,
        (uint64_t)gpu.pci->device,
        (uint64_t)gpu.pci->function);
    console_printf("gpu: bar0=%x mmio_phys=%x mmio_virt=%x flags=%x backend=%u\n",
        (uint64_t)gpu.pci->bar[0],
        gpu.mmio_physical,
        gpu.mmio_virtual,
        gpu.flags,
        gpu.backend);
    console_write("gpu: acceleration path staged: software -> intel blitter -> intel render\n");
}
