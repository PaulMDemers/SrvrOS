#include <srvros/device_abi.h>
#include <srvros/heap.h>
#include <srvros/net_device.h>
#include <srvros/net.h>
#include <srvros/usb_ecm.h>
#include <srvros/latency.h>
#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>
#include <srvros/xhci.h>
#include <srvros/usb_storage.h>
#include <srvros/apple_trackpad.h>
#include <srvros/trackpad_pointer.h>

#include <stdbool.h>
#include <stdint.h>

#define XHCI_CLASS_SERIAL_BUS 0x0c
#define XHCI_SUBCLASS_USB 0x03
#define XHCI_PROG_IF_XHCI 0x30
#define XHCI_MMIO_VIRTUAL_BASE 0xffffc00120000000ull
#define XHCI_MMIO_MAP_SIZE (64ull * 1024ull)
#define XHCI_TRB_COUNT 64
#define XHCI_EVENT_TRB_COUNT 64
#define XHCI_MAX_PORTS 32
#define XHCI_MAX_DEVICES 32
#define XHCI_MAX_HUB_PORTS 15
#define XHCI_MAX_PROTOCOLS 8
#define XHCI_RESET_TIMEOUT 10000000ull
#define XHCI_COMMAND_TIMEOUT 10000000ull
#define XHCI_TRANSFER_TIMEOUT 20000000ull
#define XHCI_INPUT_PROBE_TICKS_PER_SECOND 100ull
#define XHCI_INPUT_PROBE_TSC_PER_SECOND 3000000000ull
#define XHCI_INPUT_PROBE_SPIN_LIMIT 4000000ull

#define XHCI_USBCMD_RUN 0x00000001u
#define XHCI_USBCMD_HCRST 0x00000002u
#define XHCI_USBCMD_INTE 0x00000004u
#define XHCI_USBSTS_HCH 0x00000001u
#define XHCI_USBSTS_EINT 0x00000008u
#define XHCI_USBSTS_CNR 0x00000800u
#define XHCI_EXT_CAP_LEGACY 0x01
#define XHCI_EXT_CAP_SUPPORTED_PROTOCOL 0x02
#define XHCI_LEGACY_BIOS_OWNED (1u << 16)
#define XHCI_LEGACY_OS_OWNED (1u << 24)
/* USBLEGCTLSTS: preserve reserved/RO bits; clear enables and W1C events. */
#define XHCI_LEGACY_PRESERVE ((7u << 1) | (0xffu << 5) | (7u << 17))
#define XHCI_LEGACY_EVENTS (7u << 29)

#define XHCI_OP_USBCMD 0x00
#define XHCI_OP_USBSTS 0x04
#define XHCI_OP_PAGESIZE 0x08
#define XHCI_OP_DNCTRL 0x14
#define XHCI_OP_CRCR 0x18
#define XHCI_OP_DCBAAP 0x30
#define XHCI_OP_CONFIG 0x38
#define XHCI_OP_PORTS 0x400
#define XHCI_PORTSC_CCS 0x00000001u
#define XHCI_PORTSC_PED 0x00000002u
#define XHCI_PORTSC_PR 0x00000010u
#define XHCI_PORTSC_PP 0x00000200u
#define XHCI_PORTSC_CHANGE_MASK 0x00fe0000u
#define XHCI_PORTSC_CSC (1u << 17)
#define XHCI_PORTSC_PRC (1u << 21)
/* PORTSC neutral writes preserve RO/RWS fields, never PED (RW1C), reset
 * strobes, or change acknowledgements. PLS writes need LWS, which stays clear. */
#define XHCI_PORTSC_RO ((1u << 0) | (1u << 3) | (0xfu << 10) | (1u << 30))
#define XHCI_PORTSC_RWS ((0xfu << 5) | (1u << 9) | (3u << 14) | (7u << 25))

#define XHCI_INTR0 0x20
#define XHCI_INTR_IMAN 0x00
#define XHCI_INTR_IMOD 0x04
#define XHCI_INTR_ERSTSZ 0x08
#define XHCI_INTR_ERSTBA 0x10
#define XHCI_INTR_ERDP 0x18

#define XHCI_TRB_TYPE_ENABLE_SLOT 9
#define XHCI_TRB_TYPE_LINK 6
#define XHCI_TRB_TYPE_NORMAL 1
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_TYPE_DATA_STAGE 3
#define XHCI_TRB_TYPE_STATUS_STAGE 4
#define XHCI_TRB_TYPE_ADDRESS_DEVICE 11
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12
#define XHCI_TRB_TYPE_EVALUATE_CONTEXT 13
#define XHCI_TRB_TYPE_NOOP 23
#define XHCI_TRB_TYPE_TRANSFER_EVENT 32
#define XHCI_TRB_TYPE_COMMAND_COMPLETION 33

#define XHCI_TRB_IOC (1u << 5)
#define XHCI_TRB_ISP (1u << 2)
#define XHCI_TRB_IDT (1u << 6)
#define XHCI_SETUP_TRT_OUT (2u << 16)
#define XHCI_SETUP_TRT_IN (3u << 16)
#define XHCI_SETUP_TRT_NONE (0u << 16)
#define XHCI_DATA_DIR_IN (1u << 16)
#define XHCI_STATUS_DIR_IN (1u << 16)

#define USB_REQ_GET_DESCRIPTOR 6
#define USB_REQ_GET_STATUS 0
#define USB_REQ_SET_FEATURE 3
#define USB_REQ_CLEAR_FEATURE 1
#define USB_REQ_SET_CONFIGURATION 9
#define USB_REQ_SET_PROTOCOL 11
#define USB_DESC_DEVICE 1
#define USB_DESC_CONFIGURATION 2
#define USB_DESC_INTERFACE 4
#define USB_DESC_ENDPOINT 5
#define USB_DESC_HUB 0x29
#define USB_CLASS_HID 3
#define USB_CLASS_HUB 9
#define USB_FEATURE_PORT_ENABLE 1
#define USB_FEATURE_PORT_RESET 4
#define USB_FEATURE_PORT_POWER 8
#define USB_FEATURE_C_PORT_CONNECTION 16
#define USB_FEATURE_C_PORT_ENABLE 17
#define USB_FEATURE_C_PORT_RESET 20
#define USB_HUB_PORT_CONNECTION 0x0001u
#define USB_HUB_PORT_ENABLE 0x0002u
#define USB_HUB_PORT_LOW_SPEED 0x0200u
#define USB_HUB_PORT_HIGH_SPEED 0x0400u
#define USB_HUB_STATUS_CHANGE_MASK 0xffff0000u
#define USB_HID_SUBCLASS_BOOT 1
#define USB_HID_PROTOCOL_KEYBOARD 1
#define USB_HID_PROTOCOL_MOUSE 2
#define USB_ENDPOINT_IN 0x80
#define USB_ENDPOINT_INTERRUPT 3

struct xhci_trb {
    uint32_t parameter_low;
    uint32_t parameter_high;
    uint32_t status;
    uint32_t control;
} __attribute__((packed));

struct xhci_erst_entry {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size;
    uint32_t reserved;
} __attribute__((packed));

struct xhci_port_state {
    uint32_t portsc;
    uint32_t initial_portsc;
    uint32_t reset_portsc;
    const char *reset_result;
    uint8_t reset_attempts;
    uint8_t slot_type;
    uint8_t protocol_index;
};

struct xhci_protocol {
    bool present;
    uint8_t first_port;
    uint8_t port_count;
    uint8_t slot_type;
    uint16_t revision;
    char name[5];
};

struct usb_setup_packet {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed));

struct xhci_bulk_endpoint {
    uint8_t dci, burst;
    uint16_t packet;
    uint64_t ring_phys, enqueue;
    struct xhci_trb *ring;
    bool cycle;
};

#define XHCI_NET_QUEUE 8
struct xhci_network_endpoint {
    struct xhci_bulk_endpoint hw;
    uint64_t buffer_phys,trb;
    uint8_t *buffer;
    uint32_t requested,remaining;
    uint8_t completion;
    bool pending,done;
};
struct xhci_network {
    struct usb_ecm_config config;
    struct usb_ecm_link link;
    struct xhci_network_endpoint ep[3]; /* TX, RX, notifications */
    bool ready,failed,polling,tx_zero;
    uint8_t mac[6];
    unsigned head,count;
    uint16_t length[XHCI_NET_QUEUE];
    uint8_t queue[XHCI_NET_QUEUE][NET_DEVICE_FRAME_MAX];
    uint64_t tx_started,tx_packets,rx_packets,drops,errors,notifications;
};

/* Independent endpoint on the same physical USB slot as the keyboard.
 * Capture only until real reports validate the transport and contact model. */
struct xhci_trackpad_endpoint {
    bool candidate,ready,pending,cycle;
    uint8_t dci,interval,report_type;
    uint16_t packet;
    uint64_t ring_phys,buffer_phys,enqueue,trb;
    struct xhci_trb *ring;
    uint8_t *buffer;
    uint64_t reports,contact_reports,button_reports,malformed,errors;
    uint32_t last_length,last_completion;
    struct apple_trackpad_frame frame;
    struct trackpad_pointer pointer;
    uint64_t scroll_events,typing_suppressed,palm_suppressed,typing_until;
    uint64_t drag_frames,drag_rebases,drag_handoffs,tap_primary,tap_secondary;
    uint32_t max_touch_major,max_touch_minor;
    uint64_t pointer_motion, pointer_events,pointer_buttons,pointer_cancels;
    uint64_t lifts;
    uint32_t max_contacts;
    struct xhci_trackpad_sample samples[XHCI_TRACKPAD_SAMPLES];
};

struct xhci_device {
    bool present;
    bool addressed;
    bool configured;
    bool hid_keyboard;
    bool hid_mouse;
    bool hid_boot_mouse;
    bool hid_boot_keyboard;
    bool hid_absolute_pointer;
    bool hid_have_absolute;
    bool interrupt_pending;
    bool interrupt_ready;
    uint8_t slot_id;
    uint8_t port_id;
    uint8_t parent_slot_id;
    uint8_t hub_port_id;
    uint8_t hub_ports;
    uint8_t slot_type;
    uint8_t speed;
    uint16_t ep0_max_packet;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t configuration_count;
    uint8_t configuration_value;
    uint8_t interface_count;
    bool have_interface_descriptor;
    uint8_t first_interface_class;
    uint8_t first_interface_subclass;
    uint8_t first_interface_protocol;
    uint8_t interface_number;
    uint8_t interrupt_dci;
    uint8_t interrupt_interval;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_bcd;
    uint16_t interrupt_max_packet;
    uint8_t interrupt_max_burst;
    const char *stage;
    uint32_t route_string;
    uint64_t input_context_phys;
    uint64_t output_context_phys;
    uint64_t ep0_ring_phys;
    uint64_t interrupt_ring_phys;
    uint64_t descriptor_buffer_phys;
    uint64_t interrupt_buffer_phys;
    uint8_t *input_context;
    uint8_t *output_context;
    struct xhci_trb *ep0_ring;
    struct xhci_trb *interrupt_ring;
    uint8_t *descriptor_buffer;
    uint8_t *interrupt_buffer;
    uint64_t ep0_enqueue;
    uint64_t interrupt_enqueue;
    bool ep0_cycle;
    bool interrupt_cycle;
    uint64_t interrupt_trb;
    struct xhci_trackpad_endpoint trackpad;
    struct xhci_bulk_endpoint bulk_ep[2]; /* OUT, IN */
    uint64_t bulk_buffer_phys, bulk_trb;
    uint8_t *bulk_buffer;
    uint8_t bulk_dci, bulk_completion;
    uint32_t bulk_remaining;
    bool bulk_pending, bulk_done, bulk_failed;
    const char *bulk_reason;
    uint64_t bulk_elapsed_us, bulk_spins;
    uint32_t bulk_length;
    struct usb_storage storage;
    struct xhci_network *network;
    uint8_t previous_report[8];
    struct keyboard_hid_state keyboard_state;
    uint16_t previous_absolute_x;
    uint16_t previous_absolute_y;
    uint64_t control_success;
    uint64_t interrupt_success;
    uint64_t interrupt_errors;
    uint64_t last_interrupt_completion;
    uint64_t last_interrupt_remaining;
    uint64_t hid_reports;
    uint64_t mouse_reports;
};

struct xhci_state {
    bool present;
    bool operational;
    const char *stage;
    const char *handoff;
    uint16_t scratchpad_count;
    uint16_t scratchpads_allocated;
    const struct pci_device *pci;
    uint64_t mmio_phys;
    uint64_t mmio_virt;
    uint64_t op_base;
    uint64_t rt_base;
    uint64_t db_base;
    uint8_t cap_length;
    uint16_t hci_version;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t db_off;
    uint32_t rts_off;
    uint32_t page_size;
    uint8_t max_slots;
    uint8_t port_count;
    uint64_t dcbaa_phys;
    uint64_t *dcbaa;
    struct xhci_trb *command_ring;
    uint64_t command_ring_phys;
    uint64_t command_enqueue;
    bool command_cycle;
    struct xhci_trb *event_ring;
    uint64_t event_ring_phys;
    uint64_t event_dequeue;
    bool event_cycle;
    struct xhci_erst_entry *erst;
    uint64_t erst_phys;
    uint64_t command_events;
    uint64_t noop_completions;
    uint64_t enable_slot_completions;
    uint64_t ports_reset;
    uint64_t ports_enabled;
    uint64_t addressed_devices;
    uint64_t configured_devices;
    uint64_t hid_keyboards;
    uint64_t hid_mice;
    uint64_t hubs_seen;
    uint64_t hub_ports_powered;
    uint64_t hub_ports_reset;
    uint64_t routed_devices;
    uint64_t unsupported_devices;
    uint64_t transfer_events;
    uint64_t device_record_full;
    uint64_t last_completion_code;
    uint64_t last_slot_id;
    uint64_t last_command_pointer;
    uint64_t last_expected_command_pointer;
    uint64_t command_pointer_mismatches;
    uint64_t protocol_count;
    struct xhci_port_state ports[XHCI_MAX_PORTS];
    struct xhci_protocol protocols[XHCI_MAX_PROTOCOLS];
    struct xhci_device devices[XHCI_MAX_DEVICES];
};

static struct xhci_state xhci;
static volatile bool xhci_input_probe_active;

static bool network_enabled;
static uint64_t network_count;
void xhci_set_network_enabled(bool enabled) { network_enabled=enabled; }
static bool storage_writable;
void xhci_set_storage_writable(bool writable) { storage_writable = writable; }
static bool trackpad_capture,trackpad_pointer_enabled,trackpad_gestures;
void xhci_set_trackpad_gestures(bool enabled) { trackpad_gestures=enabled; }
void xhci_set_trackpad_pointer(bool enabled) { trackpad_pointer_enabled=enabled; }
void xhci_set_trackpad_capture(bool enabled) { trackpad_capture=enabled; }
static void dispatch_transfer_event(const struct xhci_trb *event);
static uint64_t event_irq_save(void) {
#ifdef SRVROS_XHCI_TEST_MMIO
    return 0;
#else
    uint64_t flags; __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory"); return flags;
#endif
}
static void event_irq_restore(uint64_t flags) {
#ifdef SRVROS_XHCI_TEST_MMIO
    (void)flags;
#else
    if(flags & (1ull<<9)) __asm__ volatile("sti":::"memory");
#endif
}

void xhci_trackpad_snapshot(struct xhci_trackpad_capture *out) {
    if (!out) return;
    uint64_t flags=event_irq_save();
    *out=(struct xhci_trackpad_capture){.pointer_enabled=trackpad_pointer_enabled,
        .pointer_queue_overflows=mouse_overflow_count(),.gestures_enabled=trackpad_gestures};
    for (unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *d=&xhci.devices[i];
        struct xhci_trackpad_endpoint *t=&d->trackpad;
        if (!d->present || !t->candidate) continue;
        out->found=true; out->slot=d->slot_id; out->ready=t->ready; out->pending=t->pending;
        out->last_length=t->last_length; out->completion=t->last_completion;
        out->max_contacts=t->max_contacts; out->reports=t->reports;
        out->contact_reports=t->contact_reports; out->button_reports=t->button_reports;
        out->lifts=t->lifts; out->malformed=t->malformed; out->errors=t->errors;
        out->keyboard_reports=d->hid_reports;
        out->scroll_events=t->scroll_events; out->typing_suppressed=t->typing_suppressed;
        out->palm_suppressed=t->palm_suppressed;
        out->tap_primary=t->tap_primary;out->tap_secondary=t->tap_secondary;
        out->drag_frames=t->drag_frames;out->drag_rebases=t->drag_rebases;out->drag_handoffs=t->drag_handoffs;
        out->max_touch_major=t->max_touch_major; out->max_touch_minor=t->max_touch_minor;
        out->pointer_motion=t->pointer_motion;
        out->pointer_events=t->pointer_events; out->pointer_buttons=t->pointer_buttons;
        out->pointer_cancels=t->pointer_cancels;
        for (unsigned n=0;n<XHCI_TRACKPAD_SAMPLES;n++) out->samples[n]=t->samples[n];
        break;
    }
    event_irq_restore(flags);
}
void xhci_trackpad_reset_capture(void) {
    uint64_t flags=event_irq_save();
    for (unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_trackpad_endpoint *t=&xhci.devices[i].trackpad;
        t->scroll_events=t->typing_suppressed=t->palm_suppressed=0;
        t->drag_frames=t->drag_rebases=t->drag_handoffs=t->tap_primary=t->tap_secondary=0;
        t->max_touch_major=t->max_touch_minor=0;
        t->pointer_motion=t->pointer_events=t->pointer_buttons=t->pointer_cancels=0;
        t->reports=t->contact_reports=t->button_reports=t->lifts=t->malformed=0;
        t->max_contacts=0; t->frame=(struct apple_trackpad_frame){0};
        for (unsigned n=0;n<XHCI_TRACKPAD_SAMPLES;n++) t->samples[n].length=0;
    }
    event_irq_restore(flags);
}

static void trackpad_sample(struct xhci_trackpad_endpoint *t,unsigned category) {
    struct xhci_trackpad_sample *sample=&t->samples[category];
    if (sample->length || !t->last_length || t->last_length>APPLE_TRACKPAD_REPORT_MAX) return;
    for (unsigned n=0;n<t->last_length;n++) sample->data[n]=t->buffer[n];
    sample->length=t->last_length;
}

static uint64_t rdtsc(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static uint32_t mmio_read32(uint64_t offset) {
    #ifdef SRVROS_XHCI_TEST_MMIO
    return xhci_test_read32(offset);
#else
    return *(volatile uint32_t *)(xhci.mmio_virt + offset);
#endif
}

static void mmio_write32(uint64_t offset, uint32_t value) {
#ifdef SRVROS_XHCI_TEST_MMIO
    xhci_test_write32(offset, value);
#else
    *(volatile uint32_t *)(xhci.mmio_virt + offset) = value;
#endif
}

static void mmio_write64(uint64_t offset, uint64_t value) {
    *(volatile uint32_t *)(xhci.mmio_virt + offset) = (uint32_t)value;
    *(volatile uint32_t *)(xhci.mmio_virt + offset + 4) = (uint32_t)(value >> 32);
}

static uint8_t mmio_read8(uint64_t offset) {
    return *(volatile uint8_t *)(xhci.mmio_virt + offset);
}

static uint32_t op_read32(uint64_t offset) {
    return mmio_read32(xhci.op_base + offset);
}

static void op_write32(uint64_t offset, uint32_t value) {
    mmio_write32(xhci.op_base + offset, value);
}

static void op_write64(uint64_t offset, uint64_t value) {
    mmio_write64(xhci.op_base + offset, value);
}

static void rt_write32(uint64_t offset, uint32_t value) {
    mmio_write32(xhci.rt_base + offset, value);
}

static void rt_write64(uint64_t offset, uint64_t value) {
    mmio_write64(xhci.rt_base + offset, value);
}

static void db_write32(uint64_t index, uint32_t value) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
    mmio_write32(xhci.db_base + index * 4, value);
}

static void zero_bytes(void *ptr, uint64_t size) {
    uint8_t *bytes = ptr;
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0;
    }
}

static void copy_trb(struct xhci_trb *dst, const struct xhci_trb *src) {
    dst->parameter_low = src->parameter_low;
    dst->parameter_high = src->parameter_high;
    dst->status = src->status;
    dst->control = src->control;
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        dst[i] = src[i];
    }
}

static bool bytes_equal(const uint8_t *a, const uint8_t *b, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static uint8_t context_size(void) {
    return (xhci.hcc_params1 & (1u << 2)) != 0 ? 64 : 32;
}

static uint32_t context_read32(const uint8_t *base, uint64_t context_index, uint64_t dword) {
    const uint32_t *words = (const uint32_t *)(base + context_index * context_size());
    return words[dword];
}

static void context_write32(uint8_t *base, uint64_t context_index, uint64_t dword, uint32_t value) {
    uint32_t *words = (uint32_t *)(base + context_index * context_size());
    words[dword] = value;
}

static void write_link_trb(struct xhci_trb *ring, uint64_t ring_phys, bool cycle) {
    ring[XHCI_TRB_COUNT - 1] = (struct xhci_trb) {
        .parameter_low = (uint32_t)ring_phys,
        .parameter_high = (uint32_t)(ring_phys >> 32),
        .control = (XHCI_TRB_TYPE_LINK << 10) | (1u << 1) | (cycle ? 1u : 0u),
    };
}

static uint32_t port_speed(uint32_t portsc) {
    return (portsc >> 10) & 0xfu;
}

static uint16_t default_ep0_packet_for_speed(uint8_t speed) {
    if (speed == 1 || speed == 2) {
        return 8;
    }
    if (speed == 4) {
        return 512;
    }
    return 64;
}

static void parse_supported_protocols(void) {
    uint64_t offset = ((uint64_t)(xhci.hcc_params1 >> 16) & 0xffffu) * 4;
    uint64_t limit = XHCI_MMIO_MAP_SIZE - 16;
    while (offset != 0 && offset < limit) {
        uint32_t header = mmio_read32(offset);
        uint8_t cap_id = header & 0xffu;
        uint8_t next = (header >> 8) & 0xffu;
        if (cap_id == XHCI_EXT_CAP_SUPPORTED_PROTOCOL && xhci.protocol_count < XHCI_MAX_PROTOCOLS) {
            uint32_t name = mmio_read32(offset + 4);
            uint32_t ports = mmio_read32(offset + 8);
            uint32_t slot = mmio_read32(offset + 12);
            struct xhci_protocol *protocol = &xhci.protocols[xhci.protocol_count];
            protocol->present = true;
            protocol->revision = (uint16_t)(header >> 16);
            protocol->name[0] = (char)(name & 0xffu);
            protocol->name[1] = (char)((name >> 8) & 0xffu);
            protocol->name[2] = (char)((name >> 16) & 0xffu);
            protocol->name[3] = (char)((name >> 24) & 0xffu);
            protocol->name[4] = '\0';
            protocol->first_port = ports & 0xffu;
            protocol->port_count = (ports >> 8) & 0xffu;
            protocol->slot_type = slot & 0x1fu;
            uint64_t protocol_index = xhci.protocol_count + 1;
            xhci.protocol_count++;

            for (uint64_t port = protocol->first_port;
                 port < (uint64_t)protocol->first_port + protocol->port_count &&
                 port != 0 &&
                 port <= XHCI_MAX_PORTS;
                 port++) {
                xhci.ports[port - 1].slot_type = protocol->slot_type;
                xhci.ports[port - 1].protocol_index = (uint8_t)protocol_index;
            }
            console_printf("xhci: protocol %s rev=%x ports=%u-%u slot_type=%u\n",
                protocol->name,
                (uint64_t)protocol->revision,
                (uint64_t)protocol->first_port,
                (uint64_t)(protocol->first_port + protocol->port_count - 1),
                (uint64_t)protocol->slot_type);
        }
        if (next == 0) {
            break;
        }
        offset += (uint64_t)next * 4;
    }
}

/* No reset or runtime register writes while firmware still owns the controller.
 * The polling budget is bounded, like the existing reset wait; it is not a
 * calibrated wall clock. A timeout leaves the controller untouched. */
static bool claim_firmware_ownership(void) {
    uint64_t offset = ((uint64_t)(xhci.hcc_params1 >> 16) & 0xffffu) * 4;
    xhci.handoff = "no-legacy-cap";
    while (offset != 0) {
        if (offset > XHCI_MMIO_MAP_SIZE - 8) {
            xhci.handoff = "invalid-cap";
            return false;
        }
        uint32_t header = mmio_read32(offset);
        if (header == 0xffffffffu) {
            xhci.handoff = "unreadable";
            return false;
        }
        if ((header & 0xffu) == XHCI_EXT_CAP_LEGACY) {
            mmio_write32(offset, header | XHCI_LEGACY_OS_OWNED);
            uint64_t attempts = XHCI_RESET_TIMEOUT;
            while ((mmio_read32(offset) & XHCI_LEGACY_BIOS_OWNED) != 0 && attempts-- != 0) {
                __asm__ volatile ("pause");
            }
            if ((mmio_read32(offset) & XHCI_LEGACY_BIOS_OWNED) != 0) {
                xhci.handoff = "BIOS-timeout";
                console_write("xhci: firmware ownership timeout; aborting initialization\n");
                return false;
            }
            uint32_t control = mmio_read32(offset + 4);
            mmio_write32(offset + 4, (control & XHCI_LEGACY_PRESERVE) | XHCI_LEGACY_EVENTS);
            xhci.handoff = "OS-owned";
        }
        uint8_t next = (header >> 8) & 0xffu;
        if (next == 0) {
            break;
        }
        offset += (uint64_t)next * 4;
    }
    console_printf("xhci: firmware handoff=%s\n", xhci.handoff);
    return true;
}

static uint8_t root_port_slot_type(uint8_t root_port) {
    if (root_port == 0 || root_port > XHCI_MAX_PORTS) {
        return 0;
    }
    return xhci.ports[root_port - 1].slot_type;
}

static uint8_t endpoint_dci(uint8_t endpoint_address) {
    uint8_t endpoint_number = endpoint_address & 0x0f;
    if (endpoint_number == 0) {
        return 1;
    }
    return (uint8_t)(endpoint_number * 2 + ((endpoint_address & USB_ENDPOINT_IN) != 0 ? 1 : 0));
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

static bool wait_status_set(uint32_t mask, bool set, uint64_t attempts) {
    for (uint64_t i = 0; i < attempts; i++) {
        bool value = (op_read32(XHCI_OP_USBSTS) & mask) != 0;
        if (value == set) {
            return true;
        }
    }
    return false;
}

static bool halt_controller(void) {
    uint32_t command = op_read32(XHCI_OP_USBCMD);
    command &= ~(XHCI_USBCMD_RUN | XHCI_USBCMD_INTE);
    op_write32(XHCI_OP_USBCMD, command);
    return wait_status_set(XHCI_USBSTS_HCH, true, XHCI_RESET_TIMEOUT);
}

static bool reset_controller(void) {
    if (!halt_controller()) {
        console_write("xhci: halt timeout\n");
        return false;
    }

    op_write32(XHCI_OP_USBCMD, op_read32(XHCI_OP_USBCMD) | XHCI_USBCMD_HCRST);
    for (uint64_t i = 0; i < XHCI_RESET_TIMEOUT; i++) {
        if ((op_read32(XHCI_OP_USBCMD) & XHCI_USBCMD_HCRST) == 0) {
            break;
        }
    }
    if ((op_read32(XHCI_OP_USBCMD) & XHCI_USBCMD_HCRST) != 0) {
        console_write("xhci: reset timeout\n");
        return false;
    }
    if (!wait_status_set(XHCI_USBSTS_CNR, false, XHCI_RESET_TIMEOUT)) {
        console_write("xhci: controller not ready timeout\n");
        return false;
    }
    return true;
}

static void read_ports(void) {
    uint64_t count = xhci.port_count;
    if (count > XHCI_MAX_PORTS) {
        count = XHCI_MAX_PORTS;
    }

    for (uint64_t i = 0; i < count; i++) {
        xhci.ports[i].portsc = op_read32(XHCI_OP_PORTS + i * 0x10);
    }
}

static uint64_t port_offset(uint64_t index) {
    return XHCI_OP_PORTS + index * 0x10;
}

/* MFINDEX advances in 125 us units while the controller runs. Unlike the
 * uncalibrated LAPIC tick, it supplies the USB reset recovery interval here. */
static bool wait_microframes(uint32_t count) {
    uint32_t start = mmio_read32(xhci.rt_base) & 0x3fffu;
    for (uint64_t i = 0; i < XHCI_RESET_TIMEOUT; i++) {
        uint32_t now = mmio_read32(xhci.rt_base) & 0x3fffu;
        if (((now - start) & 0x3fffu) >= count) {
            return true;
        }
        __asm__ volatile ("pause");
    }
    console_write("xhci: microframe wait timed out\n");
    return false;
}

static uint32_t portsc_neutral(uint32_t state) {
    return state & (XHCI_PORTSC_RO | XHCI_PORTSC_RWS);
}

static bool reset_port(uint64_t index) {
    struct xhci_port_state *port = &xhci.ports[index];
    uint64_t offset = port_offset(index);
    uint32_t state = op_read32(offset);
    port->reset_result = "disconnected";
    if ((state & XHCI_PORTSC_CCS) == 0) {
        port->portsc = state;
        return false;
    }

    /* A stale PRC must not satisfy the new reset. Flush its acknowledgement
     * before requesting reset; PR=0 alone does not establish completion. */
    op_write32(offset, portsc_neutral(state) | (state & XHCI_PORTSC_CHANGE_MASK));
    state = op_read32(offset);
    op_write32(offset, portsc_neutral(state) | XHCI_PORTSC_PR);
    port->reset_attempts++;
    xhci.ports_reset++;
    port->reset_result = "reset-timeout";
    for (uint32_t elapsed = 0; elapsed <= 500; elapsed++) {
        state = op_read32(offset);
        if ((state & XHCI_PORTSC_CCS) == 0) {
            port->reset_result = "disconnected";
            break;
        }
        if ((state & XHCI_PORTSC_PRC) != 0 && (state & XHCI_PORTSC_PR) == 0) {
            port->reset_result = (state & XHCI_PORTSC_PED) ? "ready" : "reset-disabled";
            break;
        }
        if (elapsed == 500) break;
        if (!wait_microframes(8)) { /* 1 ms, bounded even if MFINDEX stalls. */
            port->reset_result = "clock-stalled";
            break;
        }
    }
    port->reset_portsc = state; /* Keep completion/change bits for diagnostics. */
    op_write32(offset, portsc_neutral(state) | (state & XHCI_PORTSC_CHANGE_MASK));
    bool complete = (state & (XHCI_PORTSC_CCS | XHCI_PORTSC_PED | XHCI_PORTSC_PRC | XHCI_PORTSC_PR)) ==
        (XHCI_PORTSC_CCS | XHCI_PORTSC_PED | XHCI_PORTSC_PRC);
    if (complete && !wait_microframes(80)) { /* USB reset recovery: 10 ms. */
        port->reset_result = "clock-stalled";
        complete = false;
    }
    port->portsc = op_read32(offset);
    if ((port->portsc & (XHCI_PORTSC_CCS | XHCI_PORTSC_PED)) !=
        (XHCI_PORTSC_CCS | XHCI_PORTSC_PED)) {
        if (complete) port->reset_result = "lost-after-reset";
        complete = false;
    }
    if (complete) {
        xhci.ports_enabled++;
    } else {
        /* Enumeration must not use an old/stale enabled state after failure. */
        port->portsc &= ~XHCI_PORTSC_PED;
    }
    return complete;
}

static void reset_connected_ports(void) {
    uint64_t count = xhci.port_count < XHCI_MAX_PORTS ? xhci.port_count : XHCI_MAX_PORTS;
    uint8_t stable[XHCI_MAX_PORTS] = {0};
    bool handled[XHCI_MAX_PORTS] = {false};
    for (uint64_t i = 0; i < count; i++) {
        struct xhci_port_state *port = &xhci.ports[i];
        uint32_t state = op_read32(port_offset(i));
        port->initial_portsc = port->portsc = state;
        port->reset_result = "no-connect";
        /* Power root ports before waiting for devices to become visible. */
        op_write32(port_offset(i), portsc_neutral(state) | XHCI_PORTSC_PP |
            (state & XHCI_PORTSC_CSC));
    }
    /* HCRST may reconnect USB2 devices asynchronously. Observe for a full
     * second, requiring 100 ms of stable connection before reset. This is
     * boot discovery only; runtime hotplug is not implemented. */
    for (uint32_t sample = 0; sample < 100; sample++) {
        if (!wait_microframes(80)) {
            for (uint64_t i = 0; i < count; i++) {
                if (!handled[i]) {
                    xhci.ports[i].reset_result = "clock-stalled";
                    xhci.ports[i].portsc &= ~XHCI_PORTSC_PED;
                }
            }
            break;
        }
        for (uint64_t i = 0; i < count; i++) {
            if (handled[i]) continue;
            struct xhci_port_state *port = &xhci.ports[i];
            uint32_t state = op_read32(port_offset(i));
            port->portsc = state & ~XHCI_PORTSC_PED;
            if ((state & XHCI_PORTSC_CSC) != 0) {
                stable[i] = 0;
                op_write32(port_offset(i), portsc_neutral(state) | XHCI_PORTSC_CSC);
            }
            if ((state & XHCI_PORTSC_CCS) == 0) {
                stable[i] = 0;
                port->reset_result = "no-connect";
                continue;
            }
            port->reset_result = "debouncing";
            if (stable[i]++ < 10) continue;
            handled[i] = true;
            /* USB2 needs a fresh default address even if firmware enabled it.
             * USB3 ports can already be enabled by link training. */
            if (port_speed(state) <= 3 || (state & XHCI_PORTSC_PED) == 0) {
                reset_port(i);
            } else {
                port->portsc = state;
                port->reset_result = "link-ready";
                xhci.ports_enabled++;
            }
        }
    }
}

static bool alloc_frame_zero(uint64_t *phys_out, void **virt_out) {
    uint64_t physical = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (physical == 0) {
        return false;
    }
    void *virt = pmm_phys_to_virt(physical);
    zero_bytes(virt, PMM_FRAME_SIZE);
    *phys_out = physical;
    *virt_out = virt;
    return true;
}

static uint16_t required_scratchpads(uint32_t hcs2) {
    return (uint16_t)((((hcs2 >> 21) & 0x1fu) << 5) | ((hcs2 >> 27) & 0x1fu));
}

static bool allocate_scratchpads(void) {
    xhci.scratchpad_count = required_scratchpads(xhci.hcs_params2);
    if ((xhci.page_size & 1u) == 0) {
        console_write("xhci: controller does not support 4 KiB pages\n");
        return false;
    }
    if (xhci.scratchpad_count == 0) {
        return true;
    }
    uint64_t pages = ((uint64_t)xhci.scratchpad_count * sizeof(uint64_t) + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    uint64_t array_phys = pmm_alloc_frames_tagged(pages, PMM_FRAME_DMA);
    if (array_phys == 0) {
        return false;
    }
    uint64_t *array = pmm_phys_to_virt(array_phys);
    zero_bytes(array, pages * PMM_FRAME_SIZE);
    for (uint16_t i = 0; i < xhci.scratchpad_count; i++) {
        void *buffer;
        if (!alloc_frame_zero(&array[i], &buffer)) {
            for (uint16_t j = 0; j < i; j++) {
                pmm_free_frame(array[j]);
            }
            pmm_free_frame_range(array_phys, pages);
            xhci.scratchpads_allocated = 0;
            return false;
        }
        xhci.scratchpads_allocated++;
    }
    xhci.dcbaa[0] = array_phys;
    console_printf("xhci: scratchpads allocated=%u array=%x\n",
        (uint64_t)xhci.scratchpads_allocated, array_phys);
    return true;
}

static bool allocate_runtime_structures(void) {
    void *virt = 0;
    if (!alloc_frame_zero(&xhci.dcbaa_phys, &virt)) {
        return false;
    }
    xhci.dcbaa = virt;
    if (!allocate_scratchpads()) {
        return false;
    }

    if (!alloc_frame_zero(&xhci.command_ring_phys, &virt)) {
        return false;
    }
    xhci.command_ring = virt;
    xhci.command_enqueue = 0;
    xhci.command_cycle = true;
    write_link_trb(xhci.command_ring, xhci.command_ring_phys, true);

    if (!alloc_frame_zero(&xhci.event_ring_phys, &virt)) {
        return false;
    }
    xhci.event_ring = virt;
    xhci.event_dequeue = 0;
    xhci.event_cycle = true;

    if (!alloc_frame_zero(&xhci.erst_phys, &virt)) {
        return false;
    }
    xhci.erst = virt;
    xhci.erst[0] = (struct xhci_erst_entry) {
        .ring_segment_base = xhci.event_ring_phys,
        .ring_segment_size = XHCI_EVENT_TRB_COUNT,
        .reserved = 0,
    };
    return true;
}

static void program_rings(void) {
    op_write32(XHCI_OP_CONFIG, xhci.max_slots);
    op_write64(XHCI_OP_DCBAAP, xhci.dcbaa_phys);
    op_write64(XHCI_OP_CRCR, xhci.command_ring_phys | 1u);

    uint64_t interrupter = XHCI_INTR0;
    rt_write32(interrupter + XHCI_INTR_IMAN, 0x1); /* Polling; IE stays clear. */
    rt_write32(interrupter + XHCI_INTR_IMOD, 0);
    rt_write32(interrupter + XHCI_INTR_ERSTSZ, 1);
    rt_write64(interrupter + XHCI_INTR_ERSTBA, xhci.erst_phys);
    rt_write64(interrupter + XHCI_INTR_ERDP, xhci.event_ring_phys | (1u << 3));
    op_write32(XHCI_OP_DNCTRL, 0);
}

static bool start_controller(void) {
    op_write32(XHCI_OP_USBSTS, XHCI_USBSTS_EINT);
    op_write32(XHCI_OP_USBCMD, (op_read32(XHCI_OP_USBCMD) & ~XHCI_USBCMD_INTE) | XHCI_USBCMD_RUN);
    return wait_status_set(XHCI_USBSTS_HCH, false, XHCI_RESET_TIMEOUT);
}

static bool poll_event(struct xhci_trb *event_out) {
    struct xhci_trb *event = &xhci.event_ring[xhci.event_dequeue];
    bool cycle = (((volatile struct xhci_trb *)event)->control & 1u) != 0;
    if (cycle != xhci.event_cycle) {
        return false;
    }

    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    copy_trb(event_out, event);
    xhci.event_dequeue++;
    if (xhci.event_dequeue >= XHCI_EVENT_TRB_COUNT) {
        xhci.event_dequeue = 0;
        xhci.event_cycle = !xhci.event_cycle;
    }
    uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(struct xhci_trb);
    rt_write64(XHCI_INTR0 + XHCI_INTR_ERDP, erdp | (1u << 3));
    return true;
}

static bool wait_command_completion(uint64_t command_phys, uint64_t expected_type, uint64_t *slot_out) {
    for (uint64_t i = 0; i < XHCI_COMMAND_TIMEOUT; i++) {
        struct xhci_trb event;
        if (!poll_event(&event)) {
            continue;
        }

        uint64_t type = (event.control >> 10) & 0x3f;
        if (type != XHCI_TRB_TYPE_COMMAND_COMPLETION) {
            dispatch_transfer_event(&event);
            continue;
        }

        uint64_t pointer = (uint64_t)event.parameter_low | ((uint64_t)event.parameter_high << 32);
        uint64_t completion_code = (event.status >> 24) & 0xff;
        uint64_t slot_id = (event.control >> 24) & 0xff;
        xhci.command_events++;
        xhci.last_completion_code = completion_code;
        xhci.last_slot_id = slot_id;
        xhci.last_command_pointer = pointer;
        xhci.last_expected_command_pointer = command_phys;
        if (pointer != command_phys) {
            xhci.command_pointer_mismatches++;
            if (completion_code != 1) {
                console_printf("xhci: command type=%u pointer mismatch ptr=%x expected=%x cc=%u slot=%u\n",
                    expected_type,
                    pointer,
                    command_phys,
                    completion_code,
                    slot_id);
                continue;
            }
            console_printf("xhci: command type=%u accepted pointer mismatch ptr=%x expected=%x slot=%u\n",
                expected_type,
                pointer,
                command_phys,
                slot_id);
        }
        if (slot_out != 0) {
            *slot_out = slot_id;
        }
        if (completion_code != 1) {
            console_printf("xhci: command type=%u failed cc=%u slot=%u\n",
                expected_type,
                completion_code,
                slot_id);
            return false;
        }
        return true;
    }

    console_printf("xhci: command type=%u timeout events=%u last_cc=%u last_ptr=%x expected=%x mismatches=%u\n",
        expected_type,
        xhci.command_events,
        xhci.last_completion_code,
        xhci.last_command_pointer,
        xhci.last_expected_command_pointer,
        xhci.command_pointer_mismatches);
    return false;
}

static bool submit_command_trb(const struct xhci_trb *source, uint64_t expected_type, uint64_t *slot_out) {
    uint64_t index = xhci.command_enqueue;
    if (index >= XHCI_TRB_COUNT - 1) {
        index = 0;
        xhci.command_enqueue = 0;
    }

    uint64_t command_phys = xhci.command_ring_phys + index * sizeof(struct xhci_trb);
    struct xhci_trb trb = *source;
    trb.control = (trb.control & ~1u) | (xhci.command_cycle ? 1u : 0u);
    copy_trb(&xhci.command_ring[index], &trb);
    xhci.command_enqueue++;
    if (xhci.command_enqueue == XHCI_TRB_COUNT - 1) {
        write_link_trb(xhci.command_ring, xhci.command_ring_phys, xhci.command_cycle);
        xhci.command_enqueue = 0;
        xhci.command_cycle = !xhci.command_cycle;
    }

    db_write32(0, 0);
    return wait_command_completion(command_phys, expected_type, slot_out);
}

static bool submit_command(uint32_t type, uint32_t flags, uint64_t *slot_out) {
    struct xhci_trb trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = (type << 10) | flags,
    };
    return submit_command_trb(&trb, type, slot_out);
}

static uint64_t ring_push(struct xhci_trb *ring,
    uint64_t ring_phys,
    uint64_t *enqueue,
    bool *cycle,
    const struct xhci_trb *source) {
    uint64_t index = *enqueue;
    if (index >= XHCI_TRB_COUNT - 1) {
        index = 0;
        *enqueue = 0;
    }

    uint64_t trb_phys = ring_phys + index * sizeof(struct xhci_trb);
    struct xhci_trb trb = *source;
    trb.control = (trb.control & ~1u) | (*cycle ? 1u : 0u);
    copy_trb(&ring[index], &trb);
    *enqueue = index + 1;
    if (*enqueue == XHCI_TRB_COUNT - 1) {
        write_link_trb(ring, ring_phys, *cycle);
        *enqueue = 0;
        *cycle = !*cycle;
    }
    return trb_phys;
}

static bool wait_transfer_completion(uint8_t slot_id,
    uint8_t dci,
    uint64_t expected_trb,
    uint64_t *completion_out) {
    for (uint64_t i = 0; i < XHCI_TRANSFER_TIMEOUT; i++) {
        struct xhci_trb event;
        if (!poll_event(&event)) {
            continue;
        }

        uint64_t type = (event.control >> 10) & 0x3f;
        if (type == XHCI_TRB_TYPE_COMMAND_COMPLETION) {
            xhci.command_events++;
            xhci.last_completion_code = (event.status >> 24) & 0xff;
            xhci.last_slot_id = (event.control >> 24) & 0xff;
            continue;
        }
        if (type != XHCI_TRB_TYPE_TRANSFER_EVENT) {
            continue;
        }

        uint64_t event_slot = (event.control >> 24) & 0xff;
        uint64_t event_dci = (event.control >> 16) & 0x1f;
        uint64_t pointer = (uint64_t)event.parameter_low | ((uint64_t)event.parameter_high << 32);
        uint64_t completion = (event.status >> 24) & 0xff;
        xhci.transfer_events++;
        xhci.last_completion_code = completion;
        xhci.last_slot_id = event_slot;
        if (event_slot != slot_id || event_dci != dci) {
            dispatch_transfer_event(&event);
            continue;
        }
        if (expected_trb != 0 && pointer != expected_trb) {
            continue;
        }
        if (completion_out != 0) {
            *completion_out = completion;
        }
        return completion == 1 || completion == 13;
    }
    console_printf("xhci: transfer timeout slot=%u dci=%u last_cc=%u\n",
        (uint64_t)slot_id,
        (uint64_t)dci,
        xhci.last_completion_code);
    return false;
}

static bool control_transfer(struct xhci_device *device,
    const struct usb_setup_packet *setup,
    uint64_t buffer_phys,
    uint32_t length,
    bool data_in) {
    uint64_t setup_value =
        (uint64_t)setup->request_type |
        ((uint64_t)setup->request << 8) |
        ((uint64_t)setup->value << 16) |
        ((uint64_t)setup->index << 32) |
        ((uint64_t)setup->length << 48);

    struct xhci_trb setup_trb = {
        .parameter_low = (uint32_t)setup_value,
        .parameter_high = (uint32_t)(setup_value >> 32),
        .status = 8,
        .control = (XHCI_TRB_TYPE_SETUP_STAGE << 10) |
            XHCI_TRB_IDT |
            (length == 0 ? XHCI_SETUP_TRT_NONE : (data_in ? XHCI_SETUP_TRT_IN : XHCI_SETUP_TRT_OUT)),
    };
    (void)ring_push(device->ep0_ring,
        device->ep0_ring_phys,
        &device->ep0_enqueue,
        &device->ep0_cycle,
        &setup_trb);

    if (length != 0) {
        struct xhci_trb data_trb = {
            .parameter_low = (uint32_t)buffer_phys,
            .parameter_high = (uint32_t)(buffer_phys >> 32),
            .status = length,
            .control = (XHCI_TRB_TYPE_DATA_STAGE << 10) |
                (data_in ? XHCI_DATA_DIR_IN : 0),
        };
        (void)ring_push(device->ep0_ring,
            device->ep0_ring_phys,
            &device->ep0_enqueue,
            &device->ep0_cycle,
            &data_trb);
    }

    struct xhci_trb status_trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = (XHCI_TRB_TYPE_STATUS_STAGE << 10) |
            XHCI_TRB_IOC |
            (length == 0 || !data_in ? XHCI_STATUS_DIR_IN : 0),
    };
    uint64_t status_phys = ring_push(device->ep0_ring,
        device->ep0_ring_phys,
        &device->ep0_enqueue,
        &device->ep0_cycle,
        &status_trb);

    db_write32(device->slot_id, 1);
    uint64_t completion = 0;
    if (!wait_transfer_completion(device->slot_id, 1, status_phys, &completion)) {
        console_printf("xhci: control request=%u failed cc=%u\n",
            (uint64_t)setup->request,
            completion);
        return false;
    }
    device->control_success++;
    return true;
}

static bool get_descriptor(struct xhci_device *device,
    uint8_t type,
    uint8_t index,
    uint16_t language,
    uint16_t length) {
    struct usb_setup_packet setup = {
        .request_type = 0x80,
        .request = USB_REQ_GET_DESCRIPTOR,
        .value = (uint16_t)(((uint16_t)type << 8) | index),
        .index = language,
        .length = length,
    };
    return control_transfer(device, &setup, device->descriptor_buffer_phys, length, true);
}

static bool get_hub_descriptor(struct xhci_device *device, uint16_t length) {
    struct usb_setup_packet setup = {
        .request_type = 0xa0,
        .request = USB_REQ_GET_DESCRIPTOR,
        .value = (uint16_t)(USB_DESC_HUB << 8),
        .index = 0,
        .length = length,
    };
    return control_transfer(device, &setup, device->descriptor_buffer_phys, length, true);
}

static bool hub_set_port_feature(struct xhci_device *hub, uint8_t port, uint16_t feature) {
    struct usb_setup_packet setup = {
        .request_type = 0x23,
        .request = USB_REQ_SET_FEATURE,
        .value = feature,
        .index = port,
        .length = 0,
    };
    return control_transfer(hub, &setup, 0, 0, false);
}

static bool hub_clear_port_feature(struct xhci_device *hub, uint8_t port, uint16_t feature) {
    struct usb_setup_packet setup = {
        .request_type = 0x23,
        .request = USB_REQ_CLEAR_FEATURE,
        .value = feature,
        .index = port,
        .length = 0,
    };
    return control_transfer(hub, &setup, 0, 0, false);
}

static bool hub_get_port_status(struct xhci_device *hub, uint8_t port, uint32_t *status_out) {
    struct usb_setup_packet setup = {
        .request_type = 0xa3,
        .request = USB_REQ_GET_STATUS,
        .value = 0,
        .index = port,
        .length = 4,
    };
    if (!control_transfer(hub, &setup, hub->descriptor_buffer_phys, 4, true)) {
        return false;
    }
    *status_out =
        (uint32_t)hub->descriptor_buffer[0] |
        ((uint32_t)hub->descriptor_buffer[1] << 8) |
        ((uint32_t)hub->descriptor_buffer[2] << 16) |
        ((uint32_t)hub->descriptor_buffer[3] << 24);
    return true;
}

static void parse_device_descriptor(struct xhci_device *device) {
    device->device_class = device->descriptor_buffer[4];
    device->device_subclass = device->descriptor_buffer[5];
    device->device_protocol = device->descriptor_buffer[6];
    device->vendor_id =
        (uint16_t)device->descriptor_buffer[8] |
        ((uint16_t)device->descriptor_buffer[9] << 8);
    device->product_id =
        (uint16_t)device->descriptor_buffer[10] |
        ((uint16_t)device->descriptor_buffer[11] << 8);
    device->device_bcd =
        (uint16_t)device->descriptor_buffer[12] |
        ((uint16_t)device->descriptor_buffer[13] << 8);
    device->configuration_count = device->descriptor_buffer[17];
}

static bool set_configuration(struct xhci_device *device, uint8_t configuration) {
    struct usb_setup_packet setup = {
        .request_type = 0x00,
        .request = USB_REQ_SET_CONFIGURATION,
        .value = configuration,
        .index = 0,
        .length = 0,
    };
    return control_transfer(device, &setup, 0, 0, false);
}

static bool set_hid_boot_protocol(struct xhci_device *device) {
    struct usb_setup_packet setup = {
        .request_type = 0x21,
        .request = USB_REQ_SET_PROTOCOL,
        .value = 0,
        .index = device->interface_number,
        .length = 0,
    };
    return control_transfer(device, &setup, 0, 0, false);
}

static bool allocate_device_runtime(struct xhci_device *device) {
    void *virt = 0;
    if (!alloc_frame_zero(&device->input_context_phys, &virt)) {
        return false;
    }
    device->input_context = virt;

    if (!alloc_frame_zero(&device->output_context_phys, &virt)) {
        return false;
    }
    device->output_context = virt;

    if (!alloc_frame_zero(&device->ep0_ring_phys, &virt)) {
        return false;
    }
    device->ep0_ring = virt;
    device->ep0_cycle = true;
    write_link_trb(device->ep0_ring, device->ep0_ring_phys, true);

    if (!alloc_frame_zero(&device->descriptor_buffer_phys, &virt)) {
        return false;
    }
    device->descriptor_buffer = virt;
    return true;
}

static struct xhci_device *device_by_slot(uint8_t slot_id) {
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        if (xhci.devices[i].present && xhci.devices[i].slot_id == slot_id) {
            return &xhci.devices[i];
        }
    }
    return 0;
}

static uint8_t device_root_port(const struct xhci_device *device) {
    if (device->parent_slot_id != 0) {
        const struct xhci_device *parent = device_by_slot(device->parent_slot_id);
        if (parent != 0) {
            return parent->port_id;
        }
    }
    return device->port_id;
}

static uint32_t child_route_string(uint32_t parent_route, uint8_t hub_port_id) {
    if (hub_port_id == 0 || hub_port_id > 15) {
        return parent_route;
    }
    for (uint32_t shift = 0; shift < 20; shift += 4) {
        if (((parent_route >> shift) & 0xfu) == 0) {
            return parent_route | ((uint32_t)hub_port_id << shift);
        }
    }
    return parent_route;
}

static bool device_needs_tt(const struct xhci_device *device) {
    if (device->parent_slot_id == 0) {
        return false;
    }
    return device->speed == 1 || device->speed == 2;
}

static void setup_address_input_context(struct xhci_device *device) {
    zero_bytes(device->input_context, PMM_FRAME_SIZE);
    context_write32(device->input_context, 0, 1, (1u << 0) | (1u << 1));

    uint32_t slot0 = device->route_string | ((uint32_t)device->speed << 20) | (1u << 27);
    uint32_t slot1 = ((uint32_t)device_root_port(device) << 16);
    uint32_t slot2 = 0;
    if (device_needs_tt(device)) {
        slot2 =
            ((uint32_t)device->parent_slot_id) |
            ((uint32_t)device->hub_port_id << 8);
    }
    context_write32(device->input_context, 1, 0, slot0);
    context_write32(device->input_context, 1, 1, slot1);
    context_write32(device->input_context, 1, 2, slot2);

    uint32_t ep0_info = (3u << 1) | (4u << 3);
    uint32_t ep0_packet = ((uint32_t)device->ep0_max_packet << 16);
    context_write32(device->input_context, 2, 1, ep0_info | ep0_packet);
    context_write32(device->input_context, 2, 2, (uint32_t)(device->ep0_ring_phys | 1u));
    context_write32(device->input_context, 2, 3, (uint32_t)(device->ep0_ring_phys >> 32));
    context_write32(device->input_context, 2, 4, 8);
}

static bool address_device(struct xhci_device *device) {
    uint64_t slot = 0;
    uint32_t enable_flags = ((uint32_t)device->slot_type << 16);
    if (!submit_command(XHCI_TRB_TYPE_ENABLE_SLOT, enable_flags, &slot) || slot == 0 || slot > xhci.max_slots) {
        console_printf("xhci: enable slot failed port=%u slot_type=%u flags=%x\n",
            (uint64_t)device->port_id,
            (uint64_t)device->slot_type,
            (uint64_t)enable_flags);
        return false;
    }
    device->slot_id = (uint8_t)slot;
    xhci.enable_slot_completions++;
    xhci.last_slot_id = slot;
    xhci.dcbaa[device->slot_id] = device->output_context_phys;

    setup_address_input_context(device);
    struct xhci_trb command = {
        .parameter_low = (uint32_t)device->input_context_phys,
        .parameter_high = (uint32_t)(device->input_context_phys >> 32),
        .status = 0,
        .control = (XHCI_TRB_TYPE_ADDRESS_DEVICE << 10) | ((uint32_t)device->slot_id << 24),
    };
    if (!submit_command_trb(&command, XHCI_TRB_TYPE_ADDRESS_DEVICE, 0)) {
        return false;
    }
    device->addressed = true;
    xhci.addressed_devices++;
    return true;
}

static bool parse_trackpad_config(struct xhci_device *device,uint16_t length) {
    if ((!trackpad_capture && !trackpad_pointer_enabled) || device->vendor_id!=0x05ac || (device->product_id!=0x0291 && device->product_id!=0x0262) ||
        device->speed!=1 || length<9 || length>PMM_FRAME_SIZE) return false;
    bool pro=device->product_id==0x0262;
    bool interface=false,found=false;
    uint8_t interval=0;
    for (uint16_t offset=0;offset<length;) {
        const uint8_t *p=device->descriptor_buffer+offset;
        if (length-offset<2 || p[0]<2 || p[0]>length-offset) return false;
        if (p[1]==USB_DESC_INTERFACE) {
            if (p[0]<9) return false;
            interface=p[2]==(pro?1:2) && p[3]==0 && p[4]==1 && p[5]==USB_CLASS_HID &&
                p[6]==(pro?0:USB_HID_SUBCLASS_BOOT) && p[7]==(pro?0:USB_HID_PROTOCOL_MOUSE);
        } else if (p[1]==USB_DESC_ENDPOINT && interface) {
            if (p[0]<7 || found || p[2]!=(pro?0x81:0x83) || (p[3]&3)!=USB_ENDPOINT_INTERRUPT ||
                p[4]!=64 || p[5]!=0 || p[6]!=(pro?2:1)) return false;
            found=true; interval=p[6];
        }
        offset+=p[0];
    }
    if (!found) return false;
    device->trackpad.candidate=true;
    device->trackpad.report_type=pro?2:3;
    device->trackpad.dci=pro?3:7; device->trackpad.packet=64; device->trackpad.interval=interval;
    return true;
}

static bool parse_hid_input_config(struct xhci_device *device,
    uint16_t length,
    uint8_t *configuration_out) {
    uint64_t offset = 0;
    uint8_t current_priority = 0;
    uint8_t current_interface = 0;
    uint8_t current_class = 0;
    uint8_t current_subclass = 0;
    uint8_t current_protocol = 0;
    uint8_t best_priority = 0;
    uint8_t best_interface = 0;
    uint8_t best_dci = 0;
    uint8_t best_interval = 0;
    uint16_t best_packet = 0;
    bool best_keyboard = false;
    bool best_mouse = false;
    bool best_boot_keyboard = false;
    bool best_boot_mouse = false;
    bool best_absolute_pointer = false;
    *configuration_out = device->descriptor_buffer[5];
    device->configuration_value = device->descriptor_buffer[5];
    device->interface_count = device->descriptor_buffer[4];

    while (offset + 2 <= length) {
        uint8_t size = device->descriptor_buffer[offset];
        uint8_t type = device->descriptor_buffer[offset + 1];
        if (size < 2 || offset + size > length) {
            break;
        }
        if (type == USB_DESC_INTERFACE && size >= 9) {
            current_interface = device->descriptor_buffer[offset + 2];
            current_class = device->descriptor_buffer[offset + 5];
            current_subclass = device->descriptor_buffer[offset + 6];
            current_protocol = device->descriptor_buffer[offset + 7];
            if (!device->have_interface_descriptor) {
                device->have_interface_descriptor = true;
                device->first_interface_class = current_class;
                device->first_interface_subclass = current_subclass;
                device->first_interface_protocol = current_protocol;
            }
            bool keyboard_hid =
                current_class == USB_CLASS_HID &&
                current_protocol == USB_HID_PROTOCOL_KEYBOARD;
            bool boot_hid =
                current_class == USB_CLASS_HID &&
                current_subclass == USB_HID_SUBCLASS_BOOT &&
                (current_protocol == USB_HID_PROTOCOL_KEYBOARD ||
                    current_protocol == USB_HID_PROTOCOL_MOUSE);
            bool generic_pointer_candidate =
                current_class == USB_CLASS_HID &&
                current_subclass == 0 &&
                current_protocol == 0;
            current_priority = 0;
            if (keyboard_hid) {
                current_priority = 3;
            } else if (boot_hid) {
                current_priority = 2;
            } else if (generic_pointer_candidate) {
                current_priority = 1;
            }
        } else if (type == USB_DESC_ENDPOINT && size >= 7 && current_priority != 0) {
            uint8_t address = device->descriptor_buffer[offset + 2];
            uint8_t attributes = device->descriptor_buffer[offset + 3] & 0x03;
            if ((address & USB_ENDPOINT_IN) != 0 &&
                attributes == USB_ENDPOINT_INTERRUPT &&
                current_priority > best_priority) {
                best_priority = current_priority;
                best_interface = current_interface;
                best_dci = endpoint_dci(address);
                best_packet =
                    (uint16_t)device->descriptor_buffer[offset + 4] |
                    ((uint16_t)device->descriptor_buffer[offset + 5] << 8);
                best_interval = device->descriptor_buffer[offset + 6];
                best_keyboard = current_class == USB_CLASS_HID &&
                    current_protocol == USB_HID_PROTOCOL_KEYBOARD;
                best_mouse = current_class == USB_CLASS_HID &&
                    current_protocol == USB_HID_PROTOCOL_MOUSE;
                best_boot_keyboard = best_keyboard &&
                    current_subclass == USB_HID_SUBCLASS_BOOT;
                best_boot_mouse = best_mouse &&
                    current_subclass == USB_HID_SUBCLASS_BOOT;
                best_absolute_pointer = current_class == USB_CLASS_HID &&
                    current_subclass == 0 &&
                    current_protocol == 0;
            }
        }
        offset += size;
    }
    if (best_priority == 0) {
        return false;
    }
    device->interface_number = best_interface;
    device->interrupt_dci = best_dci;
    device->interrupt_max_packet = best_packet & 0x7ffu;
    device->interrupt_max_burst = device->speed == 3 ? (best_packet >> 11) & 3u : 0;
    device->interrupt_interval = best_interval;
    device->hid_keyboard = best_keyboard;
    device->hid_mouse = best_mouse || best_absolute_pointer;
    device->hid_boot_keyboard = best_boot_keyboard;
    device->hid_boot_mouse = best_boot_mouse;
    device->hid_absolute_pointer = best_absolute_pointer;
    return true;
}

static uint8_t interrupt_interval_exponent(uint8_t speed, uint8_t interval) {
    if (speed >= 3) {
        if (interval < 1) interval = 1;
        if (interval > 16) interval = 16;
        return interval - 1;
    }
    /* Full/low-speed interrupt bInterval is in frames, xHCI in 2^n microframes. */
    uint16_t microframes = (uint16_t)(interval != 0 ? interval : 1) * 8;
    uint8_t exponent = 0;
    while (microframes > 1) {
        microframes >>= 1;
        exponent++;
    }
    return exponent > 10 ? 10 : exponent;
}

static void setup_interrupt_input_context(struct xhci_device *device) {
    zero_bytes(device->input_context, PMM_FRAME_SIZE);
    context_write32(device->input_context, 0, 1, (1u << 0) | (1u << device->interrupt_dci));
    uint32_t slot0 = context_read32(device->output_context, 0, 0);
    slot0 = (slot0 & ~(0x1fu << 27)) | ((uint32_t)device->interrupt_dci << 27);
    context_write32(device->input_context, 1, 0, slot0);
    context_write32(device->input_context, 1, 1, context_read32(device->output_context, 0, 1));
    context_write32(device->input_context, 1, 2, context_read32(device->output_context, 0, 2));
    uint64_t ep = (uint64_t)device->interrupt_dci + 1;
    uint32_t interval = interrupt_interval_exponent(device->speed, device->interrupt_interval);
    uint32_t payload = (uint32_t)device->interrupt_max_packet * (device->interrupt_max_burst + 1u);
    context_write32(device->input_context, ep, 0, interval << 16);
    context_write32(device->input_context, ep, 1, (3u << 1) | (7u << 3) |
        ((uint32_t)device->interrupt_max_burst << 8) | ((uint32_t)device->interrupt_max_packet << 16));
    context_write32(device->input_context, ep, 2, (uint32_t)(device->interrupt_ring_phys | 1u));
    context_write32(device->input_context, ep, 3, (uint32_t)(device->interrupt_ring_phys >> 32));
    /* Average TRB Length and Max ESIT Payload are both required for periodic endpoints. */
    context_write32(device->input_context, ep, 4, payload | (payload << 16));
}

static bool configure_interrupt_endpoint(struct xhci_device *device) {
    void *virt = 0;
    if (!alloc_frame_zero(&device->interrupt_ring_phys, &virt)) {
        return false;
    }
    device->interrupt_ring = virt;
    device->interrupt_cycle = true;
    write_link_trb(device->interrupt_ring, device->interrupt_ring_phys, true);

    if (!alloc_frame_zero(&device->interrupt_buffer_phys, &virt)) {
        return false;
    }
    device->interrupt_buffer = virt;

    setup_interrupt_input_context(device);

    struct xhci_trb command = {
        .parameter_low = (uint32_t)device->input_context_phys,
        .parameter_high = (uint32_t)(device->input_context_phys >> 32),
        .status = 0,
        .control = (XHCI_TRB_TYPE_CONFIGURE_ENDPOINT << 10) | ((uint32_t)device->slot_id << 24),
    };
    return submit_command_trb(&command, XHCI_TRB_TYPE_CONFIGURE_ENDPOINT, 0);
}

static void setup_trackpad_input_context(struct xhci_device *device) {
    struct xhci_trackpad_endpoint *t=&device->trackpad;
    zero_bytes(device->input_context,PMM_FRAME_SIZE);
    /* Add only the new endpoint. Never drop/reconfigure the live keyboard. */
    context_write32(device->input_context,0,1,1u | (1u<<t->dci));
    uint32_t slot0=context_read32(device->output_context,0,0);
    uint32_t entries=(slot0>>27)&31;
    if (entries<t->dci) entries=t->dci;
    context_write32(device->input_context,1,0,(slot0&~(31u<<27))|(entries<<27));
    context_write32(device->input_context,1,1,context_read32(device->output_context,0,1));
    context_write32(device->input_context,1,2,context_read32(device->output_context,0,2));
    uint64_t ep=t->dci+1;
    context_write32(device->input_context,ep,0,(uint32_t)interrupt_interval_exponent(device->speed,t->interval)<<16);
    context_write32(device->input_context,ep,1,(3u<<1)|(7u<<3)|((uint32_t)t->packet<<16));
    context_write32(device->input_context,ep,2,(uint32_t)(t->ring_phys|1u));
    context_write32(device->input_context,ep,3,(uint32_t)(t->ring_phys>>32));
    /* USB packet/ESIT is 64 bytes; a report spans multiple transactions. */
    context_write32(device->input_context,ep,4,APPLE_TRACKPAD_REPORT_MAX|((uint32_t)t->packet<<16));
}

/* Wellspring7 requires its documented eight-byte feature mode switch.
 * Read/modify/write preserves opaque bytes. Type3 has no mode switch. */
static bool enable_trackpad_mode(struct xhci_device *device) {
    if (device->trackpad.report_type==3) return true;
    if (device->vendor_id!=0x05ac || device->product_id!=0x0262 ||
        !device->trackpad.candidate || device->trackpad.report_type!=2) return false;
    struct usb_setup_packet setup={.request_type=0xa1,.request=1,.value=0x300,.index=0,.length=8};
    zero_bytes(device->descriptor_buffer,8);
    if (!control_transfer(device,&setup,device->descriptor_buffer_phys,8,true)) return false;
    device->descriptor_buffer[0]=1;
    setup.request_type=0x21;setup.request=9;
    return control_transfer(device,&setup,device->descriptor_buffer_phys,8,false);
}

static bool configure_trackpad_endpoint(struct xhci_device *device) {
    struct xhci_trackpad_endpoint *t=&device->trackpad;
    void *virt;
    if (!alloc_frame_zero(&t->ring_phys,&virt)) return false;
    t->ring=virt; t->cycle=true;
    write_link_trb(t->ring,t->ring_phys,true);
    if (!alloc_frame_zero(&t->buffer_phys,&virt)) {
        pmm_free_frame(t->ring_phys); t->ring_phys=0; t->ring=0; return false;
    }
    t->buffer=virt;
    setup_trackpad_input_context(device);
    struct xhci_trb command={.parameter_low=(uint32_t)device->input_context_phys,
        .parameter_high=(uint32_t)(device->input_context_phys>>32),
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)device->slot_id<<24)};
    /* Keep DMA allocated on an ambiguous command timeout: the controller may
     * still own it. No doorbell is issued unless configuration succeeded. */
    t->ready=submit_command_trb(&command,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,0);
    return t->ready;
}

static void submit_trackpad_poll(struct xhci_device *device) {
    struct xhci_trackpad_endpoint *t=&device->trackpad;
    if (!t->ready || t->pending) return;
    zero_bytes(t->buffer,APPLE_TRACKPAD_REPORT_MAX);
    struct xhci_trb trb={.parameter_low=(uint32_t)t->buffer_phys,
        .parameter_high=(uint32_t)(t->buffer_phys>>32),.status=APPLE_TRACKPAD_REPORT_MAX,
        .control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    t->trb=ring_push(t->ring,t->ring_phys,&t->enqueue,&t->cycle,&trb);
    t->pending=true;
    db_write32(device->slot_id,t->dci);
}

static bool evaluate_hub_context(struct xhci_device *hub) {
    zero_bytes(hub->input_context, PMM_FRAME_SIZE);
    context_write32(hub->input_context, 0, 1, 1u << 0);
    uint32_t slot0 = context_read32(hub->output_context, 0, 0) | (1u << 26);
    uint32_t slot1 = context_read32(hub->output_context, 0, 1);
    slot1 &= ~(0xffu << 24);
    slot1 |= ((uint32_t)hub->hub_ports << 24);
    context_write32(hub->input_context, 1, 0, slot0);
    context_write32(hub->input_context, 1, 1, slot1);
    context_write32(hub->input_context, 1, 2, context_read32(hub->output_context, 0, 2));

    struct xhci_trb command = {
        .parameter_low = (uint32_t)hub->input_context_phys,
        .parameter_high = (uint32_t)(hub->input_context_phys >> 32),
        .status = 0,
        .control = (XHCI_TRB_TYPE_EVALUATE_CONTEXT << 10) | ((uint32_t)hub->slot_id << 24),
    };
    return submit_command_trb(&command, XHCI_TRB_TYPE_EVALUATE_CONTEXT, 0);
}

static void handle_hid_report(struct xhci_device *device) {
    uint8_t *report = device->interrupt_buffer;
    if (bytes_equal(report, device->previous_report, 8)) {
        return;
    }
    if (trackpad_gestures && device->trackpad.candidate && !(report[0]&0xdd)) {
        for (unsigned i=2;i<8;i++) {
            if (report[i]<4 || report[i]>0x38) continue;
            bool held=false;
            for (unsigned j=2;j<8;j++) held |= device->previous_report[j]==report[i];
            if (!held) { device->trackpad.typing_until=timer_monotonic_us()+300000; break; }
        }
    }
    keyboard_inject_hid_report(report, &device->keyboard_state);
    copy_bytes(device->previous_report, report, 8);
    device->hid_reports++;
}

static void handle_hid_mouse_report(struct xhci_device *device) {
    uint8_t *report = device->interrupt_buffer;
    uint64_t length = device->interrupt_max_packet;
    if (length > 8) {
        length = 8;
    }
    if (bytes_equal(report, device->previous_report, length)) {
        return;
    }

    int32_t dx = 0;
    int32_t dy = 0;
    if (device->hid_absolute_pointer) {
        if (length < 5) {
            return;
        }
        uint16_t x = (uint16_t)report[1] | ((uint16_t)report[2] << 8);
        uint16_t y = (uint16_t)report[3] | ((uint16_t)report[4] << 8);
        if (device->hid_have_absolute) {
            int32_t raw_dx = (int32_t)x - (int32_t)device->previous_absolute_x;
            int32_t raw_dy = (int32_t)y - (int32_t)device->previous_absolute_y;
            dx = raw_dx / 256;
            dy = raw_dy / 256;
            if (dx == 0 && raw_dx != 0) {
                dx = raw_dx > 0 ? 1 : -1;
            }
            if (dy == 0 && raw_dy != 0) {
                dy = raw_dy > 0 ? 1 : -1;
            }
        }
        device->previous_absolute_x = x;
        device->previous_absolute_y = y;
        device->hid_have_absolute = true;
    } else {
        if (length < 3) {
            return;
        }
        dx = (int8_t)report[1];
        dy = (int8_t)report[2];
    }
    mouse_inject_event(dx, dy, report[0] & 0x07);
    copy_bytes(device->previous_report, report, length);
    device->mouse_reports++;
}

static void submit_interrupt_poll(struct xhci_device *device) {
    if (!device->interrupt_ready || device->interrupt_pending || (!device->hid_keyboard && !device->hid_mouse)) {
        return;
    }

    uint64_t length = device->interrupt_max_packet;
    if (length == 0 || length > PMM_FRAME_SIZE) {
        length = 8;
    }
    zero_bytes(device->interrupt_buffer, length);
    struct xhci_trb trb = {
        .parameter_low = (uint32_t)device->interrupt_buffer_phys,
        .parameter_high = (uint32_t)(device->interrupt_buffer_phys >> 32),
        .status = (uint32_t)length,
        .control = (XHCI_TRB_TYPE_NORMAL << 10) | XHCI_TRB_IOC | XHCI_TRB_ISP,
    };
    device->interrupt_trb = ring_push(device->interrupt_ring,
        device->interrupt_ring_phys,
        &device->interrupt_enqueue,
        &device->interrupt_cycle,
        &trb);
    device->interrupt_pending = true;
    db_write32(device->slot_id, device->interrupt_dci);
}

static void cancel_trackpad_pointer(struct xhci_trackpad_endpoint *t) {
    if (!trackpad_pointer_enabled) return;
    struct trackpad_motion motion;
    if (trackpad_pointer_cancel(&t->pointer,&motion)) mouse_cancel_trackpad();
    t->pointer_cancels++;
}

#include "xhci_network.inc"

/* Every event-ring consumer routes unrelated transfers here. All callers
 * hold IRQs disabled, including the keyboard service running during disk I/O. */
static void dispatch_transfer_event(const struct xhci_trb *event) {
    if (((event->control >> 10) & 0x3f) != XHCI_TRB_TYPE_TRANSFER_EVENT) return;
    uint8_t slot = event->control >> 24, dci = (event->control >> 16) & 31;
    uint8_t completion = event->status >> 24;
    uint32_t remaining = event->status & 0xffffff;
    uint64_t pointer = event->parameter_low | ((uint64_t)event->parameter_high << 32);
    xhci.transfer_events++;
    xhci.last_completion_code = completion;
    xhci.last_slot_id = slot;
    struct xhci_device *device = device_by_slot(slot);
    if (!device) return;
    if(network_event(device,dci,pointer,completion,remaining)) return;
    struct xhci_trackpad_endpoint *t=&device->trackpad;
    if (t->pending && dci==t->dci && pointer==t->trb) {
        t->pending=false; t->last_completion=completion;
        if (!t->ready) return; /* Late completion after endpoint shutdown. */
        if ((completion!=1 && completion!=13) || remaining>APPLE_TRACKPAD_REPORT_MAX) {
            t->errors++; t->ready=false; cancel_trackpad_pointer(t); return;
        }
        t->last_length=APPLE_TRACKPAD_REPORT_MAX-remaining;
        uint8_t previous_contacts=t->frame.count;
        int decoded=apple_trackpad_decode_type(t->report_type,t->buffer,t->last_length,&t->frame);
        if (decoded<0) { t->malformed++; trackpad_sample(t,4); cancel_trackpad_pointer(t); }
        if (decoded>0) {
            t->reports++;
            if (trackpad_pointer_enabled) {
                struct trackpad_motion motion;
                uint8_t buttons=t->pointer.buttons;
                unsigned speed,options;mouse_trackpad_get_settings(&speed,&options);
                uint64_t sample_us=timer_monotonic_us();
                bool typing=sample_us<t->typing_until ||
                    (device->keyboard_state.repeat_usage && !(device->keyboard_state.previous_report[0]&0xdd));
                bool changed=trackpad_pointer_configured(&t->pointer,&t->frame,trackpad_gestures,typing,
                    sample_us,speed,options,&motion);
                t->typing_suppressed+=motion.typing_suppressed;
                t->palm_suppressed+=motion.palm_suppressed;
                t->drag_frames+=motion.two_finger_drag;t->drag_rebases+=motion.drag_rebased;
                t->drag_handoffs+=motion.drag_handoff;
                if (changed) {
                    if(buttons && motion.palm_suppressed) {
                        mouse_cancel_trackpad();t->pointer_cancels++;
                    } else mouse_inject_trackpad_scroll(motion.dx,motion.dy,motion.wheel_x,motion.wheel_y,motion.buttons);
                    if(motion.tap_buttons) {
                        mouse_inject_trackpad_tap(motion.tap_buttons);
                        t->tap_primary+=motion.tap_buttons==1;t->tap_secondary+=motion.tap_buttons==2;
                    }
                    if (motion.wheel_x || motion.wheel_y) t->scroll_events++;
                    t->pointer_events++;
                    if (motion.dx || motion.dy) t->pointer_motion++;
                    if (buttons!=motion.buttons) t->pointer_buttons++;
                }
            }
            for (unsigned i=0;i<t->frame.count;i++) {
                const struct apple_trackpad_contact *c=&t->frame.contacts[i];
                if (c->touch_major>(int32_t)t->max_touch_major) t->max_touch_major=c->touch_major;
                if (c->touch_minor>(int32_t)t->max_touch_minor) t->max_touch_minor=c->touch_minor;
            }
            trackpad_sample(t,0);
            if (t->frame.count) { t->contact_reports++; trackpad_sample(t,1); }
            if (t->frame.count>1) trackpad_sample(t,2);
            if (t->frame.button) { t->button_reports++; trackpad_sample(t,3); }
            if (previous_contacts && !t->frame.count) t->lifts++;
            if (t->max_contacts<t->frame.count) t->max_contacts=t->frame.count;
        }
        submit_trackpad_poll(device);
        return;
    }
    if (device->bulk_pending && dci == device->bulk_dci && pointer == device->bulk_trb) {
        device->bulk_completion = completion;
        device->bulk_remaining = remaining;
        device->bulk_done = true;
        device->bulk_pending = false;
        return;
    }
    if (!device->interrupt_pending || dci != device->interrupt_dci || pointer != device->interrupt_trb) return;
    device->interrupt_pending = false;
    device->last_interrupt_completion = completion;
    device->last_interrupt_remaining = remaining;
    if ((completion == 1 || completion == 13) && remaining <= device->interrupt_max_packet) {
        device->interrupt_success++;
        if (device->hid_keyboard && device->interrupt_max_packet - remaining >= 8) handle_hid_report(device);
        else if (device->hid_mouse) handle_hid_mouse_report(device);
        /* Keep the endpoint supplied even when disk I/O or another runnable
         * application delays the next worker turn. The report was consumed
         * above; only now may submission clear/reuse its DMA buffer. */
        submit_interrupt_poll(device);
    } else device->interrupt_errors++;
}

static void poll_hid_events_once(void) {
    uint64_t flags = event_irq_save();
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        submit_interrupt_poll(&xhci.devices[i]);
        submit_trackpad_poll(&xhci.devices[i]);
    }
    struct xhci_trb event;
    /* Bound work even if a malfunctioning device floods events. */
    for (unsigned i=0; i<XHCI_EVENT_TRB_COUNT && poll_event(&event); i++) dispatch_transfer_event(&event);
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) if(xhci.devices[i].network) network_service(&xhci.devices[i]);
    event_irq_restore(flags);
}

static bool xhci_io_service_ready(void *arg) {
    (void)arg;
    return xhci.operational && !xhci_input_probe_active;
}

static bool input_poll_due(void *arg) { return timer_ticks()>=*(uint64_t *)arg; }

static void xhci_poll_worker(void *arg) {
    (void)arg;
    struct scheduler_wait_queue wake={0};
    for (;;) {
        if (xhci.operational && !xhci_input_probe_active && (xhci.hid_keyboards != 0 || xhci.hid_mice != 0 || network_count)) {
            poll_hid_events_once();
            uint64_t now = timer_monotonic_us();
            latency_input_poll(now);
            for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
                struct xhci_device *d = &xhci.devices[i];
                if (d->present && d->interrupt_ready && d->hid_keyboard) {
                    uint32_t portsc = op_read32(XHCI_OP_PORTS + (d->port_id - 1) * 0x10);
                    if (portsc & XHCI_PORTSC_CCS) keyboard_repeat_hid(&d->keyboard_state, now);
                    else {
                        d->keyboard_state.repeat_usage = 0;
                        uint64_t flags=event_irq_save();
                        if (d->trackpad.ready) {
                            d->trackpad.ready=false; d->trackpad.errors++;
                            cancel_trackpad_pointer(&d->trackpad);
                        }
                        event_irq_restore(flags);
                    }
                }
            }
        }
        /* The controller is still polled, at most one timer tick apart when
         * otherwise idle. Storage waits continue dispatching transfer events
         * directly; they do not depend on this sleep finishing. */
        uint64_t due=timer_ticks()+1;
        if(!scheduler_wait_timeout(&wake,input_poll_due,&due,due)) scheduler_yield();
    }
}

static struct xhci_device *allocate_device_record(void) {
    if (xhci.addressed_devices >= XHCI_MAX_DEVICES) {
        xhci.device_record_full++;
        return 0;
    }
    return &xhci.devices[xhci.addressed_devices];
}

static uint8_t hub_status_speed(uint32_t port_status) {
    if ((port_status & USB_HUB_PORT_HIGH_SPEED) != 0) {
        return 3;
    }
    if ((port_status & USB_HUB_PORT_LOW_SPEED) != 0) {
        return 2;
    }
    return 1;
}

static void clear_hub_port_changes(struct xhci_device *hub, uint8_t port, uint32_t status) {
    if ((status & (1u << USB_FEATURE_C_PORT_CONNECTION)) != 0) {
        (void)hub_clear_port_feature(hub, port, USB_FEATURE_C_PORT_CONNECTION);
    }
    if ((status & (1u << USB_FEATURE_C_PORT_ENABLE)) != 0) {
        (void)hub_clear_port_feature(hub, port, USB_FEATURE_C_PORT_ENABLE);
    }
    if ((status & (1u << USB_FEATURE_C_PORT_RESET)) != 0) {
        (void)hub_clear_port_feature(hub, port, USB_FEATURE_C_PORT_RESET);
    }
}

static bool reset_hub_port(struct xhci_device *hub, uint8_t port, uint32_t *status_out) {
    uint32_t status = 0;
    if (!hub_set_port_feature(hub, port, USB_FEATURE_PORT_POWER)) {
        return false;
    }
    xhci.hub_ports_powered++;
    for (uint64_t i = 0; i < 250000; i++) {
        __asm__ volatile ("pause");
    }
    if (!hub_get_port_status(hub, port, &status) ||
        (status & USB_HUB_PORT_CONNECTION) == 0) {
        return false;
    }
    if (!hub_set_port_feature(hub, port, USB_FEATURE_PORT_RESET)) {
        return false;
    }
    for (uint64_t i = 0; i < XHCI_RESET_TIMEOUT; i++) {
        if (!hub_get_port_status(hub, port, &status)) {
            return false;
        }
        if ((status & USB_HUB_PORT_ENABLE) != 0 && (status & (1u << USB_FEATURE_PORT_RESET)) == 0) {
            break;
        }
    }
    clear_hub_port_changes(hub, port, status);
    xhci.hub_ports_reset++;
    *status_out = status;
    return (status & USB_HUB_PORT_ENABLE) != 0;
}

static bool enumerate_device(struct xhci_device *device);

static bool enumerate_hub(struct xhci_device *hub, uint8_t configuration) {
    if (!set_configuration(hub, configuration)) {
        return false;
    }
    hub->configured = true;
    xhci.configured_devices++;

    if (!get_hub_descriptor(hub, 8)) {
        console_printf("xhci: hub slot=%u descriptor failed\n", (uint64_t)hub->slot_id);
        return false;
    }
    hub->hub_ports = hub->descriptor_buffer[2];
    if (hub->hub_ports > XHCI_MAX_HUB_PORTS) {
        hub->hub_ports = XHCI_MAX_HUB_PORTS;
    }
    if (hub->hub_ports == 0) {
        return true;
    }
    if (!evaluate_hub_context(hub)) {
        console_printf("xhci: hub slot=%u evaluate failed\n", (uint64_t)hub->slot_id);
        return false;
    }
    console_printf("xhci: hub slot=%u root_port=%u ports=%u route=%x\n",
        (uint64_t)hub->slot_id,
        (uint64_t)hub->port_id,
        (uint64_t)hub->hub_ports,
        (uint64_t)hub->route_string);

    for (uint8_t port = 1; port <= hub->hub_ports; port++) {
        uint32_t status = 0;
        if (!reset_hub_port(hub, port, &status)) {
            continue;
        }
        struct xhci_device *child = allocate_device_record();
        if (child == 0) {
            console_printf("xhci: device table full while scanning hub slot=%u port=%u\n",
                (uint64_t)hub->slot_id,
                (uint64_t)port);
            return false;
        }
        *child = (struct xhci_device) {
            .present = true,
            .port_id = hub->port_id,
            .parent_slot_id = hub->slot_id,
            .hub_port_id = port,
            .speed = hub_status_speed(status),
            .ep0_max_packet = default_ep0_packet_for_speed(hub_status_speed(status)),
            .route_string = child_route_string(hub->route_string, port),
            .slot_type = root_port_slot_type(hub->port_id),
        };
        xhci.routed_devices++;
        if (!enumerate_device(child)) {
            console_printf("xhci: hub slot=%u port=%u child enumerate failed\n",
                (uint64_t)hub->slot_id,
                (uint64_t)port);
        }
    }
    return true;
}

/* Select one complete alternate-zero SCSI/BOT interface. Require both bulk
 * endpoints and, at SuperSpeed, their immediately following companions. */
static bool parse_storage_config(struct xhci_device *device, uint16_t length) {
    const uint8_t *b=device->descriptor_buffer;
    if(length<9 || b[0]!=9 || b[1]!=2 || (b[2]|(uint16_t)b[3]<<8)!=length || !b[5]) return false;
    bool selected=false;
    unsigned found=0;
    struct xhci_bulk_endpoint ep[2]={0};
    for(unsigned off=9;off<length;) {
        unsigned n=b[off], type=off+1<length?b[off+1]:0;
        if(n<2 || n>length-off) return false;
        if(type==4) {
            if(selected) break;
            if(n<9) return false;
            selected=b[off+3]==0 && b[off+4]==2 && b[off+5]==8 && b[off+6]==6 && b[off+7]==0x50;
            if(selected) device->interface_number=b[off+2];
        } else if(selected && type==5) {
            if(n<7 || (b[off+3]&3)!=2 || (b[off+2]&0x70) || !(b[off+2]&15)) return false;
            unsigned in=b[off+2]>>7;
            if(ep[in].dci) return false;
            ep[in].dci=endpoint_dci(b[off+2]);
            ep[in].packet=b[off+4]|(uint16_t)b[off+5]<<8;
            if(device->speed==2 || !ep[in].packet ||
                (device->speed==1 && ep[in].packet!=8 && ep[in].packet!=16 && ep[in].packet!=32 && ep[in].packet!=64) ||
                (device->speed==3 && ep[in].packet!=512) || (device->speed>=4 && ep[in].packet!=1024)) return false;
            if(device->speed>=4) {
                unsigned c=off+n;
                if(c+6>length || b[c]!=6 || b[c+1]!=0x30 || b[c+2]>15 || b[c+3] || b[c+4] || b[c+5]) return false;
                ep[in].burst=b[c+2];
            }
            found++;
        }
        off+=n;
    }
    if(found!=2) return false;
    device->bulk_ep[0]=ep[0]; device->bulk_ep[1]=ep[1];
    device->configuration_value=b[5];
    return true;
}
static void setup_bulk_input_context(struct xhci_device *d) {
    zero_bytes(d->input_context,PMM_FRAME_SIZE);
    uint8_t max=d->bulk_ep[0].dci>d->bulk_ep[1].dci?d->bulk_ep[0].dci:d->bulk_ep[1].dci;
    context_write32(d->input_context,0,1,1u|(1u<<d->bulk_ep[0].dci)|(1u<<d->bulk_ep[1].dci));
    uint32_t slot0=context_read32(d->output_context,0,0);
    context_write32(d->input_context,1,0,(slot0&~(31u<<27))|((uint32_t)max<<27));
    for(unsigned i=1;i<3;i++) context_write32(d->input_context,1,i,context_read32(d->output_context,0,i));
    for(unsigned in=0;in<2;in++) {
        struct xhci_bulk_endpoint *ep=&d->bulk_ep[in];
        uint64_t idx=ep->dci+1;
        context_write32(d->input_context,idx,1,(3u<<1)|((in?6u:2u)<<3)|((uint32_t)ep->burst<<8)|((uint32_t)ep->packet<<16));
        context_write32(d->input_context,idx,2,(uint32_t)(ep->ring_phys|1));
        context_write32(d->input_context,idx,3,(uint32_t)(ep->ring_phys>>32));
        context_write32(d->input_context,idx,4,4096); /* Average TRB length; no ESIT. */
    }
}
static bool configure_bulk_endpoints(struct xhci_device *d) {
    void *virt;
    for(unsigned in=0;in<2;in++) {
        struct xhci_bulk_endpoint *ep=&d->bulk_ep[in];
        if(!alloc_frame_zero(&ep->ring_phys,&virt)) return false;
        ep->ring=virt; ep->cycle=true;
        write_link_trb(ep->ring,ep->ring_phys,true);
    }
    if(!alloc_frame_zero(&d->bulk_buffer_phys,&virt)) return false;
    d->bulk_buffer=virt;
    setup_bulk_input_context(d);
    struct xhci_trb cmd={.parameter_low=(uint32_t)d->input_context_phys,
        .parameter_high=(uint32_t)(d->input_context_phys>>32),
        .control=(XHCI_TRB_TYPE_CONFIGURE_ENDPOINT<<10)|((uint32_t)d->slot_id<<24)};
    return submit_command_trb(&cmd,XHCI_TRB_TYPE_CONFIGURE_ENDPOINT,0);
}
static bool storage_bulk_transfer(void *context,bool in,void *buffer,uint32_t length,uint32_t *actual) {
    struct xhci_device *d=context;
    if(d->bulk_failed || d->bulk_pending || !length || length>PMM_FRAME_SIZE) return false;
    struct xhci_bulk_endpoint *ep=&d->bulk_ep[in?1:0];
    d->bulk_reason="pending"; d->bulk_length=length; d->bulk_elapsed_us=0; d->bulk_spins=0;
    d->bulk_completion=0; d->bulk_remaining=0;
    if(!in) copy_bytes(d->bulk_buffer,buffer,length);
    struct xhci_trb trb={.parameter_low=(uint32_t)d->bulk_buffer_phys,
        .parameter_high=(uint32_t)(d->bulk_buffer_phys>>32),.status=length,
        .control=(XHCI_TRB_TYPE_NORMAL<<10)|XHCI_TRB_IOC|XHCI_TRB_ISP};
    d->bulk_trb=ring_push(ep->ring,ep->ring_phys,&ep->enqueue,&ep->cycle,&trb);
    d->bulk_dci=ep->dci; d->bulk_done=false; d->bulk_pending=true;
    db_write32(d->slot_id,ep->dci);
    uint64_t start=timer_monotonic_us(), last=start, clock_at=start;
    uint64_t spins=0, stalled_spins=0;
    for(;;spins++) {
        struct xhci_trb event;
        if(poll_event(&event)) dispatch_transfer_event(&event);
        if(d->bulk_done) {
            if(d->bulk_completion!=1 && d->bulk_completion!=13) { d->bulk_reason="completion-code"; break; }
            if(d->bulk_remaining>length) { d->bulk_reason="invalid-residue"; break; }
            d->bulk_reason="completed";
            *actual=length-d->bulk_remaining;
            if(in) copy_bytes(buffer,d->bulk_buffer,*actual);
            return true;
        }
        if((spins&255)==0) {
            uint64_t now=timer_monotonic_us();
            if(now-start>=2000000) { d->bulk_reason="deadline"; break; }
            /* A fast CPU can exhaust the old total iteration limit well
             * before two seconds. Bound a stopped/unavailable clock instead;
             * every observed clock advance renews that fallback budget. */
            stalled_spins=now==clock_at ? stalled_spins+256 : 0;
            clock_at=now;
            if(stalled_spins>=XHCI_TRANSFER_TIMEOUT) { d->bulk_reason="clock-stalled"; break; }
            if(now-last>=1000 && scheduler_io_active()) { scheduler_io_yield(); last=now; }
        }
        __asm__ volatile("pause");
    }
    /* Never reuse the buffer/rings: a timed-out DMA may still be active. */
    d->bulk_elapsed_us=timer_monotonic_us()-start; d->bulk_spins=spins;
    d->bulk_failed=true; d->stage="storage-quarantined";
    console_printf("xhci: bulk failed slot=%u dci=%u cc=%u done=%u\n",(uint64_t)d->slot_id,
        (uint64_t)ep->dci,(uint64_t)d->bulk_completion,(uint64_t)d->bulk_done);
    return false;
}
static void storage_bulk_diagnostics(void *context) {
    struct xhci_device *d=context;
    console_printf("xhci-storage: reason=%s slot=%u dci=%u cc=%u done=%u pending=%u\n",
        d->bulk_reason?d->bulk_reason:"none",(uint64_t)d->slot_id,(uint64_t)d->bulk_dci,
        (uint64_t)d->bulk_completion,(uint64_t)d->bulk_done,(uint64_t)d->bulk_pending);
    console_printf("xhci-storage: requested=%u remaining=%u failure-us=%u spins=%u\n",
        (uint64_t)d->bulk_length,(uint64_t)d->bulk_remaining,d->bulk_elapsed_us,d->bulk_spins);
}
static bool enumerate_storage(struct xhci_device *d) {
    d->stage="bulk-endpoints";
    if(!set_configuration(d,d->configuration_value) || !configure_bulk_endpoints(d)) return false;
    d->configured=true; xhci.configured_devices++;
    d->stage="storage-probe";
    d->storage.diagnose=storage_bulk_diagnostics;
    bool ready=usb_storage_attach(&d->storage,d,storage_bulk_transfer,storage_writable);
    d->stage=ready?"storage-ready":(d->bulk_failed?"storage-quarantined":"storage-no-media-or-unsupported");
    console_printf("xhci: storage slot=%u vendor=%x product=%x stage=%s\n",(uint64_t)d->slot_id,
        (uint64_t)d->vendor_id,(uint64_t)d->product_id,d->stage);
    return true;
}

/* Read only the first eight descriptor bytes before choosing EP0's packet
 * size. In particular, full-speed Apple 0262 uses eight, not sixty-four.
 * Update the controller's live context before any multi-packet request. */
static bool update_ep0_packet(struct xhci_device *device) {
    const uint8_t *header = device->descriptor_buffer;
    uint16_t packet = header[7];
    if (header[0] != 18 || header[1] != USB_DESC_DEVICE) return false;
    if (device->speed == 4) {
        if (packet != 9) return false; /* SuperSpeed encodes log2(512). */
        packet = 512;
    } else if (device->speed == 3) {
        if (packet != 64) return false;
    } else if (device->speed == 2) {
        if (packet != 8) return false;
    } else if (device->speed == 1) {
        if (packet != 8 && packet != 16 && packet != 32 && packet != 64) return false;
    } else return false;
    if (packet == device->ep0_max_packet) return true;
    zero_bytes(device->input_context, PMM_FRAME_SIZE);
    context_write32(device->input_context, 0, 1, 1u << 1);
    copy_bytes(device->input_context + 2 * context_size(),
        device->output_context + context_size(), context_size());
    context_write32(device->input_context, 2, 0,
        context_read32(device->input_context, 2, 0) & ~7u);
    context_write32(device->input_context, 2, 1,
        (context_read32(device->input_context, 2, 1) & 0xffffu) | ((uint32_t)packet << 16));
    struct xhci_trb command = {
        .parameter_low = (uint32_t)device->input_context_phys,
        .parameter_high = (uint32_t)(device->input_context_phys >> 32),
        .control = (XHCI_TRB_TYPE_EVALUATE_CONTEXT << 10) | ((uint32_t)device->slot_id << 24),
    };
    if (!submit_command_trb(&command, XHCI_TRB_TYPE_EVALUATE_CONTEXT, 0)) return false;
    device->ep0_max_packet = packet;
    return true;
}

static bool enumerate_device(struct xhci_device *device) {
    if (device == 0) {
        return false;
    }

    if (xhci.addressed_devices >= XHCI_MAX_DEVICES) {
        return false;
    }

    device->stage = "address";
    if (!allocate_device_runtime(device) || !address_device(device)) {
        console_printf("xhci: enumerate port%u route=%x address failed\n",
            (uint64_t)device->port_id,
            (uint64_t)device->route_string);
        return false;
    }

    device->stage = "device-descriptor-prefix";
    zero_bytes(device->descriptor_buffer, 18);
    if (!get_descriptor(device, USB_DESC_DEVICE, 0, 0, 8)) return false;
    device->stage = "ep0-packet";
    if (!update_ep0_packet(device)) return false;
    device->stage = "device-descriptor";
    zero_bytes(device->descriptor_buffer, 18);
    if (!get_descriptor(device, USB_DESC_DEVICE, 0, 0, 18)) {
        console_printf("xhci: slot=%u device descriptor failed\n", (uint64_t)device->slot_id);
        return false;
    }
    /* Refuse a changed/malformed size instead of changing an active endpoint
     * again after a potentially truncated full descriptor. */
    uint16_t encoded = device->speed == 4 ? 9 : device->ep0_max_packet;
    if (device->descriptor_buffer[0] != 18 || device->descriptor_buffer[1] != USB_DESC_DEVICE ||
        device->descriptor_buffer[7] != encoded) return false;
    parse_device_descriptor(device);

    device->stage = "config-descriptor";
    if (!get_descriptor(device, USB_DESC_CONFIGURATION, 0, 0, 9)) {
        console_printf("xhci: slot=%u config header failed\n", (uint64_t)device->slot_id);
        return false;
    }
    uint16_t total_length =
        (uint16_t)device->descriptor_buffer[2] |
        ((uint16_t)device->descriptor_buffer[3] << 8);
    if (total_length < 9 || total_length > PMM_FRAME_SIZE) return false;
    if (!get_descriptor(device, USB_DESC_CONFIGURATION, 0, 0, total_length)) {
        console_printf("xhci: slot=%u config descriptor failed\n", (uint64_t)device->slot_id);
        return false;
    }

    device->stage = "select-interface";
    uint8_t configuration = 0;
    if (!parse_hid_input_config(device, total_length, &configuration)) {
        if (parse_storage_config(device, total_length)) return enumerate_storage(device);
        bool hub_like =
            device->device_class == USB_CLASS_HUB ||
            device->first_interface_class == USB_CLASS_HUB;
        if (!hub_like && network_enabled && enumerate_network(device,total_length)) return true;
        if (hub_like) {
            xhci.hubs_seen++;
            return enumerate_hub(device, configuration);
        } else {
            xhci.unsupported_devices++;
        }
        console_printf("xhci: slot=%u no boot HID class=%x/%x/%x iface=%x/%x/%x vendor=%x product=%x configs=%u interfaces=%u%s\n",
            (uint64_t)device->slot_id,
            (uint64_t)device->device_class,
            (uint64_t)device->device_subclass,
            (uint64_t)device->device_protocol,
            (uint64_t)device->first_interface_class,
            (uint64_t)device->first_interface_subclass,
            (uint64_t)device->first_interface_protocol,
            (uint64_t)device->vendor_id,
            (uint64_t)device->product_id,
            (uint64_t)device->configuration_count,
            (uint64_t)device->interface_count,
            hub_like ? " hub" : "");
        return true;
    }
    if (device->hid_keyboard) parse_trackpad_config(device,total_length);
    device->stage = "set-config";
    if (!set_configuration(device, configuration)) {
        return false;
    }
    device->configured = true;
    xhci.configured_devices++;
    if (device->hid_boot_keyboard || device->hid_boot_mouse) {
        device->stage = "boot-protocol";
        if (!set_hid_boot_protocol(device)) {
            console_printf("xhci: slot=%u boot protocol rejected\n", (uint64_t)device->slot_id);
            return false;
        }
    }

    device->stage = "interrupt-endpoint";
    if (!configure_interrupt_endpoint(device)) {
        console_printf("xhci: slot=%u interrupt endpoint config failed\n", (uint64_t)device->slot_id);
        return false;
    }
    device->interrupt_ready = true;
    if (device->trackpad.candidate) {
        bool ready=enable_trackpad_mode(device) && configure_trackpad_endpoint(device);
        console_printf("xhci: trackpad slot=%u dci=%u ready=%s request=%u pointer=%s\n",
            (uint64_t)device->slot_id,(uint64_t)device->trackpad.dci,ready?"yes":"no",(uint64_t)APPLE_TRACKPAD_REPORT_MAX,trackpad_pointer_enabled?"yes":"no");
    }
    device->stage = "ready";
    if (device->hid_keyboard) {
        xhci.hid_keyboards++;
    } else if (device->hid_mouse) {
        xhci.hid_mice++;
    }
    console_printf("xhci: hid %s slot=%u port=%u parent=%u hub_port=%u route=%x dci=%u packet=%u interval=%u\n",
        device->hid_mouse ? (device->hid_absolute_pointer ? "tablet" : "mouse") : "keyboard",
        (uint64_t)device->slot_id,
        (uint64_t)device->port_id,
        (uint64_t)device->parent_slot_id,
        (uint64_t)device->hub_port_id,
        (uint64_t)device->route_string,
        (uint64_t)device->interrupt_dci,
        (uint64_t)device->interrupt_max_packet,
        (uint64_t)device->interrupt_interval);
    return true;
}

static bool enumerate_port(uint64_t port_index) {
    uint32_t portsc = xhci.ports[port_index].portsc;
    if ((portsc & XHCI_PORTSC_CCS) == 0 || (portsc & XHCI_PORTSC_PED) == 0) {
        return false;
    }

    struct xhci_device *device = allocate_device_record();
    if (device == 0) {
        return false;
    }
    uint8_t speed = (uint8_t)port_speed(portsc);
    *device = (struct xhci_device) {
        .present = true,
        .port_id = (uint8_t)(port_index + 1),
        .speed = speed,
        .ep0_max_packet = default_ep0_packet_for_speed(speed),
        .route_string = 0,
        .slot_type = root_port_slot_type((uint8_t)(port_index + 1)),
    };
    return enumerate_device(device);
}

static void enumerate_connected_ports(void) {
    uint64_t count = xhci.port_count;
    if (count > XHCI_MAX_PORTS) {
        count = XHCI_MAX_PORTS;
    }
    for (uint64_t i = 0; i < count; i++) {
        (void)enumerate_port(i);
    }
    if (xhci.hid_keyboards != 0 || xhci.hid_mice != 0 || network_count) {
        if (!scheduler_spawn_io_service("xhci-hid", xhci_poll_worker, 0, xhci_io_service_ready)) {
            console_write("xhci: hid poll worker failed\n");
        }
    }
}

static bool bringup_command_path(void) {
    xhci.stage = "firmware-handoff";
    if (!claim_firmware_ownership()) {
        return false;
    }
    xhci.stage = "controller-ready";
    if (!wait_status_set(XHCI_USBSTS_CNR, false, XHCI_RESET_TIMEOUT)) {
        return false;
    }
    xhci.stage = "reset";
    if (!reset_controller()) {
        return false;
    }
    xhci.stage = "dma-allocation";
    if (!allocate_runtime_structures()) {
        console_write("xhci: DMA allocation failed\n");
        return false;
    }
    xhci.stage = "start";
    program_rings();
    if (!start_controller()) {
        console_write("xhci: run timeout\n");
        return false;
    }

    xhci.stage = "command-test";
    if (submit_command(XHCI_TRB_TYPE_NOOP, 0, 0)) {
        xhci.noop_completions++;
    } else {
        return false;
    }
    return true;
}

void xhci_init(void) {
    xhci = (struct xhci_state) { .stage = "find-controller", .handoff = "not-attempted" };
    xhci.pci = find_xhci_controller();
    if (xhci.pci == 0) {
        console_write("xhci: no controller\n");
        return;
    }

    uint16_t command = pci_read_config16(xhci.pci, 0x04);
    command |= 0x0006;
    pci_write_config16(xhci.pci, 0x04, command);

    xhci.stage = "map-mmio";
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
    xhci.op_base = xhci.cap_length;
    xhci.db_base = xhci.db_off;
    xhci.rt_base = xhci.rts_off;
    xhci.max_slots = xhci.hcs_params1 & 0xff;
    xhci.port_count = (xhci.hcs_params1 >> 24) & 0xff;
    xhci.page_size = op_read32(XHCI_OP_PAGESIZE);
    parse_supported_protocols();
    xhci.present = true;
    xhci.operational = bringup_command_path();
    if (xhci.operational) {
        xhci.stage = "enumeration";
        reset_connected_ports();
        enumerate_connected_ports();
        xhci.stage = "enumeration-complete";
    }
    read_ports();

    console_printf("xhci: bus=%u dev=%u fn=%u vendor=%x device=%x mmio=%x version=%x ports=%u slots=%u op=%s\n",
        (uint64_t)xhci.pci->bus,
        (uint64_t)xhci.pci->device,
        (uint64_t)xhci.pci->function,
        (uint64_t)xhci.pci->vendor_id,
        (uint64_t)xhci.pci->device_id,
        xhci.mmio_phys,
        (uint64_t)xhci.hci_version,
        (uint64_t)xhci.port_count,
        (uint64_t)xhci.max_slots,
        xhci.operational ? "yes" : "no");
}

bool xhci_is_present(void) {
    return xhci.present;
}

uint64_t xhci_keyboard_count(void) {
    return xhci.hid_keyboards;
}

uint64_t xhci_mouse_count(void) {
    return xhci.hid_mice;
}

uint64_t xhci_device_count(void) {
    return xhci.addressed_devices;
}

uint64_t xhci_hub_count(void) {
    return xhci.hubs_seen;
}

void xhci_probe_input(uint64_t wait_ticks) {
    uint64_t start = timer_ticks();
    uint64_t start_tsc = rdtsc();
    uint64_t wait_seconds = (wait_ticks + XHCI_INPUT_PROBE_TICKS_PER_SECOND - 1) /
        XHCI_INPUT_PROBE_TICKS_PER_SECOND;
    if (wait_seconds == 0) {
        wait_seconds = 1;
    }
    uint64_t tsc_wait = wait_seconds * XHCI_INPUT_PROBE_TSC_PER_SECOND;
    uint64_t spins = 0;
    xhci_input_probe_active = true;
    console_printf("input-probe: hold/type keys now wait_ticks=%u seconds=%u start_ticks=%u max_spins=%u\n",
        wait_ticks,
        wait_seconds,
        start,
        XHCI_INPUT_PROBE_SPIN_LIMIT);
    while (timer_ticks() - start < wait_ticks &&
           rdtsc() - start_tsc < tsc_wait &&
           spins < XHCI_INPUT_PROBE_SPIN_LIMIT) {
        if (xhci.operational && (xhci.hid_keyboards != 0 || xhci.hid_mice != 0 || network_count)) {
            poll_hid_events_once();
        }
        for (uint64_t i = 0; i < 5000; i++) {
            __asm__ volatile ("pause");
        }
        spins++;
    }
    console_printf("input-probe: done ticks=%u spins=%u ps2_irq=%u ps2_scancodes=%u ps2_last=%x keybuf=%u pushed=%u dropped=%u\n",
        timer_ticks() - start,
        spins,
        keyboard_irq_count(),
        keyboard_scancode_count(),
        keyboard_last_scancode(),
        keyboard_buffered_count(),
        keyboard_pushed_count(),
        keyboard_dropped_count());
    xhci_input_probe_active = false;
}

static void print_trackpad_capture(const struct xhci_device *device) {
    const struct xhci_trackpad_endpoint *t=&device->trackpad;
    if (!device->present || !t->candidate) return;
    console_printf("trackpad-capture: slot=%u ready=%u pending=%u dci=%u packet=%u request=%u last=%u cc=%u\n",
        (uint64_t)device->slot_id,(uint64_t)t->ready,(uint64_t)t->pending,(uint64_t)t->dci,
        (uint64_t)t->packet,(uint64_t)APPLE_TRACKPAD_REPORT_MAX,(uint64_t)t->last_length,(uint64_t)t->last_completion);
    console_printf("trackpad-capture: reports=%u contact_reports=%u button_reports=%u malformed=%u errors=%u\n",
        t->reports,t->contact_reports,t->button_reports,t->malformed,t->errors);
    console_printf("trackpad-gesture: policy=taps-v1 frames=%u rebases=%u handoffs=%u scroll=%u typing=%u palm=%u cancels=%u tap_primary=%u tap_secondary=%u\n",
        t->drag_frames,t->drag_rebases,t->drag_handoffs,t->scroll_events,t->typing_suppressed,t->palm_suppressed,t->pointer_cancels,t->tap_primary,t->tap_secondary);
    console_printf("trackpad-capture: current contacts=%u button=%u x=%d y=%d origin=%u\n",
        (uint64_t)t->frame.count,(uint64_t)t->frame.button,(int64_t)t->frame.contacts[0].x,
        (int64_t)t->frame.contacts[0].y,(uint64_t)t->frame.contacts[0].origin);
}

void xhci_print_input_summary(void) {
    if (!xhci.present) {
        console_write("input-usb: xhci=none\n");
        return;
    }
    console_printf("input-usb: op=%s devices=%u keyboards=%u mice=%u hubs=%u transfers=%u last_cc=%u keybuf=%u pushed=%u dropped=%u\n",
        xhci.operational ? "yes" : "no",
        xhci.addressed_devices,
        xhci.hid_keyboards,
        xhci.hid_mice,
        xhci.hubs_seen,
        xhci.transfer_events,
        xhci.last_completion_code,
        keyboard_buffered_count(),
        keyboard_pushed_count(),
        keyboard_dropped_count());
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        const struct xhci_device *device = &xhci.devices[i];
        print_trackpad_capture(device);
        if (!device->present || (!device->hid_keyboard && !device->hid_mouse)) {
            continue;
        }
        console_printf("input-usb: dev slot=%u keyboard=%u mouse=%u iface=%u dci=%u pkt=%u pending=%u ok=%u err=%u last_cc=%u rem=%u reports=%u prev=%x %x %x %x %x %x %x %x\n",
            (uint64_t)device->slot_id,
            (uint64_t)device->hid_keyboard,
            (uint64_t)device->hid_mouse,
            (uint64_t)device->interface_number,
            (uint64_t)device->interrupt_dci,
            (uint64_t)device->interrupt_max_packet,
            (uint64_t)device->interrupt_pending,
            device->interrupt_success,
            device->interrupt_errors,
            device->last_interrupt_completion,
            device->last_interrupt_remaining,
            device->hid_reports,
            (uint64_t)device->previous_report[0],
            (uint64_t)device->previous_report[1],
            (uint64_t)device->previous_report[2],
            (uint64_t)device->previous_report[3],
            (uint64_t)device->previous_report[4],
            (uint64_t)device->previous_report[5],
            (uint64_t)device->previous_report[6],
            (uint64_t)device->previous_report[7]);
    }
}

void xhci_print_status(void) {
    if (!xhci.present) {
        console_write("xhci: no controller detected\n");
        return;
    }
    read_ports();

    console_printf("xhci: vendor=%x device=%x bus=%u dev=%u fn=%u irq=%u pin=%u op=%s\n",
        (uint64_t)xhci.pci->vendor_id,
        (uint64_t)xhci.pci->device_id,
        (uint64_t)xhci.pci->bus,
        (uint64_t)xhci.pci->device,
        (uint64_t)xhci.pci->function,
        (uint64_t)xhci.pci->interrupt_line,
        (uint64_t)xhci.pci->interrupt_pin,
        xhci.operational ? "yes" : "no");
    console_printf("xhci: mmio_phys=%x mmio_virt=%x caplen=%u version=%x pagesize=%x dboff=%x rtsoff=%x\n",
        xhci.mmio_phys,
        xhci.mmio_virt,
        (uint64_t)xhci.cap_length,
        (uint64_t)xhci.hci_version,
        (uint64_t)xhci.page_size,
        (uint64_t)xhci.db_off,
        (uint64_t)xhci.rts_off);
    console_printf("xhci: hcs1=%x hcs2=%x hcs3=%x hcc1=%x slots=%u ports=%u scratchpads=%u protocols=%u\n",
        (uint64_t)xhci.hcs_params1,
        (uint64_t)xhci.hcs_params2,
        (uint64_t)xhci.hcs_params3,
        (uint64_t)xhci.hcc_params1,
        (uint64_t)(xhci.hcs_params1 & 0xff),
        (uint64_t)((xhci.hcs_params1 >> 24) & 0xff),
        (uint64_t)required_scratchpads(xhci.hcs_params2),
        xhci.protocol_count);
    console_printf("xhci: rings dcbaa=%x cr=%x er=%x erst=%x cmd_events=%u noop=%u enable_slot=%u last_cc=%u last_slot=%u cmd_ptr=%x expected=%x mismatches=%u\n",
        xhci.dcbaa_phys,
        xhci.command_ring_phys,
        xhci.event_ring_phys,
        xhci.erst_phys,
        xhci.command_events,
        xhci.noop_completions,
        xhci.enable_slot_completions,
        xhci.last_completion_code,
        xhci.last_slot_id,
        xhci.last_command_pointer,
        xhci.last_expected_command_pointer,
        xhci.command_pointer_mismatches);
    console_printf("xhci: ports_reset=%u ports_enabled=%u\n",
        xhci.ports_reset,
        xhci.ports_enabled);
    console_printf("xhci: usb addressed=%u configured=%u hid_keyboards=%u hid_mice=%u hubs=%u hub_power=%u hub_reset=%u routed=%u unsupported=%u full=%u transfer_events=%u\n",
        xhci.addressed_devices,
        xhci.configured_devices,
        xhci.hid_keyboards,
        xhci.hid_mice,
        xhci.hubs_seen,
        xhci.hub_ports_powered,
        xhci.hub_ports_reset,
        xhci.routed_devices,
        xhci.unsupported_devices,
        xhci.device_record_full,
        xhci.transfer_events);
    uint64_t count = xhci.port_count;
    if (count > XHCI_MAX_PORTS) {
        count = XHCI_MAX_PORTS;
    }
    for (uint64_t i = 0; i < count; i++) {
        uint32_t portsc = xhci.ports[i].portsc;
        console_printf("xhci: port%u sc=%x connected=%u enabled=%u reset=%u power=%u speed=%u pls=%u slot_type=%u proto=%u changes=%x\n",
            i + 1,
            (uint64_t)portsc,
            (uint64_t)(portsc & 1u),
            (uint64_t)((portsc >> 1) & 1u),
            (uint64_t)((portsc >> 4) & 1u),
            (uint64_t)((portsc >> 9) & 1u),
            (uint64_t)((portsc >> 10) & 0xfu),
            (uint64_t)((portsc >> 5) & 0xfu),
            (uint64_t)xhci.ports[i].slot_type,
            (uint64_t)xhci.ports[i].protocol_index,
            (uint64_t)((portsc >> 17) & 0x7fu));
    }
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        const struct xhci_device *device = &xhci.devices[i];
        if (!device->present) {
            continue;
        }
        if(device->network) {
            const struct xhci_network *n=device->network;
            console_printf("xhci-network: slot=%u ready=%u link=%s tx=%u rx=%u drops=%u errors=%u notifications=%u queued=%u\n",
                (uint64_t)device->slot_id,(uint64_t)n->ready,n->link.known?(n->link.up?"up":"down"):"unknown",
                n->tx_packets,n->rx_packets,n->drops,n->errors,n->notifications,(uint64_t)n->count);
        }
        if (device->bulk_ep[0].dci) {
            const struct usb_storage *s=&device->storage;
            console_printf("xhci: storage slot=%u stage=%s disk=%s out=%u in=%u packet=%u/%u burst=%u/%u commands=%u errors=%u offline=%u\n",
                (uint64_t)device->slot_id,device->stage,s->block?s->name:"none",
                (uint64_t)device->bulk_ep[0].dci,(uint64_t)device->bulk_ep[1].dci,
                (uint64_t)device->bulk_ep[0].packet,(uint64_t)device->bulk_ep[1].packet,
                (uint64_t)device->bulk_ep[0].burst,(uint64_t)device->bulk_ep[1].burst,
                s->commands,s->errors,(uint64_t)(s->failed || device->bulk_failed));
        }
        console_printf("xhci: device slot=%u port=%u parent=%u hub_port=%u route=%x slot_type=%u addressed=%u configured=%u class=%x/%x/%x iface=%x/%x/%x vendor=%x product=%x keyboard=%u mouse=%u absolute=%u dci=%u control=%u intr=%u reports=%u mouse_reports=%u\n",
            (uint64_t)device->slot_id,
            (uint64_t)device->port_id,
            (uint64_t)device->parent_slot_id,
            (uint64_t)device->hub_port_id,
            (uint64_t)device->route_string,
            (uint64_t)device->slot_type,
            (uint64_t)device->addressed,
            (uint64_t)device->configured,
            (uint64_t)device->device_class,
            (uint64_t)device->device_subclass,
            (uint64_t)device->device_protocol,
            (uint64_t)device->first_interface_class,
            (uint64_t)device->first_interface_subclass,
            (uint64_t)device->first_interface_protocol,
            (uint64_t)device->vendor_id,
            (uint64_t)device->product_id,
            (uint64_t)device->hid_keyboard,
            (uint64_t)device->hid_mouse,
            (uint64_t)device->hid_absolute_pointer,
            (uint64_t)device->interrupt_dci,
            device->control_success,
            device->interrupt_success,
            device->hid_reports,
            device->mouse_reports);
        print_trackpad_capture(device);
    }
}

void xhci_print_boot_diagnostic(void) {
    console_write("USB INPUT DIAGNOSTIC - photograph this screen if keys do not work\n");
    console_printf("stage=%s handoff=%s operational=%s\n",
        xhci.stage != 0 ? xhci.stage : "none",
        xhci.handoff != 0 ? xhci.handoff : "none", xhci.operational ? "yes" : "no");
    if (!xhci.present) {
        return;
    }
    console_printf("scratch=%u/%u noop=%u addressed=%u keyboards=%u last_cc=%u\n",
        (uint64_t)xhci.scratchpads_allocated, (uint64_t)required_scratchpads(xhci.hcs_params2),
        xhci.noop_completions, xhci.addressed_devices, xhci.hid_keyboards, xhci.last_completion_code);
    uint64_t count = xhci.port_count < XHCI_MAX_PORTS ? xhci.port_count : XHCI_MAX_PORTS;
    for (uint64_t i = 0; i < count; i++) {
        uint32_t state = op_read32(port_offset(i));
        if ((state & XHCI_PORTSC_CCS) != 0 || i == 4) {
            const struct xhci_port_state *p = &xhci.ports[i];
            console_printf("port %u: c=%u e=%u speed=%u reset=%s attempts=%u\n",
                i + 1, (uint64_t)(state & XHCI_PORTSC_CCS),
                (uint64_t)((state & XHCI_PORTSC_PED) != 0), (uint64_t)port_speed(state),
                p->reset_result != 0 ? p->reset_result : "not-run", (uint64_t)p->reset_attempts);
            console_printf("  initial=%x reset-end=%x now=%x\n",
                (uint64_t)p->initial_portsc, (uint64_t)p->reset_portsc, (uint64_t)state);
        }
    }
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        const struct xhci_device *d = &xhci.devices[i];
        if (!d->present) continue;
        console_printf("device port=%u slot=%u id=%x:%x\n", (uint64_t)d->port_id,
            (uint64_t)d->slot_id, (uint64_t)d->vendor_id, (uint64_t)d->product_id);
        console_printf("  stage=%s iface=%u dci=%u packet=%u\n",
            d->stage != 0 ? d->stage : "none", (uint64_t)d->interface_number,
            (uint64_t)d->interrupt_dci, (uint64_t)d->interrupt_max_packet);
        console_printf("  polls=%u reports=%u errors=%u\n",
            d->interrupt_success, d->hid_reports, d->interrupt_errors);
    }
    console_write("Match vendor/product, port, interface and endpoint against this machine's captured descriptors.\n");
}


int64_t xhci_device_snapshot(uint64_t index,struct srv_device_info *out) {
    if(!out) return -1;
    uint64_t flags=event_irq_save();
    for(uint64_t i=index;i<XHCI_MAX_DEVICES;i++) {
        const struct xhci_device *d=&xhci.devices[i];
        if(!d->present) continue;
        unsigned bound=SRV_DEVICE_PRESENT;
        if(d->configured) bound|=SRV_DEVICE_CONFIGURED;
        if(d->interrupt_ready && d->hid_keyboard) bound|=SRV_DEVICE_KEYBOARD;
        if(d->interrupt_ready && d->hid_mouse) bound|=SRV_DEVICE_POINTER;
        if(d->trackpad.ready) bound|=SRV_DEVICE_TRACKPAD;
        if(d->storage.block) bound|=SRV_DEVICE_STORAGE;
        if(d->hub_ports) bound|=SRV_DEVICE_HUB;
        if(d->network && d->network->ready) bound|=SRV_DEVICE_NETWORK;
        if(d->storage.failed || d->bulk_failed || (d->network && d->network->failed)) bound|=SRV_DEVICE_FAILED;
        *out=(struct srv_device_info){.version=SRV_DEVICE_VERSION,.size=sizeof(*out),.bus_kind=SRV_DEVICE_USB,
            .index=i,.vendor_id=d->vendor_id,.product_id=d->product_id,.revision=d->device_bcd,
            .class_code=d->device_class?d->device_class:d->first_interface_class,
            .subclass=d->device_class?d->device_subclass:d->first_interface_subclass,
            .protocol=d->device_class?d->device_protocol:d->first_interface_protocol,
            .device=d->slot_id,.port=d->port_id,.parent=d->parent_slot_id,.function=d->hub_port_id,.flags=bound,
            .reports=d->hid_reports+d->mouse_reports+d->trackpad.reports+(d->network?d->network->rx_packets:0),
            .errors=d->interrupt_errors+d->trackpad.errors+d->storage.errors+(d->network?d->network->errors:0)};
        event_irq_restore(flags);return (int64_t)i+1;
    }
    event_irq_restore(flags);return 0;
}
