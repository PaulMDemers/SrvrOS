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

static void xhci_test_write32(uint64_t offset, uint32_t value) {
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

static void test_keyboard_context(const char *fixture) {
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
    assert(device.interface_number == 1 && device.interrupt_dci == 5);
    assert(device.interrupt_max_packet == 64 && device.interrupt_interval == 8);
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
        assert(context_read32(input, 0, 1) == ((1u << 0) | (1u << 5)));
        assert(context_read32(input, 1, 2) == 0x0203);
        assert(context_read32(input, 6, 0) == 6u << 16);
        assert(context_read32(input, 6, 1) >> 16 == 64);
        assert(context_read32(input, 6, 4) == (64u | (64u << 16)));
        assert(context_read32(input, 6, 2) == 0x12345001);
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
    assert(ring[0].status == 64);
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

void mouse_trackpad_get_settings(unsigned *speed,bool *acceleration) { *speed=3;*acceleration=false; }
uint64_t mouse_overflow_count(void) { return 0; }
static unsigned injected, pointer_injected;
static struct trackpad_motion last_pointer;
void mouse_inject_trackpad(int32_t x,int32_t y,uint8_t buttons) {
    pointer_injected++; last_pointer=(struct trackpad_motion){.dx=x,.dy=y,.buttons=buttons};
}
void keyboard_inject_hid_report(const uint8_t report[8], struct keyboard_hid_state *state) {
    (void)state; assert(report[2]==4); injected++;
}
void mouse_inject_event(int32_t x,int32_t y,uint8_t buttons) { (void)x;(void)y;(void)buttons; }
void mouse_inject_trackpad_scroll(int32_t x,int32_t y,int32_t wx,int32_t wy,uint8_t buttons) {
    mouse_inject_trackpad(x,y,buttons); last_pointer.wheel_x=wx; last_pointer.wheel_y=wy;
}
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

static void test_trackpad(const char *fixture,bool pointer,bool gestures) {
    xhci_set_trackpad_gestures(false);
    xhci_set_trackpad_pointer(false); pointer_injected=0;
    reset_test();
    uint8_t descriptor[84];
    FILE *file=fopen(fixture,"rb"); assert(file);
    assert(fread(descriptor,1,sizeof(descriptor),file)==84); fclose(file);
    uint8_t input[4096]={0},output[4096]={0},keyboard[64]={0,0,4};
    uint8_t buffer[APPLE_TRACKPAD_REPORT_MAX];
    struct xhci_trb ring[64]={0};
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1,.vendor_id=0x05ac,.product_id=0x0291,.speed=1,
        .descriptor_buffer=descriptor,.input_context=input,.output_context=output,
        .hid_keyboard=true,.interrupt_dci=5,.interrupt_max_packet=64,.interrupt_buffer=keyboard,
        .interrupt_pending=true,.interrupt_trb=0xa000};
    xhci_set_trackpad_capture(false); assert(!parse_trackpad_config(d,84));
    xhci_set_trackpad_capture(!pointer); xhci_set_trackpad_pointer(pointer); xhci_set_trackpad_gestures(gestures);
    for (unsigned n=0;n<84;n++) assert(!parse_trackpad_config(d,n));
    d->product_id=0x0292; assert(!parse_trackpad_config(d,84)); d->product_id=0x0291;
    descriptor[62]=1; assert(!parse_trackpad_config(d,84)); descriptor[62]=0; /* alternate */
    descriptor[79]=0x82; assert(!parse_trackpad_config(d,84)); descriptor[79]=0x83;
    assert(parse_trackpad_config(d,84));
    assert(d->trackpad.candidate && d->trackpad.dci==7 && d->trackpad.interval==1);
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
    assert(t->reports==2 && t->frame.count==0 && !t->frame.button); /* Lift frame. */
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
    e.parameter_low=(uint32_t)t->trb; e.status=(13u<<24)|(APPLE_TRACKPAD_REPORT_MAX+1);
    dispatch_transfer_event(&e); assert(t->errors==1 && !t->ready && !t->pending);
    if (pointer) assert(!last_pointer.buttons && t->pointer_cancels==2 && t->pointer_motion==1);
    submit_trackpad_poll(d); assert(!t->pending); /* Error doesn't become a retry storm. */
    struct xhci_trackpad_capture capture;
    xhci_trackpad_snapshot(&capture);
    assert(capture.found && capture.max_contacts==2 && capture.lifts==1 && capture.errors==1);
    assert(capture.samples[2].length==94 && capture.samples[1].data[40]==123);
    xhci_trackpad_reset_capture(); xhci_trackpad_snapshot(&capture);
    assert(capture.found && !capture.reports && !capture.samples[1].length && capture.errors==1);
    if (pointer) assert(capture.pointer_enabled && capture.pointer_cancels==0);
    xhci_set_trackpad_capture(false); xhci_set_trackpad_pointer(false); xhci_set_trackpad_gestures(false);
    puts("trackpad-transport: PASS opt-in, exact descriptor, independent context/ring, multi-packet length, keyboard/bulk coexistence, stale events, errors; synthetic only");
}

int main(int argc, char **argv) {
    assert(argc == 2);
    test_storage();
    test_root_ports();
    test_scratchpads();
    test_handoff();
    test_keyboard_context(argv[1]);
    test_trackpad(argv[1],false,false);
    test_trackpad(argv[1],true,false);
    test_trackpad(argv[1],true,true);
    puts("xhci-host-test: PASS (delayed root ports, reset handshake/timeouts, scratchpads, ownership, Apple HID, BOT descriptors/contexts, HID/bulk demultiplexing, quarantine, short packets)");
}
