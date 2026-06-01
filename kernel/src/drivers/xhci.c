#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
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
#define XHCI_RESET_TIMEOUT 10000000ull
#define XHCI_COMMAND_TIMEOUT 10000000ull

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
#define XHCI_TRB_TYPE_NOOP 23
#define XHCI_TRB_TYPE_COMMAND_COMPLETION 33

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
    uint64_t last_completion_code;
    uint64_t last_slot_id;
    struct xhci_port_state ports[XHCI_MAX_PORTS];
};

static struct xhci_state xhci;

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

    if (!alloc_frame_zero(&xhci.command_ring_phys, &virt)) {
        return false;
    }
    xhci.command_ring = virt;
    xhci.command_enqueue = 0;
    xhci.command_cycle = true;
    xhci.command_ring[XHCI_TRB_COUNT - 1] = (struct xhci_trb) {
        .parameter_low = (uint32_t)xhci.command_ring_phys,
        .parameter_high = (uint32_t)(xhci.command_ring_phys >> 32),
        .control = (XHCI_TRB_TYPE_LINK << 10) | (1u << 1),
    };

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

static bool submit_command(uint32_t type, uint32_t flags, uint64_t *slot_out) {
    uint64_t index = xhci.command_enqueue;
    if (index >= XHCI_TRB_COUNT - 1) {
        index = 0;
        xhci.command_enqueue = 0;
    }

    uint64_t command_phys = xhci.command_ring_phys + index * sizeof(struct xhci_trb);
    struct xhci_trb trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = (type << 10) | flags | (xhci.command_cycle ? 1u : 0u),
    };
    copy_trb(&xhci.command_ring[index], &trb);
    xhci.command_enqueue++;
    if (xhci.command_enqueue == XHCI_TRB_COUNT - 1) {
        xhci.command_ring[XHCI_TRB_COUNT - 1].control =
            (XHCI_TRB_TYPE_LINK << 10) | (1u << 1) | (xhci.command_cycle ? 1u : 0u);
        xhci.command_enqueue = 0;
        xhci.command_cycle = !xhci.command_cycle;
    }

    db_write32(0, 0);
    return wait_command_completion(command_phys, type, slot_out);
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
    uint64_t slot = 0;
    if (submit_command(XHCI_TRB_TYPE_ENABLE_SLOT, 0, &slot)) {
        xhci.enable_slot_completions++;
        xhci.last_slot_id = slot;
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
}
