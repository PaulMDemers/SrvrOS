#ifndef SRVROS_EHCI_ENUMERATION_H
#define SRVROS_EHCI_ENUMERATION_H

#include <srvros/usb_control.h>

#include <stdbool.h>

#define EHCI_DESCRIPTOR_BYTES 512u

struct ehci_enumeration {
    bool (*control)(void *context, uint8_t address, const struct usb_setup_packet *setup,
        uint8_t *data, unsigned *received);
    void *context;
    unsigned devices;
    unsigned hubs;
    unsigned deferred;
};

/* The caller has enabled this high-speed port and isolated address zero. */
bool ehci_enumerate_device(struct ehci_enumeration *bus, uint8_t parent, uint8_t port, unsigned depth);

#endif
