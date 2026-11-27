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

static bool usb_control(struct ehci_enumeration *bus, const struct ehci_control_target *target, uint8_t type, uint8_t request,
    uint16_t value, uint16_t index, uint16_t length, uint8_t *data, unsigned *received) {
    struct usb_setup_packet setup = { type, request, value, index, length };
    return bus->control(bus->context, target, &setup, data, received) && *received <= length;
}

static bool standard_descriptor(struct ehci_enumeration *bus, const struct ehci_control_target *target, uint16_t type,
    uint8_t *data, uint16_t length) {
    unsigned received;
    return usb_control(bus, target, 0x80, USB_REQ_GET_DESCRIPTOR, type << 8, 0, length, data, &received) &&
        received == length;
}

static bool hub_feature(struct ehci_enumeration *bus, const struct ehci_control_target *hub, uint8_t port, uint8_t request,
    uint16_t feature) {
    unsigned received;
    return usb_control(bus, hub, 0x23, request, feature, port, 0, 0, &received);
}

static bool hub_status(struct ehci_enumeration *bus, const struct ehci_control_target *hub, uint8_t port, uint32_t *status) {
    uint8_t data[4];
    unsigned received;
    if (!usb_control(bus, hub, 0xa3, USB_REQ_GET_STATUS, 0, port, sizeof(data), data, &received) ||
        received != sizeof(data)) return false;
    *status = read_le16(data) | ((uint32_t)read_le16(data + 2) << 16);
    bus->step.port_status = *status;
    bus->step.port_status_valid = true;
    return true;
}

static uint16_t read_configuration(struct ehci_enumeration *bus, const struct ehci_control_target *target,
    bool hub, uint8_t data[EHCI_DESCRIPTOR_BYTES]) {
    bus->step.operation = "configuration header";
    if (!standard_descriptor(bus, target, USB_DESC_CONFIGURATION, data, 9) ||
        data[0] != 9 || data[1] != USB_DESC_CONFIGURATION || !data[4] || !data[5]) return false;
    uint16_t total = read_le16(data + 2);
    uint8_t configuration = data[5];
    bus->step.operation = "configuration body";
    if (total < 9 || total > EHCI_DESCRIPTOR_BYTES ||
        !standard_descriptor(bus, target, USB_DESC_CONFIGURATION, data, total) ||
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
    return total;
}

static bool reset_hub_port(struct ehci_enumeration *bus, const struct ehci_control_target *hub, uint8_t port,
    uint32_t *status) {
    bus->step.operation = "clear hub connection change";
    if ((*status & USB_HUB_CHANGE_CONNECTION) &&
        !hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_CONNECTION)) return false;
    bus->step.operation = "clear stale hub reset change";
    if ((*status & USB_HUB_CHANGE_RESET) &&
        !hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_RESET)) return false;
    bus->step.operation = "start hub port reset";
    if (!hub_feature(bus, hub, port, USB_REQ_SET_FEATURE, USB_FEATURE_PORT_RESET)) return false;

    uint64_t start = timer_monotonic_us();
    bus->step.operation = "wait for hub port reset";
    for (unsigned poll = 0; poll <= EHCI_HUB_RESET_US / EHCI_HUB_POLL_US; poll++) {
        if (!hub_status(bus, hub, port, status) || !(*status & USB_HUB_PORT_CONNECTION) ||
            (*status & (USB_HUB_PORT_OVERCURRENT | USB_HUB_CHANGE_CONNECTION))) return false;
        if ((*status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET | USB_HUB_CHANGE_RESET)) ==
            (USB_HUB_PORT_ENABLE | USB_HUB_CHANGE_RESET)) {
            bus->step.operation = "acknowledge hub port reset";
            if (!hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_C_PORT_RESET)) return false;
            bus->step.operation = "hub port reset recovery delay";
            return timer_delay_us(10000);
        }
        if (timer_monotonic_us() - start >= EHCI_HUB_RESET_US || !timer_delay_us(EHCI_HUB_POLL_US)) break;
    }
    return false;
}

static bool enumerate_hub(struct ehci_enumeration *bus, const struct ehci_control_target *hub, uint8_t configuration,
    unsigned depth, bool multi_tt) {
    unsigned received;
    uint8_t descriptor[64];
    bus->step.operation = "set hub configuration";
    if (!usb_control(bus, hub, 0, USB_REQ_SET_CONFIGURATION, configuration, 0, 0, 0, &received)) return false;
    if (multi_tt) {
        bus->step.operation = "select hub multi-TT";
        if (!usb_control(bus, hub, 0x01, USB_REQ_SET_INTERFACE, 1, 0, 0, 0, &received)) return false;
        uint8_t alternate;
        bus->step.operation = "verify hub multi-TT";
        if (!usb_control(bus, hub, 0x81, USB_REQ_GET_INTERFACE, 0, 0, 1, &alternate, &received) ||
            received != 1 || alternate != 1) return false;
        console_printf("ehci: hub address=%u alternate=1 multi-TT\n", (uint64_t)hub->address);
    }
    bus->step.operation = "hub descriptor";
    if (!usb_control(bus, hub, 0xa0, USB_REQ_GET_DESCRIPTOR, USB_DESC_HUB << 8, 0,
            sizeof(descriptor), descriptor, &received) || received < 9 || descriptor[1] != USB_DESC_HUB) {
        return false;
    }
    unsigned ports = descriptor[2];
    unsigned minimum = 7 + 2 * ((ports + 8) / 8);
    if (!ports || ports > EHCI_HUB_PORT_LIMIT || descriptor[0] < minimum || descriptor[0] > received) return false;
    bus->hubs++;
    console_printf("ehci: hub address=%u ports=%u\n", (uint64_t)hub->address, (uint64_t)ports);

    if ((descriptor[3] & 3) < 2) {
        for (uint8_t port = 1; port <= ports; port++) {
            bus->step = (struct ehci_scan_step) { .operation = "power hub port", .hub = hub->address, .port = port };
            if (!hub_feature(bus, hub, port, USB_REQ_SET_FEATURE, USB_FEATURE_PORT_POWER)) return false;
        }
    }
    /* Allow power-good and connection debounce before assigning address zero. */
    bus->step = (struct ehci_scan_step) { .operation = "hub power and debounce", .hub = hub->address };
    if (!timer_delay_us((uint64_t)descriptor[5] * 2000 + 100000)) return false;

    for (uint8_t port = 1; port <= ports; port++) {
        uint32_t status;
        bus->step = (struct ehci_scan_step) { .operation = "hub port preflight", .hub = hub->address, .port = port };
        if (!hub_status(bus, hub, port, &status) || (status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET))) {
            return false;
        }
    }
    for (uint8_t port = 1; port <= ports; port++) {
        uint32_t status;
        bus->step = (struct ehci_scan_step) { .operation = "hub port status", .hub = hub->address, .port = port };
        if (!hub_status(bus, hub, port, &status) || (status & (USB_HUB_PORT_ENABLE | USB_HUB_PORT_RESET))) {
            return false;
        }
        if (!(status & USB_HUB_PORT_CONNECTION)) continue;
        if ((status & (USB_HUB_PORT_POWER | USB_HUB_PORT_OVERCURRENT)) != USB_HUB_PORT_POWER ||
            !reset_hub_port(bus, hub, port, &status)) return false;
        uint32_t speed = status & (USB_HUB_PORT_HIGH_SPEED | USB_HUB_PORT_LOW_SPEED);
        bool full_speed = bus->split && !speed;
        if (speed != USB_HUB_PORT_HIGH_SPEED && !full_speed) {
            /* The next reset must have exclusive use of address zero. */
            bus->step.operation = "disable deferred hub port";
            if (!hub_feature(bus, hub, port, USB_REQ_CLEAR_FEATURE, USB_FEATURE_PORT_ENABLE)) return false;
            bus->step.operation = "verify hub port disabled";
            if (!hub_status(bus, hub, port, &status) || (status & USB_HUB_PORT_ENABLE)) return false;
            bus->deferred++;
            console_printf("ehci: hub address=%u port=%u deferred split transfer\n", (uint64_t)hub->address, (uint64_t)port);
            continue;
        }
        if (!ehci_enumerate_device(bus, hub->address, port, depth + 1, full_speed)) return false;
    }
    return true;
}

bool ehci_enumerate_device(struct ehci_enumeration *bus, uint8_t parent, uint8_t port, unsigned depth,
    bool full_speed) {
    bus->step.hub = parent;
    bus->step.port = port;
    bus->step.operation = "device admission";
    if (bus->devices >= EHCI_DEVICE_LIMIT || depth > EHCI_HUB_DEPTH_LIMIT) return false;
    struct ehci_control_target target = { .packet_size = 64 };
    uint8_t descriptor[64];
    uint16_t length = sizeof(descriptor);
    if (full_speed) {
        if (!bus->split || !parent || !port) return false;
        target = (struct ehci_control_target) { .packet_size = 8, .tt_hub = parent, .tt_port = port };
        bus->step.operation = "device descriptor prefix";
        if (!standard_descriptor(bus, &target, USB_DESC_DEVICE, descriptor, 8) ||
            descriptor[0] != 18 || descriptor[1] != USB_DESC_DEVICE || !ehci_full_speed_packet_size(descriptor[7])) {
            return false;
        }
        target.packet_size = descriptor[7];
        length = 18;
    }
    unsigned received;
    bus->step.operation = "device descriptor";
    if (!usb_control(bus, &target, 0x80, USB_REQ_GET_DESCRIPTOR, USB_DESC_DEVICE << 8, 0,
            length, descriptor, &received) || received != 18 ||
        descriptor[0] != 18 || descriptor[1] != USB_DESC_DEVICE ||
        descriptor[7] != target.packet_size || !descriptor[17]) return false;
    bool hub = descriptor[4] == USB_CLASS_HUB;
    if (hub && !full_speed && (descriptor[6] < 1 || descriptor[6] > 2 || depth == EHCI_HUB_DEPTH_LIMIT)) return false;

    uint8_t address = (uint8_t)(bus->devices + 1);
    bus->step.operation = "set device address";
    if (!usb_control(bus, &target, 0, USB_REQ_SET_ADDRESS, address, 0, 0, 0, &received)) return false;
    bus->step.operation = "device address recovery delay";
    if (!timer_delay_us(2000)) return false;
    target.address = address;
    uint8_t configuration[EHCI_DESCRIPTOR_BYTES];
    uint16_t configuration_length = read_configuration(bus, &target, hub, configuration);
    if (!configuration_length) return false;
    bus->devices++;
    uint16_t vendor = read_le16(descriptor + 8);
    uint16_t product = read_le16(descriptor + 10);
    if (bus->device_found) bus->device_found(bus->context, &target, vendor, product, parent, port,
        configuration, configuration_length);
    console_printf("ehci: device address=%u parent=%u port=%u vendor=%x product=%x class=%x configuration=%u\n",
        (uint64_t)address, (uint64_t)parent, (uint64_t)port, (uint64_t)vendor,
        (uint64_t)product, (uint64_t)descriptor[4], (uint64_t)configuration[5]);
    if (full_speed) {
        bus->split_devices++;
        console_printf("ehci: split address=%u hub=%u port=%u packet=%u\n",
            (uint64_t)address, (uint64_t)target.tt_hub, (uint64_t)target.tt_port, (uint64_t)target.packet_size);
        if (hub) console_printf("ehci: hub address=%u deferred downstream scan\n", (uint64_t)address);
        return true;
    }
    bool multi_tt = bus->trackpad_multi_tt && hub && !parent && port == 1 && vendor == 0x0424 && product == 0x2513;
    if (multi_tt) {
        bus->step.operation = "match hub multi-TT descriptor";
        if (descriptor[6] != 2 || configuration[4] != 1 || configuration[5] != 1) return false;
        bool alternate = false;
        for (unsigned pos = 9; pos < configuration_length; pos += configuration[pos]) {
            const uint8_t *entry = configuration + pos;
            if (entry[0] == 9 && entry[1] == USB_DESC_INTERFACE && !entry[2] && entry[3] == 1 &&
                entry[4] == 1 && entry[5] == USB_CLASS_HUB && !entry[6] && entry[7] == 2) alternate = true;
        }
        if (!alternate) return false;
    }
    return !hub || enumerate_hub(bus, &target, configuration[5], depth, multi_tt);
}
