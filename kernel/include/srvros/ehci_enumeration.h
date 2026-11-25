#ifndef SRVROS_EHCI_ENUMERATION_H
#define SRVROS_EHCI_ENUMERATION_H

#include <srvros/usb_control.h>

#include <stdbool.h>

#define EHCI_DESCRIPTOR_BYTES 512u

/* A high-speed device has no TT route. A full-speed device uses its
 * high-speed parent hub's address and one-based downstream port. */
struct ehci_control_target {
    uint8_t address;
    uint8_t packet_size;
    uint8_t tt_hub;
    uint8_t tt_port;
};

static inline bool ehci_full_speed_packet_size(uint8_t size) {
    return size == 8 || size == 16 || size == 32 || size == 64;
}

struct ehci_scan_step {
    const char *operation;
    uint32_t port_status;
    uint8_t hub;
    uint8_t port;
    bool port_status_valid;
};

struct ehci_enumeration {
    bool (*control)(void *context, const struct ehci_control_target *target, const struct usb_setup_packet *setup,
        uint8_t *data, unsigned *received);
    void *context;
    void (*device_found)(void *context, const struct ehci_control_target *target,
        uint16_t vendor, uint16_t product, uint8_t parent, uint8_t port,
        const uint8_t *configuration, uint16_t configuration_length);
    bool split;
    unsigned devices;
    unsigned split_devices;
    unsigned hubs;
    unsigned deferred;
    struct ehci_scan_step step;
};

/* The caller has enabled the port and isolated address zero. */
bool ehci_enumerate_device(struct ehci_enumeration *bus, uint8_t parent, uint8_t port, unsigned depth,
    bool full_speed);

#endif
