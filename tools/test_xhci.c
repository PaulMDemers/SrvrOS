/* Host regression tests execute the driver's actual static helpers.
 * Unused kernel entry points are discarded by --gc-sections. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#define SRVROS_XHCI_TEST_MMIO
static uint32_t xhci_test_read32(uint64_t offset);
static void xhci_test_write32(uint64_t offset, uint32_t value);
struct xhci_device;
static void xhci_test_control_publish(struct xhci_device *, uint64_t, bool);
#include "../kernel/src/drivers/xhci.c"

/* Model a controller inspecting setup before software gives it ownership. */
static void xhci_test_control_publish(struct xhci_device *d, uint64_t phys, bool cycle) {
    unsigned i=(phys-d->ep0_ring_phys)/16;
    assert((d->ep0_ring[i].control & 1) != cycle);
    unsigned stages=(d->ep0_ring[i].control & (3u<<16)) ? 3 : 2;
    for(unsigned n=1;n<stages;n++) {
        if(++i==63) { assert((d->ep0_ring[i].control & 1)==cycle); i=0; cycle=!cycle; }
        assert((d->ep0_ring[i].control & 1)==cycle);
        assert(((d->ep0_ring[i].control>>10)&63)==(n==stages-1 ? XHCI_TRB_TYPE_STATUS_STAGE : XHCI_TRB_TYPE_DATA_STAGE));
    }
}

static _Alignas(4096) uint8_t pool[5 * 1024 * 1024];
static uint64_t pool_used;
static uint64_t fail_after;
static uint64_t freed;
static _Alignas(4096) uint8_t registers[XHCI_MMIO_MAP_SIZE];

/* A deterministic root-port model: delayed connection/reset acknowledgement,
 * real RW1C change bits, and a wrapping MFINDEX. Other tests use plain memory. */
static struct {
    bool active, frozen, connected, reset_pending, never_complete, disabled;
    uint32_t now, connect_at, disconnect_at, reset_at, finished_at, state;
    unsigned resets;
} port_model;

static void model_update(void) {
    if (!port_model.connected && port_model.now >= port_model.connect_at) {
        port_model.connected = true;
        port_model.state |= XHCI_PORTSC_CCS | XHCI_PORTSC_CSC;
    }
    if (port_model.reset_pending) {
        uint32_t age = port_model.now - port_model.reset_at;
        if (age >= 80) port_model.state |= XHCI_PORTSC_PR; /* Posted reset. */
        if (age >= 800 && !port_model.never_complete) {
            port_model.state &= ~XHCI_PORTSC_PR;
            port_model.state |= XHCI_PORTSC_PRC;
            if (!port_model.disabled) port_model.state |= XHCI_PORTSC_PED;
            port_model.reset_pending = false;
            port_model.finished_at = port_model.now;
        }
    }
    if (port_model.disconnect_at && port_model.now >= port_model.disconnect_at) {
        port_model.state &= ~(XHCI_PORTSC_CCS | XHCI_PORTSC_PED);
        port_model.state |= XHCI_PORTSC_CSC;
    }
}

static uint32_t xhci_test_read32(uint64_t offset) {
    if (port_model.active) {
        if (offset == xhci.rt_base) {
            if (!port_model.frozen) port_model.now++;
            model_update();
            return port_model.now & 0x3fff;
        }
        if (offset == xhci.op_base + XHCI_OP_PORTS) {
            model_update();
            return port_model.state;
        }
    }
    return *(uint32_t *)(registers + offset);
}

static struct xhci_device *mode_device;
static unsigned mode_doorbells;
static struct xhci_device *wifi_test_device;
static unsigned wifi_doorbells;
static struct xhci_device *recovery_device;
static unsigned recovery_fault, recovery_calls;
static bool recovery_integrated, recovery_delay_ok=true;
bool timer_delay_us(uint64_t us) { assert(us==10000); return recovery_delay_ok; }

static void xhci_test_write32(uint64_t offset, uint32_t value) {
    if(recovery_device) {
        struct xhci_device *d=recovery_device;
        unsigned e=xhci.event_dequeue;unsigned cycle=xhci.event_cycle?1:0;
        if(offset==xhci.db_base && value==0) {
            unsigned ci=xhci.command_enqueue-1;
            struct xhci_trb *cmd=&xhci.command_ring[ci];unsigned type=(cmd->control>>10)&63;
            uint64_t ptr=cmd->parameter_low|((uint64_t)cmd->parameter_high<<32);
            if(type==15) {
                context_write32(d->output_context,1,0,3);
                context_write32(d->output_context,1,2,(uint32_t)d->firmware.control_data|1);
                xhci.event_ring[e++]=(struct xhci_trb){.parameter_low=d->firmware.control_data,.status=(26u<<24)|4,.control=(32u<<10)|(1u<<16)|(1u<<24)|cycle};
            } else {
                assert(type==16 && cmd->status==0);
                if(recovery_fault!=4) {
                    context_write32(d->output_context,1,2,(uint32_t)ptr);
                    context_write32(d->output_context,1,3,ptr>>32);
                }
            }
            if(recovery_fault!=3)xhci.event_ring[e]=(struct xhci_trb){
                .parameter_low=(uint32_t)(xhci.command_ring_phys+ci*16),
                .status=(recovery_fault==2?5u:1u)<<24,
                .control=(33u<<10)|((recovery_fault==1?2u:1u)<<24)|cycle};
        }
        if(recovery_integrated && offset==xhci.db_base+4 && value==1 && recovery_calls++) {
            uint64_t ptr=strict_control.status;
            if(strict_control.data_in)d->descriptor_buffer[0]=1;
            /* A stale success for the retired TD must not complete this retry. */
            xhci.event_ring[e++]=(struct xhci_trb){.parameter_low=0x9020,.status=1u<<24,.control=(32u<<10)|(1u<<16)|(1u<<24)|cycle};
            if(recovery_fault!=11)xhci.event_ring[e]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,.status=1u<<24,.control=(32u<<10)|(1u<<16)|(1u<<24)|cycle};
        }
    }

    if (wifi_test_device && offset == xhci.db_base+4 && value == 1) {
        wifi_doorbells++;
        uint8_t *p = wifi_test_device->descriptor_buffer;
        p[0] = 1; p[1] = 0; p[2] = 0x10; p[3] = 0x76;
    }
    if (mode_device && offset==xhci.db_base+4 && value==1) {
        if (!mode_doorbells) for (unsigned i=0;i<8;i++) mode_device->descriptor_buffer[i]=0x80+i;
        else {
            assert(mode_device->descriptor_buffer[0]==1);
            for (unsigned i=1;i<8;i++) assert(mode_device->descriptor_buffer[i]==0x80+i);
        }
        mode_doorbells++;
    }
    if (port_model.active && offset == xhci.op_base + XHCI_OP_PORTS) {
        assert(!(value & XHCI_PORTSC_PED)); /* Would disable the port. */
        port_model.state = (port_model.state & ~XHCI_PORTSC_RWS) | (value & XHCI_PORTSC_RWS);
        port_model.state &= ~(value & XHCI_PORTSC_CHANGE_MASK);
        if (value & XHCI_PORTSC_PR) {
            assert(!(port_model.state & XHCI_PORTSC_PRC)); /* Stale PRC cleared. */
            assert(!port_model.reset_pending);
            port_model.reset_pending = true;
            port_model.reset_at = port_model.now;
            port_model.resets++;
        }
        return;
    }
    *(uint32_t *)(registers + offset) = value;
}

void console_write(const char *s) { (void)s; }
void console_printf(const char *fmt, ...) { (void)fmt; }
uint64_t pmm_alloc_frames_tagged(uint64_t count, enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA);
    if (pool_used + count > fail_after) return 0;
    uint64_t address = (uint64_t)(pool + pool_used * PMM_FRAME_SIZE);
    pool_used += count;
    assert(pool_used * PMM_FRAME_SIZE <= sizeof(pool));
    return address;
}
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) { return pmm_alloc_frames_tagged(1, tag); }
void *pmm_phys_to_virt(uint64_t address) { return (void *)address; }
void pmm_free_frame(uint64_t address) { assert(address != 0); freed++; }
void pmm_free_frame_range(uint64_t address, uint64_t count) { assert(address != 0); freed += count; }

static void reset_test(void) {
    firmware_command_exact=firmware_command_failed=wifi_firmware_enabled=wifi_mac_startup=false;
    memset(&port_model, 0, sizeof(port_model));
    memset(&xhci, 0, sizeof(xhci));
    memset(registers, 0, sizeof(registers));
    memset(pool, 0xa5, sizeof(pool));
    pool_used = freed = 0;
    fail_after = sizeof(pool) / PMM_FRAME_SIZE;
    xhci.mmio_virt = (uint64_t)registers;
    xhci.page_size = 1;
}

static void test_scratchpads(void) {
    assert(required_scratchpads(0) == 0);
    assert(required_scratchpads(2u << 27) == 2);
    assert(required_scratchpads((1u << 21) | (3u << 27)) == 35);
    assert(required_scratchpads(0xffffffffu) == 1023);
    const uint16_t counts[] = {0, 2, 35, 513, 1023};
    for (unsigned n = 0; n < sizeof(counts) / sizeof(counts[0]); n++) {
        reset_test();
        uint64_t dcbaa[256] = {0};
        xhci.dcbaa = dcbaa;
        uint16_t count = counts[n];
        xhci.hcs_params2 = ((uint32_t)(count & 31) << 27) | ((uint32_t)(count >> 5) << 21);
        assert(allocate_scratchpads());
        assert(xhci.scratchpads_allocated == count);
        assert((dcbaa[0] != 0) == (count != 0));
        uint64_t *array = (uint64_t *)dcbaa[0];
        for (uint16_t i = 0; i < count; i++) {
            assert((array[i] & 4095) == 0);
            assert(i == 0 || array[i] != array[i - 1]);
            uint8_t *buffer = (uint8_t *)array[i];
            for (unsigned j = 0; j < PMM_FRAME_SIZE; j++) assert(buffer[j] == 0);
        }
    }
    reset_test();
    uint64_t dcbaa[256] = {0};
    xhci.dcbaa = dcbaa;
    xhci.hcs_params2 = 2u << 27;
    fail_after = 2; /* Pointer array + first buffer succeed, second buffer fails. */
    assert(!allocate_scratchpads());
    assert(dcbaa[0] == 0 && freed == 2 && xhci.scratchpads_allocated == 0);
    xhci.page_size = 2;
    assert(!allocate_scratchpads());
}

static void test_handoff(void) {
    reset_test();
    assert(claim_firmware_ownership());
    assert(strcmp(xhci.handoff, "no-legacy-cap") == 0);
    xhci.hcc_params1 = 0x20u << 16; /* Extended capability at byte 0x80. */
    mmio_write32(0x80, XHCI_EXT_CAP_LEGACY);
    mmio_write32(0x84, 0xffffffffu);
    assert(claim_firmware_ownership());
    assert(mmio_read32(0x80) & XHCI_LEGACY_OS_OWNED);
    assert(mmio_read32(0x84) == (XHCI_LEGACY_PRESERVE | XHCI_LEGACY_EVENTS));
    /* Firmware never releases ownership. Do not alter SMI control or start/reset. */
    mmio_write32(0x80, XHCI_EXT_CAP_LEGACY | XHCI_LEGACY_BIOS_OWNED);
    mmio_write32(0x84, 0x12345678);
    assert(!claim_firmware_ownership());
    assert(strcmp(xhci.handoff, "BIOS-timeout") == 0);
    assert(mmio_read32(0x84) == 0x12345678);
    xhci.hcc_params1 = 0xffffu << 16;
    assert(!claim_firmware_ownership());
}

static void test_ep0_packet(void) {
    assert(default_ep0_packet_for_speed(1) == 8); /* Pro 0262, full speed. */
    assert(default_ep0_packet_for_speed(2) == 8);
    assert(default_ep0_packet_for_speed(3) == 64);
    assert(default_ep0_packet_for_speed(4) == 512);
    for (unsigned wide = 0; wide < 2; wide++) {
        for (unsigned failed = 0; failed < 2; failed++) {
            reset_test(); xhci.hcc_params1 = wide ? 4 : 0;
            uint8_t input[4096] = {0}, output[4096] = {0};
            uint8_t descriptor[18] = {18, USB_DESC_DEVICE}; descriptor[7] = 64;
            struct xhci_trb commands[64] = {0}, events[64] = {0};
            xhci.command_ring = commands; xhci.command_ring_phys = (uintptr_t)commands;
            xhci.command_cycle = true; xhci.event_ring = events; xhci.event_cycle = true;
            xhci.rt_base = 0x2000; xhci.db_base = 0x1000;
            events[0] = (struct xhci_trb){.parameter_low = (uint32_t)(uintptr_t)commands,
                .parameter_high = (uint32_t)((uintptr_t)commands >> 32),
                .status = (failed ? 17u : 1u) << 24,
                .control = 1 | (XHCI_TRB_TYPE_COMMAND_COMPLETION << 10) | (3u << 24)};
            struct xhci_device d = {.speed = 1, .slot_id = 3, .ep0_max_packet = 8,
                .input_context = input, .input_context_phys = (uintptr_t)input,
                .output_context = output, .descriptor_buffer = descriptor};
            context_write32(output, 1, 0, 0x10001); /* Preserve interval, clear EP state. */
            context_write32(output, 1, 1, (8u << 16) | 0x26);
            context_write32(output, 1, 2, 0x12345031); /* Live dequeue, not ring start. */
            context_write32(output, 1, 3, 0x12);
            assert(update_ep0_packet(&d) == !failed);
            assert(d.ep0_max_packet == (failed ? 8 : 64));
            assert(context_read32(input, 0, 0) == 0 && context_read32(input, 0, 1) == 2);
            assert(context_read32(input, 1, 0) == 0); /* No slot reconfiguration. */
            assert(context_read32(input, 2, 0) == 0x10000);
            assert(context_read32(input, 2, 1) == ((64u << 16) | 0x26));
            assert(context_read32(input, 2, 2) == 0x12345031 && context_read32(input, 2, 3) == 0x12);
            assert(((commands[0].control >> 10) & 63) == XHCI_TRB_TYPE_EVALUATE_CONTEXT);
            assert(context_read32(output, 1, 1) >> 16 == 8); /* Driver never writes output context. */
            unsigned queued = xhci.command_enqueue;
            descriptor[7] = 12; assert(!update_ep0_packet(&d));
            descriptor[0] = 0; descriptor[7] = 8; assert(!update_ep0_packet(&d));
            descriptor[0] = 18; descriptor[1] = USB_DESC_CONFIGURATION; assert(!update_ep0_packet(&d));
            descriptor[1] = USB_DESC_DEVICE; d.speed = 2; descriptor[7] = 64; assert(!update_ep0_packet(&d));
            d.speed = 3; descriptor[7] = 8; assert(!update_ep0_packet(&d));
            d.speed = 4; descriptor[7] = 0; assert(!update_ep0_packet(&d));
            descriptor[7] = 9; d.ep0_max_packet = 512; assert(update_ep0_packet(&d));
            d.speed = 1; descriptor[7] = 8; d.ep0_max_packet = 8; assert(update_ep0_packet(&d));
            assert(xhci.command_enqueue == queued);
        }
    }
    puts("xhci-ep0: PASS full-speed prefix sizing, live-context update, rejection, both context sizes and SuperSpeed encoding");
}

static void test_keyboard_context(const char *fixture, uint8_t interface, uint8_t dci, uint16_t packet) {
    reset_test();
    uint8_t descriptor[4096] = {0};
    FILE *file = fopen(fixture, "rb");
    assert(file != NULL);
    size_t length = fread(descriptor, 1, sizeof(descriptor), file);
    fclose(file);
    assert(length == 84);
    uint8_t config;
    uint8_t input[4096] = {0}, output[4096] = {0};
    struct xhci_device device = {.descriptor_buffer = descriptor, .speed = 1,
        .input_context = input, .output_context = output, .interrupt_ring_phys = 0x12345000};
    assert(parse_hid_input_config(&device, length, &config));
    assert(config == 1 && device.interface_count == 3);
    assert(device.interface_number == interface && device.interrupt_dci == dci);
    assert(device.interrupt_max_packet == packet && device.interrupt_interval == 8);
    assert(device.hid_boot_keyboard && !device.hid_mouse);
    assert(interrupt_interval_exponent(1, 8) == 6); /* 8ms = 64 microframes. */
    assert(interrupt_interval_exponent(2, 10) == 6);
    assert(interrupt_interval_exponent(1, 255) == 10);
    assert(interrupt_interval_exponent(3, 8) == 7);
    assert(interrupt_interval_exponent(3, 1) == 0);
    assert(interrupt_interval_exponent(3, 16) == 15);
    for (unsigned context64 = 0; context64 < 2; context64++) {
        xhci.hcc_params1 = context64 ? 1u << 2 : 0;
        context_write32(output, 0, 2, 0x0203); /* Preserve TT hub and port. */
        setup_interrupt_input_context(&device);
        assert(context_read32(input, 0, 1) == ((1u << 0) | (1u << dci)));
        assert(context_read32(input, 1, 2) == 0x0203);
        assert(context_read32(input, dci + 1, 0) == 6u << 16);
        assert(context_read32(input, dci + 1, 1) >> 16 == packet);
        assert(context_read32(input, dci + 1, 4) == (packet | ((uint32_t)packet << 16)));
        assert(context_read32(input, dci + 1, 2) == 0x12345001);
    }
    /* Configured interrupt TRBs request short packet events, e.g. 8 bytes
     * returned from a 64-byte Apple endpoint. Failed endpoint setup must not poll. */
    struct xhci_trb ring[64] = {0};
    uint8_t buffer[64];
    device.interrupt_ring = ring;
    device.interrupt_buffer = buffer;
    device.interrupt_buffer_phys = (uint64_t)buffer;
    device.interrupt_cycle = true;
    xhci.db_base = 0x1000;
    submit_interrupt_poll(&device);
    assert(!device.interrupt_pending);
    device.interrupt_ready = true;
    submit_interrupt_poll(&device);
    assert(device.interrupt_pending);
    assert((ring[0].control & (XHCI_TRB_ISP | XHCI_TRB_IOC)) == (XHCI_TRB_ISP | XHCI_TRB_IOC));
    assert(ring[0].status == packet);
}

static void setup_port_model(uint32_t connect_at) {
    reset_test();
    xhci.op_base = 0x100;
    xhci.rt_base = 0x2000;
    xhci.port_count = 1;
    port_model.active = true;
    port_model.connect_at = connect_at;
    port_model.state = (1u << 10) | XHCI_PORTSC_PP; /* Full speed, initially absent. */
}

static void test_address_recovery(void) {
    for (unsigned mode=0;mode<3;mode++) {
        setup_port_model(0);
        port_model.now=16350; /* Recovery crosses the 14-bit MFINDEX wrap. */
        port_model.frozen=mode==1;
        uint64_t dcbaa[256]={0};
        struct xhci_trb commands[256]={0},events[256]={0};
        xhci.dcbaa=dcbaa;xhci.max_slots=8;xhci.db_base=0x1000;
        xhci.command_ring=commands;xhci.command_ring_phys=(uintptr_t)commands;
        xhci.command_cycle=true;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned i=0;i<2;i++) {
            uint64_t ptr=(uintptr_t)&commands[i];
            events[i]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,
                .parameter_high=ptr>>32,.status=((mode==2 && i==1)?17u:1u)<<24,
                .control=1|(XHCI_TRB_TYPE_COMMAND_COMPLETION<<10)|(1u<<24)};
        }
        struct xhci_device d={.speed=3,.port_id=1};
        assert(allocate_device_runtime(&d));
        uint64_t allocated=pool_used;
        assert(address_device(&d)==(mode==0));
        assert(d.ep0_enqueue==0); /* No early descriptor TD is submitted. */
        if(mode==0)assert(port_model.now>=16431 && !d.ep0_quarantined && d.addressed);
        if(mode==1)assert(port_model.now==16350 && d.ep0_quarantined && d.addressed);
        if(mode==2)assert(port_model.now==16350 && !d.addressed);
        assert(pool_used==allocated && freed==0); /* Retain controller-owned memory. */
    }
    puts("xhci-address: PASS recovery interval, clock wrap/stall and failed address command");
}

static void test_root_ports(void) {
    /* The old one-shot scan misses this device entirely. The reset is also
     * deliberately invisible for 10 ms; PR=0 is not reset completion. */
    setup_port_model(1600); /* 200 ms after controller start. */
    port_model.state &= ~XHCI_PORTSC_PP;
    port_model.state |= 7u << 25; /* Preserve wake enables on neutral writes. */
    reset_connected_ports();
    assert(port_model.state & XHCI_PORTSC_PP);
    assert((port_model.state & (7u << 25)) == (7u << 25));
    assert(!(xhci.ports[0].initial_portsc & XHCI_PORTSC_CCS));
    assert(xhci.ports[0].portsc & XHCI_PORTSC_PED);
    assert(port_model.reset_at >= 2400); /* Connection debounce >= 100 ms. */
    assert(port_model.resets == 1 && xhci.ports_enabled == 1);
    assert(port_model.finished_at >= port_model.reset_at + 800);
    assert(strcmp(xhci.ports[0].reset_result, "ready") == 0);
    assert(xhci.ports[0].reset_portsc & XHCI_PORTSC_PRC);

    /* Old completion and enabled bits must not short circuit a fresh reset. */
    setup_port_model(0);
    port_model.state |= XHCI_PORTSC_PRC | XHCI_PORTSC_PED;
    port_model.now = 16300; /* Reset/recovery crosses MFINDEX wrap. */
    assert(reset_port(0));
    assert(port_model.resets == 1 && port_model.finished_at != 0);
    assert(port_model.now - port_model.finished_at >= 80);

    setup_port_model(0);
    port_model.state |= XHCI_PORTSC_PED | XHCI_PORTSC_PRC;
    port_model.never_complete = true;
    assert(!reset_port(0));
    assert(!(xhci.ports[0].portsc & XHCI_PORTSC_PED));
    assert(strcmp(xhci.ports[0].reset_result, "reset-timeout") == 0);
    assert(port_model.now >= 4000 && port_model.now < 5000);

    setup_port_model(0);
    port_model.disabled = true;
    assert(!reset_port(0));
    assert(strcmp(xhci.ports[0].reset_result, "reset-disabled") == 0);

    setup_port_model(0);
    port_model.disconnect_at = 200;
    assert(!reset_port(0));
    assert(strcmp(xhci.ports[0].reset_result, "disconnected") == 0);

    setup_port_model(0);
    port_model.frozen = true;
    assert(!reset_port(0));
    assert(strcmp(xhci.ports[0].reset_result, "clock-stalled") == 0);

    setup_port_model(0);
    port_model.disconnect_at = 400; /* Disconnect after 50 ms: never stable. */
    reset_connected_ports();
    assert(port_model.resets == 0 && xhci.ports_enabled == 0);

    setup_port_model(0);
    port_model.frozen = true;
    port_model.state |= XHCI_PORTSC_PED;
    reset_connected_ports();
    assert(!(xhci.ports[0].portsc & XHCI_PORTSC_PED));
    assert(strcmp(xhci.ports[0].reset_result, "clock-stalled") == 0);

    setup_port_model(20000); /* No device within the bounded discovery window. */
    reset_connected_ports();
    assert(port_model.resets == 0 && xhci.ports_enabled == 0);
    assert(strcmp(xhci.ports[0].reset_result, "no-connect") == 0);
    assert(port_model.now >= 8000 && port_model.now < 9000);

    setup_port_model(0);
    port_model.state = (4u << 10) | XHCI_PORTSC_PP | XHCI_PORTSC_PED;
    reset_connected_ports();
    assert(port_model.resets == 0 && xhci.ports_enabled == 1);
    assert(strcmp(xhci.ports[0].reset_result, "link-ready") == 0);
}

static unsigned test_pointer_options,tap_injected;static uint8_t last_tap;
void mouse_trackpad_get_settings(unsigned *speed,unsigned *options) { *speed=3;*options=test_pointer_options; }
uint64_t mouse_overflow_count(void) { return 0; }
static unsigned injected, pointer_injected, pointer_canceled, net_wakeups;
void net_notify_rx(void) { net_wakeups++; }
static struct trackpad_motion last_pointer;
void mouse_inject_trackpad(int32_t x,int32_t y,uint8_t buttons) {
    pointer_injected++; last_pointer=(struct trackpad_motion){.dx=x,.dy=y,.buttons=buttons};
}
void mouse_cancel_trackpad(void) { pointer_canceled++;mouse_inject_trackpad(0,0,0); }
void keyboard_inject_hid_report(const uint8_t report[8], struct keyboard_hid_state *state) {
    (void)state; assert(report[2]==4); injected++;
}
void mouse_inject_event(int32_t x,int32_t y,uint8_t buttons) { (void)x;(void)y;(void)buttons; }
void mouse_inject_trackpad_tap(uint8_t button) { tap_injected++;last_tap=button;mouse_inject_trackpad(0,0,button);mouse_inject_trackpad(0,0,0); }
void mouse_inject_trackpad_scroll(int32_t x,int32_t y,int32_t wx,int32_t wy,uint8_t buttons) {
    mouse_inject_trackpad(x,y,buttons); last_pointer.wheel_x=wx; last_pointer.wheel_y=wy;
}
static uint64_t test_time, test_time_step=1000, complete_at;
static struct xhci_device *complete_device;
static bool complete_control;
uint64_t timer_monotonic_us(void) {
    test_time+=test_time_step;
    if(complete_control && test_time>=complete_at && strict_control.device) {
        uint64_t ptr=strict_control.status;
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
            .parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,.status=1u<<24,
            .control=(32u<<10)|((uint32_t)strict_control.device->slot_id<<24)|(1u<<16)|(xhci.event_cycle?1u:0u)};
        complete_control=false;
    }
    if(complete_device && test_time>=complete_at && complete_device->bulk_pending) {
        struct xhci_device *d=complete_device;
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){
            .parameter_low=(uint32_t)d->bulk_trb,.parameter_high=(uint32_t)(d->bulk_trb>>32),
            .status=1u<<24,.control=(32u<<10)|((uint32_t)d->slot_id<<24)|
                ((uint32_t)d->bulk_dci<<16)|(xhci.event_cycle?1u:0u)};
        complete_device=NULL;
    }
    return test_time;
}
bool scheduler_io_active(void) { return false; }
bool scheduler_io_yield(void) { return false; }
static void test_storage(void) {
    reset_test();
    uint8_t descriptor[44], input[4096], output[4096]={0};
    FILE *file=fopen("tools/fixtures/phison-13fe-6700-config.bin","rb"); assert(file);
    assert(fread(descriptor,1,sizeof(descriptor),file)==sizeof(descriptor)); fclose(file);
    struct xhci_device d={.speed=4,.descriptor_buffer=descriptor,.input_context=input,.output_context=output};
    assert(parse_storage_config(&d,sizeof(descriptor)));
    assert(d.bulk_ep[0].dci==4 && d.bulk_ep[1].dci==3 && d.configuration_value==1);
    for(unsigned i=0;i<2;i++) assert(d.bulk_ep[i].packet==1024 && d.bulk_ep[i].burst==3);
    for(unsigned size64=0;size64<2;size64++) {
        xhci.hcc_params1=size64?1u<<2:0;
        d.bulk_ep[0].ring_phys=0x12000; d.bulk_ep[1].ring_phys=0x13000;
        context_write32(output,0,2,0x0304); setup_bulk_input_context(&d);
        assert(context_read32(input,0,1)==25);
        assert(context_read32(input,1,0)>>27==4 && context_read32(input,1,2)==0x0304);
        assert(context_read32(input,5,1)==((1024u<<16)|(3u<<8)|(2u<<3)|6));
        assert(context_read32(input,4,1)==((1024u<<16)|(3u<<8)|(6u<<3)|6));
        assert(context_read32(input,5,2)==0x12001 && context_read32(input,4,2)==0x13001);
        assert(context_read32(input,5,4)==4096);
    }
    for(unsigned i=0;i<sizeof(descriptor);i++) assert(!parse_storage_config(&d,i));
    descriptor[27]=16; assert(!parse_storage_config(&d,44)); descriptor[27]=3; /* burst */
    descriptor[28]=1; assert(!parse_storage_config(&d,44)); descriptor[28]=0; /* streams */
    descriptor[33]=0x81; assert(!parse_storage_config(&d,44)); descriptor[33]=2; /* duplicate IN */
    descriptor[25]=5; assert(!parse_storage_config(&d,44)); descriptor[25]=6;
    descriptor[12]=1; assert(!parse_storage_config(&d,44)); descriptor[12]=0; /* alternate */
    d.speed=2; assert(!parse_storage_config(&d,44));

    /* A HID service consumes events while a bulk caller has yielded. Wrong
     * pointers are ignored, exact matches are retained for their waiter. */
    reset_test(); injected=0;
    struct xhci_trb events[64]={0}; xhci.event_ring=events; xhci.event_cycle=true;
    xhci.event_ring_phys=0x1000; xhci.rt_base=0x2000;
    struct xhci_device *disk=&xhci.devices[0], *kbd=&xhci.devices[1];
    *disk=(struct xhci_device){.present=true,.slot_id=1,.bulk_pending=true,.bulk_trb=0x9000,.bulk_dci=3};
    uint8_t report[64]={0,0,4};
    *kbd=(struct xhci_device){.present=true,.slot_id=2,.hid_keyboard=true,.interrupt_pending=true,
        .interrupt_trb=0xa000,.interrupt_dci=5,.interrupt_max_packet=64,.interrupt_buffer=report};
    events[0]=(struct xhci_trb){.parameter_low=0x9990,.status=1u<<24,.control=(32u<<10)|(1u<<24)|(3u<<16)|1};
    events[1]=(struct xhci_trb){.parameter_low=0xa000,.status=(13u<<24)|56,.control=(32u<<10)|(2u<<24)|(5u<<16)|1};
    events[2]=(struct xhci_trb){.parameter_low=0x9000,.status=1u<<24,.control=(32u<<10)|(1u<<24)|(3u<<16)|1};
    poll_hid_events_once();
    assert(injected==1 && kbd->hid_reports==1 && !kbd->interrupt_pending);
    assert(disk->bulk_done && !disk->bulk_pending && disk->bulk_completion==1);
    /* A successful HID completion rearms immediately, including when consumed
     * by a bulk waiter. Old TRB events cannot consume the replacement request. */
    struct xhci_trb hid_ring[64]={0};
    kbd->interrupt_ready=true;kbd->interrupt_pending=true;kbd->interrupt_trb=0xa000;
    kbd->interrupt_ring=hid_ring;kbd->interrupt_ring_phys=0xb000;kbd->interrupt_cycle=true;
    report[0]=2; /* A distinct modifier report, not the duplicate suppression path. */
    dispatch_transfer_event(&events[1]);
    assert(kbd->interrupt_pending && kbd->interrupt_trb==0xb000 && kbd->hid_reports==2);
    dispatch_transfer_event(&events[1]);assert(kbd->hid_reports==2);
    report[2]=4;
    struct xhci_trb next_key=events[1];next_key.parameter_low=0xb000;
    dispatch_transfer_event(&next_key);
    assert(kbd->interrupt_pending && kbd->interrupt_trb==0xb010 && kbd->hid_reports==3);
    /* Bounded stalled transfer: reject all later submissions, preserving DMA. */
    struct xhci_trb ring[64]={0}; uint8_t bounce[4096], caller[512];
    disk->bulk_ep[1]=(struct xhci_bulk_endpoint){.dci=3,.ring=ring,.ring_phys=0x9000,.cycle=true};
    disk->bulk_buffer=bounce; disk->bulk_buffer_phys=0x10000;
    uint32_t actual=0;
    assert(!storage_bulk_transfer(disk,true,caller,512,&actual));
    assert(disk->bulk_failed && disk->bulk_pending && disk->bulk_ep[1].enqueue==1);
    assert(!strcmp(disk->bulk_reason,"deadline") && !disk->bulk_done && !disk->bulk_completion);
    assert(disk->bulk_length==512 && disk->bulk_elapsed_us>=2000000 && disk->bulk_spins>0);
    assert(!storage_bulk_transfer(disk,true,caller,512,&actual) && disk->bulk_ep[1].enqueue==1);
    /* Fresh simulated controller: distinguish a received error event from a timeout. */
    disk->bulk_failed=disk->bulk_pending=false;disk->bulk_ep[1].enqueue=0;
    events[3]=(struct xhci_trb){.parameter_low=0x9000,.status=6u<<24,
        .control=(32u<<10)|(1u<<24)|(3u<<16)|1};
    assert(!storage_bulk_transfer(disk,true,caller,13,&actual));
    assert(!strcmp(disk->bulk_reason,"completion-code") && disk->bulk_completion==6 && disk->bulk_done);
    assert(disk->bulk_length==13 && disk->bulk_spins==0);
    /* A 500 ms status read on a fast CPU must survive the former total spin
     * budget. Model three microseconds per 256 polls (as on the laptop). */
    disk->bulk_failed=disk->bulk_pending=false;
    test_time_step=3;complete_device=disk;complete_at=test_time+500000;
    memset(bounce,0x5a,sizeof(bounce));memset(caller,0,sizeof(caller));
    assert(storage_bulk_transfer(disk,true,caller,13,&actual));
    assert(actual==13 && !memcmp(caller,bounce,13) && !disk->bulk_failed);
    assert(!strcmp(disk->bulk_reason,"completed") && test_time>=complete_at);
    assert(disk->bulk_ep[1].enqueue==2); /* Exactly one submission, no retry. */
    /* A progressing clock still enforces the original two-second deadline,
     * even when reaching it needs more than the old iteration allowance. */
    uint64_t before=test_time;
    assert(!storage_bulk_transfer(disk,true,caller,13,&actual));
    assert(!strcmp(disk->bulk_reason,"deadline") && test_time-before>=2000000);
    assert(disk->bulk_spins>XHCI_TRANSFER_TIMEOUT && disk->bulk_failed && disk->bulk_pending);
    /* A frozen reference must remain bounded and preserve DMA quarantine. */
    disk->bulk_failed=disk->bulk_pending=false;test_time_step=0;
    assert(!storage_bulk_transfer(disk,true,caller,13,&actual));
    assert(!strcmp(disk->bulk_reason,"clock-stalled") && disk->bulk_elapsed_us==0);
    assert(disk->bulk_spins<=XHCI_TRANSFER_TIMEOUT && disk->bulk_failed && disk->bulk_pending);
    uint64_t enqueue=disk->bulk_ep[1].enqueue;
    assert(!storage_bulk_transfer(disk,true,caller,13,&actual));
    assert(disk->bulk_ep[1].enqueue==enqueue);
    test_time_step=1000;
}

static void test_pro_trackpad(const char *fixture) {
    reset_test();
    uint8_t descriptor[4096]={0},input[4096]={0},output[4096]={0};
    FILE *file=fopen(fixture,"rb");assert(file && fread(descriptor,1,84,file)==84);fclose(file);
    struct xhci_device d={.present=true,.slot_id=1,.vendor_id=0x05ac,.product_id=0x0262,.speed=1,
        .descriptor_buffer=descriptor,.descriptor_buffer_phys=(uintptr_t)descriptor,
        .input_context=input,.output_context=output};
    xhci_set_trackpad_capture(true);
    assert(parse_trackpad_config(&d,84));
    assert(d.trackpad.dci==3 && d.trackpad.report_type==2 && d.trackpad.packet==64 && d.trackpad.interval==2);
    d.product_id=0x0263;assert(!parse_trackpad_config(&d,84));d.product_id=0x0262;
    descriptor[54]=0x82;assert(!parse_trackpad_config(&d,84));descriptor[54]=0x81;
    descriptor[56]=32;assert(!parse_trackpad_config(&d,84));descriptor[56]=64;
    for(unsigned wide=0;wide<2;wide++) {
        xhci.hcc_params1=wide?4:0;
        context_write32(output,0,0,7u<<27);setup_trackpad_input_context(&d);
        assert(context_read32(input,0,1)==(1u|(1u<<3)));
        assert(context_read32(input,1,0)>>27==7 && context_read32(input,8,1)==0);
        assert(context_read32(input,4,1)>>16==64);
    }
    for(unsigned fail=0;fail<3;fail++) {
        struct xhci_trb ring[64]={0},events[64]={0};
        d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_enqueue=0;d.ep0_cycle=true;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;xhci.event_dequeue=0;
        for(unsigned i=0;i<2;i++) events[i]=(struct xhci_trb){.parameter_low=0x9020+48*i,
            .status=((fail==i+1)?6u:1u)<<24,.control=(32u<<10)|(1u<<24)|(1u<<16)|1};
        mode_device=&d;mode_doorbells=0;
        assert(enable_trackpad_mode(&d)==(fail==0));
        assert(mode_doorbells==(fail==1?1u:2u));
        assert(((ring[0].control>>16)&3)==3 && (ring[1].control&XHCI_DATA_DIR_IN));
        if(fail!=1) {
            assert(((ring[3].control>>16)&3)==2 && !(ring[4].control&XHCI_DATA_DIR_IN));
            assert(ring[5].control&XHCI_STATUS_DIR_IN);
            assert(ring[3].parameter_low==0x03000921);
        }
        mode_device=NULL;
    }
    xhci_set_trackpad_capture(false);
    struct xhci_trb input_ring[64]={0};uint8_t report[APPLE_TRACKPAD_REPORT_MAX]={0};
    xhci.devices[0]=d;struct xhci_trackpad_endpoint *t=&xhci.devices[0].trackpad;
    t->ready=true;t->pending=true;t->trb=0xa000;t->ring=input_ring;t->ring_phys=0xb000;
    t->cycle=true;t->buffer=report;t->buffer_phys=(uintptr_t)report;
    report[15]=1;report[30]=4;report[32]=123;report[34]=45;report[46]=10;
    struct xhci_trb event={.parameter_low=0xa000,.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-58),
        .control=(32u<<10)|(1u<<24)|(3u<<16)};
    dispatch_transfer_event(&event);
    assert(t->reports==1 && t->frame.count==1 && t->frame.button && t->frame.contacts[0].x==123);
    puts("pro-trackpad: PASS exact interface/DCI, keyboard preservation, feature read/modify/write and failures");
}

static void test_trackpad(const char *fixture,bool pointer,bool gestures,uint16_t product) {
    xhci_set_trackpad_gestures(false);
    xhci_set_trackpad_pointer(false); pointer_injected=0;pointer_canceled=0;
    tap_injected=0;test_pointer_options=0;
    reset_test();
    uint8_t descriptor[84];
    FILE *file=fopen(fixture,"rb"); assert(file);
    assert(fread(descriptor,1,sizeof(descriptor),file)==84); fclose(file);
    uint8_t input[4096]={0},output[4096]={0},keyboard[64]={0,0,4};
    uint8_t buffer[APPLE_TRACKPAD_REPORT_MAX];
    struct xhci_trb ring[64]={0};
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1,.vendor_id=0x05ac,.product_id=product,.device_bcd=0x0171,.speed=1,
        .descriptor_buffer=descriptor,.input_context=input,.output_context=output,
        .hid_keyboard=true,.interrupt_dci=5,.interrupt_max_packet=64,.interrupt_buffer=keyboard,
        .interrupt_pending=true,.interrupt_trb=0xa000};
    xhci_set_trackpad_capture(false); assert(!parse_trackpad_config(d,84));
    xhci_set_trackpad_capture(!pointer); xhci_set_trackpad_pointer(pointer); xhci_set_trackpad_gestures(gestures);
    for (unsigned n=0;n<84;n++) assert(!parse_trackpad_config(d,n));
    d->product_id=0x0292; assert(!parse_trackpad_config(d,84)); d->product_id=product;
    if (product==0x0290) {
        d->device_bcd=0x0170; assert(!parse_trackpad_config(d,84)); d->device_bcd=0x0171;
        d->vendor_id=0x1234; assert(!parse_trackpad_config(d,84)); d->vendor_id=0x05ac;
        d->speed=3; assert(!parse_trackpad_config(d,84)); d->speed=1;
        descriptor[81]=32; assert(!parse_trackpad_config(d,84)); descriptor[81]=64;
        descriptor[83]=2; assert(!parse_trackpad_config(d,84)); descriptor[83]=1;
        assert(!pointer_injected);
    }
    descriptor[62]=1; assert(!parse_trackpad_config(d,84)); descriptor[62]=0; /* alternate */
    descriptor[79]=0x82; assert(!parse_trackpad_config(d,84)); descriptor[79]=0x83;
    assert(parse_trackpad_config(d,84));
    assert(d->trackpad.candidate && d->trackpad.dci==7 && d->trackpad.interval==1);
    assert(d->trackpad.report_type==3 && enable_trackpad_mode(d)); /* No EP0 mode write. */
    struct xhci_trackpad_endpoint *t=&d->trackpad;
    t->ring_phys=0xb000; t->ring=ring; t->buffer=buffer; t->buffer_phys=0xc000; t->cycle=true;
    for (unsigned wide=0;wide<2;wide++) {
        xhci.hcc_params1=wide?1u<<2:0;
        context_write32(output,0,0,5u<<27); context_write32(output,0,2,0x203);
        setup_trackpad_input_context(d);
        assert(context_read32(input,0,1)==(1u|(1u<<7)));
        assert(context_read32(input,1,0)>>27==7 && context_read32(input,1,2)==0x203);
        assert(context_read32(input,6,1)==0); /* Keyboard context untouched. */
        assert(context_read32(input,8,0)==3u<<16);
        assert(context_read32(input,8,1)>>16==64);
        assert(context_read32(input,8,4)==(APPLE_TRACKPAD_REPORT_MAX|(64u<<16)));
        assert(context_read32(input,8,2)==0xb001);
        context_write32(output,0,0,9u<<27); setup_trackpad_input_context(d);
        assert(context_read32(input,1,0)>>27==9); /* Preserve larger slot entries. */
    }
    submit_trackpad_poll(d); assert(!t->pending);
    t->ready=true; submit_trackpad_poll(d);
    assert(t->pending && t->trb==0xb000 && ring[0].status==APPLE_TRACKPAD_REPORT_MAX);
    assert((ring[0].control&(XHCI_TRB_IOC|XHCI_TRB_ISP))==(XHCI_TRB_IOC|XHCI_TRB_ISP));
    buffer[23]=1; buffer[38]=4; buffer[40]=123; buffer[42]=45; buffer[54]=10;
    struct xhci_trb e={.parameter_low=0xb000,.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66),
        .control=(32u<<10)|(1u<<24)|(7u<<16)};
    d->bulk_pending=true; d->bulk_dci=3; d->bulk_trb=0xd000;
    dispatch_transfer_event(&e);
    assert(pointer_injected==(pointer?1u:0u));
    if (pointer) assert(last_pointer.buttons==1 && !last_pointer.dx && !last_pointer.dy);
    assert(t->reports==1 && t->contact_reports==1 && t->button_reports==1);
    assert(t->frame.count==1 && t->frame.contacts[0].x==123 && t->last_length==66);
    assert(t->samples[1].length==66 && t->samples[1].data[40]==123 && t->samples[3].length==66);
    assert(t->pending && t->trb==0xb010 && d->interrupt_pending && d->bulk_pending);
    dispatch_transfer_event(&e); assert(t->reports==1); /* Old pointer ignored. */
    e.parameter_low=0xb010; e.control=(32u<<10)|(1u<<24)|(5u<<16);
    dispatch_transfer_event(&e); assert(t->reports==1 && t->pending); /* Wrong DCI ignored. */
    e.parameter_low=0xa000; e.status=(13u<<24)|56;
    unsigned before=injected; dispatch_transfer_event(&e);
    assert(injected==before+1 && !d->interrupt_pending && t->pending && d->bulk_pending);
    if (gestures) assert(t->typing_until>test_time);
    e.parameter_low=0xb010; e.control=(32u<<10)|(1u<<24)|(7u<<16);
    e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-38); dispatch_transfer_event(&e);
    assert(t->reports==2 && t->frame.count==0 && !t->frame.button && !t->last_contact_us); /* Lift frame. */
    assert(pointer_injected==(pointer?2u:0u));
    if (pointer) assert(!last_pointer.buttons);
    assert(t->lifts==1 && t->max_contacts==1 && t->samples[1].data[40]==123);
    e.parameter_low=0xb020; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-65);
    dispatch_transfer_event(&e); assert(t->malformed==1 && t->reports==2 && t->pending);
    assert(t->samples[4].length==65);
    buffer[54]=10; buffer[82]=10;
    e.parameter_low=0xb030; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-94);
    dispatch_transfer_event(&e); assert(t->max_contacts==2 && t->samples[2].length==94);
    if (pointer) {
        /* Release after malformed input, then start a new drag on the same
         * endpoint while the original bulk request remains pending. */
        buffer[23]=1; buffer[54]=10; buffer[40]=32;
        e.parameter_low=(uint32_t)t->trb; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66);
        dispatch_transfer_event(&e); assert(last_pointer.buttons==1 && d->bulk_pending);
        buffer[23]=1; buffer[54]=10; buffer[40]=64;
        e.parameter_low=(uint32_t)t->trb;
        dispatch_transfer_event(&e); assert(last_pointer.dx==2 && last_pointer.buttons==1);
    }
    if (gestures) {
        t->typing_until=0;
        buffer[54]=buffer[82]=10;
        e.parameter_low=(uint32_t)t->trb; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-94);
        dispatch_transfer_event(&e); /* New centroid baseline, releases the drag. */
        buffer[54]=buffer[82]=10; buffer[42]=buffer[70]=160;
        e.parameter_low=(uint32_t)t->trb; dispatch_transfer_event(&e);
        assert(last_pointer.wheel_y==10 && !last_pointer.dx && !last_pointer.dy && t->scroll_events==1);
        buffer[23]=1; buffer[54]=10;
        e.parameter_low=(uint32_t)t->trb; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66);
        dispatch_transfer_event(&e); assert(last_pointer.buttons==1);
    }
    if (gestures) {
        /* Actual Type 3 bytes through the shared transfer dispatcher: the
         * stationary click contact must not dilute second-finger steering. */
        unsigned events=pointer_injected;
        for(unsigned step=0;step<3;step++) {
            unsigned x=1000+160*step;
            buffer[23]=1;buffer[54]=buffer[82]=10;
            buffer[40]=64;buffer[42]=160;
            buffer[68]=x&255;buffer[69]=x>>8;buffer[70]=160;
            e.parameter_low=(uint32_t)t->trb;
            e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-94);
            dispatch_transfer_event(&e);
            assert(d->bulk_pending && t->pointer.buttons==1);
        }
        assert(pointer_injected==events+1 && last_pointer.dx==10 && !last_pointer.dy);
        assert(last_pointer.buttons==1 && !last_pointer.wheel_x && !last_pointer.wheel_y);
        assert(t->drag_frames==3 && t->drag_rebases==2 && !t->drag_handoffs);
    }
    if(gestures) {
        /* Full Apple reports complete one primary and one secondary tap. */
        e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-38);
        dispatch_transfer_event(&e);test_pointer_options=SRV_POINTER_TAPS;test_time_step=20000;
        for(unsigned contacts=1;contacts<=2;contacts++) {
            unsigned before=tap_injected;
            buffer[54]=10;buffer[40]=64;
            if(contacts==2) { buffer[82]=10;buffer[68]=232;buffer[69]=3; }
            e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-(38+28*contacts));
            dispatch_transfer_event(&e);assert(tap_injected==before);
            e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-38);
            dispatch_transfer_event(&e);
            assert(tap_injected==before+1 && last_tap==contacts && !last_pointer.buttons && d->bulk_pending);
        }
        assert(t->tap_primary==1 && t->tap_secondary==1);test_pointer_options=0;test_time_step=1000;
        /* Physical secondary press survives one finger lifting, even with
         * tapping disabled, while bulk transport remains active. */
        buffer[23]=1;buffer[54]=buffer[82]=10;
        e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-94);
        dispatch_transfer_event(&e);assert(last_pointer.buttons==2 && d->bulk_pending);
        buffer[23]=1;buffer[54]=10;buffer[40]=64;
        e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66);
        dispatch_transfer_event(&e);assert(t->pointer.buttons==2 && !t->pointer.tap_armed);
        buffer[23]=0;e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-38);
        dispatch_transfer_event(&e);assert(!last_pointer.buttons && tap_injected==2);
    }
    e.parameter_low=(uint32_t)t->trb; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX+1);
    dispatch_transfer_event(&e); assert(t->errors==1 && !t->ready && !t->pending);
    if (pointer) assert(!last_pointer.buttons && t->pointer_cancels==2 && t->pointer_motion==(gestures?2u:1u));
    if(pointer && !gestures) assert(pointer_canceled==1); /* Fault is not a normal release. */
    submit_trackpad_poll(d); assert(!t->pending); /* Error doesn't become a retry storm. */
    struct xhci_trackpad_capture capture;
    xhci_trackpad_snapshot(&capture);
    assert(capture.found && capture.max_contacts==2 && capture.lifts==(gestures?5u:1u) && capture.errors==1);
    assert(capture.tap_primary==(gestures?1u:0u) && capture.tap_secondary==(gestures?1u:0u));
    assert(capture.drag_frames==(gestures?4u:0u) && capture.drag_rebases==(gestures?3u:0u));
    assert(capture.samples[2].length==94 && capture.samples[1].data[40]==123);
    xhci_trackpad_reset_capture(); xhci_trackpad_snapshot(&capture);
    assert(capture.found && !capture.reports && !capture.samples[1].length && capture.errors==1);
    assert(!capture.drag_frames && !capture.drag_rebases && !capture.drag_handoffs && !capture.tap_primary && !capture.tap_secondary);
    if (pointer) assert(capture.pointer_enabled && capture.pointer_cancels==0);
    if(gestures) {
        /* A broad contact interrupting a held press is cancellation, not a
         * widget-activating release. Do not rearm while the pad stays pressed. */
        t->ready=true;t->pointer=(struct trackpad_pointer){0};submit_trackpad_poll(d);
        buffer[23]=1;buffer[54]=10;buffer[55]=0;
        e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66);
        dispatch_transfer_event(&e);assert(last_pointer.buttons==1);
        unsigned canceled=pointer_canceled;
        buffer[54]=601&255;buffer[55]=601>>8;e.parameter_low=(uint32_t)t->trb;
        dispatch_transfer_event(&e);assert(pointer_canceled==canceled+1 && !last_pointer.buttons);
        buffer[54]=10;buffer[55]=0;e.parameter_low=(uint32_t)t->trb;
        dispatch_transfer_event(&e);assert(!t->pointer.buttons && pointer_canceled==canceled+1);
        buffer[23]=0;e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-38);
        dispatch_transfer_event(&e);
        buffer[23]=1;e.parameter_low=(uint32_t)t->trb;e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX-66);
        dispatch_transfer_event(&e);assert(last_pointer.buttons==1);
        cancel_trackpad_pointer(t);assert(pointer_canceled==canceled+2);
    }
    assert(t->rearm_time.count>0);
    if(gestures) assert(t->contact_gap.count>0);
    xhci_set_trackpad_capture(false); xhci_set_trackpad_pointer(false); xhci_set_trackpad_gestures(false);
    puts("trackpad-transport: PASS opt-in, exact descriptor, independent context/ring, multi-packet length, keyboard/bulk coexistence, stale events, errors; synthetic only");
}

static void test_network_transport(void) {
    reset_test();test_time=0;test_time_step=1;complete_device=NULL;
    xhci.op_base=0x100;xhci.db_off=0x3000;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=XHCI_PORTSC_CCS;
    struct xhci_network n={.ready=true};
    struct xhci_device d={.network=&n,.slot_id=1,.port_id=1};
    struct xhci_trb rings[3][XHCI_TRB_COUNT];uint8_t buffers[3][4096];
    memset(rings,0,sizeof(rings));memset(buffers,0,sizeof(buffers));
    for(unsigned i=0;i<3;i++) {
        n.ep[i].hw=(struct xhci_bulk_endpoint){.dci=(uint8_t)(i+2),.packet=64,
            .ring=rings[i],.ring_phys=(uint64_t)rings[i],.cycle=true};
        n.ep[i].buffer=buffers[i];n.ep[i].buffer_phys=(uint64_t)buffers[i];
        write_link_trb(rings[i],(uint64_t)rings[i],true);
    }
    network_service(&d);assert(n.ep[1].pending && n.ep[2].pending && !n.ep[0].pending);
    /* Captured ECM configuration 3 reports speed/link on data interface 1.
     * Generic devices must continue to reject that unexpected interface. */
    uint8_t speed[]={0xa1,0x2a,0,0,1,0,8,0,0,0xca,0x9a,0x3b,0,0xca,0x9a,0x3b};
    n.config.data_interface=1;
    memcpy(n.ep[2].buffer,speed,sizeof(speed));
    unsigned notification_wakes=net_wakeups;
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(speed)));
    assert(net_wakeups==notification_wakes+1 && n.notify_completed_us==test_time);
    network_service(&d);
    assert(n.notifications==0 && !n.link.downstream_bps);
    n.config.notify_data_interface=true;
    memcpy(n.ep[2].buffer,speed,sizeof(speed));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(speed)));network_service(&d);
    assert(n.notifications==1 && n.link.downstream_bps==1000000000);
    uint8_t frame[1514];memset(frame,0x5a,sizeof(frame));
    assert(!network_send(&d,frame,13) && !network_send(&d,frame,1515));
    for(unsigned i=0;i<XHCI_NET_QUEUE;i++) assert(network_send(&d,frame,64));
    assert(!network_send(&d,frame,64));frame[0]=0; /* Queue owns a copy. */
    uint64_t wakes=net_wakeups;
    assert(!network_event(&d,3,n.ep[1].trb+16,1,0));
    assert(!network_event(&d,4,n.ep[1].trb,1,0));
    assert(network_event(&d,3,n.ep[1].trb,13,4096-60));
    assert(net_wakeups==wakes+1 && network_complete(&d,1));
    n.ep[1].done=false;
    for(unsigned i=0;i<300;i++) { /* Cross the link TRB repeatedly with ZLPs. */
        network_service(&d);assert(n.ep[0].pending && n.ep[0].requested==64);
        assert(n.ep[0].buffer[0]==0x5a && n.tx_zero);
        uint64_t original=n.ep[0].trb;
        assert(network_event(&d,2,original,1,0));
        network_service(&d);assert(n.ep[0].pending && !n.ep[0].requested && !n.tx_zero);
        assert(n.tx_packets==i && !network_event(&d,2,original,1,0));
        assert(network_event(&d,2,n.ep[0].trb,1,0));
        network_service(&d);assert(n.tx_packets==i+1);
        frame[0]=0x5a;assert(network_send(&d,frame,64));
    }
    n.count=0; /* Finish the in-flight frame before exercising a short frame. */
    assert(network_event(&d,2,n.ep[0].trb,1,0));network_service(&d);
    assert(n.ep[0].pending && !n.ep[0].requested);
    assert(network_event(&d,2,n.ep[0].trb,1,0));network_service(&d);
    uint64_t count=n.tx_packets;
    assert(network_send(&d,frame,65));network_service(&d);
    assert(n.ep[0].requested==65 && !n.tx_zero);
    assert(network_event(&d,2,n.ep[0].trb,1,0));network_service(&d);
    assert(n.tx_packets==count+1 && !n.ep[0].pending);
    /* Ethernet cable changes preserve USB ownership, drop stale queued TX,
     * reject new TX while down, and resume on a fresh up notification. */
    assert(network_send(&d,frame,65));
    uint8_t down[]={0xa1,0,0,0,1,0,0,0};
    memcpy(n.ep[2].buffer,down,sizeof(down));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(down)));
    network_service(&d);
    assert(n.ready && !n.failed && n.link.known && !n.link.up);
    assert(!n.count && !n.ep[0].pending && n.ep[1].pending);
    assert(!network_send(&d,frame,65));
    down[2]=1;memcpy(n.ep[2].buffer,down,sizeof(down));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(down)));
    network_service(&d);assert(n.link.up && n.ready);
    assert(network_send(&d,frame,65));network_service(&d);
    assert(n.ep[0].pending);
    assert(network_event(&d,2,n.ep[0].trb,1,0));network_service(&d);
    /* A cable loss with an in-flight transfer must not revoke DMA ownership. */
    assert(network_send(&d,frame,65));network_service(&d);
    uint64_t inflight=n.ep[0].trb;
    assert(network_send(&d,frame,65));
    down[2]=0;memcpy(n.ep[2].buffer,down,sizeof(down));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(down)));
    network_service(&d);
    assert(n.ready && n.ep[0].pending && n.ep[0].trb==inflight && !n.count && !freed);
    test_time+=3000000;network_service(&d);
    assert(n.ready && n.tx_suspended && n.ep[0].pending && n.ep[0].trb==inflight && !freed);
    down[2]=1;memcpy(n.ep[2].buffer,down,sizeof(down));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(down)));
    network_service(&d);assert(n.ready && n.link.up && !n.tx_suspended);
    assert(n.ep[0].pending && n.ep[0].trb==inflight && !freed);
    assert(network_event(&d,2,inflight,1,0));network_service(&d);
    assert(n.ready && !n.ep[0].pending && n.ep[1].pending);
    /* Malformed residue quarantines; pending receive buffers remain owned. */
    assert(network_event(&d,3,n.ep[1].trb,13,4097));network_service(&d);
    assert(n.failed && !n.ready && n.errors==1 && !freed);
    assert(!network_send(&d,frame,64));
    n.failed=false;n.ready=true;n.link.known=false;n.ep[1].done=false;
    assert(network_send(&d,frame,64));network_service(&d);
    test_time+=2000000;network_service(&d);assert(n.failed && n.errors==2 && !freed);
    n.failed=false;n.ready=true;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=0;
    network_service(&d);assert(n.failed && n.errors==3 && !freed);
    puts("xhci-network: PASS owned TX queue, ZLP framing, ring wrap, stale events, RX wakeup, residue errors, deadlines and disconnect quarantine");
}

static void test_device_inventory(void) {
    memset(&xhci,0,sizeof(xhci));
    struct xhci_device *d=&xhci.devices[3];
    d->present=true;d->configured=true;d->slot_id=7;d->port_id=5;d->vendor_id=0x05ac;d->product_id=0x0291;
    d->hid_keyboard=true;d->interrupt_ready=true;d->trackpad.ready=true;d->hid_reports=14;d->trackpad.reports=25;
    d->first_interface_class=3;struct srv_device_info info;
    assert(xhci_device_snapshot(0,&info)==4 && info.index==3 && info.bus_kind==SRV_DEVICE_USB);
    assert(info.vendor_id==0x05ac && info.product_id==0x0291 && info.port==5 && info.reports==39);
    assert((info.flags&(SRV_DEVICE_KEYBOARD|SRV_DEVICE_TRACKPAD))==(SRV_DEVICE_KEYBOARD|SRV_DEVICE_TRACKPAD));
    d->trackpad.ready=false;d->interrupt_ready=false;d->bulk_failed=true;
    assert(xhci_device_snapshot(3,&info)==4 && !(info.flags&(SRV_DEVICE_KEYBOARD|SRV_DEVICE_TRACKPAD)) && (info.flags&SRV_DEVICE_FAILED));
    assert(!xhci_device_snapshot(4,&info) && !xhci_device_snapshot(UINT64_MAX,&info));
}

static struct xhci_trb control_event(uint64_t pointer, unsigned cc, unsigned residue) {
    return (struct xhci_trb){.parameter_low = (uint32_t)pointer,
        .parameter_high = pointer >> 32, .status = (cc << 24) | residue,
        .control = (32u << 10) | (1u << 24) | (1u << 16) | 1};
}

static void test_control_worker_completion(void) {
    reset_test();
    struct xhci_device d = {.slot_id=1};
    strict_control.device=&d; strict_control.setup=0x9000;
    strict_control.data=0x9010; strict_control.status=0x9020;
    strict_control.length=4; strict_control.actual=4; strict_control.data_in=true;
    strict_control.short_seen=false; strict_control.done=false;
    struct xhci_trb event=control_event(0xdead,1,0);
    dispatch_transfer_event(&event);
    assert(!strict_control.done);
    event=control_event(0x9010,13,1);
    dispatch_transfer_event(&event);
    assert(!strict_control.done && strict_control.actual==3);
    event=control_event(0x9020,1,0);
    dispatch_transfer_event(&event);
    assert(strict_control.done && !d.ep0_quarantined);
    /* Completion consumed by another worker must remain visible to the owner. */
    assert(wait_control_exact(&d)==3 && !strict_control.device);
    puts("xhci-control-worker: PASS dispatched short/status completion and stale events");
}

static void test_strict_no_data(void) {
    for (unsigned fault=0;fault<6;fault++) {
        reset_test();
        struct xhci_trb ring[64]={0}, events[64]={0};
        struct xhci_device d={.slot_id=1,.ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        struct usb_setup_packet request={.request_type=0x40,.request=1,.value=1};
        events[0]=control_event(0x9010,1,0);
        if(fault==1) events[0]=control_event(0x9000,6,0);
        if(fault==2) events[0]=control_event(0x9010,6,0);
        if(fault==3) events[0]=control_event(0x9010,13,0);
        if(fault==4) events[0]=control_event(0x9010,1,1);
        if(fault==5) events[0]=(struct xhci_trb){0};
        int actual=-1;
        assert(control_transfer_internal(&d,&request,0,0,false,&actual)==!fault);
        assert(d.ep0_enqueue==2 && ((ring[0].control>>16)&3)==0);
        assert(ring[1].control&XHCI_STATUS_DIR_IN);
        assert(d.ep0_quarantined==!!fault && actual==(fault?-1:0));
        if(fault) assert(!control_transfer_internal(&d,&request,0,0,false,&actual) && d.ep0_enqueue==2);
    }
    puts("xhci-no-data: PASS exact status retirement, setup/status errors, residue, timeout and quarantine");
}

static void test_strict_control(void) {
    for (unsigned output = 0; output < 2; ++output) for (unsigned wrap = 0; wrap < 2; ++wrap) for (unsigned scenario = 0; scenario < 16; ++scenario) {
        reset_test();
        struct xhci_trb ring[64] = {0}, events[64] = {0};
        uint8_t buffer[64] = {0};
        unsigned first = wrap ? 61 : 0;
        uint64_t setup = 0x9000 + first*16, data = setup+16, status = wrap ? 0x9000 : setup+32;
        struct xhci_device d = {.slot_id = 1, .ep0_ring = ring, .ep0_ring_phys = 0x9000,
            .ep0_enqueue = first, .ep0_cycle = true, .descriptor_buffer = buffer};
        xhci.db_base = 0x1000; xhci.rt_base = 0x2000;
        xhci.event_ring = events; xhci.event_cycle = true;
        struct usb_setup_packet request = {.request_type = output ? 0x40 : 0xc0, .request = output ? 6 : 7, .length = 4};
        int expected = -1;
        switch (scenario) {
        case 0: events[0] = control_event(status, 1, 0); expected = 4; break;
        case 1: case 2: case 3: case 4:
            events[0] = control_event(data, 13, scenario);
            events[1] = control_event(status, 1, 0); expected = 4-scenario; break;
        case 5: events[0] = control_event(data, 13, 5); break;
        case 6: events[0] = control_event(setup, 6, 0); break;
        case 7: events[0] = control_event(data, 6, 0); break;
        case 8: events[0] = control_event(status, 6, 0); break;
        case 9: events[0] = control_event(data, 13, 2); break; /* Missing status. */
        case 10: break; /* No event: bounded timeout. */
        case 11: events[0] = control_event(status, 1, 1); break;
        case 12: events[0] = control_event(data, 13, 2);
            events[1] = events[0]; events[2] = control_event(status, 1, 0); break;
        case 14: events[0] = control_event(status, 13, 0); break;
        case 15: events[0] = control_event(status, 1, 0); events[0].control |= 4; break;
        case 13: /* Stale same-endpoint and unrelated slot events cannot retire us. */
            events[0] = control_event(0xdead, 1, 0);
            events[1] = control_event(status, 1, 0); events[1].control ^= 3u << 24;
            events[2] = control_event(status, 1, 0); expected = 4; break;
        }
        if (output && scenario >= 1 && scenario <= 4) expected = -1;
        int actual = 42;
        assert(control_transfer_internal(&d, &request, 0xa000, 4, !output, &actual) == (expected >= 0));
        assert(actual == expected && d.ep0_quarantined == (expected < 0));
        assert(!!(ring[first+1].control & XHCI_TRB_ISP) == !output);
        assert(!!(ring[wrap ? 0 : 2].control & XHCI_STATUS_DIR_IN) == !!output);
        assert(!(ring[first+1].control & XHCI_TRB_IOC));
        assert(ring[wrap ? 0 : 2].control & XHCI_TRB_IOC);
        assert(!freed && d.ep0_ring == ring && d.descriptor_buffer == buffer);
        if (expected < 0) {
            uint64_t enqueue = d.ep0_enqueue;
            assert(!control_transfer(&d, &request, 0xa000, 4, true));
            uint8_t wire[8] = {0xc0,7,0,0,0,0,4,0}, output[4];
            buffer[0] = 0x5a;
            assert(wifi_read_identity(&d, wire, output) == -1);
            assert(buffer[0] == 0x5a && d.ep0_enqueue == enqueue && !freed);
        }
    }
    puts("xhci-strict-control: PASS full/short/zero, stale events, ring wrap, faults and retained DMA quarantine");
}

static void test_wifi_probe(void) {
    uint8_t fixture[92];
    FILE *file = fopen("tools/fixtures/edimax-7392-a711-usb.bin", "rb");
    assert(file && fread(fixture, 1, sizeof fixture, file) == sizeof fixture); fclose(file);
    assert(mt76_probe_config(fixture+18,74));
    for (unsigned n = 0; n < 74; ++n) assert(!mt76_probe_config(fixture+18,n));
    for (unsigned n = 9; n < 74; ++n) {
        fixture[18+n] ^= 1; assert(!mt76_probe_config(fixture+18,74)); fixture[18+n] ^= 1;
    }
    for (unsigned fault = 0; fault < 11; ++fault) {
        reset_test();
        struct xhci_trb ring[64] = {0}, events[64] = {0};
        uint8_t buffer[4096] = {0}; memcpy(buffer, fixture+18, 74);
        struct xhci_device d = {.slot_id = 1, .vendor_id = 0x7392, .product_id = 0xa711,
            .device_bcd = 0x0100, .speed = 3, .ep0_max_packet = 64, .configuration_count = 1,
            .ep0_ring = ring, .ep0_ring_phys = 0x9000, .ep0_cycle = true,
            .descriptor_buffer = buffer, .descriptor_buffer_phys = (uintptr_t)buffer};
        xhci.db_base = 0x1000; xhci.rt_base = 0x2000; xhci.event_ring = events; xhci.event_cycle = true;
        events[0] = control_event(0x9020, 1, 0); events[1] = control_event(0x9050, 1, 0);
        if (fault == 9) {
            events[1] = control_event(0x9040, 13, 1);
            events[2] = control_event(0x9050, 1, 0);
        }
        if (fault == 10) events[0] = control_event(0x9010, 6, 0);
        wifi_test_device = &d; wifi_doorbells = 0; xhci_set_wifi_probe_enabled(fault != 1);
        if (fault == 2) d.vendor_id++;
        if (fault == 3) d.product_id++;
        if (fault == 4) d.device_bcd++;
        if (fault == 5) d.speed = 1;
        if (fault == 6) buffer[18+2] ^= 1;
        if (fault == 7) d.ep0_max_packet = 8;
        if (fault == 8) d.configuration_count = 2;
        assert(probe_wifi(&d,74) == (fault == 0 || fault >= 9));
        assert(wifi_doorbells == (fault == 0 || fault == 9 ? 2u : fault == 10 ? 1u : 0u));
        assert(d.ep0_quarantined == (fault == 10));
        if (fault) assert(!d.wifi_identity.asic && !d.wifi_identity.mac);
        assert(d.wifi_probe_ok == (fault == 0));
        if (!fault) {
            assert(d.wifi_identity.asic == 0x76100001 && d.wifi_identity.mac == 0x76100001);
            assert(!d.network && !d.configured);
            assert(ring[0].parameter_low == 0x000007c0 && ring[0].parameter_high == 0x00040000);
            assert(ring[3].parameter_high == 0x00041000);
        }
        wifi_test_device = NULL;
    }
    assert(!xhci_wifi_identity_ready());
    xhci.devices[0] = (struct xhci_device){.present=true,.wifi_probe_attempted=true,.wifi_probe_ok=true};
    assert(xhci_wifi_identity_ready());
    xhci.devices[0].ep0_quarantined=true; assert(!xhci_wifi_identity_ready());
    xhci.devices[0].ep0_quarantined=false;
    xhci.devices[1] = (struct xhci_device){.present=true,.wifi_probe_attempted=true};
    assert(!xhci_wifi_identity_ready());
    xhci_set_wifi_probe_enabled(false);
    puts("xhci-wifi: PASS opt-in, captured profile refusals and two read-only register requests");
}

static void test_wifi_write_guard(void) {
    for (unsigned fault=0;fault<10;++fault) {
        reset_test();
        uint8_t buffer[4096]={0}; struct xhci_trb ring[64]={0},events[64]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_calibration_active=true,
            .wifi_identity={0x76100002,0x76502000},.wifi_efuse_control_valid=true,.wifi_efuse_control=0x80003fc0,
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,
            .descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,1,0);
        uint32_t address=0x24,value=0xc0103f00;
        if(fault==1) address=0x28;
        if(fault==2) value|=0x40; /* Never programming/physical mode. */
        if(fault==3) value=0xc2003f00; /* Address outside logical EEPROM. */
        if(fault==4) value|=1u<<16; /* Unaligned block. */
        if(fault==5) d.wifi_calibration_active=false;
        if(fault==6) d.wifi_efuse_control_valid=false;
        if(fault==7) d.wifi_identity.mac++;
        if(fault==8) value^=0x100; /* Cannot change timing fields. */
        if(fault==9) events[0]=control_event(0x9010,6,0);
        assert(wifi_calibration_write(&d,address,value)==(fault==0));
        assert(d.ep0_enqueue==(fault==0||fault==9?3u:0u));
        assert(d.ep0_quarantined==(fault==9));
        if(fault==0) {
            assert(ring[0].parameter_low==0x00000640 && ring[0].parameter_high==0x00040024);
            assert(buffer[0]==0 && buffer[1]==0x3f && buffer[2]==0x10 && buffer[3]==0xc0);
            assert(!wifi_calibration_write(&d,address,value) && d.ep0_enqueue==3); /* Fresh read required. */
        }
    }
    puts("xhci-wifi-write: PASS logical mode/address/timing/identity/ownership gates and error quarantine");
}

static void test_wifi_firmware_configuration(void) {
    for(unsigned failure=0;failure<4;failure++) {
        reset_test();uint8_t input[4096]={0},output[4096]={0};struct xhci_trb ring[64]={0},commands[64]={0},events[64]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.input_context=input,.input_context_phys=(uintptr_t)input,.output_context=output};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        events[0]=control_event(0x9010,1,0);
        events[1]=(struct xhci_trb){.parameter_low=failure==1?0xb000:0xa000,.status=(failure==2?6u:1u)<<24,
            .control=(XHCI_TRB_TYPE_COMMAND_COMPLETION<<10)|(1u<<24)|1};
        if(failure==3)d.wifi_identity.mac++;
        assert(configure_wifi_firmware(&d)==!failure);
        assert(d.firmware.configured==!failure && !freed);
        if(failure<3){assert(!(d.firmware.buffer_phys&16383));assert(d.firmware.buffer_phys>=d.firmware.allocation);
            assert(d.firmware.buffer_phys+16384<=d.firmware.allocation+8*4096);
            assert(context_read32(input,0,1)==(1u|(1u<<16)));
            assert(context_read32(input,17,1)==((3u<<1)|(2u<<3)|(512u<<16)));
        }
        assert(firmware_command_failed==(failure==1||failure==2));
        if(failure==3)assert(!d.ep0_enqueue && !pool_used);
    }
    puts("xhci-firmware-config: PASS dedicated EP08 context, aligned retained DMA, exact command pointer, failed command quarantine and identity gate");
}

static void test_wifi_firmware_stop(void) {
    for(unsigned fault=0;fault<9;fault++) {
        reset_test();struct xhci_trb ring[64]={0},commands[64]={0},events[64]={0};
        uint8_t output[4096]={0},buffer[4096]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.ep0_quarantined=true,
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.descriptor_buffer=buffer,.output_context=output};
        d->firmware=(struct xhci_wifi_firmware){.active=true,.configured=true,.control_timeout=true,
            .control_setup=0x9000,.control_data=0x9010,.control_status=0x9020};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;
        /* Model the output context returned by a completed Stop Endpoint. */
        context_write32(output,1,0,3);context_write32(output,1,2,0x9011);
        events[0]=control_event(0x9010,26,4);
        events[1]=(struct xhci_trb){.parameter_low=0xdead,.status=1u<<24,
            .control=(33u<<10)|(1u<<24)|1}; /* Stale successful command. */
        events[2]=(struct xhci_trb){.parameter_low=0xa000,.status=(fault==1?5u:1u)<<24,
            .control=(33u<<10)|((fault==2?2u:1u)<<24)|1};
        if(fault==3)events[2]=(struct xhci_trb){0};
        if(fault==4)d->firmware.control_timeout=false;
        if(fault==5)d->firmware.active=false;
        if(fault==6)d->ep0_quarantined=false;
        if(fault==7)firmware_command_failed=true;
        if(fault==8)strict_control.device=d;
        wifi_fw_stop_timeout(d);
        bool attempted=fault<4,ok=fault==0;
        assert(d->firmware.stop_attempted==attempted && d->firmware.stop_ok==ok);
        assert(xhci.command_enqueue==(attempted?1u:0u));
        assert(firmware_command_failed==((fault>=1 && fault<=3)||fault==7));
        if(attempted) {
            assert(commands[0].control==((15u<<10)|(1u<<16)|(1u<<24)|1));
            assert(!commands[0].parameter_low && !commands[0].parameter_high && !commands[0].status);
            assert(d->firmware.stop_event_cc==26 && d->firmware.stop_event_pointer==0x9010);
            assert(d->firmware.stop_event_residue==4 && d->ep0_quarantined);
        }
        assert(d->firmware.stop_state==(ok?3u:0u) && d->firmware.stop_dequeue==(ok?0x9011u:0u));
        wifi_fw_stop_timeout(d);assert(xhci.command_enqueue==(attempted?1u:0u));
        assert(!d->ep0_enqueue && !freed && d->ep0_ring==ring && d->descriptor_buffer==buffer);
        strict_control.device=0;
    }
    puts("xhci-firmware-stop: PASS exact stop command, stale/wrong-slot/error/timeout, refusal, retained DMA and no restart");
}

static void test_wifi_firmware_control_guards(void) {
    for(unsigned fault=0;fault<7;fault++) {
        reset_test();struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.configured=true,.active=true,
            .preparation={.mac_startup=fault!=2,.attempted=fault!=3},.reset_seen=fault==4,.mac_startup_written=fault==5};
        d.ep0_max_packet=64;
        wifi_mac_startup=fault!=1;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==6?6:1,0);
        assert(wifi_fw_write(&d,0x1004,0x2c)==!fault);
        if(!fault)assert(d.firmware.control_idt && (ring[1].control & XHCI_TRB_IDT) && ring[1].parameter_low==0x2c && !ring[1].parameter_high);
        assert(d.ep0_enqueue==(fault==0||fault==6?3u:0u));
        if(!fault)assert(d.firmware.mac_startup_written && !wifi_fw_write(&d,0x1004,0x2c));
        assert(!freed);
    }

    for(unsigned fault=0;fault<7;fault++) {
        reset_test();struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.configured=true,.active=true,
            .wlan_valid=fault!=1,.wlan_before=fault==2?0xffffffffu:fault==3?0x5a120004:0x5a120000,.wlan_written=fault==4};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==6?6:1,0);
        assert(wifi_fw_write(&d,0x80,fault==5?0x5b120003:0x5a120003)==!fault);
        assert(d.ep0_enqueue==(fault==0||fault==6?3u:0u));
        if(!fault) {
            assert(ring[0].parameter_high==0x00040080 && buffer[0]==3 && buffer[3]==0x5a);
            assert(!wifi_fw_write(&d,0x80,0x5a120003) && d.ep0_enqueue==3);
        }
        assert(!freed);
    }

    for(unsigned fault=0;fault<5;fault++) {
        reset_test();struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.configured=true,.active=true};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        if(fault==3)d.firmware.active=false;
        assert(wifi_fw_write(&d,fault==2?0x1008:0x1004,fault==1?0x2c:0x20)==!fault);
        assert(d.ep0_enqueue==(fault==0||fault==4?3u:0u));
        assert(d.firmware.failed==(fault==4));
        if(!fault)assert(buffer[0]==0x20 && ring[0].parameter_high==0x00041004);
    }
    for(unsigned fault=0;fault<8;fault++) {
        reset_test();test_time=0;test_time_step=fault==7?0:1000;complete_control=fault==6;complete_at=250000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t output[4096]={0};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.output_context=output};
        context_write32(output,1,0,1);context_write32(output,1,2,0x9031);
        d.firmware=(struct xhci_wifi_firmware){.configured=true,.active=true};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(fault==1?0x9000:fault>=4?0x9990:0x9010,
            fault==1||fault==2||fault>=4?6:1,fault==3?1:0);
        if(fault==5)events[1]=control_event(0x9010,1,0);
        bool ok=fault==0||fault==5||fault==6;
        assert(wifi_fw_reset(&d)==ok);
        assert(!wifi_fw_reset(&d) && d.ep0_enqueue==2 && d.firmware.control_count==1);
        assert(ring[0].parameter_low==0x00010140 && ring[0].parameter_high==0);
        assert(d.firmware.control_request==1 && d.firmware.control_value==1);
        assert(d.firmware.control_length==0 && d.firmware.control_index==0);
        assert(d.firmware.control_ok==ok && d.firmware.control_actual==(ok?0:-1));
        assert(d.firmware.control_timeout==(fault==4||fault==7));
        assert(d.firmware.control_cc==(fault==4||fault==7?0u:fault==1||fault==2?6u:1u));
        assert(d.firmware.control_stage==(fault==4||fault==7?0u:fault==1?1u:3u));
        assert(d.firmware.control_residue==(fault==3?1u:0u));
        assert(d.ep0_quarantined==!ok && !freed);
        assert(d.firmware.control_ep_state==1 && d.firmware.control_dequeue==0x9031);
        assert(context_read32(output,1,2)==0x9031);
        if(fault==4)assert(d.firmware.control_elapsed_us>=300000);
        if(fault==6)assert(d.firmware.control_elapsed_us>=249000 && d.firmware.control_elapsed_us<300000);
        if(fault==7)assert(d.firmware.control_elapsed_us==0);
        test_time_step=1000;complete_control=false;
    }
    puts("xhci-firmware-control: PASS disabled-MAC-only write, register/ownership refusals, I/O quarantine and one-shot reset");
}

static void test_wifi_firmware_transport(void) {
    for(unsigned fault=0;fault<6;fault++) {
        reset_test();
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t scratch[32]={0},dma[32]={0},image[104]={67,0,0,0,5};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.active=true,.configured=true,.phase=2,.scratch=scratch,.buffer=dma,
            .buffer_phys=0x10000,.image=image,.image_size=sizeof image,.ep={.ring=ring,.ring_phys=0x9000,.cycle=true,.dci=16}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9000,1,0);events[0].control=(32u<<10)|(1u<<24)|(16u<<16)|1;
        if(fault==1)events[0].status=13u<<24;
        if(fault==2)events[0].status=(1u<<24)|1;
        if(fault==3)events[0].control|=4;
        if(fault==4)events[0]=(struct xhci_trb){0};
        if(fault==5)f->active=false;
        assert(wifi_fw_bulk(d,scratch,12)==(fault?-1:12));
        assert(f->failed==(fault>0 && fault<5));assert(freed==0);
        if(fault && fault<5){uint64_t enqueue=f->ep.enqueue;assert(wifi_fw_bulk(d,scratch,12)==-1 && f->ep.enqueue==enqueue);}
        if(!fault){assert(f->phase==3 && !memcmp(dma,scratch,12));assert(ring[0].parameter_low==0x10000 && ring[0].status==12);}
    }
    for(unsigned fault=0;fault<4;fault++) {
        reset_test();struct xhci_trb ring[64]={0},events[64]={0};uint8_t image[104]={67,0,0,0,5};
        struct xhci_device d={.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true};
        d.firmware=(struct xhci_wifi_firmware){.configured=true,.active=true,.image=image,.image_size=sizeof image,
            .preparation={.prepared=true},.upload={.attempted=true}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9010,1,0);events[1]=control_event(0x9030,fault==1?6:1,0);
        if(fault==2)d.firmware.active=false;
        assert(wifi_fw_fce(&d,fault==3?0x238:0x230,64)==!fault);
        if(!fault){assert(ring[0].parameter_low==0x00404240 && ring[0].parameter_high==0x230);
            assert(ring[2].parameter_low==0x4240 && ring[2].parameter_high==0x232 && d.firmware.phase==1);}
        if(fault==1)assert(d.ep0_quarantined && d.firmware.failed && d.firmware.phase==0);
        if(fault>=2)assert(!d.ep0_enqueue);
    }
    puts("xhci-firmware: PASS dedicated bulk completion/residue/ED/timeout quarantine, ownership and exact FCE halfword requests");
}

static void test_control_publication(void) {
    for(unsigned cycle=0;cycle<2;cycle++) for(unsigned first=0;first<63;first++)
    for(unsigned kind=0;kind<3;kind++) {
        reset_test();
        struct xhci_trb ring[64]={0}, events[64]={0};
        struct xhci_device d={.slot_id=1,.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_enqueue=first,.ep0_cycle=cycle};
        xhci.db_base=0x1000; xhci.rt_base=0x2000;
        xhci.event_ring=events; xhci.event_cycle=true;
        unsigned last=(first+(kind?2:1))%63;
        events[0]=control_event(0x9000+last*16,1,0);
        struct usb_setup_packet req={.request_type=kind==2?0xc0:0x40,.request=6,.length=kind?4:0};
        int actual=-1;
        assert(control_transfer_internal(&d,&req,0xa000,req.length,kind==2,&actual));
        assert(actual==req.length);
        assert((ring[first].control&1)==cycle);
    }
}

static void test_firmware_immediate_data(void) {
    for(unsigned fault=0;fault<12;fault++) {
        uint8_t bytes[8]={0x12,0x34,0x80,0xff,0xaa,0xbb,0xcc,0xdd};
        struct xhci_device d={.descriptor_buffer=bytes,.descriptor_buffer_phys=0x12340000,.ep0_max_packet=64};
        d.firmware.active=true; d.firmware.configured=true;
        struct usb_setup_packet req={.request_type=0x40,.request=6,.length=4};
        uint64_t phys=d.descriptor_buffer_phys; unsigned length=4; bool in=false;
        if(fault==1)d.firmware.active=false;
        if(fault==2)d.firmware.configured=false;
        if(fault==3)d.descriptor_buffer=NULL;
        if(fault==4)phys++;
        if(fault==5)d.ep0_max_packet=4;
        if(fault==6)in=true;
        if(fault==7)length=64;
        if(fault==8)req.length=0;
        if(fault==9)req.request_type=0xc0;
        if(fault==10)req.request=1;
        if(fault==11)phys=d.descriptor_buffer_phys=0;
        struct xhci_trb t={.parameter_low=0x12340000,.parameter_high=1,.status=4,.control=XHCI_TRB_TYPE_DATA_STAGE<<10};
        struct xhci_trb before=t;
        assert(firmware_immediate_data(&d,&req,phys,length,in,&t)==!fault);
        if(fault)assert(!memcmp(&t,&before,sizeof(t)));
        else assert(t.parameter_low==0xff803412 && !t.parameter_high && t.status==4 && t.control==((XHCI_TRB_TYPE_DATA_STAGE<<10)|XHCI_TRB_IDT));
    }
}

static void test_wifi_recovery(void) {
    for(unsigned in=0;in<2;in++) for(unsigned fault=0;fault<12;fault++) {
        reset_test();struct xhci_trb ring[64]={0},commands[64]={0},events[64]={0};
        uint8_t output[4096]={0},buffer[4096]={1};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
          .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.ep0_quarantined=true,.ep0_max_packet=64,
          .descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer,.output_context=output};
        d->firmware=(struct xhci_wifi_firmware){.active=true,.configured=true,.control_timeout=true,.reset_seen=true,
          .stop_ok=true,.stop_state=3,.stop_event_cc=26,.stop_event_residue=4,.stop_event_pointer=0x9010,
          .control_data=0x9010,.stop_dequeue=0x9011,.preparation={.prepared=in},.upload={.attempted=in},.phase=in?4:0};
        context_write32(output,1,0,3);context_write32(output,1,2,0x9011);
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;
        recovery_device=d;recovery_fault=fault;recovery_integrated=false;recovery_delay_ok=fault!=10;
        if(fault==5)d->firmware.stop_state=1;
        if(fault==6)d->firmware.retry_mask=in?2:1;
        if(fault==7)d->firmware.reset_seen=false,d->firmware.phase=0;
        if(fault==8)d->wifi_identity.asic++;
        if(fault==9)fail_after=pool_used+1;
        if(fault==11)d->firmware.stop_event_pointer=0xdead;
        struct usb_setup_packet req={.request_type=in?0xc0:0x40,.request=in?7:6,.index=in?0x730:0x800,.length=4};
        uint64_t oldbuffer=d->descriptor_buffer_phys;
        assert(wifi_fw_recover(d,&req,oldbuffer,4,in)==!fault);
        assert(d->ep0_quarantined==!!fault && !freed);
        if(!fault) {
            assert(d->ep0_ring!=ring && d->descriptor_buffer!=buffer && d->ep0_cycle && !d->ep0_enqueue);
            assert(d->descriptor_buffer[0]==(in?0:1));
            assert(d->firmware.retired_ring[0]==0x9000 && d->firmware.retired_buffer[0]==oldbuffer);
            assert(!wifi_fw_recover(d,&req,d->descriptor_buffer_phys,4,in));
        }
        recovery_device=NULL;
    }
    for(unsigned in=0;in<2;in++) for(unsigned fault=0;fault<=11;fault+=11) {
        reset_test();struct xhci_trb ring[64]={0},commands[64]={0},events[64]={0};
        uint8_t output[4096]={0},buffer[4096]={1};struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000},
            .ep0_ring=ring,.ep0_ring_phys=0x9000,.ep0_cycle=true,.ep0_max_packet=64,.descriptor_buffer=buffer,
            .descriptor_buffer_phys=(uintptr_t)buffer,.output_context=output};
        d->firmware=(struct xhci_wifi_firmware){.active=true,.configured=true,.reset_seen=true,.preparation={.prepared=in},.upload={.attempted=in},.phase=in?4:0};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        xhci.event_ring=events;xhci.event_cycle=true;
        recovery_device=d;recovery_fault=fault;recovery_integrated=true;recovery_calls=0;recovery_delay_ok=true;
        uint32_t value=0;
        assert((in?wifi_fw_read(d,0x730,&value):wifi_fw_write(d,0x800,1))==!fault);
        if(in && !fault)assert(value==1);
        assert(recovery_calls==2 && d->firmware.recovery_success==1 && d->firmware.control_count==2);
        assert(d->ep0_quarantined==!!fault && !freed);
        recovery_device=NULL;recovery_integrated=false;
    }
    recovery_delay_ok=true;
    puts("xhci-wifi-recovery: PASS fresh DMA, exact dequeue, faults, bounded retry and stale-event exclusion");
}

static void test_wifi_radio_binding(void) {
    for(unsigned release=0;release<2;release++)for(unsigned fault=0;fault<10;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset_active=true,.radio_reset_writes=release,
            .radio_reset={.attempted=true,.reset_attempted=true,.writes=release+1,.delay_us=release?200000:0}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==8?6:1,0);
        uint32_t address=0x1004,value=release?0:3;
        if(fault==1)d.firmware.radio_reset_active=false;
        if(fault==2)d.firmware.ready=false;
        if(fault==3)value=0x0c;
        if(fault==4)address=0x1000;
        if(fault==5)d.firmware.radio_reset.writes++;
        if(fault==6)d.firmware.mcu.configured=true;
        if(fault==7)d.firmware.radio_reset.reset_attempted=false;
        if(fault==9) { d.firmware.radio_reset_writes=1;d.firmware.radio_reset.writes=2;
            d.firmware.radio_reset.delay_us=199999;value=0; }
        assert(wifi_radio_write(&d,address,value)==!fault);
        assert(!freed);
        if(!fault) {
            assert(buffer[0]==value && d.firmware.radio_reset_writes==release+1);
            assert(!wifi_radio_write(&d,address,value));
        }
        if(fault && fault!=8)assert(d.ep0_enqueue==0);
        if(fault==8)assert(d.ep0_quarantined && d.firmware.failed);
        uint32_t ignored;
        assert(!wifi_radio_read(&d,0x80,&ignored));
    }
    puts("xhci-radio-reset: PASS ordered restricted writes, ownership/phase gates and transfer-failure quarantine");
}

static void test_wifi_mcu_queue_order(void) {
    for(unsigned fault=0;fault<4;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<3;n++) {
            events[n]=control_event(n==2?0xb000:0x9000+16*n,n==2?13:1,n==2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n==2?11u:16u)<<16)|1;
        }
        if(fault)events[fault-1].status=6u<<24;
        int queued,sent;int got=wifi_mcu_read_identity(d,&queued,&sent);
        assert(queued==(fault==1?-1:16));
        assert(sent==(fault==1 || fault==2?-1:16));
        assert(got==(fault?-1:16));assert(!freed);
        assert(f->ep.enqueue==(fault==1?1u:2u));
        assert(f->mcu.response.enqueue==(fault==1 || fault==2?0u:1u));
        if(fault)assert(f->failed && f->mcu.failed && d->ep0_quarantined);
        if(fault==1)assert(tx[2]==0x10 && tx[4]==1 && tx[8]==1);
        else assert(tx[2]==0xa1 && tx[4]==0 && tx[5]==0x10 && tx[6]==0x41);
    }
    puts("xhci-mcu-queue: PASS ordered no-reply selection/read/response and stop on each failure");
}

static void test_wifi_mcu_write_probe(void) {
    for(unsigned fault=0;fault<10;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<2;n++) {
            events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1016:0);
            events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.write_attempted=true;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        int sent;int got=wifi_mcu_write_disabled(d,&sent);
        assert(got==(fault?-1:8));assert(!freed);
        if(fault>=1 && fault<=6)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            const uint8_t expected[16]={8,0,0xc2,0x50,4,0x10,0x41,0,0,0,0,0,0,0,0,0};
            assert(!memcmp(tx,expected,16));
            assert(f->ep.enqueue==1 && f->mcu.write_attempted);
            assert(wifi_mcu_write_disabled(d,&sent)==-1);
            assert(f->ep.enqueue==1);
            if(fault)assert(f->failed && f->mcu.failed && d->ep0_quarantined);
        }
    }
    puts("xhci-mcu-write: PASS fixed disabled-MAC packet, prerequisites, one attempt, stale events and failure quarantine");
}

static void test_wifi_mcu_pair_probe(void) {
    for(unsigned fault=0;fault<12;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,.write_attempted=true,.write_validated=true,
            .buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<2;n++) {
            events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1016:0);
            events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.pair_attempted=true;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.write_attempted=false;
        int sent;int got=wifi_mcu_write_pair(d,&sent,false);
        assert(got==(fault?-1:8));assert(!freed);
        if((fault>=1 && fault<=6) || fault>=10)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            const uint8_t expected[24]={16,0,0xc3,0x50,4,0x10,0x41,0,0,0,0,0,4,0x10,0x41,0,0,0,0,0,0,0,0,0};
            assert(!memcmp(tx,expected,24));
            assert(f->ep.enqueue==1 && f->mcu.pair_attempted);
            assert(wifi_mcu_write_pair(d,&sent,false)==-1);
            assert(f->ep.enqueue==1);
            if(fault)assert(f->failed && f->mcu.failed && d->ep0_quarantined);
        }
    }
    puts("xhci-mcu-pair: PASS fixed two-pair disabled-MAC packet, prerequisites, one attempt, stale events and failure quarantine");
}

static void test_wifi_mcu_timing_probe(void) {
    for(unsigned fault=0;fault<13;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,.write_attempted=true,.write_validated=true,.timing_active=true,
            .buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<2;n++) {
            events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1016:0);
            events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.pair_attempted=true;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.write_attempted=false;
        if(fault==12)f->mcu.timing_active=false;
        int sent;int got=wifi_mcu_write_pair(d,&sent,true);
        assert(got==(fault?-1:8));assert(!freed);
        if((fault>=1 && fault<=6) || fault>=10)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            const uint8_t expected[24]={16,0,0xc3,0x50,0,0x11,0x41,0,0x10,0x10,0xa4,0x33,4,0x11,0x41,0,9,2,0,0,0,0,0,0};
            assert(!memcmp(tx,expected,24));
            assert(f->ep.enqueue==1 && f->mcu.pair_attempted);
            assert(wifi_mcu_write_pair(d,&sent,true)==-1);
            assert(f->ep.enqueue==1);
            if(fault)assert(f->failed && f->mcu.failed && d->ep0_quarantined);
        }
    }
    puts("xhci-mcu-timing: PASS fixed distinct timing packet, prerequisites, one attempt, stale events and failure quarantine");
}

static void test_wifi_timing_reads(void) {
    for(unsigned reg=0;reg<2;reg++)for(unsigned fault=0;fault<7;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .mcu={.timing_active=true,.write_validated=true}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==6?6:1,0);
        uint32_t address=reg?0x1104:0x1100,value;
        if(fault==1)d.firmware.mcu.timing_active=false;
        if(fault==2)d.firmware.mcu.write_validated=false;
        if(fault==3)d.firmware.mcu.pending=true;
        if(fault==4)address=0x1004;
        if(fault==5)d.firmware.active=false;
        assert(wifi_mcu_timing_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault) {
            assert(d.firmware.control_type==0xc0);
            assert(d.firmware.control_request==7);
            assert(d.firmware.control_index==address && d.firmware.control_length==4);
        }
        if(fault && fault!=6)assert(d.ep0_enqueue==0);
        if(fault==6)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-timing-read: PASS exact offsets/read request, phase/ownership gates and transfer failure quarantine");
}

static void test_wifi_timing_exchange_refusal(void) {
    reset_test();
    struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
        .wifi_identity={0x76100002,0x76502000}};
    d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
        .radio_reset={.prepared=true},.mcu={.timing_active=true,.configured=true,
        .identity_validated=true,.write_validated=true,.write_attempted=true}};
    uint8_t packet[24]={16,0,0xc3,0x50,0,0x11,0x41,0,0x10,0x10,0xa4,0x33,
        4,0x11,0x41,0,9,2,0,0,0,0,0,0},reply[1024];
    for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
        packet[i]^=1u<<bit;
        assert(wifi_timing_exchange(&d,packet,24,reply,1024)==-1);
        assert(!d.firmware.mcu.pair_attempted);
        packet[i]^=1u<<bit;
    }
    assert(wifi_timing_exchange(&d,packet,23,reply,1024)==-1);
    assert(wifi_timing_exchange(&d,packet,24,reply,1023)==-1);
    assert(wifi_timing_exchange(&d,NULL,24,reply,1024)==-1);
    assert(wifi_timing_exchange(&d,packet,24,NULL,1024)==-1);
    assert(!d.firmware.mcu.pair_attempted && !freed);
    puts("xhci-timing-exchange: PASS exact packet enforcement and no submission on malformed input");
}

static void test_wifi_mac_binding(void) {
    for(unsigned fault=0;fault<19;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[24],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.mac_active=true,.timing={.complete=true,.ack_validated=true},
            .mac={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.mac_active=false;
        if(fault==13)f->mcu.mac.attempted=false;
        if(fault==14)f->mcu.mac.batches_attempted=2;
        if(fault==15)f->mcu.mac.batches_completed=1;
        if(fault==16)f->mcu.mac.ack_mask=1;
        if(fault==17)f->mcu.mac_submitted=2;
        if(fault==18)f->mcu.configured=false;
        assert(mt76_mcu_mac_packet(0,packet,sizeof packet)==24);
        assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.mac_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,24) && f->mcu.mac_submitted==1);
            assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==-1);
            mt76_mcu_mac_packet(1,packet,24);
            f->mcu.mac.batches_attempted=2;
            assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.mac.ack_mask=1;
            assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.mac.batches_completed=1;
            for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_mac_exchange(d,packet,23,reply,sizeof reply)==-1);
            assert(wifi_mac_exchange(d,packet,24,reply,1023)==-1);
            assert(wifi_mac_exchange(d,NULL,24,reply,sizeof reply)==-1);
            assert(wifi_mac_exchange(d,packet,24,NULL,sizeof reply)==-1);
            assert(f->mcu.mac_submitted==1 && f->ep.enqueue==1);
            assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==16);
            assert(!memcmp(tx,packet,24) && f->mcu.mac_submitted==2);
            assert(wifi_mac_exchange(d,packet,24,reply,sizeof reply)==-1);
        }
    }
    puts("xhci-mac-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_mac_reads(void) {
    const uint32_t addresses[]={0x1004,0x1408,0x140c,0x1400,0x1018};
    for(unsigned reg=0;reg<5;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.mac_active=true,
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.mac_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_mac_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-mac-read: PASS exact read scope and failure quarantine");
}

static void test_wifi_format_binding(void) {
    for(unsigned fault=0;fault<22;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[24],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.format_active=true,.mac={.complete=true,.ack_mask=3,.batches_completed=2},.timing={.complete=true,.ack_validated=true},
            .format={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.format_active=false;
        if(fault==13)f->mcu.format.attempted=false;
        if(fault==14)f->mcu.format.batches_attempted=2;
        if(fault==15)f->mcu.format.batches_completed=1;
        if(fault==16)f->mcu.format.ack_mask=1;
        if(fault==17)f->mcu.format_submitted=2;
        if(fault==18)f->mcu.configured=false;
        if(fault==19)f->mcu.mac.complete=false;
        if(fault==20)f->mcu.mac.ack_mask=1;
        if(fault==21)f->mcu.mac.batches_completed=1;
        assert(mt76_mcu_format_packet(0,packet,sizeof packet)==24);
        assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.format_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,24) && f->mcu.format_submitted==1);
            assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==-1);
            mt76_mcu_format_packet(1,packet,24);
            f->mcu.format.batches_attempted=2;
            assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.format.ack_mask=1;
            assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.format.batches_completed=1;
            for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_format_exchange(d,packet,23,reply,sizeof reply)==-1);
            assert(wifi_format_exchange(d,packet,24,reply,1023)==-1);
            assert(wifi_format_exchange(d,NULL,24,reply,sizeof reply)==-1);
            assert(wifi_format_exchange(d,packet,24,NULL,sizeof reply)==-1);
            assert(f->mcu.format_submitted==1 && f->ep.enqueue==1);
            assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==16);
            assert(!memcmp(tx,packet,24) && f->mcu.format_submitted==2);
            assert(wifi_format_exchange(d,packet,24,reply,sizeof reply)==-1);
        }
    }
    puts("xhci-format-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_format_reads(void) {
    const uint32_t addresses[]={0x1004,0x0250,0x0260,0x1410,0x150c};
    for(unsigned reg=0;reg<5;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.format_active=true,.mac={.complete=true,.ack_mask=3,.batches_completed=2},
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.format_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_format_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-format-read: PASS exact read scope and failure quarantine");
}

static void test_wifi_bbp_setup_binding(void) {
    for(unsigned fault=0;fault<26;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[24],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.bbp_setup_active=true,.format={.complete=true,.ack_mask=3},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},.timing={.complete=true,.ack_validated=true},
            .bbp_setup={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.bbp_setup_active=false;
        if(fault==13)f->mcu.bbp_setup.attempted=false;
        if(fault==14)f->mcu.bbp_setup.batches_attempted=2;
        if(fault==15)f->mcu.bbp_setup.batches_completed=1;
        if(fault==16)f->mcu.bbp_setup.ack_mask=1;
        if(fault==17)f->mcu.bbp_setup_submitted=2;
        if(fault==18)f->mcu.configured=false;
        if(fault==19)f->mcu.mac.complete=false;
        if(fault==20)f->mcu.mac.ack_mask=1;
        if(fault==21)f->mcu.mac.batches_completed=1;
        if(fault==22)f->mcu.format.complete=false;
        if(fault==23)f->mcu.format.ack_mask=1;
        if(fault==24)f->mcu.bbp.complete=false;
        if(fault==25)f->mcu.bbp.core=0;
        assert(mt76_mcu_bbp_packet(0,packet,sizeof packet)==24);
        assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.bbp_setup_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_setup_submitted==1);
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==-1);
            mt76_mcu_bbp_packet(1,packet,24);
            f->mcu.bbp_setup.batches_attempted=2;
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_setup.ack_mask=1;
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_setup.batches_completed=1;
            for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_bbp_setup_exchange(d,packet,23,reply,sizeof reply)==-1);
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,1023)==-1);
            assert(wifi_bbp_setup_exchange(d,NULL,24,reply,sizeof reply)==-1);
            assert(wifi_bbp_setup_exchange(d,packet,24,NULL,sizeof reply)==-1);
            assert(f->mcu.bbp_setup_submitted==1 && f->ep.enqueue==1);
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==16);
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_setup_submitted==2);
            assert(wifi_bbp_setup_exchange(d,packet,24,reply,sizeof reply)==-1);
        }
    }
    puts("xhci-bbp-core-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_bbp_setup_reads(void) {
    const uint32_t addresses[]={0x1004,0x2004,0x2010,0x2060,0x2080,0x2000};
    for(unsigned reg=0;reg<6;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.bbp_setup_active=true,.format={.complete=true,.ack_mask=3},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.bbp_setup_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_bbp_setup_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-bbp-core-read: PASS exact read scope and failure quarantine");
}

static void test_wifi_bbp_front_binding(void) {
    for(unsigned fault=0;fault<29;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[24],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.bbp_front_active=true,.format={.complete=true,.ack_mask=3},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},.timing={.complete=true,.ack_validated=true},
            .bbp_front={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.bbp_front_active=false;
        if(fault==13)f->mcu.bbp_front.attempted=false;
        if(fault==14)f->mcu.bbp_front.batches_attempted=2;
        if(fault==15)f->mcu.bbp_front.batches_completed=1;
        if(fault==16)f->mcu.bbp_front.ack_mask=1;
        if(fault==17)f->mcu.bbp_front_submitted=2;
        if(fault==18)f->mcu.configured=false;
        if(fault==19)f->mcu.mac.complete=false;
        if(fault==20)f->mcu.mac.ack_mask=1;
        if(fault==21)f->mcu.mac.batches_completed=1;
        if(fault==22)f->mcu.format.complete=false;
        if(fault==23)f->mcu.format.ack_mask=1;
        if(fault==24)f->mcu.bbp.complete=false;
        if(fault==25)f->mcu.bbp.core=0;
        if(fault==26)f->mcu.bbp_setup.complete=false;
        if(fault==27)f->mcu.bbp_setup.ack_mask=1;
        if(fault==28)f->mcu.bbp_setup.batches_completed=1;
        assert(mt76_mcu_bbp_front_packet(0,packet,sizeof packet)==24);
        assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.bbp_front_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_front_submitted==1);
            assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==-1);
            mt76_mcu_bbp_front_packet(1,packet,24);
            f->mcu.bbp_front.batches_attempted=2;
            assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_front.ack_mask=1;
            assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_front.batches_completed=1;
            for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_bbp_front_exchange(d,packet,23,reply,sizeof reply)==-1);
            assert(wifi_bbp_front_exchange(d,packet,24,reply,1023)==-1);
            assert(wifi_bbp_front_exchange(d,NULL,24,reply,sizeof reply)==-1);
            assert(wifi_bbp_front_exchange(d,packet,24,NULL,sizeof reply)==-1);
            assert(f->mcu.bbp_front_submitted==1 && f->ep.enqueue==1);
            assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==16);
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_front_submitted==2);
            assert(wifi_bbp_front_exchange(d,packet,24,reply,sizeof reply)==-1);
        }
    }
    puts("xhci-bbp-front-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_bbp_front_reads(void) {
    const uint32_t addresses[]={0x1004,0x20a8,0x20b0,0x212c,0x2300,0x2000};
    for(unsigned reg=0;reg<6;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.bbp_front_active=true,.format={.complete=true,.ack_mask=3},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.bbp_front_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_bbp_front_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-bbp-front-read: PASS exact read scope and failure quarantine");
}

static void test_wifi_bbp_agc_binding(void) {
    for(unsigned fault=0;fault<32;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[24],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.bbp_agc_active=true,.format={.complete=true,.ack_mask=3},.bbp_front={.complete=true,.ack_mask=3,.batches_completed=2},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},.timing={.complete=true,.ack_validated=true},
            .bbp_agc={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.bbp_agc_active=false;
        if(fault==13)f->mcu.bbp_agc.attempted=false;
        if(fault==14)f->mcu.bbp_agc.batches_attempted=2;
        if(fault==15)f->mcu.bbp_agc.batches_completed=1;
        if(fault==16)f->mcu.bbp_agc.ack_mask=1;
        if(fault==17)f->mcu.bbp_agc_submitted=2;
        if(fault==18)f->mcu.configured=false;
        if(fault==19)f->mcu.mac.complete=false;
        if(fault==20)f->mcu.mac.ack_mask=1;
        if(fault==21)f->mcu.mac.batches_completed=1;
        if(fault==22)f->mcu.format.complete=false;
        if(fault==23)f->mcu.format.ack_mask=1;
        if(fault==24)f->mcu.bbp.complete=false;
        if(fault==25)f->mcu.bbp.core=0;
        if(fault==26)f->mcu.bbp_setup.complete=false;
        if(fault==27)f->mcu.bbp_setup.ack_mask=1;
        if(fault==28)f->mcu.bbp_setup.batches_completed=1;
        if(fault==29)f->mcu.bbp_front.complete=false;
        if(fault==30)f->mcu.bbp_front.ack_mask=1;
        if(fault==31)f->mcu.bbp_front.batches_completed=1;
        assert(mt76_mcu_bbp_agc_packet(0,packet,sizeof packet)==24);
        assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.bbp_agc_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_agc_submitted==1);
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==-1);
            mt76_mcu_bbp_agc_packet(1,packet,24);
            f->mcu.bbp_agc.batches_attempted=2;
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_agc.ack_mask=1;
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==-1);
            f->mcu.bbp_agc.batches_completed=1;
            for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_bbp_agc_exchange(d,packet,23,reply,sizeof reply)==-1);
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,1023)==-1);
            assert(wifi_bbp_agc_exchange(d,NULL,24,reply,sizeof reply)==-1);
            assert(wifi_bbp_agc_exchange(d,packet,24,NULL,sizeof reply)==-1);
            assert(f->mcu.bbp_agc_submitted==1 && f->ep.enqueue==1);
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==16);
            assert(!memcmp(tx,packet,24) && f->mcu.bbp_agc_submitted==2);
            assert(wifi_bbp_agc_exchange(d,packet,24,reply,sizeof reply)==-1);
        }
    }
    puts("xhci-bbp-agc-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_bbp_agc_reads(void) {
    const uint32_t addresses[]={0x1004,0x2304,0x2308,0x233c,0x2358,0x2000};
    for(unsigned reg=0;reg<6;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.bbp_agc_active=true,.format={.complete=true,.ack_mask=3},.bbp_front={.complete=true,.ack_mask=3,.batches_completed=2},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.bbp_agc_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_bbp_agc_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-bbp-agc-read: PASS exact read scope and failure quarantine");
}

static void test_wifi_bbp_batch_binding(void) {
    for(unsigned fault=0;fault<32;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[40],reply[1024];
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.buffer=tx,.buffer_phys=0x10000,
            .ep={.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,
            .write_validated=true,.bbp_batch_active=true,.format={.complete=true,.ack_mask=3},.bbp_front={.complete=true,.ack_mask=3,.batches_completed=2},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},.timing={.complete=true,.ack_validated=true},
            .bbp_batch={.attempted=true,.batches_attempted=1},.buffer=rx,.buffer_phys=0x20000,
            .response={.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<4;n++) {
            events[n]=control_event((n%2?0xb000:0x9000)+(n/2)*16,n%2?13:1,n%2?1008:0);
            events[n].control=(32u<<10)|(1u<<24)|((n%2?11u:16u)<<16)|1;
        }
        if(fault==1)f->mcu.identity_validated=false;
        if(fault==2)f->radio_reset.prepared=false;
        if(fault==3)f->mcu.timing.complete=false;
        if(fault==4)f->mcu.pending=true;
        if(fault==5)f->active=false;
        if(fault==6)d->wifi_identity.mac=0;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[1].status=6u<<24;
        if(fault==9)events[1].parameter_low+=16;
        if(fault==10)f->mcu.write_validated=false;
        if(fault==11)f->mcu.timing.ack_validated=false;
        if(fault==12)f->mcu.bbp_batch_active=false;
        if(fault==13)f->mcu.bbp_batch.attempted=false;
        if(fault==14)f->mcu.bbp_batch.batches_attempted=2;
        if(fault==15)f->mcu.bbp_batch.batches_completed=1;
        if(fault==16)f->mcu.bbp_batch.ack_mask=1;
        if(fault==17)f->mcu.bbp_batch_submitted=2;
        if(fault==18)f->mcu.configured=false;
        if(fault==19)f->mcu.mac.complete=false;
        if(fault==20)f->mcu.mac.ack_mask=1;
        if(fault==21)f->mcu.mac.batches_completed=1;
        if(fault==22)f->mcu.format.complete=false;
        if(fault==23)f->mcu.format.ack_mask=1;
        if(fault==24)f->mcu.bbp.complete=false;
        if(fault==25)f->mcu.bbp.core=0;
        if(fault==26)f->mcu.bbp_setup.complete=false;
        if(fault==27)f->mcu.bbp_setup.ack_mask=1;
        if(fault==28)f->mcu.bbp_setup.batches_completed=1;
        if(fault==29)f->mcu.bbp_front.complete=false;
        if(fault==30)f->mcu.bbp_front.ack_mask=1;
        if(fault==31)f->mcu.bbp_front.batches_completed=1;
        assert(mt76_mcu_bbp_batch_packet(0,packet,sizeof packet)==40);
        if(!fault) {
            for(unsigned i=0;i<40;i++)for(unsigned bit=0;bit<8;bit++) {
                packet[i]^=1u<<bit;
                assert(wifi_bbp_batch_exchange(d,packet,40,reply,sizeof reply)==-1);
                packet[i]^=1u<<bit;
            }
            assert(wifi_bbp_batch_exchange(d,packet,39,reply,sizeof reply)==-1);
            assert(wifi_bbp_batch_exchange(d,packet,40,reply,1023)==-1);
            assert(wifi_bbp_batch_exchange(d,NULL,40,reply,sizeof reply)==-1);
            assert(wifi_bbp_batch_exchange(d,packet,40,NULL,sizeof reply)==-1);
            assert(f->ep.enqueue==0 && f->mcu.bbp_batch_submitted==0);
        }
        assert(wifi_bbp_batch_exchange(d,packet,40,reply,sizeof reply)==(fault?-1:16));
        assert(!freed);
        if(fault>=7 && fault<=9)assert(f->mcu.bbp_batch_submitted==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(f->ep.enqueue==0 && f->mcu.response.enqueue==0);
        else {
            assert(!memcmp(tx,packet,40) && f->mcu.bbp_batch_submitted==1);
            assert(wifi_bbp_batch_exchange(d,packet,40,reply,sizeof reply)==-1);
            f->mcu.bbp_batch.batches_attempted=2;f->mcu.bbp_batch.batches_completed=1;f->mcu.bbp_batch.ack_mask=1;
            assert(wifi_bbp_batch_exchange(d,packet,40,reply,sizeof reply)==-1);
            assert(f->ep.enqueue==1 && f->mcu.bbp_batch_submitted==1);
        }
    }
    puts("xhci-bbp-batch-binding: PASS prerequisites, ordered unique batches, exact packets, stale events and quarantine");
}

static void test_wifi_bbp_batch_reads(void) {
    const uint32_t addresses[]={0x1004,0x2304,0x2308,0x233c,0x2358,0x2000};
    for(unsigned reg=0;reg<6;reg++)for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.bbp_batch_active=true,.format={.complete=true,.ack_mask=3},.bbp_front={.complete=true,.ack_mask=3,.batches_completed=2},.bbp_setup={.complete=true,.ack_mask=3,.batches_completed=2},.bbp={.complete=true,.core=0xf000f200},.mac={.complete=true,.ack_mask=3,.batches_completed=2},
            .identity_validated=true,.write_validated=true,.timing={.complete=true,.ack_validated=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.bbp_batch_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x1100;
        assert(wifi_bbp_batch_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4)assert(d.ep0_enqueue==0);
        if(fault==4)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-bbp-batch-read: PASS exact read scope and failure quarantine");
}

static void compact_fixture(struct xhci_device *d,unsigned phase) {
    *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,.wifi_identity={0x76100002,0x76502000}};
    d->firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,.radio_reset={.prepared=true}};
    struct xhci_wifi_mcu *m=&d->firmware.mcu;
    *m=(struct xhci_wifi_mcu){.configured=true,.identity_validated=true,.write_validated=true,
        .timing={.complete=true,.ack_validated=true},.compact_mode=true,.compact_active=true,
        .compact_limit=12,.compact_next=phase,.bbp={.complete=true,.core=0xf000f200}};
    for(unsigned n=0;n<phase;n++) {m->compact[n]=(struct mt76_mcu_mac){.complete=true,.ack_mask=1,.batches_completed=1};m->compact_submitted[n]=1;}
    m->compact[phase]=(struct mt76_mcu_mac){.attempted=true,.batches_attempted=1};
}
static void test_wifi_compact_binding(void) {
    for(unsigned pair=0;pair<2;pair++) for(unsigned phase=pair?11:0;phase<12;phase++)for(unsigned fault=0;fault<(pair?35u:28u);fault++) {
        if((fault==11 || fault==12) && phase<2)continue;
        if(fault>=13 && fault<=16 && !phase)continue;
        if(fault==27 && phase<5)continue;
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
        uint8_t tx[200]={0},rx[1024]={0},packet[40],reply[1024];
        struct xhci_device *d=&xhci.devices[0];compact_fixture(d,phase);
        struct xhci_wifi_firmware *f=&d->firmware;struct xhci_wifi_mcu *m=&f->mcu;
        if(pair){m->clock_pair_mode=true;m->clock_mode=true;m->clock.complete=true;m->clock.written=true;m->clock_submitted=true;m->clock.pbf_after=0x80c00;m->bbp.pbf=0x80c00;}
        if(fault==28)m->clock_mode=false;
        if(fault==29)m->clock.complete=false;
        if(fault==30)m->clock.written=false;
        if(fault==31)m->clock_submitted=false;
        if(fault==32)m->clock_active=true;
        if(fault==33)m->clock.pbf_after=0x80000;
        if(fault==34)m->bbp.pbf=0x80000;
        f->buffer=tx;f->buffer_phys=0x10000;f->ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16};
        m->buffer=rx;m->buffer_phys=0x20000;m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        for(unsigned n=0;n<2;n++){events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1008:0);events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;}
        if(fault==1)m->identity_validated=false;
        if(fault==2)m->write_validated=false;
        if(fault==3)m->timing.complete=false;
        if(fault==4)m->timing.ack_validated=false;
        if(fault==5)f->radio_reset.prepared=false;
        if(fault==6)m->pending=true;
        if(fault==7)f->active=false;
        if(fault==8)m->compact_mode=false;
        if(fault==9)m->compact_active=false;
        if(fault==10)m->compact_next=12;
        if(fault==11)m->bbp.complete=false;
        if(fault==12)m->bbp.core=0;
        if(fault==13)m->compact[phase-1].complete=false;
        if(fault==14)m->compact[phase-1].ack_mask=0;
        if(fault==15)m->compact[phase-1].batches_completed=0;
        if(fault==16)m->compact_submitted[phase-1]=0;
        if(fault==17)m->compact[phase].attempted=false;
        if(fault==18)m->compact[phase].batches_attempted=2;
        if(fault==19)m->compact[phase].batches_completed=1;
        if(fault==20)m->compact[phase].ack_mask=1;
        if(fault==21)m->compact_submitted[phase]=1;
        if(fault==22)events[0].status=6u<<24;
        if(fault==23)events[1].status=6u<<24;
        if(fault==24)events[1].parameter_low+=16;
        if(fault==25)m->compact_limit=0;
        if(fault==26)m->compact_limit=13;
        if(fault==27)m->compact_limit=phase;
        if(!fault && phase<11)m->compact_limit=phase<5?5:phase+1; /* Preserve the accepted five-phase profile. */
        if(pair){uint32_t v;assert(!wifi_compact_read(d,0x2730,&v));assert(!wifi_compact_read(d,0x2734,&v));}
        size_t length=pair?24:40;
        assert((pair?mt76_mcu_clock_pair_packet(packet,40):mt76_mcu_compact_packet(phase,packet,40))==length);
        if(!fault){
            for(unsigned i=0;i<length;i++)for(unsigned bit=0;bit<8;bit++){
                packet[i]^=1u<<bit;assert(wifi_compact_exchange(d,packet,length,reply,1024)==-1);packet[i]^=1u<<bit;
            }
            assert(wifi_compact_exchange(d,packet,length-1,reply,1024)==-1);
            assert(wifi_compact_exchange(d,packet,length,reply,1023)==-1);
            assert(wifi_compact_exchange(d,NULL,length,reply,1024)==-1);
            assert(wifi_compact_exchange(d,packet,length,NULL,1024)==-1);
            assert(!f->ep.enqueue && !m->compact_submitted[phase]);
        }
        assert(wifi_compact_exchange(d,packet,length,reply,1024)==(fault?-1:16));assert(!freed);
        if(fault>=22 && fault<=24)assert(m->compact_submitted[phase]==1 && f->failed && d->ep0_quarantined);
        else if(fault)assert(!f->ep.enqueue && !m->response.enqueue);
        else {assert(!memcmp(tx,packet,length) && m->compact_submitted[phase]==1);assert(wifi_compact_exchange(d,packet,length,reply,1024)==-1);}
    }
    reset_test();struct xhci_device d;compact_fixture(&d,2);d.firmware.mcu.bbp_active=true;
    assert(wifi_bbp_owned(&d));d.firmware.mcu.compact[0].ack_mask=0;assert(!wifi_bbp_owned(&d));
    d.firmware.mcu.compact[0].ack_mask=1;d.firmware.mcu.compact_next=3;assert(!wifi_bbp_owned(&d));
    puts("xhci-compact-binding: PASS twelve phases, prerequisite/prefix/readiness guards, exact packets, no retry and quarantine");
}
static void test_wifi_compact_reads(void) {
    const uint32_t addresses[12][6]={{0x1004,0x1408,0x140c,0x1400,0x1018,0x1100},{0x1004,0x250,0x260,0x1410,0x150c,0x2000},{0x1004,0x2004,0x2010,0x2060,0x2080,0x2000},{0x1004,0x20a8,0x20b0,0x212c,0x2300,0x2000},{0x1004,0x233c,0x2358,0x2304,0x2308,0x2000},{0x1004,0x235c,0x2360,0x2364,0x2368,0x2000},{0x1004,0x2384,0x2388,0x2394,0x23a4,0x2000},{0x1004,0x23e4,0x23ec,0x23fc,0x2400,0x2000},{0x1004,0x2404,0x2504,0x2508,0x250c,0x2000},{0x1004,0x2510,0x2514,0x251c,0x2620,0x2000},{0x1004,0x2700,0x2710,0x2718,0x2720,0x2000},{0x1004,0x2724,0x2728,0x2730,0x2734,0x2000}};
    for(unsigned phase=0;phase<12;phase++)for(unsigned n=0;n<6;n++)for(unsigned fault=0;fault<4;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};struct xhci_device d;compact_fixture(&d,phase);
        d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==3?6:1,0);
        if(fault==1)d.firmware.mcu.pending=true;
        if(fault==2)d.firmware.mcu.compact_active=false;
        bool allowed=phase>=2 || n<5;uint32_t value;
        assert(wifi_compact_read(&d,addresses[phase][n],&value)==(!fault && allowed));assert(!freed);
        if(!allowed || fault==1 || fault==2)assert(!d.ep0_enqueue);
        else if(fault==3)assert(d.ep0_quarantined && d.firmware.failed);
        else assert(d.firmware.control_index==addresses[phase][n] && d.firmware.control_type==0xc0 && d.firmware.control_request==7 && d.firmware.control_length==4);
    }
    puts("xhci-compact-read: PASS fixed phase read scopes, ownership and failure quarantine");
}

static void test_wifi_txbe_observe(void) {
 const uint32_t addresses[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x272c};
 for(unsigned reg=0;reg<7;reg++)for(unsigned fault=0;fault<25;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
  struct xhci_device d;compact_fixture(&d,11);struct xhci_wifi_mcu *m=&d.firmware.mcu;
  m->compact_limit=11;m->compact_active=false;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_observe_active=true;
  d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  events[0]=control_event(0x9020,fault==24?6:1,0);
  if(fault==1)m->txbe_observe_active=false;
  if(fault==2)m->compact_mode=false;
  if(fault==3)m->compact_active=true;
  if(fault==4)m->compact_limit=12;
  if(fault==5)m->compact_next=10;
  if(fault==6)m->compact_submitted[11]=1;
  if(fault==7)m->compact[11].attempted=true;
  if(fault==8)m->configured=false;
  if(fault==9)m->pending=true;
  if(fault==10)m->identity_validated=false;
  if(fault==11)m->write_validated=false;
  if(fault==12)m->timing.complete=false;
  if(fault==13)m->timing.ack_validated=false;
  if(fault==14)d.firmware.radio_reset.prepared=false;
  if(fault==15)m->bbp.complete=false;
  if(fault==16)m->bbp.core=0;
  if(fault==17)m->compact[10].complete=false;
  if(fault==18)m->compact[10].ack_mask=0;
  if(fault==19)m->compact[10].batches_completed=0;
  if(fault==20)m->compact_submitted[10]=0;
  if(fault==21)d.firmware.active=false;
  if(fault==22)d.wifi_identity.mac=0;
  if(fault==23)m->txbe_observation.samples=8;
  uint32_t value;
  assert(wifi_txbe_observe_read(&d,addresses[reg],&value)==(!fault && reg<6));
  assert(!freed && m->compact_submitted[11]==(fault==6));
  if(reg==6 || (fault && fault!=24))assert(!d.ep0_enqueue);
  else if(fault==24)assert(d.ep0_quarantined && d.firmware.failed);
  else assert(d.firmware.control_index==addresses[reg] && d.firmware.control_type==0xc0 && d.firmware.control_request==7 && d.firmware.control_length==4);
 }
 puts("xhci TXBE observation: PASS exact read scope, prerequisite refusal and failure quarantine");
}

static void test_wifi_txbe_first_reads(void) {
 const uint32_t addresses[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x272c};
 for(unsigned stage=0;stage<2;stage++)for(unsigned reg=0;reg<7;reg++)for(unsigned fault=0;fault<26;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
  struct xhci_device d;compact_fixture(&d,11);struct xhci_wifi_mcu *m=&d.firmware.mcu;
  m->compact_limit=11;m->compact_active=false;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_first_active=true;
  m->txbe_first.write_attempted=stage;m->txbe_first.ack_validated=stage;
  d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  events[0]=control_event(0x9020,fault==24?6:1,0);
  if(fault==1)m->txbe_first_active=false;
  if(fault==2)m->compact_mode=false;
  if(fault==3)m->compact_active=true;
  if(fault==4)m->compact_limit=12;
  if(fault==5)m->compact_next=10;
  if(fault==6)m->compact_submitted[11]=1;
  if(fault==7)m->compact[11].attempted=true;
  if(fault==8)m->configured=false;
  if(fault==9)m->pending=true;
  if(fault==10)m->identity_validated=false;
  if(fault==11)m->write_validated=false;
  if(fault==12)m->timing.complete=false;
  if(fault==13)m->timing.ack_validated=false;
  if(fault==14)d.firmware.radio_reset.prepared=false;
  if(fault==15)m->bbp.complete=false;
  if(fault==16)m->bbp.core=0;
  if(fault==17)m->compact[10].complete=false;
  if(fault==18)m->compact[10].ack_mask=0;
  if(fault==19)m->compact[10].batches_completed=0;
  if(fault==20)m->compact_submitted[10]=0;
  if(fault==21)d.firmware.active=false;
  if(fault==22)d.wifi_identity.mac=0;
  if(fault==23){if(stage)m->txbe_first.after.samples=8;else m->txbe_first.before.samples=8;}
  if(fault==25){if(stage)m->txbe_first.ack_validated=false;else m->txbe_observation.attempted=true;}
  uint32_t value;
  assert(wifi_txbe_first_read(&d,addresses[reg],&value)==(!fault && reg<6));
  assert(!freed && m->compact_submitted[11]==(fault==6));
  if(reg==6 || (fault && fault!=24))assert(!d.ep0_enqueue);
  else if(fault==24)assert(d.ep0_quarantined && d.firmware.failed);
  else assert(d.firmware.control_index==addresses[reg] && d.firmware.control_type==0xc0 && d.firmware.control_request==7 && d.firmware.control_length==4);
 }
 puts("xhci TXBE first reads: PASS exact read scope, prerequisite refusal and failure quarantine");
}

static void test_wifi_txbe_first_binding(void) {
 for(unsigned second=0;second<2;second++)for(unsigned fault=0;fault<18;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
  uint8_t tx[200]={0},rx[1024]={0},packet[16],reply[1024];
  struct xhci_device *d=&xhci.devices[0];compact_fixture(d,11);
  struct xhci_wifi_firmware *f=&d->firmware;struct xhci_wifi_mcu *m=&f->mcu;
  m->compact_active=false;m->compact_limit=11;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_first_active=true;m->txbe_second_mode=second;
  m->txbe_first=(struct mt76_txbe_first){.attempted=true,.write_attempted=true,.before={.attempted=true,.complete=true,.reads=64,.samples=8}};
  f->buffer=tx;f->buffer_phys=0x10000;f->ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16};
  m->buffer=rx;m->buffer_phys=0x20000;m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11};
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  for(unsigned n=0;n<2;n++){events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1008:0);events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;}
  if(fault==1)m->txbe_first_active=false;
  if(fault==2)m->txbe_observe_active=true;
  if(fault==3)m->txbe_observation.attempted=true;
  if(fault==4)m->txbe_first.attempted=false;
  if(fault==5)m->txbe_first.write_attempted=false;
  if(fault==6)m->txbe_first.before.complete=false;
  if(fault==7)m->txbe_first.before.reads=63;
  if(fault==8)m->txbe_first.before.samples=7;
  if(fault==9)m->txbe_first.after.attempted=true;
  if(fault==10)m->txbe_first.ack_validated=true;
  if(fault==11)m->txbe_first_submitted=true;
  if(fault==12)m->compact_submitted[11]=1;
  if(fault==13)m->compact_limit=12;
  if(fault==14)m->compact[10].complete=false;
  if(fault==15)events[0].status=6u<<24;
  if(fault==16)events[1].status=6u<<24;
  if(fault==17)events[1].parameter_low+=16;
  assert((second?mt76_txbe_second_packet(packet,16):mt76_txbe_first_packet(packet,16))==16);
  if(!fault){
   for(unsigned i=0;i<16;i++)for(unsigned b=0;b<8;b++){
    packet[i]^=1u<<b;assert(wifi_txbe_first_exchange(d,packet,16,reply,1024)==-1);packet[i]^=1u<<b;
   }
   assert(wifi_txbe_first_exchange(d,packet,15,reply,1024)==-1);
   assert(wifi_txbe_first_exchange(d,packet,16,reply,1023)==-1);
   assert(wifi_txbe_first_exchange(d,NULL,16,reply,1024)==-1);
   assert(wifi_txbe_first_exchange(d,packet,16,NULL,1024)==-1);
   assert(!m->txbe_first_submitted && !f->ep.enqueue);
  }
  assert(wifi_txbe_first_exchange(d,packet,16,reply,1024)==(fault?-1:16));assert(!freed);
  if(fault>=15)assert(m->txbe_first_submitted && f->failed && d->ep0_quarantined);
  else if(fault)assert(!f->ep.enqueue && !m->response.enqueue);
  else {assert(!memcmp(tx,packet,16) && m->txbe_first_submitted);assert(wifi_txbe_first_exchange(d,packet,16,reply,1024)==-1);}
 }
 puts("xhci TXBE first binding: PASS exact packet, lifecycle prerequisites, no retry, error quarantine");
}

static void test_wifi_txbe_access_reads(void) {
 const uint32_t addresses[]={0x1004,0x2000,0x2724,0x2728,0x2730,0x2734,0x272c};
 for(unsigned stage=0;stage<2;stage++)for(unsigned reg=0;reg<7;reg++)for(unsigned fault=0;fault<28;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
  struct xhci_device d;compact_fixture(&d,11);struct xhci_wifi_mcu *m=&d.firmware.mcu;
  m->compact_limit=11;m->compact_active=false;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_access_active=true;
  m->txbe_access.request_attempted=stage;m->txbe_access.reply_validated=stage;
  d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  events[0]=control_event(0x9020,fault==24?6:1,0);
  if(fault==1)m->txbe_access_active=false;
  if(fault==2)m->compact_mode=false;
  if(fault==3)m->compact_active=true;
  if(fault==4)m->compact_limit=12;
  if(fault==5)m->compact_next=10;
  if(fault==6)m->compact_submitted[11]=1;
  if(fault==7)m->compact[11].attempted=true;
  if(fault==8)m->configured=false;
  if(fault==9)m->pending=true;
  if(fault==10)m->identity_validated=false;
  if(fault==11)m->write_validated=false;
  if(fault==12)m->timing.complete=false;
  if(fault==13)m->timing.ack_validated=false;
  if(fault==14)d.firmware.radio_reset.prepared=false;
  if(fault==15)m->bbp.complete=false;
  if(fault==16)m->bbp.core=0;
  if(fault==17)m->compact[10].complete=false;
  if(fault==18)m->compact[10].ack_mask=0;
  if(fault==19)m->compact[10].batches_completed=0;
  if(fault==20)m->compact_submitted[10]=0;
  if(fault==21)d.firmware.active=false;
  if(fault==22)d.wifi_identity.mac=0;
  if(fault==23){if(stage)m->txbe_access.after.samples=8;else m->txbe_access.before.samples=8;}
  if(fault==25){if(stage)m->txbe_access.reply_validated=false;else m->txbe_observation.attempted=true;}
  if(fault==26)m->txbe_first.attempted=true;
  if(fault==27)m->txbe_first_active=true;
  uint32_t value;
  assert(wifi_txbe_access_read(&d,addresses[reg],&value)==(!fault && reg<6));
  assert(!freed && m->compact_submitted[11]==(fault==6));
  if(reg==6 || (fault && fault!=24))assert(!d.ep0_enqueue);
  else if(fault==24)assert(d.ep0_quarantined && d.firmware.failed);
  else assert(d.firmware.control_index==addresses[reg] && d.firmware.control_type==0xc0 && d.firmware.control_request==7 && d.firmware.control_length==4);
 }
 puts("xhci TXBE first reads: PASS exact read scope, prerequisite refusal and failure quarantine");
}

static void test_wifi_txbe_access_binding(void) {
 for(unsigned direct=0;direct<2;direct++)for(unsigned fault=0;fault<20;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb out[64]={0},in[64]={0},events[64]={0};
  uint8_t tx[200]={0},rx[1024]={0},packet[16],reply[1024];
  struct xhci_device *d=&xhci.devices[0];compact_fixture(d,11);
  struct xhci_wifi_firmware *f=&d->firmware;struct xhci_wifi_mcu *m=&f->mcu;
  m->compact_active=false;m->compact_limit=11;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_access_active=true;
  m->txbe_access=(struct mt76_txbe_access){.attempted=true,.request_attempted=true,.before={.attempted=true,.complete=true,.reads=64,.samples=8}};
  m->txbe_direct_mode=direct;m->txbe_direct_submitted=direct;m->txbe_access.control_written=direct;
  f->buffer=tx;f->buffer_phys=0x10000;f->ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16};
  m->buffer=rx;m->buffer_phys=0x20000;m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11};
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  for(unsigned n=0;n<2;n++){events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1008:0);events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;}
  if(fault==1)m->txbe_access_active=false;
  if(fault==2)m->txbe_observe_active=true;
  if(fault==3)m->txbe_observation.attempted=true;
  if(fault==4)m->txbe_access.attempted=false;
  if(fault==5)m->txbe_access.request_attempted=false;
  if(fault==6)m->txbe_access.before.complete=false;
  if(fault==7)m->txbe_access.before.reads=63;
  if(fault==8)m->txbe_access.before.samples=7;
  if(fault==9)m->txbe_access.after.attempted=true;
  if(fault==10)m->txbe_access.reply_validated=true;
  if(fault==11)m->txbe_access_submitted=true;
  if(fault==12)m->compact_submitted[11]=1;
  if(fault==13)m->compact_limit=12;
  if(fault==14)m->compact[10].complete=false;
  if(fault==15)events[0].status=6u<<24;
  if(fault==16)events[1].status=6u<<24;
  if(fault==17)events[1].parameter_low+=16;
  if(fault==18){m->txbe_direct_mode=true;m->txbe_access.control_written=false;}
  if(fault==19){m->txbe_direct_mode=true;m->txbe_access.control_written=true;m->txbe_direct_submitted=false;}
  assert(mt76_txbe_access_packet(packet,16)==16);
  if(!fault){
   for(unsigned i=0;i<16;i++)for(unsigned b=0;b<8;b++){
    packet[i]^=1u<<b;assert(wifi_txbe_access_exchange(d,packet,16,reply,1024)==-1);packet[i]^=1u<<b;
   }
   assert(wifi_txbe_access_exchange(d,packet,15,reply,1024)==-1);
   assert(wifi_txbe_access_exchange(d,packet,16,reply,1023)==-1);
   assert(wifi_txbe_access_exchange(d,NULL,16,reply,1024)==-1);
   assert(wifi_txbe_access_exchange(d,packet,16,NULL,1024)==-1);
   assert(!m->txbe_access_submitted && !f->ep.enqueue);
  }
  assert(wifi_txbe_access_exchange(d,packet,16,reply,1024)==(fault?-1:16));assert(!freed);
  if(fault>=15 && fault<=17)assert(m->txbe_access_submitted && f->failed && d->ep0_quarantined);
  else if(fault)assert(!f->ep.enqueue && !m->response.enqueue);
  else {assert(!memcmp(tx,packet,16) && m->txbe_access_submitted);assert(wifi_txbe_access_exchange(d,packet,16,reply,1024)==-1);}
 }
 puts("xhci TXBE first binding: PASS exact packet, lifecycle prerequisites, no retry, error quarantine");
}


static void test_wifi_txbe_direct(void) {
 for(unsigned clocked=0;clocked<2;clocked++) for(unsigned fault=0;fault<(clocked?25u:19u);fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096];memset(buffer,0xa5,sizeof buffer);
  struct xhci_device d;compact_fixture(&d,11);struct xhci_wifi_mcu *m=&d.firmware.mcu;
  m->compact_limit=11;m->compact_active=false;m->compact[11]=(struct mt76_mcu_mac){0};m->txbe_access_active=true;m->txbe_direct_mode=true;
  m->txbe_access=(struct mt76_txbe_access){.attempted=true,.control_attempted=true,.before={.complete=true,.reads=64,.samples=8}};
  d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  events[0]=control_event(0x9020,fault==18?6:1,0);
  if(clocked){m->clock_mode=true;m->clock.complete=true;m->clock.written=true;m->clock_submitted=true;m->clock.pbf_after=0x80c00;m->bbp.pbf=0x80c00;}
  if(fault==19)m->clock.complete=false;
  if(fault==20)m->clock.written=false;
  if(fault==21)m->clock_submitted=false;
  if(fault==22)m->clock_active=true;
  if(fault==23)m->clock.pbf_after=0x80000;
  if(fault==24)m->bbp.pbf=0x80000;
  if(fault==1)m->txbe_direct_mode=false;
  if(fault==2)m->txbe_direct_submitted=true;
  if(fault==3)m->txbe_access.attempted=false;
  if(fault==4)m->txbe_access.control_attempted=false;
  if(fault==5)m->txbe_access.control_written=true;
  if(fault==6)m->txbe_access.before.complete=false;
  if(fault==7)m->txbe_access.before.reads=63;
  if(fault==8)m->txbe_access.before.samples=7;
  if(fault==9)m->txbe_access.request_attempted=true;
  if(fault==10)m->txbe_access.after.attempted=true;
  if(fault==11)m->txbe_access_submitted=true;
  if(fault==12)m->txbe_access.before.values[7][2]=1;
  if(fault==13)m->txbe_access_active=false;
  if(fault==14)m->txbe_first.attempted=true;
  if(fault==15)m->compact_submitted[11]=1;
  if(fault==16)m->pending=true;
  if(fault==17)m->compact[10].complete=false;
  assert(wifi_txbe_direct_write(&d)==(!fault));
  if(!fault){assert(m->txbe_direct_submitted && !buffer[0] && !buffer[1] && !buffer[2] && !buffer[3]);assert(d.firmware.control_index==0x2728 && d.firmware.control_type==0x40 && d.firmware.control_request==6 && d.firmware.control_length==4);assert(!wifi_txbe_direct_write(&d));}
  else if(fault==18)assert(d.ep0_quarantined && d.firmware.failed && m->txbe_direct_submitted);
  else assert(!d.ep0_enqueue);
 }
 puts("xhci TXBE direct: PASS fixed zero payload, exclusive ownership, no retry, control error quarantine");
}

static void test_wifi_full_table(void){
 for(unsigned chunk=0;chunk<3;chunk++)for(unsigned fault=0;fault<26;fault++){
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb out[64]={0},in[64]={0},events[64]={0};uint8_t tx[200]={0},rx[1024]={0},packet[200],reply[1024];
  struct xhci_device *d=&xhci.devices[0];compact_fixture(d,2);struct xhci_wifi_firmware *f=&d->firmware;struct xhci_wifi_mcu *m=&f->mcu;
  m->compact_active=false;m->compact_limit=2;m->compact[2]=(struct mt76_mcu_mac){0};m->compact_submitted[2]=0;
  m->table_mode=true;m->table_active=true;m->clock_mode=true;m->clock.complete=true;m->clock.written=true;m->clock_submitted=true;m->clock.pbf_after=0x80c00;m->bbp.pbf=0x80c00;
  m->table=(struct mt76_bbp_table){.attempted=true,.reads=62,.chunks_attempted=chunk+1,.chunks_completed=chunk};m->table_submitted=chunk;
  f->buffer=tx;f->buffer_phys=0x10000;f->ep=(struct xhci_bulk_endpoint){.ring=out,.ring_phys=0x9000,.cycle=true,.dci=16};
  m->buffer=rx;m->buffer_phys=0x20000;m->response=(struct xhci_bulk_endpoint){.ring=in,.ring_phys=0xb000,.cycle=true,.dci=11};
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  for(unsigned n=0;n<2;n++){events[n]=control_event(n?0xb000:0x9000,n?13:1,n?1008:0);events[n].control=(32u<<10)|(1u<<24)|((n?11u:16u)<<16)|1;}
  if(fault==1)m->table_mode=false;
  if(fault==2)m->table_active=false;
  if(fault==3)m->clock.complete=false;
  if(fault==4)m->clock.written=false;
  if(fault==5)m->clock_submitted=false;
  if(fault==6)m->clock.pbf_after=0x80000;
  if(fault==7)m->bbp.pbf=0x80000;
  if(fault==8)m->compact_limit=11;
  if(fault==9)m->compact_next=3;
  if(fault==10)m->compact[1].complete=false;
  if(fault==11)m->pending=true;
  if(fault==12)m->timing.ack_validated=false;
  if(fault==13)m->table.reads=61;
  if(fault==14)m->table.chunks_attempted++;
  if(fault==15)m->table.chunks_completed++;
  if(fault==16)m->table.ack_validated=true;
  if(fault==17)m->table.complete=true;
  if(fault==18)m->table.attempted=false;
  if(fault==19)m->clock_active=true;
  if(fault==20)m->compact[2].attempted=true;
  if(fault==21)events[0].status=6u<<24;
  if(fault==22){if(chunk<2)continue;events[1].status=6u<<24;}
  if(fault==25)m->mac_raw_mode=true;
  if(fault==23)m->mac_snapshot_mode=true;
  if(fault==24){m->mac_snapshot_mode=true;m->mac_snapshot.complete=true;m->mac_snapshot_active=true;}
  size_t length=mt76_bbp_table_packet(chunk,packet,sizeof packet);
  if(!fault){
   uint32_t value;assert(!wifi_table_read(d,0x1000,&value));
   assert(!wifi_mac_snapshot_owned(d));m->mac_snapshot_mode=true;m->mac_snapshot_active=true;m->table.attempted=false;
   if(!chunk){assert(wifi_mac_snapshot_owned(d));assert(!wifi_mac_snapshot_read(d,0x1000,&value));assert(!wifi_mac_snapshot_read(d,0x2728,&value));}
   else assert(!wifi_mac_snapshot_owned(d));
   m->mac_snapshot_active=false;m->mac_snapshot.complete=true;m->table.attempted=true;

   for(unsigned i=0;i<length;i++)for(unsigned bit=0;bit<8;bit++){packet[i]^=1u<<bit;assert(wifi_table_exchange(d,packet,length,reply,sizeof reply)==-1);packet[i]^=1u<<bit;}
   assert(wifi_table_exchange(d,packet,length-1,reply,sizeof reply)==-1);
  }
  assert(wifi_table_exchange(d,packet,length,reply,sizeof reply)==(fault?-1:chunk<2?0:16));
  if(!fault){assert(!memcmp(tx,packet,length) && m->table_submitted==chunk+1);assert(m->response.enqueue==(chunk==2?1u:0u));assert(wifi_table_exchange(d,packet,length,reply,sizeof reply)==-1);}
  else if(fault==21 || fault==22)assert(m->table_submitted==chunk+1 && f->failed && d->ep0_quarantined);
  else assert(!f->ep.enqueue && !m->response.enqueue);
 }
 puts("xhci full BBP: PASS exact fixed chunks, no response for first chunks, ownership, no retry and quarantine");
}

static void test_wifi_clock(void) {
 for(unsigned pair=0;pair<3;pair++) for(unsigned write=0;write<2;write++)for(unsigned fault=0;fault<26;fault++) {
  reset_test();test_time=0;test_time_step=1000;
  struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
  struct xhci_device d;compact_fixture(&d,2);struct xhci_wifi_mcu *m=&d.firmware.mcu;
  m->table_mode=pair==2;m->clock_pair_mode=pair==1;m->compact_limit=pair==2?2:pair?12:11;m->compact_active=false;m->compact[2]=(struct mt76_mcu_mac){0};m->compact_submitted[2]=0;m->bbp=(struct mt76_bbp_probe){0};
  m->clock_mode=true;m->clock_active=true;
  m->clock=(struct mt76_clock_probe){.attempted=true,.write_attempted=true,.reads=3,.pbf_before=0x80000};
  d.ep0_ring=ring;d.ep0_ring_phys=0x9000;d.ep0_cycle=true;d.descriptor_buffer=buffer;d.descriptor_buffer_phys=(uintptr_t)buffer;
  xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
  events[0]=control_event(0x9020,fault==25?6:1,0);
  if(fault==1)m->clock_mode=false;
  if(fault==2)m->clock_active=false;
  if(fault==3)m->compact_active=true;
  if(fault==4)m->compact_limit=pair?10:12;
  if(fault==5)m->compact_next=3;
  if(fault==6)m->compact[2].attempted=true;
  if(fault==7)m->compact_submitted[2]=1;
  if(fault==8)m->bbp.attempted=true;
  if(fault==9)m->pending=true;
  if(fault==10)m->timing.ack_validated=false;
  if(fault==11)m->compact[1].complete=false;
  if(fault==12)m->identity_validated=false;
  if(fault==13)m->write_validated=false;
  if(fault==14)d.firmware.radio_reset.prepared=false;
  if(fault==15)m->configured=false;
  if(write){
   if(fault==16)m->clock_submitted=true;
   if(fault==17)m->clock.attempted=false;
   if(fault==18)m->clock.write_attempted=false;
   if(fault==19)m->clock.written=true;
   if(fault==20)m->clock.complete=true;
   if(fault==21)m->clock.reads=2;
   if(fault==22)m->clock.mac_before=4;
   if(fault==23)m->clock.status_before=1;
   if(fault==24)m->clock.pbf_before=0x80c00;
   if(!fault){assert(!wifi_clock_write(&d,0x401,0x80c00));assert(!wifi_clock_write(&d,0x400,0x80000));}
   assert(wifi_clock_write(&d,0x400,0x80c00)==(!fault));
   if(!fault){assert(buffer[0]==0 && buffer[1]==12 && buffer[2]==8 && buffer[3]==0);assert(!wifi_clock_write(&d,0x400,0x80c00));}
  }else{
   if(fault>=16 && fault<=24)continue;
   uint32_t value;
   if(!fault)assert(!wifi_clock_read(&d,0x2728,&value));
   assert(wifi_clock_read(&d,0x400,&value)==(!fault));
  }
  if(fault==25)assert(d.ep0_quarantined && d.firmware.failed);
  else if(fault)assert(!d.ep0_enqueue);
  else assert(d.firmware.control_index==0x400 && d.firmware.control_type==(write?0x40:0xc0) && d.firmware.control_request==(write?6:7) && d.firmware.control_length==4);
 }
 puts("xhci clock: PASS exact control transfer, completed-phase ownership, one write and quarantine");
}
static void test_wifi_bbp_reads(void) {
    const uint32_t addresses[]={0x1004,0x1200,0x0400,0x2000};
    for(unsigned reg=0;reg<4;reg++)for(unsigned fault=0;fault<13;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[4096]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.ep0_ring=ring,.ep0_ring_phys=0x9000,
            .ep0_cycle=true,.descriptor_buffer=buffer,.descriptor_buffer_phys=(uintptr_t)buffer};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .radio_reset={.prepared=true},.mcu={.configured=true,.bbp_active=true,
            .format={.complete=true,.ack_mask=3,.batches_completed=2},.mac={.complete=true},
            .identity_validated=true,.write_validated=true,.timing={.complete=true}}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9020,fault==4?6:1,0);
        uint32_t address=addresses[reg],value;
        if(fault==1)d.firmware.mcu.bbp_active=false;
        if(fault==2)d.firmware.mcu.pending=true;
        if(fault==3)address=0x2004;
        if(fault==5)d.firmware.mcu.format.complete=false;
        if(fault==6)d.firmware.mcu.format.ack_mask=1;
        if(fault==7)d.firmware.mcu.format.batches_completed=1;
        if(fault==8)d.firmware.mcu.mac.complete=false;
        if(fault==9)d.firmware.mcu.timing.complete=false;
        if(fault==10)d.firmware.active=false;
        if(fault==11)d.wifi_identity.mac=0;
        if(fault==12)events[0].parameter_low+=16;
        assert(wifi_bbp_read(&d,address,&value)==!fault);
        assert(!freed);
        if(!fault)assert(d.firmware.control_type==0xc0 && d.firmware.control_request==7 &&
            d.firmware.control_index==address && d.firmware.control_length==4);
        if(fault && fault!=4 && fault!=12)assert(d.ep0_enqueue==0);
        if(fault==4 || fault==12)assert(d.ep0_quarantined && d.firmware.failed);
    }
    puts("xhci-bbp-read: PASS exact offsets, prior phase completion, ownership, stale events and quarantine");
}

static void test_wifi_mcu_transport(void) {
    for(unsigned in=0;in<2;in++)for(unsigned fault=0;fault<9;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        struct xhci_trb ring[64]={0},events[64]={0};uint8_t buffer[1024]={0};
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000}};
        struct xhci_wifi_firmware *f=&d->firmware;
        *f=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true,
            .buffer=buffer,.buffer_phys=0x10000,
            .ep={.ring=ring,.ring_phys=0x9000,.cycle=true,.dci=16}};
        f->mcu=(struct xhci_wifi_mcu){.configured=true,.buffer=buffer,.buffer_phys=0x10000,
            .response={.ring=ring,.ring_phys=0x9000,.cycle=true,.dci=11}};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        events[0]=control_event(0x9000,1,0);
        events[0].control=(32u<<10)|(1u<<24)|((in?11u:16u)<<16)|1;
        if(fault==1)events[0].status=(13u<<24)|8;
        if(fault==2)events[0].status=(1u<<24)|33;
        if(fault==3)events[0].control|=4;
        if(fault==4)events[0].parameter_low=0x9010;
        if(fault==5)events[0].control=(32u<<10)|(2u<<24)|((in?11u:16u)<<16)|1;
        if(fault==6)f->ready=false;
        if(fault==7)events[0].status=6u<<24;
        if(fault==8)events[0].control=(32u<<10)|(1u<<24)|(9u<<16)|1;
        int expected=fault==0?32:(fault==1 && in?24:-1);
        assert(wifi_mcu_bulk(d,in,32)==expected);
        assert(!freed);
        if(expected<0 && fault!=6) {
            assert(f->mcu.failed && f->failed && d->ep0_quarantined);
            assert(wifi_mcu_bulk(d,in,32)==-1);
        }
        if(fault==6)assert(!f->mcu.pending && !f->mcu.failed);
    }
    for(unsigned fault=0;fault<5;fault++) {
        reset_test();test_time=0;test_time_step=1000;
        uint8_t input[4096]={0},output[4096]={0};struct xhci_trb commands[64]={0},events[64]={0};
        struct xhci_device d={.present=true,.slot_id=1,.wifi_probe_ok=true,
            .wifi_identity={0x76100002,0x76502000},.input_context=input,
            .input_context_phys=(uintptr_t)input,.output_context=output};
        d.firmware=(struct xhci_wifi_firmware){.ready=true,.active=true,.configured=true};
        xhci.db_base=0x1000;xhci.rt_base=0x2000;xhci.event_ring=events;xhci.event_cycle=true;
        xhci.command_ring=commands;xhci.command_ring_phys=0xa000;xhci.command_cycle=true;
        events[0]=(struct xhci_trb){.parameter_low=0xa000,.status=(fault==1?6u:1u)<<24,
            .control=(XHCI_TRB_TYPE_COMMAND_COMPLETION<<10)|((fault==2?2u:1u)<<24)|1};
        if(fault==3)fail_after=1;
        if(fault==4)d.wifi_identity.mac++;
        assert(configure_wifi_mcu(&d)==!fault);
        assert(!freed);
        if(!fault) {
            assert(context_read32(input,0,1)==(1u|(1u<<11)));
            assert(context_read32(input,12,1)==((3u<<1)|(6u<<3)|(512u<<16)));
            assert((context_read32(input,1,0)>>27)==16);
            assert(!configure_wifi_mcu(&d));
        }
    }
    puts("xhci-mcu: PASS endpoint configuration, bounded IN/OUT, short packets, stale events, quarantine and DMA retention");
}

int main(int argc, char **argv) {
    assert(argc == 4);
    test_wifi_radio_binding();
    test_wifi_mcu_queue_order();
    test_wifi_mcu_write_probe();
    test_wifi_mcu_pair_probe();
    test_wifi_mcu_timing_probe();
    test_wifi_timing_reads();
    test_wifi_timing_exchange_refusal();
    test_wifi_mac_binding();
    test_wifi_mac_reads();
    test_wifi_format_binding();
    test_wifi_format_reads();
    test_wifi_bbp_setup_binding();
    test_wifi_bbp_setup_reads();
    test_wifi_bbp_front_binding();
    test_wifi_bbp_front_reads();
    test_wifi_bbp_agc_binding();
    test_wifi_bbp_agc_reads();
    test_wifi_bbp_batch_binding();
    test_wifi_bbp_batch_reads();
    test_wifi_compact_binding();
    test_wifi_compact_reads();
    test_wifi_txbe_observe();
    test_wifi_full_table();
    test_wifi_clock();
    test_wifi_txbe_direct();
    test_wifi_txbe_access_reads();
    test_wifi_txbe_access_binding();
    test_wifi_txbe_first_reads();
    test_wifi_txbe_first_binding();
    test_wifi_bbp_reads();
    test_wifi_mcu_transport();
    test_wifi_recovery();
    test_firmware_immediate_data();
    test_control_publication();
    test_wifi_firmware_stop();
    test_wifi_firmware_control_guards();
    test_wifi_firmware_configuration();
    test_wifi_firmware_transport();
    test_control_worker_completion();
    test_strict_control();
    test_strict_no_data();
    test_wifi_write_guard();
    test_wifi_probe();
    test_ep0_packet();
    test_network_transport();
    test_device_inventory();
    test_storage();
    test_root_ports();
    test_address_recovery();
    test_scratchpads();
    test_handoff();
    test_keyboard_context(argv[1], 1, 5, 64);
    test_keyboard_context(argv[2], 0, 7, 10);
    test_pro_trackpad(argv[2]);
    test_trackpad(argv[1],false,false,0x0291);
    test_keyboard_context(argv[3],1,5,64);
    test_trackpad(argv[3],false,false,0x0290);
    test_trackpad(argv[3],true,false,0x0290);
    test_trackpad(argv[3],true,true,0x0290);
    test_trackpad(argv[1],true,false,0x0291);
    test_trackpad(argv[1],true,true,0x0291);
    puts("xhci-host-test: PASS (delayed root ports, reset handshake/timeouts, scratchpads, ownership, Apple HID, BOT descriptors/contexts, HID/bulk demultiplexing, quarantine, short packets)");
}
