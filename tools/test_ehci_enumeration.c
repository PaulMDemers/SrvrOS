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
    bool configured;
    bool bad_hub;
    bool stuck_reset;
    bool stuck_enable;
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

static bool control_fixture(void *context, uint8_t address, const struct usb_setup_packet *setup,
    uint8_t *data, unsigned *received) {
    struct fixture *f = context;
    *received = 0;
    if (setup->request_type == 0x80 && setup->request == USB_REQ_GET_DESCRIPTOR) {
        assert(setup->index == 0);
        if (setup->value == (USB_DESC_DEVICE << 8)) {
            assert(!address && (!f->addresses || f->default_port == 2));
            uint8_t bytes[] = {18, USB_DESC_DEVICE, 0, 2, USB_CLASS_HUB, 0, 1, 64,
                0x24, 0x04, 0x13, 0x25, 0, 1, 0, 0, 0, 1};
            if (f->addresses) { bytes[4] = 0; bytes[6] = 0; }
            return reply(setup, data, received, bytes, sizeof(bytes));
        }
        assert(setup->value == (USB_DESC_CONFIGURATION << 8) && address && address <= f->addresses);
        uint8_t bytes[] = {9, USB_DESC_CONFIGURATION, 25, 0, 1, 1, 0, 0xe0, 0,
            9, USB_DESC_INTERFACE, 0, 0, 1, USB_CLASS_HUB, 0, 0, 0,
            7, USB_DESC_ENDPOINT, 0x81, 3, 1, 0, 12};
        if (address == 2) bytes[14] = 3;
        return reply(setup, data, received, bytes, sizeof(bytes));
    }
    if (setup->request_type == 0 && setup->request == USB_REQ_SET_ADDRESS) {
        assert(!address && !setup->length && !setup->index && setup->value == f->addresses + 1);
        assert((!f->addresses && !f->default_port) || (f->addresses == 1 && f->default_port == 2));
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
    if (setup->request_type == 0xa0) {
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
    *bus = (struct ehci_enumeration) { .control = control_fixture, .context = f };
    return ehci_enumerate_device(bus, 0, 1, 0);
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

    f = (struct fixture) { .stuck_reset = true };
    assert(!run(&f, &bus));
    assert(f.addresses == 1 && f.default_port == 1 && (f.ports[0] & USB_HUB_PORT_RESET));
    assert(now >= 200000 && now < 400000);
    puts("EHCI hub tests passed");
}
