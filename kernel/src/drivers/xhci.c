#include <srvros/console.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>
#include <srvros/xhci.h>

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
#define XHCI_RESET_TIMEOUT 10000000ull
#define XHCI_COMMAND_TIMEOUT 10000000ull
#define XHCI_TRANSFER_TIMEOUT 20000000ull

#define XHCI_USBCMD_RUN 0x00000001u
#define XHCI_USBCMD_HCRST 0x00000002u
#define XHCI_USBCMD_INTE 0x00000004u
#define XHCI_USBSTS_HCH 0x00000001u
#define XHCI_USBSTS_EINT 0x00000008u
#define XHCI_USBSTS_CNR 0x00000800u

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
#define XHCI_TRB_IDT (1u << 6)
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
};

struct usb_setup_packet {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed));

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
    uint8_t slot_id;
    uint8_t port_id;
    uint8_t parent_slot_id;
    uint8_t hub_port_id;
    uint8_t hub_ports;
    uint8_t speed;
    uint8_t ep0_max_packet;
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
    uint8_t previous_report[8];
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
    struct xhci_port_state ports[XHCI_MAX_PORTS];
    struct xhci_device devices[XHCI_MAX_DEVICES];
};

static struct xhci_state xhci;
static volatile bool xhci_input_probe_active;

static uint32_t mmio_read32(uint64_t offset) {
    return *(volatile uint32_t *)(xhci.mmio_virt + offset);
}

static void mmio_write32(uint64_t offset, uint32_t value) {
    *(volatile uint32_t *)(xhci.mmio_virt + offset) = value;
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
    command &= ~XHCI_USBCMD_RUN;
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

static bool reset_port(uint64_t index) {
    uint64_t offset = port_offset(index);
    uint32_t portsc = op_read32(offset);
    if ((portsc & XHCI_PORTSC_CCS) == 0) {
        return false;
    }

    uint32_t write_value = (portsc & XHCI_PORTSC_PP) | XHCI_PORTSC_PR;
    op_write32(offset, write_value);
    for (uint64_t i = 0; i < XHCI_RESET_TIMEOUT; i++) {
        portsc = op_read32(offset);
        if ((portsc & XHCI_PORTSC_PR) == 0) {
            break;
        }
    }

    portsc = op_read32(offset);
    if ((portsc & XHCI_PORTSC_CHANGE_MASK) != 0) {
        op_write32(offset, (portsc & XHCI_PORTSC_PP) | (portsc & XHCI_PORTSC_CHANGE_MASK));
        portsc = op_read32(offset);
    }
    xhci.ports[index].portsc = portsc;
    xhci.ports_reset++;
    if ((portsc & XHCI_PORTSC_PED) != 0) {
        xhci.ports_enabled++;
        return true;
    }
    return false;
}

static void reset_connected_ports(void) {
    uint64_t count = xhci.port_count;
    if (count > XHCI_MAX_PORTS) {
        count = XHCI_MAX_PORTS;
    }
    for (uint64_t i = 0; i < count; i++) {
        uint32_t portsc = op_read32(port_offset(i));
        if ((portsc & XHCI_PORTSC_CCS) != 0 && (portsc & XHCI_PORTSC_PED) == 0) {
            reset_port(i);
        } else {
            xhci.ports[i].portsc = portsc;
            if ((portsc & XHCI_PORTSC_PED) != 0) {
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

static bool allocate_runtime_structures(void) {
    void *virt = 0;
    if (!alloc_frame_zero(&xhci.dcbaa_phys, &virt)) {
        return false;
    }
    xhci.dcbaa = virt;

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
    rt_write32(interrupter + XHCI_INTR_IMAN, 0x3);
    rt_write32(interrupter + XHCI_INTR_IMOD, 0);
    rt_write32(interrupter + XHCI_INTR_ERSTSZ, 1);
    rt_write64(interrupter + XHCI_INTR_ERSTBA, xhci.erst_phys);
    rt_write64(interrupter + XHCI_INTR_ERDP, xhci.event_ring_phys | (1u << 3));
    op_write32(XHCI_OP_DNCTRL, 0);
}

static bool start_controller(void) {
    op_write32(XHCI_OP_USBSTS, XHCI_USBSTS_EINT);
    op_write32(XHCI_OP_USBCMD, op_read32(XHCI_OP_USBCMD) | XHCI_USBCMD_RUN | XHCI_USBCMD_INTE);
    return wait_status_set(XHCI_USBSTS_HCH, false, XHCI_RESET_TIMEOUT);
}

static bool poll_event(struct xhci_trb *event_out) {
    struct xhci_trb *event = &xhci.event_ring[xhci.event_dequeue];
    bool cycle = (event->control & 1u) != 0;
    if (cycle != xhci.event_cycle) {
        return false;
    }

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
            continue;
        }

        uint64_t pointer = (uint64_t)event.parameter_low | ((uint64_t)event.parameter_high << 32);
        uint64_t completion_code = (event.status >> 24) & 0xff;
        uint64_t slot_id = (event.control >> 24) & 0xff;
        xhci.command_events++;
        xhci.last_completion_code = completion_code;
        xhci.last_slot_id = slot_id;
        if (pointer != command_phys) {
            continue;
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

    console_printf("xhci: command type=%u timeout events=%u last_cc=%u\n",
        expected_type,
        xhci.command_events,
        xhci.last_completion_code);
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
            (length == 0 ? XHCI_SETUP_TRT_NONE : XHCI_SETUP_TRT_IN),
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
    if (!submit_command(XHCI_TRB_TYPE_ENABLE_SLOT, 0, &slot) || slot == 0 || slot > xhci.max_slots) {
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
    device->interrupt_max_packet = best_packet;
    device->interrupt_interval = best_interval;
    device->hid_keyboard = best_keyboard;
    device->hid_mouse = best_mouse || best_absolute_pointer;
    device->hid_boot_keyboard = best_boot_keyboard;
    device->hid_boot_mouse = best_boot_mouse;
    device->hid_absolute_pointer = best_absolute_pointer;
    return true;
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

    zero_bytes(device->input_context, PMM_FRAME_SIZE);
    uint32_t add_flags = (1u << 0) | (1u << device->interrupt_dci);
    context_write32(device->input_context, 0, 1, add_flags);
    uint32_t slot0 = context_read32(device->output_context, 0, 0);
    slot0 &= ~(0x1fu << 27);
    slot0 |= ((uint32_t)device->interrupt_dci << 27);
    context_write32(device->input_context, 1, 0, slot0);
    context_write32(device->input_context, 1, 1, context_read32(device->output_context, 0, 1));

    uint64_t endpoint_context = (uint64_t)device->interrupt_dci + 1;
    uint32_t ep_info = ((uint32_t)device->interrupt_interval << 16);
    uint32_t ep_type_packet = (3u << 1) | (7u << 3) | ((uint32_t)device->interrupt_max_packet << 16);
    context_write32(device->input_context, endpoint_context, 0, ep_info);
    context_write32(device->input_context, endpoint_context, 1, ep_type_packet);
    context_write32(device->input_context, endpoint_context, 2, (uint32_t)(device->interrupt_ring_phys | 1u));
    context_write32(device->input_context, endpoint_context, 3, (uint32_t)(device->interrupt_ring_phys >> 32));
    context_write32(device->input_context, endpoint_context, 4, device->interrupt_max_packet);

    struct xhci_trb command = {
        .parameter_low = (uint32_t)device->input_context_phys,
        .parameter_high = (uint32_t)(device->input_context_phys >> 32),
        .status = 0,
        .control = (XHCI_TRB_TYPE_CONFIGURE_ENDPOINT << 10) | ((uint32_t)device->slot_id << 24),
    };
    return submit_command_trb(&command, XHCI_TRB_TYPE_CONFIGURE_ENDPOINT, 0);
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

static const char hid_usage[256] = {
    [0x04] = 'a', [0x05] = 'b', [0x06] = 'c', [0x07] = 'd',
    [0x08] = 'e', [0x09] = 'f', [0x0a] = 'g', [0x0b] = 'h',
    [0x0c] = 'i', [0x0d] = 'j', [0x0e] = 'k', [0x0f] = 'l',
    [0x10] = 'm', [0x11] = 'n', [0x12] = 'o', [0x13] = 'p',
    [0x14] = 'q', [0x15] = 'r', [0x16] = 's', [0x17] = 't',
    [0x18] = 'u', [0x19] = 'v', [0x1a] = 'w', [0x1b] = 'x',
    [0x1c] = 'y', [0x1d] = 'z',
    [0x1e] = '1', [0x1f] = '2', [0x20] = '3', [0x21] = '4',
    [0x22] = '5', [0x23] = '6', [0x24] = '7', [0x25] = '8',
    [0x26] = '9', [0x27] = '0', [0x28] = '\n', [0x29] = 27,
    [0x2a] = '\b', [0x2b] = '\t', [0x2c] = ' ', [0x2d] = '-',
    [0x2e] = '=', [0x2f] = '[', [0x30] = ']', [0x31] = '\\',
    [0x33] = ';', [0x34] = '\'', [0x35] = '`', [0x36] = ',',
    [0x37] = '.', [0x38] = '/',
};

static const char hid_usage_shift[256] = {
    [0x04] = 'A', [0x05] = 'B', [0x06] = 'C', [0x07] = 'D',
    [0x08] = 'E', [0x09] = 'F', [0x0a] = 'G', [0x0b] = 'H',
    [0x0c] = 'I', [0x0d] = 'J', [0x0e] = 'K', [0x0f] = 'L',
    [0x10] = 'M', [0x11] = 'N', [0x12] = 'O', [0x13] = 'P',
    [0x14] = 'Q', [0x15] = 'R', [0x16] = 'S', [0x17] = 'T',
    [0x18] = 'U', [0x19] = 'V', [0x1a] = 'W', [0x1b] = 'X',
    [0x1c] = 'Y', [0x1d] = 'Z',
    [0x1e] = '!', [0x1f] = '@', [0x20] = '#', [0x21] = '$',
    [0x22] = '%', [0x23] = '^', [0x24] = '&', [0x25] = '*',
    [0x26] = '(', [0x27] = ')', [0x28] = '\n', [0x29] = 27,
    [0x2a] = '\b', [0x2b] = '\t', [0x2c] = ' ', [0x2d] = '_',
    [0x2e] = '+', [0x2f] = '{', [0x30] = '}', [0x31] = '|',
    [0x33] = ':', [0x34] = '"', [0x35] = '~', [0x36] = '<',
    [0x37] = '>', [0x38] = '?',
};

static bool usage_was_down(const uint8_t *report, uint8_t usage) {
    for (uint64_t i = 2; i < 8; i++) {
        if (report[i] == usage) {
            return true;
        }
    }
    return false;
}

static void handle_hid_report(struct xhci_device *device) {
    uint8_t *report = device->interrupt_buffer;
    if (bytes_equal(report, device->previous_report, 8)) {
        return;
    }

    bool shift = (report[0] & 0x22) != 0;
    for (uint64_t i = 2; i < 8; i++) {
        uint8_t usage = report[i];
        if (usage == 0 || usage_was_down(device->previous_report, usage)) {
            continue;
        }
        char c = shift ? hid_usage_shift[usage] : hid_usage[usage];
        if (c != 0) {
            keyboard_inject_char(c);
        }
    }
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
    if (device->interrupt_pending || (!device->hid_keyboard && !device->hid_mouse)) {
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
        .control = (XHCI_TRB_TYPE_NORMAL << 10) | XHCI_TRB_IOC,
    };
    (void)ring_push(device->interrupt_ring,
        device->interrupt_ring_phys,
        &device->interrupt_enqueue,
        &device->interrupt_cycle,
        &trb);
    db_write32(device->slot_id, device->interrupt_dci);
    device->interrupt_pending = true;
}

static void poll_hid_events_once(void) {
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        submit_interrupt_poll(&xhci.devices[i]);
    }

    for (;;) {
        struct xhci_trb event;
        if (!poll_event(&event)) {
            return;
        }
        uint64_t type = (event.control >> 10) & 0x3f;
        if (type != XHCI_TRB_TYPE_TRANSFER_EVENT) {
            continue;
        }
        uint64_t slot_id = (event.control >> 24) & 0xff;
        uint64_t dci = (event.control >> 16) & 0x1f;
        uint64_t completion = (event.status >> 24) & 0xff;
        uint64_t remaining = event.status & 0xffffffu;
        xhci.transfer_events++;
        xhci.last_completion_code = completion;
        xhci.last_slot_id = slot_id;
        for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
            struct xhci_device *device = &xhci.devices[i];
            if ((!device->hid_keyboard && !device->hid_mouse) ||
                device->slot_id != slot_id ||
                device->interrupt_dci != dci) {
                continue;
            }
            device->interrupt_pending = false;
            device->last_interrupt_completion = completion;
            device->last_interrupt_remaining = remaining;
            if (completion == 1 || completion == 13) {
                device->interrupt_success++;
                if (device->hid_keyboard) {
                    handle_hid_report(device);
                } else if (device->hid_mouse) {
                    handle_hid_mouse_report(device);
                }
            } else {
                device->interrupt_errors++;
            }
        }
    }
}

static void xhci_poll_worker(void *arg) {
    (void)arg;
    for (;;) {
        if (xhci.operational && !xhci_input_probe_active && (xhci.hid_keyboards != 0 || xhci.hid_mice != 0)) {
            poll_hid_events_once();
        }
        for (uint64_t i = 0; i < 10000; i++) {
            __asm__ volatile ("pause");
        }
        scheduler_yield();
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
            .ep0_max_packet = 64,
            .route_string = child_route_string(hub->route_string, port),
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

static bool enumerate_device(struct xhci_device *device) {
    if (device == 0) {
        return false;
    }

    if (xhci.addressed_devices >= XHCI_MAX_DEVICES) {
        return false;
    }

    if (!allocate_device_runtime(device) || !address_device(device)) {
        console_printf("xhci: enumerate port%u route=%x address failed\n",
            (uint64_t)device->port_id,
            (uint64_t)device->route_string);
        return false;
    }

    if (!get_descriptor(device, USB_DESC_DEVICE, 0, 0, 18)) {
        console_printf("xhci: slot=%u device descriptor failed\n", (uint64_t)device->slot_id);
        return false;
    }
    parse_device_descriptor(device);
    if (device->descriptor_buffer[7] != 0) {
        device->ep0_max_packet = device->descriptor_buffer[7];
    }

    if (!get_descriptor(device, USB_DESC_CONFIGURATION, 0, 0, 9)) {
        console_printf("xhci: slot=%u config header failed\n", (uint64_t)device->slot_id);
        return false;
    }
    uint16_t total_length =
        (uint16_t)device->descriptor_buffer[2] |
        ((uint16_t)device->descriptor_buffer[3] << 8);
    if (total_length > PMM_FRAME_SIZE) {
        total_length = PMM_FRAME_SIZE;
    }
    if (!get_descriptor(device, USB_DESC_CONFIGURATION, 0, 0, total_length)) {
        console_printf("xhci: slot=%u config descriptor failed\n", (uint64_t)device->slot_id);
        return false;
    }

    uint8_t configuration = 0;
    if (!parse_hid_input_config(device, total_length, &configuration)) {
        bool hub_like =
            device->device_class == USB_CLASS_HUB ||
            device->first_interface_class == USB_CLASS_HUB;
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
    if (!set_configuration(device, configuration)) {
        return false;
    }
    device->configured = true;
    xhci.configured_devices++;
    if (device->hid_boot_keyboard || device->hid_boot_mouse) {
        (void)set_hid_boot_protocol(device);
    }

    if (!configure_interrupt_endpoint(device)) {
        console_printf("xhci: slot=%u interrupt endpoint config failed\n", (uint64_t)device->slot_id);
        return false;
    }
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
    *device = (struct xhci_device) {
        .present = true,
        .port_id = (uint8_t)(port_index + 1),
        .speed = (uint8_t)port_speed(portsc),
        .ep0_max_packet = 64,
        .route_string = 0,
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
    if (xhci.hid_keyboards != 0 || xhci.hid_mice != 0) {
        if (!scheduler_spawn("xhci-hid", xhci_poll_worker, 0)) {
            console_write("xhci: hid poll worker failed\n");
        }
    }
}

static bool bringup_command_path(void) {
    if (!reset_controller()) {
        return false;
    }
    if (!allocate_runtime_structures()) {
        console_write("xhci: DMA allocation failed\n");
        return false;
    }
    program_rings();
    if (!start_controller()) {
        console_write("xhci: run timeout\n");
        return false;
    }

    if (submit_command(XHCI_TRB_TYPE_NOOP, 0, 0)) {
        xhci.noop_completions++;
    } else {
        return false;
    }
    return true;
}

void xhci_init(void) {
    xhci = (struct xhci_state) { 0 };
    xhci.pci = find_xhci_controller();
    if (xhci.pci == 0) {
        console_write("xhci: no controller\n");
        return;
    }

    uint16_t command = pci_read_config16(xhci.pci, 0x04);
    command |= 0x0006;
    pci_write_config16(xhci.pci, 0x04, command);

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
    xhci.present = true;
    xhci.operational = bringup_command_path();
    if (xhci.operational) {
        reset_connected_ports();
        enumerate_connected_ports();
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
    if (!xhci.operational || (xhci.hid_keyboards == 0 && xhci.hid_mice == 0)) {
        return;
    }

    uint64_t start = timer_ticks();
    uint64_t spins = 0;
    xhci_input_probe_active = true;
    console_printf("input-probe: hold/type keys now wait_ticks=%u start_ticks=%u\n",
        wait_ticks,
        start);
    while (timer_ticks() - start < wait_ticks && spins < wait_ticks * 2000000ull) {
        poll_hid_events_once();
        for (uint64_t i = 0; i < 5000; i++) {
            __asm__ volatile ("pause");
        }
        spins++;
    }
    console_printf("input-probe: done ticks=%u spins=%u keybuf=%u pushed=%u dropped=%u\n",
        timer_ticks() - start,
        spins,
        keyboard_buffered_count(),
        keyboard_pushed_count(),
        keyboard_dropped_count());
    xhci_input_probe_active = false;
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
    console_printf("xhci: hcs1=%x hcs2=%x hcs3=%x hcc1=%x slots=%u ports=%u scratchpads=%u\n",
        (uint64_t)xhci.hcs_params1,
        (uint64_t)xhci.hcs_params2,
        (uint64_t)xhci.hcs_params3,
        (uint64_t)xhci.hcc_params1,
        (uint64_t)(xhci.hcs_params1 & 0xff),
        (uint64_t)((xhci.hcs_params1 >> 24) & 0xff),
        (uint64_t)((xhci.hcs_params2 >> 21) & 0x1f));
    console_printf("xhci: rings dcbaa=%x cr=%x er=%x erst=%x cmd_events=%u noop=%u enable_slot=%u last_cc=%u last_slot=%u\n",
        xhci.dcbaa_phys,
        xhci.command_ring_phys,
        xhci.event_ring_phys,
        xhci.erst_phys,
        xhci.command_events,
        xhci.noop_completions,
        xhci.enable_slot_completions,
        xhci.last_completion_code,
        xhci.last_slot_id);
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
        console_printf("xhci: port%u sc=%x connected=%u enabled=%u reset=%u power=%u speed=%u pls=%u changes=%x\n",
            i + 1,
            (uint64_t)portsc,
            (uint64_t)(portsc & 1u),
            (uint64_t)((portsc >> 1) & 1u),
            (uint64_t)((portsc >> 4) & 1u),
            (uint64_t)((portsc >> 9) & 1u),
            (uint64_t)((portsc >> 10) & 0xfu),
            (uint64_t)((portsc >> 5) & 0xfu),
            (uint64_t)((portsc >> 17) & 0x7fu));
    }
    for (uint64_t i = 0; i < XHCI_MAX_DEVICES; i++) {
        const struct xhci_device *device = &xhci.devices[i];
        if (!device->present) {
            continue;
        }
        console_printf("xhci: device slot=%u port=%u parent=%u hub_port=%u route=%x addressed=%u configured=%u class=%x/%x/%x iface=%x/%x/%x vendor=%x product=%x keyboard=%u mouse=%u absolute=%u dci=%u control=%u intr=%u reports=%u mouse_reports=%u\n",
            (uint64_t)device->slot_id,
            (uint64_t)device->port_id,
            (uint64_t)device->parent_slot_id,
            (uint64_t)device->hub_port_id,
            (uint64_t)device->route_string,
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
    }
}
