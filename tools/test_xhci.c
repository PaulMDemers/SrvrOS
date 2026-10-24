/* Host regression tests execute the driver's actual static helpers.
 * Unused kernel entry points are discarded by --gc-sections. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define SRVROS_XHCI_TEST_MMIO
static uint32_t xhci_test_read32(uint64_t offset);
static void xhci_test_write32(uint64_t offset, uint32_t value);
#include "../kernel/src/drivers/xhci.c"

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
static void xhci_test_write32(uint64_t offset, uint32_t value) {
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
static bool trace_bulk;
static uint64_t resume_delay;
bool usb_storage_timing_enabled(void) { return trace_bulk; }
static uint64_t test_time, test_time_step=1000, complete_at;
static struct xhci_device *complete_device;
uint64_t timer_monotonic_us(void) {
    test_time+=test_time_step;
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
static bool test_io_active,wait_refused,wait_keyboard,test_io_foreign;
static unsigned wait_calls,bulk_wakes;
uint64_t timer_ticks(void) { return test_time/10000; }
bool scheduler_io_busy(void) { return test_io_foreign; }
bool scheduler_io_active(void) { return test_io_active; }
bool scheduler_io_yield(void) { return false; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q;bulk_wakes++; }
bool scheduler_io_wait_timeout(struct scheduler_wait_queue *q,
    scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    struct xhci_device *d=arg;
    assert(q==&d->bulk_wait && test_io_active && deadline==timer_ticks()+1);
    wait_calls++;
    if(wait_refused)return false;
    if(ready(arg))return true;
    if(wait_keyboard) {
        struct xhci_device *kbd=&xhci.devices[1];
        struct xhci_trb key={.parameter_low=(uint32_t)kbd->interrupt_trb,.status=(13u<<24)|56,
            .control=(32u<<10)|(2u<<24)|(5u<<16)|1};
        dispatch_transfer_event(&key);wait_keyboard=false;
    }
    test_time=deadline*10000;
    (void)timer_monotonic_us();poll_hid_events_once();
    if(d->bulk_done)test_time+=resume_delay;
    return true;
}
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
    disk->bulk_timed=true;
    dispatch_transfer_event(&events[0]);assert(!disk->bulk_timing.observed);
    poll_hid_events_once();
    assert(disk->bulk_timing.observed && !disk->bulk_timing.seen);
    uint64_t observed=disk->bulk_timing.observed;
    dispatch_transfer_event(&events[2]);assert(disk->bulk_timing.observed==observed);
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

static void test_storage_wait(void) {
    assert(storage_poll_us==20000);storage_poll_us=4000; /* Retain initial-policy regression cases. */
    reset_test();test_time=100000;test_time_step=3;test_io_active=true;
    struct xhci_trb events[64]={0},ring[64]={0};
    uint8_t bounce[4096]={0x5a},caller[512]={0},report[64]={0,0,4};
    xhci.event_ring=events;xhci.event_cycle=true;xhci.event_ring_phys=0x1000;xhci.rt_base=0x2000;
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1,.bulk_buffer=bounce,.bulk_buffer_phys=0x10000};
    d->bulk_ep[1]=(struct xhci_bulk_endpoint){.dci=3,.ring=ring,.ring_phys=0x9000,.cycle=true};
    xhci.devices[1]=(struct xhci_device){.present=true,.slot_id=2,.hid_keyboard=true,.interrupt_pending=true,
        .interrupt_trb=0xa000,.interrupt_dci=5,.interrupt_max_packet=64,.interrupt_buffer=report};
    uint32_t actual=0;unsigned before=injected;wait_calls=bulk_wakes=0;
    trace_bulk=true;
    complete_device=d;complete_at=test_time+2000;
    assert(storage_bulk_transfer(d,true,caller,13,&actual) && !wait_calls && actual==13);
    struct usb_bulk_timing sample;storage_bulk_timing(d,&sample);
    assert(sample.seen && sample.observed-sample.submitted>=2000 && sample.resumed-sample.observed==3);
    resume_delay=12345;
    complete_device=d;complete_at=test_time+500000;wait_keyboard=true;
    assert(storage_bulk_transfer(d,true,caller,13,&actual) && wait_calls>20);
    storage_bulk_timing(d,&sample);
    assert(sample.seen && sample.observed-sample.submitted>=500000 && sample.resumed-sample.observed==resume_delay+2*test_time_step);
    resume_delay=0;
    assert(injected==before+1 && bulk_wakes==2 && actual==13 && caller[0]==0x5a);
    assert(d->bulk_wait_calls==wait_calls && d->bulk_wait_us>400000);
    assert(d->bulk_ep[1].enqueue==2 && !d->bulk_pending && !d->bulk_failed);
    uint64_t start=test_time;unsigned waited=wait_calls;
    assert(!storage_bulk_transfer(d,true,caller,13,&actual));
    assert(wait_calls>waited && test_time-start>=2000000 && test_time-start<2020000);
    assert(d->bulk_failed && d->bulk_pending && !strcmp(d->bulk_reason,"deadline"));
    storage_bulk_timing(d,&sample);assert(!sample.seen && !sample.observed && !sample.resumed);
    trace_bulk=false;
    uint64_t enqueue=d->bulk_ep[1].enqueue;
    assert(!storage_bulk_transfer(d,true,caller,13,&actual) && d->bulk_ep[1].enqueue==enqueue);
    d->bulk_failed=d->bulk_pending=false;wait_refused=true;
    complete_device=d;complete_at=test_time+10000;
    assert(storage_bulk_transfer(d,true,caller,13,&actual)); /* bounded poll fallback */
    wait_refused=false;
    uint64_t old=99;test_io_foreign=true;
    assert(!xhci_storage_wait_policy(0,&old) && old==99);test_io_foreign=false;
    test_io_active=false;assert(!xhci_storage_wait_policy(0,&old));test_io_active=true;
    assert(!xhci_storage_wait_policy(123,&old) && !xhci_storage_wait_policy(0,NULL));
    d->bulk_pending=true;assert(!xhci_storage_wait_policy(0,&old));d->bulk_pending=false;
    d->bulk_failed=true;assert(!xhci_storage_wait_policy(0,&old));d->bulk_failed=false;
    /* Controlled completion latency, identical driver/event fixture and clock
     * phase. Demonstrate the tick-quantization cost rather than hide it. */
    static const uint64_t delay[]={2000,5000,9000,15000,25000,100000};
    for(unsigned i=0;i<sizeof(delay)/sizeof(delay[0]);i++) {
        uint64_t duration[3];static const uint64_t policies[]={0,4000,20000};
        for(unsigned mode=0;mode<3;mode++) {
            assert(xhci_storage_wait_policy(policies[mode],&old));
            test_time=((test_time/10000)+1)*10000;
            uint64_t began=test_time;complete_device=d;complete_at=began+delay[i];
            unsigned before_wait=wait_calls;
            assert(storage_bulk_transfer(d,true,caller,13,&actual));
            duration[mode]=test_time-began;
            assert(duration[mode]>=delay[i] && duration[mode]<=delay[i]+10020);
            if(!policies[mode] || delay[i]<policies[mode])assert(wait_calls==before_wait);
            if(policies[mode] && delay[i]>policies[mode]+10000)assert(wait_calls>before_wait);
        }
        if(delay[i]==5000)assert(duration[1]>duration[0]+4000 && duration[2]<duration[0]+20);
        printf("storage-wait-latency: device_us=%llu poll_us=%llu wait4_us=%llu wait20_us=%llu\n",
            (unsigned long long)delay[i],(unsigned long long)duration[0],
            (unsigned long long)duration[1],(unsigned long long)duration[2]);
    }
    assert(xhci_storage_wait_policy(20000,&old));
    test_io_active=false;test_time_step=1000;
    puts("xhci-storage-wait: PASS fast path, delayed completion/HID, wake, bounded deadline, no retry and refusal fallback");
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
    t->pointer.tap_rejections.count[TAP_REJECT_MOVEMENT]=7;
    t->pointer.tap_rejections.two_finger[TAP_REJECT_MOVEMENT]=3;
    t->pointer.tap_rejections.max_age_us[TAP_REJECT_MOVEMENT]=20000;
    t->pointer.tap_rejections.max_travel[TAP_REJECT_MOVEMENT]=96;
    bool armed=t->pointer.tap_armed;
    xhci_trackpad_reset_capture(); xhci_trackpad_snapshot(&capture);
    assert(t->pointer.tap_armed==armed);
    for(unsigned i=0;i<TAP_REJECT_COUNT;i++) {
        assert(!t->pointer.tap_rejections.count[i] && !t->pointer.tap_rejections.two_finger[i]);
        assert(!t->pointer.tap_rejections.max_age_us[i] && !t->pointer.tap_rejections.max_travel[i]);
    }
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
    xhci_poll_worker_started=false;xhci.operational=true;xhci_input_probe_active=false;
    assert(!network_rx_wake_ready(&d));
    xhci_poll_worker_started=true;assert(network_rx_wake_ready(&d));
    xhci_input_probe_active=true;assert(!network_rx_wake_ready(&d));xhci_input_probe_active=false;
    xhci.operational=false;assert(!network_rx_wake_ready(&d));xhci.operational=true;
    n.ready=false;assert(!network_rx_wake_ready(&d));n.ready=true;
    xhci_poll_worker_started=false;
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
    assert(net_wakeups==notification_wakes && n.notify_completed_us==test_time);
    network_service(&d);
    assert(n.notifications==0 && !n.link.downstream_bps);
    n.config.notify_data_interface=true;
    memcpy(n.ep[2].buffer,speed,sizeof(speed));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(speed)));network_service(&d);
    assert(n.notifications==1 && n.link.downstream_bps==1000000000);
    assert(net_wakeups==notification_wakes); /* Speed updates alone do not wake RX. */
    uint8_t up[]={0xa1,0,1,0,1,0,0,0};
    memcpy(n.ep[2].buffer,up,sizeof(up));
    assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(up)));network_service(&d);
    assert(n.link.up && net_wakeups==notification_wakes+1);
    for(unsigned i=0;i<20;i++) {
        memcpy(n.ep[2].buffer,up,sizeof(up));
        assert(network_event(&d,4,n.ep[2].trb,13,64-sizeof(up)));network_service(&d);
    }
    assert(net_wakeups==notification_wakes+1); /* Repeated link-up reports stay quiet. */
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

int main(int argc, char **argv) {
    assert(argc == 4);
    test_ep0_packet();
    test_network_transport();
    test_device_inventory();
    test_storage();
    test_storage_wait();
    test_root_ports();
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
