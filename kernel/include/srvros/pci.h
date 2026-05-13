#ifndef SRVROS_PCI_H
#define SRVROS_PCI_H

#include <stdbool.h>
#include <stdint.h>

#define PCI_MAX_DEVICES 128

struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint32_t bar[6];
};

void pci_init(void);
uint64_t pci_device_count(void);
const struct pci_device *pci_device_at(uint64_t index);
const struct pci_device *pci_find_by_class(uint8_t class_code, uint8_t subclass);
const char *pci_class_name(uint8_t class_code, uint8_t subclass);
uint32_t pci_read_config32(const struct pci_device *device, uint8_t offset);
void pci_write_config32(const struct pci_device *device, uint8_t offset, uint32_t value);
uint16_t pci_read_config16(const struct pci_device *device, uint8_t offset);
void pci_write_config16(const struct pci_device *device, uint8_t offset, uint16_t value);

#endif
