#include <srvros/console.h>
#include <srvros/pci.h>

#include "io.h"

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define PCI_VENDOR_INVALID 0xffff

static struct pci_device devices[PCI_MAX_DEVICES];
static uint64_t device_count;

static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xfc);

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, device, function, offset);
    return (value >> ((offset & 2) * 8)) & 0xffff;
}

static void pci_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address =
        (1u << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xfc);

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static void pci_write16_raw(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t aligned = pci_read32(bus, device, function, offset);
    uint32_t shift = (offset & 2) * 8;
    aligned &= ~(0xffffu << shift);
    aligned |= (uint32_t)value << shift;
    pci_write32(bus, device, function, offset, aligned);
}

static uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read32(bus, device, function, offset);
    return (value >> ((offset & 3) * 8)) & 0xff;
}

const char *pci_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
    case 0x01:
        return "storage";
    case 0x02:
        if (subclass == 0x00) {
            return "ethernet";
        }
        return "network";
    case 0x03:
        return "display";
    case 0x06:
        if (subclass == 0x00) {
            return "host bridge";
        }
        if (subclass == 0x01) {
            return "isa bridge";
        }
        if (subclass == 0x04) {
            return "pci bridge";
        }
        return "bridge";
    case 0x0c:
        return "serial bus";
    default:
        return "device";
    }
}

static void record_device(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor = pci_read16(bus, device, function, 0x00);
    if (vendor == PCI_VENDOR_INVALID || device_count >= PCI_MAX_DEVICES) {
        return;
    }

    uint16_t device_id = pci_read16(bus, device, function, 0x02);
    uint32_t class_reg = pci_read32(bus, device, function, 0x08);
    struct pci_device *out = &devices[device_count++];

    out->bus = bus;
    out->device = device;
    out->function = function;
    out->vendor_id = vendor;
    out->device_id = device_id;
    out->revision = class_reg & 0xff;
    out->prog_if = (class_reg >> 8) & 0xff;
    out->subclass = (class_reg >> 16) & 0xff;
    out->class_code = (class_reg >> 24) & 0xff;
    out->header_type = pci_read8(bus, device, function, 0x0e);
    out->interrupt_line = pci_read8(bus, device, function, 0x3c);
    out->interrupt_pin = pci_read8(bus, device, function, 0x3d);

    uint8_t header_layout = out->header_type & 0x7f;
    if (header_layout == 0x00) {
        for (uint8_t i = 0; i < 6; i++) {
            out->bar[i] = pci_read32(bus, device, function, 0x10 + i * 4);
        }
    } else if (header_layout == 0x01) {
        for (uint8_t i = 0; i < 2; i++) {
            out->bar[i] = pci_read32(bus, device, function, 0x10 + i * 4);
        }
    }
}

static void scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    if (pci_read16(bus, device, function, 0x00) == PCI_VENDOR_INVALID) {
        return;
    }

    record_device(bus, device, function);
}

static void scan_device(uint8_t bus, uint8_t device) {
    if (pci_read16(bus, device, 0, 0x00) == PCI_VENDOR_INVALID) {
        return;
    }

    scan_function(bus, device, 0);
    uint8_t header_type = pci_read8(bus, device, 0, 0x0e);
    if ((header_type & 0x80) != 0) {
        for (uint8_t function = 1; function < 8; function++) {
            scan_function(bus, device, function);
        }
    }
}

void pci_init(void) {
    device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            scan_device((uint8_t)bus, device);
        }
    }

    console_printf("pci: devices=%u\n", device_count);
    for (uint64_t i = 0; i < device_count; i++) {
        const struct pci_device *dev = &devices[i];
        if (dev->class_code == 0x02) {
            console_printf("pci: network bus=%u dev=%u fn=%u vendor=%x device=%x type=%s irq=%u bar0=%x\n",
                (uint64_t)dev->bus,
                (uint64_t)dev->device,
                dev->function,
                (uint64_t)dev->vendor_id,
                (uint64_t)dev->device_id,
                pci_class_name(dev->class_code, dev->subclass),
                (uint64_t)dev->interrupt_line,
                (uint64_t)dev->bar[0]);
        }
    }
}

uint64_t pci_device_count(void) {
    return device_count;
}

const struct pci_device *pci_device_at(uint64_t index) {
    if (index >= device_count) {
        return 0;
    }
    return &devices[index];
}

const struct pci_device *pci_find_by_class(uint8_t class_code, uint8_t subclass) {
    for (uint64_t i = 0; i < device_count; i++) {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass) {
            return &devices[i];
        }
    }
    return 0;
}

uint16_t pci_read_config16(const struct pci_device *device, uint8_t offset) {
    if (device == 0) {
        return 0xffff;
    }

    return pci_read16(device->bus, device->device, device->function, offset);
}

uint32_t pci_read_config32(const struct pci_device *device, uint8_t offset) {
    if (device == 0) {
        return 0xffffffffu;
    }

    return pci_read32(device->bus, device->device, device->function, offset);
}

void pci_write_config32(const struct pci_device *device, uint8_t offset, uint32_t value) {
    if (device == 0) {
        return;
    }

    pci_write32(device->bus, device->device, device->function, offset, value);
}

void pci_write_config16(const struct pci_device *device, uint8_t offset, uint16_t value) {
    if (device == 0) {
        return;
    }

    pci_write16_raw(device->bus, device->device, device->function, offset, value);
}
