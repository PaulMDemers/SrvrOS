#include <srvros/console.h>
#include <srvros/ehci_enumeration.h>
#include <srvros/timer.h>

#define EHCI_DEVICE_LIMIT 16u
#define EHCI_HUB_PORT_LIMIT 16u
#define EHCI_HUB_DEPTH_LIMIT 4u
#define EHCI_HUB_RESET_US 200000u
#define EHCI_HUB_POLL_US 2000u
#define USB_HUB_CHANGE_CONNECTION (1u << USB_FEATURE_C_PORT_CONNECTION)
#define USB_HUB_CHANGE_RESET (1u << USB_FEATURE_C_PORT_RESET)

static uint16_t read_le16(const uint8_t *bytes) {
    return bytes[0] | ((uint16_t)bytes[1] << 8);
}

static bool usb_control(struct ehci_enumeration *bus, uint8_t address, uint8_t type, uint8_t request,
    uint16_t value, uint16_t index, uint16_t length, uint8_t *data, unsigned *received) {
    struct usb_setup_packet setup = { type, request, value, index, length };
    return bus->control(bus->context, address, &setup, data, received) && *received <= length;
}

static bool standard_descriptor(struct ehci_enumeration *bus, uint8_t address, uint16_t type,
    uint8_t *data, uint16_t length) {
    unsigned received;
    return usb_control(bus, address, 0x80, USB_REQ_GET_DESCRIPTOR, type << 8, 0, length, data, &received) &&
        received == length;
}

static bool hub_feature(struct ehci_enumeration *bus, uint8_t hub, uint8_t port, uint8_t request,
    uint16_t feature) {
    unsigned received;
    return usb_control(bus, hub, 0x23, request, feature, port, 0, 0, &received);
}

static bool hub_status(struct ehci_enumeration *bus, uint8_t hub, uint8_t port, uint32_t *status) {
    uint8_t data[4];
    unsigned received;
    if (!usb_control(bus, hub, 0xa3, USB_REQ_GET_STATUS, 0, port, sizeof(data), data, &received) ||
        received != sizeof(data)) return false;
    *status = read_le16(data) | ((uint32_t)read_le16(data + 2) << 16);
    return true;
}

static bool read_configuration(struct ehci_enumeration *bus, uint8_t address, bool hub, uint8_t *value) {
    uint8_t data[EHCI_DESCRIPTOR_BYTES];
    if (!standard_descriptor(bus, address, USB_DESC_CONFIGURATION, data, 9) ||
        data[0] != 9 || data[1] != USB_DESC_CONFIGURATION || !data[4] || !data[5]) return false;
    uint16_t total = read_le16(data + 2);
    uint8_t configuration = data[5];
    if (total < 9 || total > sizeof(data) ||
        !standard_descriptor(bus, address, USB_DESC_CONFIGURATION, data, total) ||
        data[0] != 9 || data[1] != USB_DESC_CONFIGURATION || read_le16(data + 2) != total ||
        !data[4] || data[5] != configuration) return false;
    bool hub_interface = false;
    for (unsigned pos = 9; pos < total;) {
        unsigned length = data[pos];
        if (length < 2 || length > total - pos) return false;
        if (data[pos + 1] == USB_DESC_INTERFACE) {
            if (length < 9) return false;
            if (!data[pos + 3] && data[pos + 5] == USB_CLASS_HUB) hub_interface = true;
        }
        pos += length;
    }
    if (hub && !hub_interface) return false;
    *value = configuration;
    return true;
}

static bool reset_hub_port(struct ehci_enumeration *bus, uint8_t hub, uint8_t port, uint32_t *status) {
    if ((*status & USB_HUB_CHANGE_CONNECTION) &&
        !hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_CONNECTION)) return false;
    if ((*status & USB_HUB_CHANGE_RESET) &&
        !hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_RESET)) return false;
    if (!hub_feature(bus, hub, port, USB_REQ_SET_FEATURE, USB_FEATURE_PORT_RESET)) return false;

    uint64_t start = timer_monotonic_us();
    for (unsigned poll = 0; poll <= EHCI_HUB_RESET_US / EHCI_HUB_POLL_US; poll++) {
        if (!hub_status(bus, hub, port, status) || !(*status & USB_HUB_PORT_CONNECTION) ||
            (*status & (USB_HUB_PORT_OVERCURRENT | USB_HUB_CHANGE_CONNECTION))) return false;
        if ((*status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET | USB_HUB_CHANGE_RESET)) ==
            (USB_HUB_PORT_ENABLE | USB_HUB_CHANGE_RESET)) {
            return hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_RESET) &&
                timer_delay_us(10000);
        }
        if (timer_monotonic_us() - start >= EHCI_HUB_RESET_US || !timer_delay_us(EHCI_HUB_POLL_US)) break;
    }
    return false;
}

static bool enumerate_hub(struct ehci_enumeration *bus, uint8_t address, uint8_t configuration, unsigned depth) {
    unsigned received;
    uint8_t descriptor[64];
    if (!usb_control(bus, address, 0, USB_REQ_SET_CONFIGURATION, configuration, 0, 0, 0, &received) ||
        !usb_control(bus, address, 0xa0, USB_REQ_GET_DESCRIPTOR, USB_DESC_HUB << 8, 0,
            sizeof(descriptor), descriptor, &received) || received < 9 || descriptor[1] != USB_DESC_HUB) {
        return false;
    }
    unsigned ports = descriptor[2];
    unsigned minimum = 7 + 2 * ((ports + 8) / 8);
    if (!ports || ports > EHCI_HUB_PORT_LIMIT || descriptor[0] < minimum || descriptor[0] > received) return false;
    bus->hubs++;
    console_printf("ehci: hub address=%u ports=%u\n", (uint64_t)address, (uint64_t)ports);

    if ((descriptor[3] & 3) < 2) {
        for (uint8_t port = 1; port <= ports; port++) {
            if (!hub_feature(bus, address, port, USB_REQ_SET_FEATURE, USB_FEATURE_PORT_POWER)) return false;
        }
    }
    /* Allow power-good and connection debounce before assigning address zero. */
    if (!timer_delay_us((uint64_t)descriptor[5] * 2000 + 100000)) return false;

    for (uint8_t port = 1; port <= ports; port++) {
        uint32_t status;
        if (!hub_status(bus, address, port, &status) || (status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET))) {
            return false;
        }
    }
    for (uint8_t port = 1; port <= ports; port++) {
        uint32_t status;
        if (!hub_status(bus, address, port, &status) || (status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET))) {
            return false;
        }
        if (!(status & USB_HUB_PORT_CONNECTION)) continue;
        if ((status & (USB_HUB_PORT_POWER | USB_HUB_PORT_OVERCURRENT)) != USB_HUB_PORT_POWER ||
            !reset_hub_port(bus, address, port, &status)) return false;
        if ((status & (USB_HUB_PORT_HIGH_SPEED | USB_HUB_PORT_LOW_SPEED)) != USB_HUB_PORT_HIGH_SPEED) {
            /* The next reset must have exclusive use of address zero. */
            if (!hub_feature(bus, address, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_PORT_ENABLE) ||
                !hub_status(bus, address, port, &status) || (status & USB_HUB_PORT_ENABLE)) return false;
            bus->deferred++;
            console_printf("ehci: hub address=%u port=%u deferred split transfer\n", (uint64_t)address, (uint64_t)port);
            continue;
        }
        if (!ehci_enumerate_device(bus, address, port, depth + 1)) return false;
    }
    return true;
}

bool ehci_enumerate_device(struct ehci_enumeration *bus, uint8_t parent, uint8_t port, unsigned depth) {
    if (bus->devices >= EHCI_DEVICE_LIMIT || depth > EHCI_HUB_DEPTH_LIMIT) return false;
    uint8_t descriptor[64];
    unsigned received;
    if (!usb_control(bus, 0, 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0,
            sizeof(descriptor), descriptor, &received) || received != 18 ||
        descriptor[0] != 18 || descriptor[1] != USB_DESC_DEVICE ||
        descriptor[7] != 64 || !descriptor[17]) return false;
    bool hub = descriptor[4] == USB_CLASS_HUB;
    if (hub && (descriptor[6] < 1 || descriptor[6] > 2 || depth == EHCI_HUB_DEPTH_LIMIT)) return false;

    uint8_t address = (uint8_t)(bus->devices + 1);
    if (!usb_control(bus, 0, 0, USB_REQ_SET_ADDRESS, address, 0, 0, 0, &received) ||
        !timer_delay_us(2000)) return false;
    uint8_t configuration;
    if (!read_configuration(bus, address, hub, &configuration)) return false;
    bus->devices++;
    console_printf("ehci: device address=%u parent=%u port=%u vendor=%x product=%x class=%x configuration=%u\n",
        (uint64_t)address, (uint64_t)parent, (uint64_t)port, (uint64_t)read_le16(descriptor + 8),
        (uint64_t)read_le16(descriptor + 10), (uint64_t)descriptor[4], (uint64_t)configuration);
    return !hub || enumerate_hub(bus, address, configuration, depth);
}
