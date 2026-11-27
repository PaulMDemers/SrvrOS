#include <srvros/console.h>
#include <srvros/ehci_enumeration.h>
#include <srvros/timer.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PORT_CHANGED (1u << USB_FEATURE_C_PORT_CONNECTION)
#define RESET_CHANGED (1u << USB_FEATURE_C_PORT_RESET)

struct fixture {
    uint32_t ports[3];
    unsigned reset_polls[3];
    unsigned default_port;
    unsigned addresses;
    unsigned split_requests;
    uint64_t split_address_time;
    bool configured;
    bool bad_hub;
    bool stuck_reset;
    bool stuck_enable;
    bool split;
    bool low_speed;
    bool bad_packet;
    bool split_stuck;
    bool multi_tt, multi_tt_selected, wrong_alternate, missing_alternate;
};

static uint64_t now;

void console_printf(const char *format, ...) { (void)format; }
uint64_t timer_monotonic_us(void) { return now; }
bool timer_delay_us(uint64_t duration) { now += duration; return true; }

static bool reply(const struct usb_setup_packet *setup, uint8_t *data, unsigned *received,
    const uint8_t *bytes, unsigned length) {
    *received = length < setup->length ? length : setup->length;
    memcpy(data, bytes, *received);
    return true;
}

static bool split_control(struct fixture *f, const struct ehci_control_target *target, const struct usb_setup_packet *setup,
    uint8_t *data, unsigned *received) {
    assert(f->split && !f->low_speed && target->tt_hub == 1 && target->tt_port == 1 && f->addresses <= 2);
    assert(!f->reset_polls[1] && (f->ports[0] & USB_HUB_PORT_ENABLE));
    assert(!setup->index);
    unsigned request = f->split_requests++;
    if (request < 2) {
        assert(!target->address && f->default_port == 1);
        assert(setup->request_type == 0x80 && setup->request == USB_REQ_GET_DESCRIPTOR &&
            setup->value == USB_DESC_DEVICE << 8);
        assert(target->packet_size == (request ? 16 : 8) && setup->length == (request ? 18 : 8));
        if (request && f->split_stuck) return false;
        uint8_t bytes[] = {18, USB_DESC_DEVICE, 0x10, 1, 0, 0, 0, f->bad_packet ? 24 : 16,
            0xac, 0x05, 0x51, 0x02, 0, 1, 0, 0, 0, 1};
        return reply(setup, data, received, bytes, sizeof(bytes));
    }
    assert(target->packet_size == 16);
    if (request == 2) {
        assert(!target->address && f->default_port == 1 && f->addresses == 1);
        assert(!setup->request_type && setup->request == USB_REQ_SET_ADDRESS && setup->value == 2 && !setup->length);
        f->addresses++;
        f->default_port = 0;
        f->split_address_time = now;
        return true;
    }
    assert(target->address == 2 && !f->default_port && now - f->split_address_time >= 2000);
    assert(setup->request_type == 0x80 && setup->request == USB_REQ_GET_DESCRIPTOR &&
        setup->value == USB_DESC_CONFIGURATION << 8);
    const uint8_t bytes[] = {9, USB_DESC_CONFIGURATION, 25, 0, 1, 1, 0, 0xa0, 50,
        9, USB_DESC_INTERFACE, 0, 0, 1, 3, 1, 1, 0,
        7, USB_DESC_ENDPOINT, 0x81, 3, 8, 0, 10};
    assert(request <= 4 && setup->length == (request == 3 ? 9 : sizeof(bytes)));
    return reply(setup, data, received, bytes, sizeof(bytes));
}

static bool control_fixture(void *context, const struct ehci_control_target *target, const struct usb_setup_packet *setup,
    uint8_t *data, unsigned *received) {
    struct fixture *f = context;
    *received = 0;
    if (target->tt_hub) return split_control(f, target, setup, data, received);
    assert(!target->tt_port && target->packet_size == 64);
    uint8_t address = target->address;
    if (setup->request_type == 0x80 && setup->request == USB_REQ_GET_DESCRIPTOR) {
        assert(setup->index == 0);
        if (setup->value == (USB_DESC_DEVICE << 8)) {
            assert(!address && (!f->addresses || f->default_port == 2));
            uint8_t bytes[] = {18, USB_DESC_DEVICE, 0, 2, USB_CLASS_HUB, 0, 1, 64,
                0x24, 0x04, 0x13, 0x25, 0, 1, 0, 0, 0, 1};
            if (f->multi_tt) bytes[6] = 2;
            if (f->addresses) { bytes[4] = 0; bytes[6] = 0; }
            return reply(setup, data, received, bytes, sizeof(bytes));
        }
        assert(setup->value == (USB_DESC_CONFIGURATION << 8) && address && address <= f->addresses);
        if (address == 1 && f->multi_tt && !f->missing_alternate) {
            const uint8_t bytes[] = {9, USB_DESC_CONFIGURATION, 41, 0, 1, 1, 0, 0xe0, 1,
                9, USB_DESC_INTERFACE, 0, 0, 1, USB_CLASS_HUB, 0, 1, 0,
                7, USB_DESC_ENDPOINT, 0x81, 3, 1, 0, 12,
                9, USB_DESC_INTERFACE, 0, 1, 1, USB_CLASS_HUB, 0, 2, 0,
                7, USB_DESC_ENDPOINT, 0x81, 3, 1, 0, 12};
            return reply(setup, data, received, bytes, sizeof(bytes));
        }
        uint8_t bytes[] = {9, USB_DESC_CONFIGURATION, 25, 0, 1, 1, 0, 0xe0, 0,
            9, USB_DESC_INTERFACE, 0, 0, 1, USB_CLASS_HUB, 0, 0, 0,
            7, USB_DESC_ENDPOINT, 0x81, 3, 1, 0, 12};
        if (address > 1) bytes[14] = 3;
        return reply(setup, data, received, bytes, sizeof(bytes));
    }
    if (setup->request_type == 0 && setup->request == USB_REQ_SET_ADDRESS) {
        assert(!address && !setup->length && !setup->index && setup->value == f->addresses + 1);
        assert((!f->addresses && !f->default_port) || (f->addresses && f->default_port == 2));
        f->addresses++;
        f->default_port = 0;
        return true;
    }

    assert(address == 1);
    if (setup->request_type == 0 && setup->request == USB_REQ_SET_CONFIGURATION) {
        assert(setup->value == 1 && !setup->length && !setup->index && !f->configured);
        f->configured = true;
        return true;
    }
    assert(f->configured);
    if (setup->request_type == 0x01 && setup->request == USB_REQ_SET_INTERFACE) {
        assert(f->multi_tt && !f->multi_tt_selected && setup->value == 1 && !setup->index && !setup->length);
        assert(!(f->ports[0] & USB_HUB_PORT_POWER));
        f->multi_tt_selected = true;
        return true;
    }
    if (setup->request_type == 0x81 && setup->request == USB_REQ_GET_INTERFACE) {
        assert(f->multi_tt_selected && !setup->value && !setup->index && setup->length == 1);
        uint8_t alternate = f->wrong_alternate ? 0 : 1;
        return reply(setup, data, received, &alternate, 1);
    }
    if (setup->request_type == 0xa0) {
        assert(!f->multi_tt || f->multi_tt_selected);
        assert(setup->request == USB_REQ_GET_DESCRIPTOR && setup->value == (USB_DESC_HUB << 8) && !setup->index);
        uint8_t bytes[] = {9, USB_DESC_HUB, 3, 1, 0, 5, 0, 0, 0xff};
        if (f->bad_hub) bytes[2] = 16; /* Declared bitmap cannot fit in the response. */
        return reply(setup, data, received, bytes, sizeof(bytes));
    }
    unsigned port = setup->index;
    assert(port >= 1 && port <= 3);
    uint32_t *status = &f->ports[port - 1];
    if (setup->request_type == 0xa3) {
        assert(setup->request == USB_REQ_GET_STATUS && !setup->value && setup->length == 4);
        if ((*status & USB_HUB_PORT_RESET) && !f->stuck_reset && ++f->reset_polls[port - 1] == 2) {
            *status = (*status & ~USB_HUB_PORT_RESET) | USB_HUB_PORT_ENABLE | RESET_CHANGED;
            if (port == 2) *status |= USB_HUB_PORT_HIGH_SPEED;
            if (port == 1 && f->low_speed) *status |= USB_HUB_PORT_LOW_SPEED;
        }
        uint8_t bytes[] = { *status, *status >> 8, *status >> 16, *status >> 24 };
        return reply(setup, data, received, bytes, sizeof(bytes));
    }
    assert(setup->request_type == 0x23 && !setup->length);
    if (setup->request == USB_REQ_SET_FEATURE && setup->value == USB_FEATURE_PORT_POWER) {
        *status |= USB_HUB_PORT_POWER;
        return true;
    }
    if (setup->request == USB_REQ_SET_FEATURE && setup->value == USB_FEATURE_PORT_RESET) {
        assert(!f->default_port && (*status & USB_HUB_PORT_CONNECTION) && (*status & USB_HUB_PORT_POWER));
        assert(!(*status & (PORT_CHANGED | RESET_CHANGED | USB_HUB_PORT_ENABLE)));
        f->default_port = port;
        *status |= USB_HUB_PORT_RESET;
        return true;
    }
    assert(setup->request == USB_REQ_CLEAR_FEATURE);
    if (setup->value == USB_FEATURE_PORT_ENABLE) {
        assert(port == 1 && f->default_port == 1 && (*status & USB_HUB_PORT_ENABLE));
        if (!f->stuck_enable) {
            *status &= ~USB_HUB_PORT_ENABLE;
            f->default_port = 0;
        }
    } else {
        assert(setup->value == USB_FEATURE_C_PORT_CONNECTION || setup->value == USB_FEATURE_C_PORT_RESET);
        *status &= ~(1u << setup->value);
    }
    return true;
}

static bool run(struct fixture *f, struct ehci_enumeration *bus) {
    f->ports[0] = f->ports[1] = USB_HUB_PORT_CONNECTION | PORT_CHANGED;
    now = 0;
    *bus = (struct ehci_enumeration) { .control = control_fixture, .context = f, .split = f->split,
        .trackpad_multi_tt = f->multi_tt };
    return ehci_enumerate_device(bus, 0, 1, 0, false);
}

int main(void) {
    struct ehci_enumeration bus;
    struct fixture f = {0};
    assert(run(&f, &bus));
    assert(bus.devices == 2 && bus.hubs == 1 && bus.deferred == 1 && f.addresses == 2);
    assert(!f.default_port && !f.reset_polls[2]);

    f = (struct fixture) { .bad_hub = true };
    assert(!run(&f, &bus));
    assert(f.addresses == 1 && !bus.hubs && !f.default_port && !(f.ports[0] & USB_HUB_PORT_POWER));

    f = (struct fixture) { .stuck_enable = true };
    assert(!run(&f, &bus));
    assert(f.addresses == 1 && f.default_port == 1 && !f.reset_polls[1]);
    assert(!strcmp(bus.step.operation, "verify hub port disabled") && bus.step.hub == 1 && bus.step.port == 1);
    assert(bus.step.port_status_valid && bus.step.port_status & USB_HUB_PORT_ENABLE);

    f = (struct fixture) { .stuck_reset = true };
    assert(!run(&f, &bus));
    assert(f.addresses == 1 && f.default_port == 1 && (f.ports[0] & USB_HUB_PORT_RESET));
    assert(now >= 200000 && now < 400000);
    assert(!strcmp(bus.step.operation, "wait for hub port reset") && bus.step.hub == 1 && bus.step.port == 1);
    assert(bus.step.port_status_valid && bus.step.port_status & USB_HUB_PORT_RESET);

    f = (struct fixture) { .split = true };
    assert(run(&f, &bus));
    assert(bus.devices == 3 && bus.split_devices == 1 && bus.hubs == 1 && !bus.deferred && f.addresses == 3);
    assert(f.split_requests == 5 && !f.default_port && !f.reset_polls[2]);

    f = (struct fixture) { .split = true, .multi_tt = true };
    assert(run(&f, &bus) && f.multi_tt_selected && f.split_requests == 5);

    f = (struct fixture) { .split = true, .multi_tt = true, .wrong_alternate = true };
    assert(!run(&f, &bus) && f.multi_tt_selected && !f.split_requests);
    assert(!strcmp(bus.step.operation, "verify hub multi-TT"));

    f = (struct fixture) { .split = true, .multi_tt = true, .missing_alternate = true };
    assert(!run(&f, &bus) && !f.configured && !f.multi_tt_selected);
    assert(!strcmp(bus.step.operation, "match hub multi-TT descriptor"));

    f = (struct fixture) { .split = true, .low_speed = true };
    assert(run(&f, &bus));
    assert(bus.devices == 2 && !bus.split_devices && bus.deferred == 1 && !f.split_requests && !f.default_port);

    f = (struct fixture) { .split = true, .bad_packet = true };
    assert(!run(&f, &bus));
    assert(f.split_requests == 1 && f.addresses == 1 && f.default_port == 1 && !f.reset_polls[1]);

    f = (struct fixture) { .split = true, .split_stuck = true };
    assert(!run(&f, &bus));
    assert(f.split_requests == 2 && f.addresses == 1 && f.default_port == 1 && !f.reset_polls[1]);
    assert(!strcmp(bus.step.operation, "device descriptor") && bus.step.hub == 1 && bus.step.port == 1);
    assert(bus.step.port_status_valid && bus.step.port_status & USB_HUB_PORT_ENABLE);
    puts("EHCI hub tests passed");
}
