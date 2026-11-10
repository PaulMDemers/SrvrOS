/* Host regression tests execute the driver's actual static helpers.
 * Unused kernel entry points are discarded by --gc-sections. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define XHCI_COMMAND_TIMEOUT 256ull
#define XHCI_TRANSFER_TIMEOUT 256ull
#define XHCI_RESET_TIMEOUT 256ull
#define XHCI_HALT_TIMEOUT 256ull
#define SRVROS_XHCI_TEST_MMIO
static uint32_t xhci_test_read32(uint64_t offset);
static void xhci_test_write32(uint64_t offset, uint32_t value);
#include "../kernel/src/drivers/xhci.c"

/* Exercise USB provider retirement through the real cache/alias registry. */
static uint64_t block_test_irq_save(void) { return 0; }
static void block_test_irq_restore(uint64_t flags) { (void)flags; }
#define SRVROS_BLOCK_HOST_TEST
#include "../kernel/src/block.c"
#define SRVROS_BLOCK_CLAIM_HOST_TEST
#include "../kernel/src/block_claim.c"

static _Alignas(4096) uint8_t pool[5 * 1024 * 1024];
static uint64_t pool_used;
static uint64_t fail_after;
static uint64_t freed;
static bool reclaim_check;static bool released_pages[1280];
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

static bool reject_halt;static unsigned halt_reads,halt_after,halt_writes;
static uint32_t xhci_test_read32(uint64_t offset) {
    if(halt_after && offset==xhci.op_base+XHCI_OP_USBSTS && ++halt_reads>=halt_after)
        *(uint32_t *)(registers+offset)=XHCI_USBSTS_HCH;
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

static bool command_nested,event_flood,stop_on_doorbell;
static bool disable_auto,disable_wrong_slot,start_status_model;
static struct xhci_device *control_model;
static unsigned control_model_cc=1,control_model_stage=2;
static uint32_t hub_model_status=3;
static bool recovery_auto,recovery_bad_dequeue,recovery_wrong_slot,control_fail_clear;
static unsigned recovery_commands,recovery_fail_at,recovery_timeout_at;
static unsigned recovery_trace[8],recovery_trace_count;
static unsigned tt_calls,tt_fail_at,tt_change_address_at;static uint16_t tt_values[2],tt_indices[2];
static uint8_t disabled_order[32];static unsigned disabled_count;
static uint64_t test_space=1;
static bool retire_ready=true,storage_retire_ready=true,reject_bus_master;
static unsigned retire_calls;static uint16_t pci_command=6;
static struct pci_device reclaim_pci;
static unsigned flood_dequeues;
static struct xhci_device *mode_device;
static unsigned mode_doorbells;
static void xhci_test_write32(uint64_t offset, uint32_t value) {
    if(control_model && offset==xhci.db_base+4*control_model->slot_id && value==1) {
        struct xhci_device *d=control_model;
        struct xhci_trb *setup=(struct xhci_trb *)(uintptr_t)d->control_td[0];
        unsigned request=(setup->parameter_low>>8)&255;
        unsigned cc=control_fail_clear && ((setup->parameter_low>>8)&255)==USB_REQ_CLEAR_FEATURE?6:control_model_cc;
        if(recovery_auto && ((setup->parameter_low>>8)&255)==USB_REQ_CLEAR_FEATURE) {
            assert(recovery_trace_count<8);recovery_trace[recovery_trace_count++]=1;
        }
        if(request==8) {
            assert(tt_calls<2 && d->tt_cleanup_owner==1);
            tt_values[tt_calls]=setup->parameter_low>>16;tt_indices[tt_calls]=setup->parameter_high;
            tt_calls++;assert(recovery_trace_count<8);recovery_trace[recovery_trace_count++]=7+tt_calls;
            if(tt_calls==tt_fail_at)cc=6;
            if(tt_calls==tt_change_address_at)context_write32(xhci.devices[0].output_context,0,3,
                (context_read32(xhci.devices[0].output_context,0,3)&~255u)|38);
        }
        uint64_t ptr=d->control_td[cc==1 || control_model_stage>=d->control_td_count?d->control_td_count-1u:control_model_stage];
        if(cc==6)context_write32(d->output_context,1,0,2);
        if(d->hub_ports)for(unsigned i=0;i<4;i++)d->descriptor_buffer[i]=hub_model_status>>(i*8);
        else {d->descriptor_buffer[0]=18;d->descriptor_buffer[1]=1;}
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,
            .parameter_high=ptr>>32,.status=cc<<24,
            .control=(xhci.event_cycle?1u:0u)|(32u<<10)|((uint32_t)d->slot_id<<24)|(1u<<16)};
    }
    if(recovery_auto && offset==xhci.db_base) {
        unsigned index=(xhci.command_enqueue+XHCI_TRB_COUNT-2)%(XHCI_TRB_COUNT-1);
        struct xhci_trb *cmd=&xhci.command_ring[index];unsigned type=(cmd->control>>10)&63;
        assert(type==14 || type==15 || type==16);recovery_commands++;
        assert(recovery_trace_count<8);recovery_trace[recovery_trace_count++]=type;
        unsigned dci=(cmd->control>>16)&31;
        if(recovery_commands==recovery_timeout_at)return;
        unsigned cc=recovery_commands==recovery_fail_at?19:1;
        struct xhci_device *d=&xhci.devices[0];
        if(cc==1 && (type==14 || type==15))context_write32(d->output_context,dci,0,3);
        if(cc==1 && type==16 && !recovery_bad_dequeue) {
            context_write32(d->output_context,dci,2,cmd->parameter_low);
            context_write32(d->output_context,dci,3,cmd->parameter_high);
        }
        uint64_t ptr=xhci.command_ring_phys+index*sizeof(*cmd);
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,
            .parameter_high=ptr>>32,.status=cc<<24,
            .control=(xhci.event_cycle?1u:0u)|(33u<<10)|((recovery_wrong_slot?2u:1u)<<24)};
    }
    if(disable_auto && offset==xhci.db_base) {
        uint64_t index=(xhci.command_enqueue+XHCI_TRB_COUNT-2)%(XHCI_TRB_COUNT-1);
        const struct xhci_trb *cmd=&xhci.command_ring[index];
        assert(((cmd->control>>10)&63)==XHCI_TRB_TYPE_DISABLE_SLOT);
        uint8_t slot=cmd->control>>24;assert(disabled_count<32);disabled_order[disabled_count++]=slot;
        uint64_t ptr=xhci.command_ring_phys+index*sizeof(*cmd);
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)ptr,
            .parameter_high=ptr>>32,.status=1u<<24,
            .control=(xhci.event_cycle?1u:0u)|(33u<<10)|((uint32_t)(disable_wrong_slot?slot+1:slot)<<24)};
    }
    if(stop_on_doorbell && offset>=xhci.db_base && offset<xhci.db_base+8) {
        stop_on_doorbell=false;assert(xhci.sync_users==1 && !xhci_stop());
    }
    if(start_status_model && offset==xhci.op_base+XHCI_OP_USBSTS) {
        *(uint32_t *)(registers+offset)&=~(value&XHCI_USBSTS_EINT);return;
    }
    if(offset==xhci.op_base+XHCI_OP_USBCMD) {
        halt_writes++;if(reject_halt)return;
    }
    if(event_flood && offset==xhci.rt_base+XHCI_INTR0+XHCI_INTR_ERDP) {
        flood_dequeues++;
        /* Model a controller immediately refilling every retired event. */
        xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=0xa000,
            .status=1u<<24,.control=(xhci.event_cycle?1u:0u)|(32u<<10)|(1u<<24)|(3u<<16)};
    }
    if(command_nested && offset==xhci.db_base) {
        command_nested=false;uint64_t index=xhci.command_enqueue;
        assert(!submit_command(XHCI_TRB_TYPE_NOOP,0,NULL) && xhci.command_enqueue==index);
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

/* Removal admission must return before any provider registration/allocation. */
void *kcalloc(size_t count,size_t size) { (void)count;(void)size;abort(); }
bool net_device_register(const struct net_device_ops *ops) { (void)ops;abort(); }
void net_handle_ethernet_frame(const uint8_t *frame,uint16_t length) { (void)frame;(void)length;abort(); }
bool usb_storage_attach(struct usb_storage *s,void *context,usb_bulk_fn bulk,bool writable) {
    (void)s;(void)context;(void)bulk;(void)writable;abort();
}
bool usb_storage_retire(struct usb_storage *s) {if(!storage_retire_ready || s->busy || s->diagnostic_users)return false;s->failed=s->retired=true;return true;}
uint16_t pci_read_config16(const struct pci_device *p,uint8_t offset) {assert(p==&reclaim_pci && offset==4);return pci_command;}
void pci_write_config16(const struct pci_device *p,uint8_t offset,uint16_t value) {assert(p==&reclaim_pci && offset==4);if(!reject_bus_master)pci_command=value;}
bool net_device_retire(const struct net_device_ops *ops) { assert(ops && ops->send && ops->context);retire_calls++;return retire_ready; }
uint64_t vmm_current_address_space(void) { return test_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
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
void pmm_free_frame(uint64_t address) {
    assert(address!=0);
    if(reclaim_check) {
        assert(xhci.stopped && !(pci_command&4) && xhci.worker_retired && !xhci.poll_users && !xhci.diagnostic_users);
        assert(address>=(uint64_t)pool && address<(uint64_t)pool+sizeof(pool));
        uint64_t index=(address-(uint64_t)pool)/PMM_FRAME_SIZE;
        assert(!released_pages[index]);released_pages[index]=true;memset((void *)address,0xdd,PMM_FRAME_SIZE);
    }
    freed++;
}
void pmm_free_frame_range(uint64_t address,uint64_t count) {for(uint64_t i=0;i<count;i++)pmm_free_frame(address+i*PMM_FRAME_SIZE);}

static void reset_test(void) {
    tt_calls=tt_fail_at=tt_change_address_at=0;
    control_model=NULL;control_model_cc=1;control_model_stage=2;hub_model_status=3;
    recovery_auto=recovery_bad_dequeue=recovery_wrong_slot=control_fail_clear=false;
    recovery_commands=recovery_fail_at=recovery_timeout_at=recovery_trace_count=0;
    disable_auto=disable_wrong_slot=start_status_model=false;disabled_count=0;
    reclaim_check=false;memset(released_pages,0,sizeof(released_pages));
    storage_retire_ready=true;reject_bus_master=false;pci_command=6;
    reject_halt=false;halt_reads=halt_after=halt_writes=0;test_space=1;retire_ready=true;retire_calls=0;stop_on_doorbell=false;
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
static unsigned injected, pointer_injected, pointer_canceled, net_wakeups,usb_mouse_cancels,last_usb_mouse;
static uint8_t usb_mouse_buttons;
static unsigned usb_mouse_events;static int32_t last_usb_dx,last_usb_dy;
static void assert_restart_refused(bool terminal);
void net_notify_rx(void) { net_wakeups++; }
static struct trackpad_motion last_pointer;
void mouse_inject_trackpad(int32_t x,int32_t y,uint8_t buttons) {
    pointer_injected++; last_pointer=(struct trackpad_motion){.dx=x,.dy=y,.buttons=buttons};
}
void mouse_cancel_trackpad(void) { pointer_canceled++;mouse_inject_trackpad(0,0,0); }
static unsigned keyboard_release_reports;
void keyboard_inject_hid_report(const uint8_t report[8], struct keyboard_hid_state *state) {
    (void)state; assert(report[2]==4 || report[2]==0); if(report[2]==4)injected++;
    if(!memcmp(report,(uint8_t[8]){0},8))keyboard_release_reports++;
}
void mouse_inject_event(int32_t x,int32_t y,uint8_t buttons) { (void)x;(void)y;(void)buttons; }
void mouse_inject_usb(unsigned source,int32_t x,int32_t y,uint8_t buttons) {
    usb_mouse_events++;last_usb_dx=x;last_usb_dy=y;last_usb_mouse=source;usb_mouse_buttons=buttons;
}
void mouse_cancel_usb(unsigned source) {last_usb_mouse=source;usb_mouse_buttons=0;usb_mouse_cancels++;}
void mouse_inject_trackpad_tap(uint8_t button) { tap_injected++;last_tap=button;mouse_inject_trackpad(0,0,button);mouse_inject_trackpad(0,0,0); }
void mouse_inject_trackpad_scroll(int32_t x,int32_t y,int32_t wx,int32_t wy,uint8_t buttons) {
    mouse_inject_trackpad(x,y,buttons); last_pointer.wheel_x=wx; last_pointer.wheel_y=wy;
}
static bool trace_bulk;
static uint64_t resume_delay,service_pause;
bool usb_storage_timing_enabled(void) { return trace_bulk; }
static uint64_t test_time, test_time_step=1000, complete_at;
static uint64_t test_clock_reads;
static struct xhci_device *complete_device;
uint64_t timer_monotonic_us(void) {
    test_clock_reads++;
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
static bool test_io_active,wait_refused,wait_keyboard,test_io_foreign,stop_on_wait;
static unsigned wait_calls,bulk_wakes;
uint64_t timer_ticks(void) { return test_time/10000; }
bool scheduler_io_busy(void) { return test_io_foreign; }
bool scheduler_io_active(void) { return test_io_active; }
bool scheduler_io_yield(void) { return false; }
bool scheduler_io_begin(void) { return false; }
void scheduler_io_end(void) { abort(); }
bool timer_reference_ready(void) { return false; }
void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)q;bulk_wakes++; }
bool scheduler_io_wait_timeout(struct scheduler_wait_queue *q,
    scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    struct xhci_device *d=arg;
    assert(q==&d->bulk_wait && test_io_active && deadline==timer_ticks()+1);
    wait_calls++;
    if(stop_on_wait) {
        stop_on_wait=false;unsigned before=bulk_wakes;assert(xhci.sync_users==1 && !xhci_stop());
        assert(ready(arg) && bulk_wakes>before && d->bulk_pending);return true;
    }
    if(wait_refused)return false;
    if(ready(arg))return true;
    if(wait_keyboard) {
        struct xhci_device *kbd=&xhci.devices[1];
        struct xhci_trb key={.parameter_low=(uint32_t)kbd->interrupt_trb,.status=(13u<<24)|56,
            .control=(32u<<10)|(2u<<24)|(5u<<16)|1};
        dispatch_transfer_event(&key);wait_keyboard=false;
    }
    test_time=deadline*10000+service_pause;service_pause=0;
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
    /* A late exact completion retires pending state, but cannot unquarantine
     * storage or reclaim/reuse backing after an uncertain timeout/removal. */
    uint64_t retained_frees=freed;
    struct xhci_trb late_bulk={.parameter_low=0x9000,.status=1u<<24,
        .control=(32u<<10)|(1u<<24)|(3u<<16)|1};
    dispatch_transfer_event(&late_bulk);
    assert(disk->bulk_done && !disk->bulk_pending && disk->bulk_failed);
    assert(!storage_bulk_transfer(disk,true,caller,512,&actual));
    assert(disk->bulk_ep[1].enqueue==1 && disk->bulk_buffer==bounce &&
        disk->bulk_ep[1].ring==ring && freed==retained_frees);
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

static void test_service_gap_edges(void) {
    struct xhci_device d={0};
    test_time=100;test_time_step=0;complete_device=NULL;
    d.bulk_timing.service_valid=true;d.bulk_service_at=100;storage_timing_device=&d;
    storage_service_check();assert(d.bulk_timing.service_checks==1 && d.bulk_timing.service_samples==1);
    for(unsigned i=0;i<255;i++)storage_service_check();
    assert(d.bulk_timing.service_checks==256 && d.bulk_timing.service_samples==1);
    test_time=100100;storage_service_check();
    assert(d.bulk_timing.service_gap_max_us==100000 && d.bulk_timing.service_samples==2);
    storage_service_sample(&d,100101,true);assert(d.bulk_timing.completion_gap_us==1);
    storage_service_sample(&d,99,false);assert(!d.bulk_timing.service_valid);
    d.bulk_timing.service_valid=true;d.bulk_timing.service_checks=UINT64_MAX;
    storage_service_check();assert(!d.bulk_timing.service_valid && d.bulk_timing.service_checks==UINT64_MAX);
    storage_timing_device=NULL;test_time_step=1000;
}
static void test_service_ring_results(void) {
    reset_test();test_time=100;test_time_step=0;complete_device=NULL;
    struct xhci_device d={0};
    struct xhci_trb ring[XHCI_EVENT_TRB_COUNT]={0},event={0};
    xhci.event_ring=ring;xhci.event_ring_phys=0x1000;xhci.event_cycle=true;xhci.rt_base=0x2000;
    d.bulk_timing.service_valid=true;d.bulk_service_at=100;storage_timing_device=&d;
    uint64_t clocks=test_clock_reads;
    assert(!poll_event(&event));
    assert(d.bulk_timing.service_checks==1 && d.bulk_timing.empty_checks==1 &&
        !d.bulk_timing.event_checks && d.bulk_timing.last_empty_sample_us==100);
    test_time=200;
    for(unsigned i=0;i<255;i++)assert(!poll_event(&event));
    assert(d.bulk_timing.service_checks==256 && d.bulk_timing.empty_checks==256 &&
        d.bulk_timing.last_empty_sample_us==100); /* Unsampled checks add no clock. */
    ring[0]=(struct xhci_trb){.parameter_low=0x1234,.control=(32u<<10)|1u};
    assert(poll_event(&event) && event.parameter_low==0x1234 && xhci.event_dequeue==1);
    assert(d.bulk_timing.service_checks==257 && d.bulk_timing.event_checks==1 &&
        d.bulk_timing.last_empty_sample_us==100 && d.bulk_timing.service_samples==2);
    test_time=300;
    for(unsigned i=0;i<255;i++)assert(!poll_event(&event));
    assert(d.bulk_timing.last_empty_sample_us==100);
    assert(!poll_event(&event));
    assert(d.bulk_timing.service_checks==513 && d.bulk_timing.empty_checks==512 &&
        d.bulk_timing.event_checks==1 && d.bulk_timing.last_empty_sample_us==300 &&
        d.bulk_timing.service_samples==3 && d.bulk_timing.service_valid);
    assert(test_clock_reads-clocks==3); /* Only the original every256 samples. */
    /* Overflow/rollback invalidate evidence without altering ring behavior. */
    d.bulk_timing.empty_checks=UINT64_MAX;
    assert(!poll_event(&event) && d.bulk_timing.empty_checks==UINT64_MAX &&
        !d.bulk_timing.service_valid && xhci.event_dequeue==1);
    d.bulk_timing.service_valid=true;d.bulk_timing.empty_checks=0;
    d.bulk_timing.event_checks=UINT64_MAX;ring[1].control=1;
    assert(poll_event(&event) && d.bulk_timing.event_checks==UINT64_MAX &&
        !d.bulk_timing.service_valid && xhci.event_dequeue==2);
    d.bulk_timing=(struct usb_bulk_timing){.service_valid=true};d.bulk_service_at=300;
    test_time=299;
    assert(!poll_event(&event) && !d.bulk_timing.service_valid && !d.bulk_timing.last_empty_sample_us);
    /* A stale saved observer cannot classify a replacement's check. */
    struct xhci_device peer={0};storage_timing_device=&peer;
    storage_service_result(&d,true,false);
    assert(!d.bulk_timing.service_valid && !peer.bulk_timing.empty_checks);
    storage_timing_device=NULL;
    struct usb_bulk_timing saved=d.bulk_timing;
    clocks=test_clock_reads;
    assert(!poll_event(&event) && !memcmp(&saved,&d.bulk_timing,sizeof(saved)));
    assert(test_clock_reads==clocks);
    test_time_step=1000;
    puts("xhci-service-ring: PASS exact existing checks, sampled empty head, unrelated event and invalid/foreign observation");
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
    assert(sample.service_valid && sample.service_checks>=sample.service_samples);
    assert(sample.service_samples==(sample.service_checks-1)/256+1);
    assert(sample.empty_checks>0 && sample.event_checks>0 &&
        sample.empty_checks+sample.event_checks==sample.service_checks &&
        sample.last_empty_sample_us>=sample.submitted && sample.last_empty_sample_us<=sample.observed);
    assert(sample.service_gap_max_us<11000 && sample.completion_gap_us<=sample.service_gap_max_us);
    assert(!storage_timing_device);
    resume_delay=0;
    complete_device=d;complete_at=test_time+500000;service_pause=75000;
    assert(storage_bulk_transfer(d,true,caller,13,&actual));
    storage_bulk_timing(d,&sample);
    assert(sample.service_valid && sample.service_gap_max_us>=75000 && sample.service_gap_max_us<86000);
    assert(injected==before+1 && bulk_wakes==3 && actual==13 && caller[0]==0x5a);
    assert(d->bulk_wait_calls==wait_calls && d->bulk_wait_us>400000);
    assert(d->bulk_ep[1].enqueue==3 && !d->bulk_pending && !d->bulk_failed);
    uint64_t start=test_time;unsigned waited=wait_calls;
    assert(!storage_bulk_transfer(d,true,caller,13,&actual));
    assert(wait_calls>waited && test_time-start>=2000000 && test_time-start<2020000);
    assert(d->bulk_failed && d->bulk_pending && !strcmp(d->bulk_reason,"deadline"));
    storage_bulk_timing(d,&sample);assert(!sample.seen && !sample.observed && !sample.resumed && !storage_timing_device);
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
        d.control_failed=d.control_pending=false; /* Independent modeled device per failure case. */
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
    /* Device removal followed by exact late events cannot rearm endpoints,
     * clear quarantine, or release the retained DMA allocations. */
    struct xhci_network_endpoint retained[3];memcpy(retained,n.ep,sizeof(retained));
    for(unsigned i=0;i<3;i++) if(retained[i].pending)
        assert(network_event(&d,retained[i].hw.dci,retained[i].trb,1,0));
    network_service(&d);
    assert(n.failed && !n.ready && !freed && !network_send(&d,frame,64));
    for(unsigned i=0;i<3;i++) assert(n.ep[i].buffer==retained[i].buffer &&
        n.ep[i].hw.ring==retained[i].hw.ring && n.ep[i].hw.enqueue==retained[i].hw.enqueue);

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

static void test_command_identity(void) {
    reset_test();
    static struct xhci_trb commands[XHCI_TRB_COUNT],events[XHCI_EVENT_TRB_COUNT];
    memset(commands,0,sizeof(commands));memset(events,0,sizeof(events));
    xhci.command_ring=commands;xhci.command_ring_phys=(uint64_t)commands;
    xhci.event_ring=events;xhci.event_ring_phys=(uint64_t)events;
    xhci.command_cycle=xhci.event_cycle=true;xhci.rt_base=0x2000;xhci.db_base=0x1000;
    uint64_t pointer=(uint64_t)commands;
    events[0]=(struct xhci_trb){.parameter_low=(uint32_t)(pointer+16),.parameter_high=(uint32_t)((pointer+16)>>32),.status=1u<<24,.control=1|(33u<<10)|(3u<<24)};
    events[1]=(struct xhci_trb){.parameter_low=(uint32_t)pointer,.parameter_high=(uint32_t)(pointer>>32),.status=1u<<24,.control=1|(33u<<10)|(7u<<24)};
    command_nested=true;
    uint64_t slot=99;assert(submit_command(XHCI_TRB_TYPE_NOOP,0,&slot) && slot==7);
    for(unsigned i=0;i<2*XHCI_TRB_COUNT;i++) {
        uint64_t current=pointer+xhci.command_enqueue*sizeof(*commands);
        events[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=(uint32_t)current,
            .parameter_high=(uint32_t)(current>>32),.status=(i==0?17u:1u)<<24,
            .control=(xhci.event_cycle?1u:0u)|(33u<<10)|(7u<<24)};
        slot=99;assert(submit_command(XHCI_TRB_TYPE_NOOP,0,&slot)==(i!=0));
        assert(slot==(i?7u:99u) && !xhci.command_pending && !xhci.command_failed);
    }
    assert(((commands[XHCI_TRB_COUNT-1].control&1u)!=0)!=xhci.command_cycle);
    memset(events,0,sizeof(events));xhci.event_dequeue=0;xhci.event_cycle=true;
    uint64_t late=pointer+xhci.command_enqueue*sizeof(*commands);
    slot=99;assert(!submit_command(XHCI_TRB_TYPE_NOOP,0,&slot) && slot==99);
    uint64_t enqueue=xhci.command_enqueue;
    static struct xhci_trb saved[XHCI_TRB_COUNT];memcpy(saved,commands,sizeof(saved));
    /* A late success cannot authorize reuse after ambiguous progress. */
    events[0]=(struct xhci_trb){.parameter_low=(uint32_t)late,.parameter_high=(uint32_t)(late>>32),.status=1u<<24,.control=1|(33u<<10)};
    for(unsigned i=0;i<2*XHCI_TRB_COUNT;i++)assert(!submit_command(XHCI_TRB_TYPE_NOOP,0,&slot));
    assert(xhci.command_enqueue==enqueue && !memcmp(saved,commands,sizeof(saved)) && !freed);
    puts("xhci-command: PASS exact identity, timeout, late completion and saturation refusal");
}
static void stop_fixture(void) {
    reset_test();
    static struct xhci_trb commands[XHCI_TRB_COUNT],events[XHCI_EVENT_TRB_COUNT];
    memset(commands,0,sizeof(commands));memset(events,0,sizeof(events));
    xhci.present=xhci.operational=xhci.controller_owned=true;xhci.op_base=0x100;xhci.rt_base=0x2000;xhci.db_base=0x1000;
    xhci.command_ring=commands;xhci.command_ring_phys=(uint64_t)commands;xhci.command_cycle=true;
    xhci.event_ring=events;xhci.event_cycle=true;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBCMD)=XHCI_USBCMD_RUN;
    halt_after=4;
}
static void test_terminal_stop(void) {
    stop_fixture();
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBCMD)=UINT32_MAX;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=UINT32_MAX;
    assert(!halt_controller()); /* Inaccessible MMIO is not a halt acknowledgement. */
    stop_fixture();reject_halt=true;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=XHCI_USBSTS_HCH;
    assert(!halt_controller()); /* Stale HCH cannot override rejected R/S write. */
    stop_fixture();xhci.controller_owned=false;assert(!xhci_stop() && !halt_writes && xhci.operational);
    stop_fixture();test_space=2;assert(!xhci_stop() && xhci.operational);test_space=1;
    struct xhci_network network={.ready=true,.registered=true};
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1,.network=&network,.bulk_buffer_phys=0x9000};
    xhci.sync_users=1;retire_ready=false;
    assert(!xhci_stop() && xhci.stopping && !xhci.operational && !halt_writes);
    xhci.sync_users=0;assert(!xhci_stop() && !halt_writes);
    retire_ready=true;
    xhci.event_ring[0]=(struct xhci_trb){.control=1|(32u<<10)|(1u<<24)};
    xhci.event_ring[1]=(struct xhci_trb){.control=1|(33u<<10)};
    assert(xhci_stop() && xhci.stopped && xhci.stop_events==2 && !network.registered);
    assert(d->bulk_buffer_phys==0x9000 && !freed && retire_calls==3);
    unsigned writes=halt_writes;assert(xhci_stop() && halt_writes==writes);
    uint64_t slot=99;assert(!submit_command(XHCI_TRB_TYPE_NOOP,0,&slot) && slot==99);
    stop_fixture();stop_on_doorbell=true;slot=99;
    assert(!submit_command(XHCI_TRB_TYPE_NOOP,0,&slot) && slot==99 && !xhci.sync_users);
    assert(xhci_stop() && !freed);
    stop_fixture();stop_on_doorbell=true;
    struct xhci_device control={.slot_id=1,.ep0_ring=xhci.command_ring,
        .ep0_ring_phys=xhci.command_ring_phys,.ep0_cycle=true};
    struct usb_setup_packet setup={0};
    assert(!control_transfer(&control,&setup,0,0,false) && !xhci.sync_users);
    assert(xhci_stop());
    stop_fixture();
    static uint8_t bounce[4096],caller[64];
    d=&xhci.devices[0];*d=(struct xhci_device){.present=true,.slot_id=1,.bulk_buffer=bounce,
        .bulk_buffer_phys=(uint64_t)bounce};
    d->bulk_ep[1]=(struct xhci_bulk_endpoint){.dci=3,.ring=xhci.command_ring,
        .ring_phys=xhci.command_ring_phys,.cycle=true};
    test_time=0;test_time_step=1000;test_io_active=true;stop_on_wait=true;uint32_t actual=99;
    assert(!storage_bulk_transfer(d,true,caller,sizeof(caller),&actual));
    assert(!stop_on_wait && !xhci.sync_users && actual==99 && !freed);
    test_io_active=false;assert(xhci_stop());
    stop_fixture();halt_after=0;assert(!xhci_stop() && !xhci.stopped && !freed);
    halt_after=1;assert(xhci_stop()); /* Explicit retry after a late acknowledgement. */
    stop_fixture();event_flood=true;xhci.event_ring[0]=(struct xhci_trb){.control=1|(32u<<10)};
    assert(!xhci_stop() && !xhci.stopped && !freed);event_flood=false;
    assert(xhci_stop() && !freed);
    puts("xhci-stop: PASS verified halt, caller/provider drain, in-call cancellation, bounded events, retry and pinned backing");
}
static struct xhci_device *control_fixture(void) {
    stop_fixture();xhci.port_count=1;halt_after=0;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.configured=true,.slot_id=1,.port_id=1,.speed=1};
    assert(allocate_device_runtime(d));return d;
}
static void test_ep0_recovery(void) {
    struct usb_setup_packet setup={.request=USB_REQ_GET_STATUS,.length=4};
    for(unsigned stage=0;stage<3;stage++) {
        struct xhci_device *d=control_fixture();control_model=d;control_model_cc=6;control_model_stage=stage;
        d->ep0_enqueue=62; /* Setup/Data/Status straddle the Link TRB. */
        assert(!control_transfer(d,&setup,d->descriptor_buffer_phys,4,true));
        assert(d->control_failed && !d->control_pending && d->control_completion==6 && !xhci.sync_users);
        uint64_t old=d->ep0_ring_phys,allocated=pool_used;
        xhci.sync_users=1;assert(!xhci_recover_control(1) && pool_used==allocated);xhci.sync_users=0;
        d->parent_slot_id=2;assert(!xhci_recover_control(1) && pool_used==allocated);d->parent_slot_id=0;
        d->storage.block=(struct block_device *)1;
        assert(!xhci_recover_control(1) && pool_used==allocated);d->storage.block=NULL;
        recovery_auto=true;assert(xhci_recover_control(1));
        assert(recovery_commands==2 && !d->control_failed && d->retired_ep0_ring_phys==old &&
            d->ep0_ring_phys!=old && d->ep0_enqueue==0 && d->ep0_cycle && !freed);
        struct xhci_trb late={.parameter_low=(uint32_t)old,.parameter_high=old>>32,
            .status=6u<<24,.control=(32u<<10)|(1u<<24)|(1u<<16)};
        assert(!control_event_result(d,&late));
        control_model_cc=1;assert(control_transfer(d,&setup,d->descriptor_buffer_phys,4,true));
        assert(!xhci_recover_control(1) && recovery_commands==2 && pool_used==allocated+1);
    }
    for(unsigned failure=0;failure<6;failure++) {
        struct xhci_device *d=control_fixture();d->control_failed=true;d->control_completion=6;
        context_write32(d->output_context,1,0,2);recovery_auto=true;
        if(failure<2)recovery_fail_at=failure+1;
        if(failure==2 || failure==3)recovery_timeout_at=failure-1;
        if(failure==4)recovery_bad_dequeue=true;
        if(failure==5)recovery_wrong_slot=true;
        uint64_t old=d->ep0_ring_phys;
        assert(!xhci_recover_control(1) && d->control_failed && d->ep0_ring_phys==old &&
            d->retired_ep0_ring_phys && d->control_recovery_attempted && !freed);
        assert(!xhci_recover_control(1));
    }
    struct xhci_device *d=control_fixture();
    assert(!control_transfer(d,&setup,d->descriptor_buffer_phys,4,true));
    assert(d->control_pending && !d->control_completion && !xhci_recover_control(1));
    puts("xhci-ep0: PASS every TD stage/wrap, fresh dequeue, no replay, wrong state/owner/storage/command refusal and retained rings");
}
static void test_hub_changes(void) {
    struct xhci_device *hub=control_fixture();hub->hub_ports=2;
    struct xhci_device *child=&xhci.devices[1],*grandchild=&xhci.devices[2],*sibling=&xhci.devices[3];
    *child=(struct xhci_device){.present=true,.configured=true,.slot_id=2,.port_id=1,.parent_slot_id=1,.hub_port_id=1};
    *grandchild=(struct xhci_device){.present=true,.configured=true,.slot_id=3,.port_id=1,.parent_slot_id=2,.hub_port_id=1};
    *sibling=(struct xhci_device){.present=true,.configured=true,.slot_id=4,.port_id=1,.parent_slot_id=1,.hub_port_id=2};
    control_model=hub;hub_model_status=3|(1u<<16); /* CCS high after missed unplug/replug. */
    test_time=100;test_time_step=0;
    service_hub_changes();assert(hub->hub_status_pending && hub->control_pending && !child->removed);
    struct xhci_trb event;assert(poll_event(&event));dispatch_transfer_event(&event);
    assert(child->removed && grandchild->removed && sibling->configured && !sibling->removed &&
        !hub->removed && !hub->control_pending && hub->hub_poll_port==2 && !freed);
    hub_model_status=3;service_hub_changes();assert(poll_event(&event));dispatch_transfer_event(&event);
    assert(!sibling->removed && hub->hub_poll_port==1);
    /* Short status refuses stale trailing bytes, even with later successful Status. */
    control_model=NULL;service_hub_changes();assert(hub->hub_status_pending);
    uint64_t ptr=hub->control_td[1];event=(struct xhci_trb){.parameter_low=(uint32_t)ptr,.parameter_high=ptr>>32,
        .status=(13u<<24)|2,.control=(32u<<10)|(1u<<24)|(1u<<16)};
    dispatch_transfer_event(&event);assert(hub->hub_status_pending);
    ptr=hub->control_td[2];event.parameter_low=ptr;event.parameter_high=ptr>>32;event.status=1u<<24;
    dispatch_transfer_event(&event);assert(hub->removed && sibling->removed && !freed);
    hub=control_fixture();hub->hub_ports=1;test_time=100;test_time_step=0;
    service_hub_changes();assert(hub->control_pending);
    test_time=2000100;service_hub_changes();assert(hub->removed && hub->control_pending && !freed);
    uint64_t enqueue=hub->ep0_enqueue;service_hub_changes();assert(hub->ep0_enqueue==enqueue);
    /* A reset bit stuck high must never be reported as successful enumeration. */
    hub=control_fixture();hub->hub_ports=1;control_model=hub;hub_model_status=3|(1u<<4);
    uint32_t status=0xdeadbeef;
    assert(!reset_hub_port(hub,1,&status) && status==0xdeadbeef && !xhci.hub_ports_reset);
    hub=control_fixture();hub->hub_ports=1;control_model=hub;hub_model_status=3;
    assert(!reset_hub_port(hub,1,&status) && !xhci.hub_ports_reset); /* Rejected reset with stale enable. */
    hub=control_fixture();hub->hub_ports=1;control_model=hub;hub_model_status=3|(1u<<20);control_fail_clear=true;
    assert(!reset_hub_port(hub,1,&status) && hub->control_failed && hub->control_completion==6 && !xhci.hub_ports_reset);
    puts("xhci-hub-change: PASS asynchronous poll, missed disconnect, descendant/sibling isolation, short/timeout quarantine and stuck reset refusal");
}
static struct xhci_device *tt_cleanup_fixture(void) {
    struct xhci_device *d=control_fixture();
    d->parent_slot_id=3;d->hub_port_id=4;d->addressed=true;
    context_write32(d->output_context,0,2,2u|(6u<<8));context_write32(d->output_context,0,3,(2u<<27)|37);
    d->control_failed=true;d->control_completion=6;context_write32(d->output_context,1,0,2);
    struct xhci_device *hub=&xhci.devices[1];
    *hub=(struct xhci_device){.present=true,.configured=true,.slot_id=2,.speed=3,.port_id=1,.hub_ports=8};
    assert(allocate_device_runtime(hub));context_write32(hub->output_context,0,0,1u<<26);
    context_write32(hub->output_context,0,3,(3u<<27)|9);
    xhci.devices[2]=(struct xhci_device){.present=true,.configured=true,.slot_id=3,.speed=1,.port_id=1,
        .hub_ports=8,.parent_slot_id=2,.hub_port_id=6,.control_pending=true};
    xhci.devices[3]=(struct xhci_device){.present=true,.configured=true,.slot_id=4,.speed=1,.port_id=1,.parent_slot_id=2,.hub_port_id=7};
    xhci.devices[4]=(struct xhci_device){.present=true,.configured=true,.slot_id=5,.speed=3,.port_id=1,.parent_slot_id=2,.hub_port_id=8};
    control_model=hub;recovery_auto=true;return d;
}
static void test_shared_tt_cleanup(void) {
    for(unsigned failure=0;failure<5;failure++) {
        struct xhci_device *d=tt_cleanup_fixture(),*hub=&xhci.devices[1];
        uint64_t allocated=pool_used,old=d->ep0_ring_phys;
        hub->control_pending=true;assert(!xhci_recover_control(1) && pool_used==allocated);hub->control_pending=false;
        hub->tt_cleanup_owner=7;assert(!xhci_recover_control(1) && pool_used==allocated);hub->tt_cleanup_owner=0;
        context_write32(d->output_context,0,3,(2u<<27)|128);assert(!xhci_recover_control(1) && pool_used==allocated);
        context_write32(d->output_context,0,3,37);assert(!xhci_recover_control(1) && pool_used==allocated);
        context_write32(d->output_context,0,3,0);assert(!xhci_recover_control(1) && pool_used==allocated);
        context_write32(d->output_context,0,3,(2u<<27)|37);
        context_write32(hub->output_context,0,0,(1u<<26)|(1u<<25));assert(!xhci_recover_control(1));
        context_write32(hub->output_context,0,0,1u<<26);
        if(failure==1 || failure==2)tt_fail_at=failure;
        if(failure==3)tt_change_address_at=1;
        if(failure==4)recovery_timeout_at=1;
        assert(xhci_recover_control(1)==(failure==0));
        assert(!hub->tt_cleanup_owner && !freed && d->control_recovery_attempted);
        if(!failure) {
            assert(tt_calls==2 && tt_values[0]==(37u<<4) && tt_values[1]==((37u<<4)|0x8000) &&
                tt_indices[0]==1 && tt_indices[1]==1);
            assert(recovery_trace_count==4 && recovery_trace[0]==14 && recovery_trace[1]==8 &&
                recovery_trace[2]==9 && recovery_trace[3]==16);
            assert(d->ep0_ring_phys!=old && d->retired_ep0_ring_phys==old && !d->control_failed);
            assert(!xhci.devices[3].removed && !xhci.devices[4].removed);
        } else {
            assert(hub->tt_cleanup_failed && d->removed && xhci.devices[3].removed);
            assert(d->ep0_ring_phys==old && d->retired_ep0_ring_phys);
            if(failure==1 || failure==2)assert(hub->removed && xhci.devices[4].removed);
            else assert(!hub->removed && xhci.devices[4].configured);
            assert(recovery_commands==1); /* No fresh dequeue after failed cleanup/proof. */
        }
    }
    struct xhci_device *d=tt_cleanup_fixture(),*hub=&xhci.devices[1];
    hub->tt_cleanup_owner=1;uint64_t before=hub->ep0_enqueue;
    test_time=0;test_time_step=0;service_hub_changes();
    assert(hub->ep0_enqueue==before && !hub->control_pending);hub->tt_cleanup_owner=0;
    /* A periodic endpoint never receives CLEAR_TT_BUFFER. */
    d->control_failed=false;d->control_completion=0;d->hid_mouse=true;context_write32(d->output_context,1,0,1);
    void *ring=NULL,*buffer=NULL;assert(alloc_frame_zero(&d->interrupt_ring_phys,&ring));d->interrupt_ring=ring;
    assert(alloc_frame_zero(&d->interrupt_buffer_phys,&buffer));d->interrupt_buffer=buffer;
    d->interrupt_dci=3;d->interrupt_max_packet=8;d->interrupt_failed=true;d->last_interrupt_completion=6;
    context_write32(d->output_context,3,0,2);control_model=d;
    assert(xhci_recover_endpoint(1,3) && !tt_calls);
    d=tt_cleanup_fixture();hub=&xhci.devices[1];
    d->control_failed=false;d->bulk_pending=true;d->bulk_failed=true;
    unsigned wake_before=bulk_wakes;quarantine_unsafe_tt(d);
    assert(hub->tt_cleanup_failed && d->removed && d->bulk_pending && bulk_wakes>wake_before &&
        xhci.devices[3].removed && !xhci.devices[4].removed && !freed);
    puts("xhci-shared-tt: PASS physical address/both directions/single-TT index, owner lease, periodic exclusion, failure domain and pinned DMA");
}
static void test_hid_cancellation(void) {
    struct xhci_device *d=control_fixture();static uint8_t report[8]={1,0,0};
    d->hid_mouse=true;d->interrupt_buffer=report;d->interrupt_max_packet=8;
    handle_hid_mouse_report(d,8);assert(last_usb_mouse==0 && usb_mouse_buttons==1);
    d->retiring=true;unsigned before=usb_mouse_cancels;
    assert(device_removed(d) && usb_mouse_cancels==before+1 && !usb_mouse_buttons && !d->hid_have_absolute);
    d=control_fixture();d->hid_mouse=true;d->interrupt_pending=true;d->interrupt_ready=true;
    d->interrupt_trb=0xa000;d->interrupt_dci=3;d->interrupt_max_packet=8;
    struct xhci_trb error={.parameter_low=0xa000,.status=6u<<24,.control=(32u<<10)|(1u<<24)|(3u<<16)};
    before=usb_mouse_cancels;dispatch_transfer_event(&error);
    assert(usb_mouse_cancels==before+1 && d->interrupt_failed && !d->interrupt_ready);
    puts("xhci-hid-cancel: PASS per-record pointer source cancellation on removal and endpoint failure");
}
static struct xhci_device *hid_report_fixture(unsigned kind) {
    struct xhci_device *d=control_fixture();void *ring=NULL,*buffer=NULL;
    assert(alloc_frame_zero(&d->interrupt_ring_phys,&ring));d->interrupt_ring=ring;
    assert(alloc_frame_zero(&d->interrupt_buffer_phys,&buffer));d->interrupt_buffer=buffer;
    d->interrupt_dci=3;d->interrupt_max_packet=8;d->interrupt_ready=d->interrupt_cycle=true;
    d->hid_keyboard=kind==0;d->hid_mouse=kind!=0;d->hid_absolute_pointer=kind==2;
    submit_interrupt_poll(d);return d;
}
static void complete_hid_report(struct xhci_device *d,unsigned actual) {
    struct xhci_trb event={.parameter_low=d->interrupt_trb,.parameter_high=d->interrupt_trb>>32,
        .status=((actual<8?13u:1u)<<24)|(8-actual),.control=(32u<<10)|(1u<<24)|(3u<<16)};
    dispatch_transfer_event(&event);
}
static void test_short_hid_recovery(void) {
    for(unsigned kind=0;kind<3;kind++) {
        unsigned minimum=kind==0?8:kind==1?3:5;
        for(unsigned short_length=0;short_length<minimum;short_length++) {
            struct xhci_device *d=hid_report_fixture(kind);uint8_t *report=d->interrupt_buffer;
            report[0]=1;
            if(kind==0)report[2]=4;
            else if(kind==1) {report[1]=5;report[2]=2;}
            else {report[2]=0x40;report[4]=0x60;}
            complete_hid_report(d,minimum);
            uint64_t accepted=kind?d->mouse_reports:d->hid_reports;
            assert(accepted==1);
            if(kind==1) {
                unsigned events=usb_mouse_events;
                report[0]=1;report[1]=5;report[2]=2;
                complete_hid_report(d,minimum);
                assert(d->mouse_reports==2 && usb_mouse_events==events+1 && last_usb_dx==5 && last_usb_dy==2);
                accepted=2; /* Equal relative deltas describe additional motion. */
            }
            d->keyboard_state.repeat_usage=4;
            unsigned events=usb_mouse_events,cancels=usb_mouse_cancels,releases=keyboard_release_reports;
            uint64_t enqueue=d->interrupt_enqueue;
            /* The fresh DMA buffer is zeroed; missing bytes must not become coordinates. */
            report[0]=1;
            complete_hid_report(d,short_length);
            assert((kind?d->mouse_reports:d->hid_reports)==accepted && d->interrupt_errors==1);
            assert(d->interrupt_ready && !d->interrupt_failed && d->interrupt_pending && d->interrupt_enqueue==enqueue+1);
            if(kind)assert(usb_mouse_events==events && usb_mouse_cancels==cancels+1 && !d->hid_have_absolute);
            else assert(!d->keyboard_state.repeat_usage && keyboard_release_reports==releases+1);
            report[0]=1;
            if(kind==0)report[2]=4;
            else if(kind==1) {report[1]=7;report[2]=(uint8_t)-3;}
            else {report[2]=0x70;report[4]=0x20;}
            complete_hid_report(d,minimum);
            assert((kind?d->mouse_reports:d->hid_reports)==accepted+1 && d->interrupt_pending);
            if(kind==1)assert(last_usb_dx==7 && last_usb_dy==-3);
            if(kind==2)assert(last_usb_dx==0 && last_usb_dy==0);
        }
    }
    puts("xhci-hid-short: PASS keyboard/relative/absolute minima, no fabricated input, local cancel/rearm and valid-report recovery");
}
static void test_class_recovery_refusal(void) {
    struct xhci_device *d=hid_report_fixture(1);d->interrupt_pending=false;
    d->interrupt_failed=true;d->last_interrupt_completion=6;context_write32(d->output_context,3,0,2);
    for(unsigned kind=0;kind<4;kind++) {
        struct xhci_network network={.ready=true,.registered=true};
        d->storage.block=kind==0?(struct block_device *)1:NULL;d->network=kind==1?&network:NULL;
        d->bulk_failed=kind==2;
        if(kind==3) {d->hid_mouse=false;d->trackpad.dci=3;d->trackpad.ready=false;d->trackpad.last_completion=6;}
        static struct xhci_device saved;saved=*d;uint64_t allocated=pool_used;unsigned commands=recovery_commands;
        assert(!xhci_recover_endpoint(1,3) && !memcmp(d,&saved,sizeof(saved)) &&
            pool_used==allocated && recovery_commands==commands && !freed);
    }
    puts("xhci-class-policy: PASS published BOT/network/ambiguous bulk/trackpad refusal without mutation or allocation");
}
static void test_nested_tt(void) {
    for(unsigned size64=0;size64<2;size64++) {
        stop_fixture();xhci.port_count=2;xhci.hcc_params1=size64?1u<<2:0;
        static uint8_t input[4096],before[4096];
        for(unsigned i=0;i<3;i++)xhci.devices[i]=(struct xhci_device){.present=true,.configured=true,
            .slot_id=i+1,.port_id=1,.speed=i?1:3,.hub_ports=8,.parent_slot_id=i?i:0,.hub_port_id=i?i+3:0};
        struct xhci_device leaf={.port_id=1,.speed=2,.parent_slot_id=3,.hub_port_id=7,.input_context=input};
        uint32_t tt=99;assert(device_tt_context(&leaf,&tt) && tt==(1u|(4u<<8)));
        assert(setup_address_input_context(&leaf) && context_read32(input,1,2)==tt);
        xhci.devices[0].speed=1;assert(device_tt_context(&leaf,&tt) && tt==0);
        xhci.devices[0].speed=3;xhci.devices[1].speed=3;
        assert(device_tt_context(&leaf,&tt) && tt==(2u|(5u<<8)));
        leaf.speed=3;leaf.parent_slot_id=1;assert(device_tt_context(&leaf,&tt) && !tt);
        leaf.speed=2;leaf.parent_slot_id=3;
        memset(input,0xa5,sizeof(input));memcpy(before,input,sizeof(input));
#define BAD_TT(statement,restore) do {statement;tt=99;     assert(!device_tt_context(&leaf,&tt) && tt==99);assert(!setup_address_input_context(&leaf));     assert(!memcmp(input,before,sizeof(input)));restore;} while(0)
        BAD_TT(leaf.parent_slot_id=31,leaf.parent_slot_id=3);
        xhci.devices[1].speed=1; /* Exercise an otherwise-valid all-FS cycle. */
        BAD_TT(xhci.devices[1].parent_slot_id=3,xhci.devices[1].parent_slot_id=1);
        xhci.devices[1].speed=3;
        BAD_TT(xhci.devices[2].removed=true,xhci.devices[2].removed=false);
        BAD_TT(xhci.devices[2].slot_disabled=true,xhci.devices[2].slot_disabled=false);
        BAD_TT(xhci.devices[2].configured=false,xhci.devices[2].configured=true);
        BAD_TT(xhci.devices[2].port_id=2,xhci.devices[2].port_id=1);
        BAD_TT(leaf.hub_port_id=0,leaf.hub_port_id=7);
        BAD_TT(leaf.hub_port_id=9,leaf.hub_port_id=7);
        BAD_TT(leaf.speed=4,leaf.speed=2);
#undef BAD_TT
        uint64_t enqueued=xhci.command_enqueue;
        leaf.parent_slot_id=31;assert(!address_device(&leaf) && xhci.command_enqueue==enqueued && !freed);
        /* Configure hub metadata with the correct command and its own think time. */
        struct xhci_device *hub=&xhci.devices[0];static uint8_t output[4096];hub->input_context=input;hub->output_context=output;
        for(unsigned think=0;think<4;think++) {
            hub->hub_tt_think_time=think;memset(output,0,sizeof(output));
            context_write32(output,0,0,(1u<<27)|(1u<<25));context_write32(output,0,2,0x510000);
            uint64_t ptr=xhci.command_ring_phys+xhci.command_enqueue*sizeof(struct xhci_trb);
            unsigned cmd=xhci.command_enqueue;
            xhci.event_ring[xhci.event_dequeue]=(struct xhci_trb){.parameter_low=ptr,.parameter_high=ptr>>32,
                .status=1u<<24,.control=(xhci.event_cycle?1u:0u)|(33u<<10)|(1u<<24)};
            assert(configure_hub_context(hub));
            assert(((xhci.command_ring[cmd].control>>10)&63)==12 &&
                ((context_read32(input,1,2)>>16)&3)==think && !(context_read32(input,1,0)&(1u<<25)));
        }
    }
    puts("xhci-tt: PASS HS boundary/port across FS hubs, no fictitious FS translator, full ancestry refusal and hub Configure/think time");
}
static void test_interrupt_recovery(void) {
    for(unsigned failure=0;failure<5;failure++) {
        struct xhci_device *d=control_fixture();
        d->hid_keyboard=true;d->interrupt_dci=3;d->interrupt_max_packet=8;
        void *ring=NULL,*buffer=NULL;
        assert(alloc_frame_zero(&d->interrupt_ring_phys,&ring));d->interrupt_ring=ring;
        assert(alloc_frame_zero(&d->interrupt_buffer_phys,&buffer));d->interrupt_buffer=buffer;
        d->interrupt_failed=true;d->last_interrupt_completion=6;
        context_write32(d->output_context,3,0,2);
        uint64_t old=d->interrupt_ring_phys,allocated=pool_used;
        d->last_interrupt_completion=0;assert(!xhci_recover_endpoint(1,3) && pool_used==allocated);d->last_interrupt_completion=6;
        d->interrupt_pending=true;assert(!xhci_recover_endpoint(1,3));d->interrupt_pending=false;
        d->parent_slot_id=2;assert(!xhci_recover_endpoint(1,3));d->parent_slot_id=0;
        d->storage.block=(struct block_device *)1;assert(!xhci_recover_endpoint(1,3));d->storage.block=NULL;
        assert(!xhci_recover_endpoint(1,4) && !xhci_recover_endpoint(1,0));
        recovery_auto=true;control_model=d;
        if(failure==1)control_fail_clear=true;
        if(failure==2)recovery_fail_at=2;
        if(failure==3)recovery_timeout_at=1;
        if(failure==4)recovery_bad_dequeue=true;
        assert(xhci_recover_endpoint(1,3)==(failure==0));
        assert(d->interrupt_recovery_attempted && !freed && pool_used==allocated+1);
        if(failure==1)assert(recovery_commands==1 && d->control_failed);
        if(failure)assert(d->interrupt_failed && !d->interrupt_ready && d->interrupt_ring_phys==old && d->retired_interrupt_ring_phys);
        else {
            assert(d->interrupt_pending && d->interrupt_ring_phys!=old && d->retired_interrupt_ring_phys==old);
            assert(recovery_trace_count==3 && recovery_trace[0]==14 && recovery_trace[1]==1 && recovery_trace[2]==16);
            struct xhci_trb *setup=(struct xhci_trb *)(uintptr_t)d->control_td[0];
            assert((setup->parameter_low&0xffffff)==0x000102 && (setup->parameter_high&0xffff)==0x81);
            uint64_t enqueue=d->interrupt_enqueue;
            struct xhci_trb late={.parameter_low=old,.parameter_high=old>>32,.status=1u<<24,
                .control=(32u<<10)|(1u<<24)|(3u<<16)};
            dispatch_transfer_event(&late);assert(d->interrupt_enqueue==enqueue);
        }
        assert(!xhci_recover_endpoint(1,3));
    }
    /* Stopped recovery must preserve USB/host toggles: no reset or clear-halt. */
    for(unsigned failure=0;failure<3;failure++) {
        struct xhci_device *d=control_fixture();void *ring=NULL,*buffer=NULL;
        assert(alloc_frame_zero(&d->interrupt_ring_phys,&ring));d->interrupt_ring=ring;
        assert(alloc_frame_zero(&d->interrupt_buffer_phys,&buffer));d->interrupt_buffer=buffer;
        d->hid_keyboard=true;d->interrupt_dci=3;d->interrupt_max_packet=8;
        d->interrupt_pending=d->interrupt_ready=true;d->interrupt_cycle=true;
        context_write32(d->output_context,3,0,1);recovery_auto=true;
        if(failure==1)recovery_timeout_at=1;
        if(failure==2)recovery_bad_dequeue=true;
        assert(xhci_test_endpoint_recovery(1)==(failure==0));
        assert(recovery_trace[0]==15 && !d->control_td_count && !freed);
        if(!failure)assert(recovery_trace_count==2 && recovery_trace[1]==16 && d->interrupt_ready && d->interrupt_pending);
        else assert(d->interrupt_failed && !d->interrupt_ready);
    }
    puts("xhci-interrupt-recovery: PASS shared reset/clear-halt/dequeue order, fresh poll, owner/storage/TT/refusal and retained failures");
}
static void test_hub_route_depth(void) {
    uint32_t route=0;
    for(unsigned depth=0;depth<5;depth++) {
        uint32_t next=child_route_string(route,depth+1);
        assert(next==(route|((depth+1)<<(4*depth))) && next!=route);route=next;
    }
    assert(child_route_string(route,1)==UINT32_MAX);
    assert(child_route_string(0,0)==UINT32_MAX && child_route_string(0,16)==UINT32_MAX);
    puts("xhci-hub: PASS five route nibbles and exhausted/invalid path refusal");
}
static void test_slot_retirement(void) {
    stop_fixture();
    static uint64_t dcbaa[256];memset(dcbaa,0,sizeof(dcbaa));xhci.dcbaa=dcbaa;
    for(unsigned i=0;i<3;i++) {
        xhci.devices[i]=(struct xhci_device){.present=true,.configured=true,.slot_id=i+1,
            .port_id=1,.parent_slot_id=i?i:0,.bulk_buffer_phys=0x9000+i*4096};
        dcbaa[i+1]=0xa000+i*4096;
    }
    xhci.devices[3]=(struct xhci_device){.present=true,.configured=true,.slot_id=4,.port_id=2};
    assert(!xhci_retire_slot(0) && !xhci_retire_slot(5) && !disabled_count);
    test_space=2;assert(!xhci_retire_slot(1) && !xhci.devices[0].removed);test_space=1;
    xhci.sync_users=1;assert(!xhci_retire_slot(1));xhci.sync_users=0;
    for(unsigned i=0;i<3;i++)assert(xhci.devices[i].removed && xhci.devices[i].retiring);
    xhci.diagnostic_users=1;assert(!xhci_retire_slot(1));xhci.diagnostic_users=0;
    xhci.poll_users=1;assert(!xhci_retire_slot(1));xhci.poll_users=0;
    storage_retire_ready=false;assert(!xhci_retire_slot(1));storage_retire_ready=true;
    struct xhci_network network={.registered=true};xhci.devices[2].network=&network;
    retire_ready=false;assert(!xhci_retire_slot(1) && !disabled_count);retire_ready=true;
    disable_auto=true;assert(!xhci_retire_slot(1) && disabled_count==1);
    assert(!xhci_retire_slot(1) && disabled_count==2);assert(xhci_retire_slot(1));
    assert(disabled_count==3 && disabled_order[0]==3 && disabled_order[1]==2 && disabled_order[2]==1);
    for(unsigned i=0;i<3;i++)assert(xhci.devices[i].slot_disabled && !device_by_slot(i+1) &&
        !dcbaa[i+1] && xhci.devices[i].bulk_buffer_phys==0x9000+i*4096);
    assert(xhci_retire_slot(1) && disabled_count==3 && !freed && xhci.operational);
    assert(xhci.devices[3].configured && !xhci.devices[3].removed && device_by_slot(4)==&xhci.devices[3]);
    assert(!enumerate_port(0)); /* Permanent records block accidental re-enumeration. */
    stop_fixture();xhci.devices[0]=(struct xhci_device){.present=true,.slot_id=1};
    disable_auto=true;disable_wrong_slot=true;
    assert(!xhci_retire_slot(1) && xhci.command_failed && !xhci.devices[0].slot_disabled && !freed);
    stop_fixture();xhci.devices[0]=(struct xhci_device){.present=true,.slot_id=1};
    assert(!xhci_retire_slot(1) && xhci.command_failed && !freed);
    disable_auto=true;assert(!xhci_retire_slot(1) && !disabled_count); /* Late completion cannot recover timeout. */
    stop_fixture();xhci.max_slots=8;xhci.port_count=1;xhci.dcbaa=dcbaa;dcbaa[1]=0xabcdef;
    xhci.devices[1]=(struct xhci_device){.present=true,.port_id=1,.speed=1};
    xhci.devices[0]=(struct xhci_device){.present=true,.slot_id=1};
    xhci.event_ring[0]=(struct xhci_trb){.parameter_low=(uint32_t)xhci.command_ring_phys,
        .parameter_high=xhci.command_ring_phys>>32,.status=1u<<24,.control=1|(33u<<10)|(1u<<24)};
    assert(!address_device(&xhci.devices[1]) && xhci.command_failed && dcbaa[1]==0xabcdef);
    puts("xhci-slot: PASS subtree admission, provider drain, leaf-first disable, timeout/identity refusal and retained tombstones");
}
static void test_start_reset_boundaries(void) {
    stop_fixture();halt_after=0;start_status_model=true;
    static uint64_t dcbaa[256];static struct xhci_erst_entry erst;
    xhci.dcbaa=dcbaa;xhci.erst=&erst;
    /* Already-clear HCH plus a rejected Run write used to falsely pass. */
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBCMD)=0;
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=0;
    reject_halt=true;assert(!start_controller());reject_halt=false;
    assert(start_controller());
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=UINT32_MAX;
    assert(!wait_status_set(XHCI_USBSTS_HCH,true,1) && !start_controller());
    *(uint32_t *)(registers+xhci.op_base+XHCI_OP_USBSTS)=XHCI_USBSTS_CNR;
    assert(!start_controller());
    unsigned writes=halt_writes;assert(!reset_controller() && halt_writes==writes);
    xhci.stopping=true;assert(!start_controller() && !reset_controller() && halt_writes==writes);
    xhci.stopping=false;xhci.reclaimed=true;assert(!start_controller());
    puts("xhci-start: PASS Run readback, CNR/inaccessible refusal and no reset/restart over retained DMA");
}
static void test_backing_reclaim(void) {
    stop_fixture();xhci.pci=&reclaim_pci;
    /* Real allocator paths, including multi-page scratchpad metadata. */
    xhci.hcs_params2=(16u<<21)|(1u<<27); /* 513 scratchpads, two metadata pages. */
    assert(allocate_runtime_structures());
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1};assert(allocate_device_runtime(d));
    d->retired_ep0_ring_phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);assert(d->retired_ep0_ring_phys);
    d->retired_interrupt_ring_phys=pmm_alloc_frame_tagged(PMM_FRAME_DMA);assert(d->retired_interrupt_ring_phys);
    void *memory;assert(alloc_frame_zero(&d->interrupt_ring_phys,&memory));d->interrupt_ring=memory;
    assert(alloc_frame_zero(&d->interrupt_buffer_phys,&memory));d->interrupt_buffer=memory;
    assert(alloc_frame_zero(&d->bulk_buffer_phys,&memory));d->bulk_buffer=memory;
    for(unsigned ep=0;ep<2;ep++) {assert(alloc_frame_zero(&d->bulk_ep[ep].ring_phys,&memory));d->bulk_ep[ep].ring=memory;}
    assert(alloc_frame_zero(&d->trackpad.ring_phys,&memory));d->trackpad.ring=memory;
    assert(alloc_frame_zero(&d->trackpad.buffer_phys,&memory));d->trackpad.buffer=memory;
    static struct xhci_network network;memset(&network,0,sizeof(network));d->network=&network;
    for(unsigned ep=0;ep<3;ep++) {
        assert(alloc_frame_zero(&network.ep[ep].hw.ring_phys,&memory));network.ep[ep].hw.ring=memory;
        assert(alloc_frame_zero(&network.ep[ep].buffer_phys,&memory));network.ep[ep].buffer=memory;
    }
    uint64_t expected=pool_used;reclaim_check=true;xhci_poll_worker_started=true;
    assert(!xhci_reclaim() && !freed); /* Worker has not acknowledged retirement. */
    assert_restart_refused(true);
    assert(xhci_io_service_ready(NULL) && retire_poll_worker());
    xhci.poll_users=1;assert(!xhci_reclaim() && !freed);xhci.poll_users=0;
    xhci.diagnostic_users=1;assert(!xhci_reclaim() && !freed);xhci.diagnostic_users=0;
    storage_retire_ready=false;assert(!xhci_reclaim() && !freed);storage_retire_ready=true;
    reject_bus_master=true;assert(!xhci_reclaim() && !freed);reject_bus_master=false;
    for(unsigned i=0;i<128 && !xhci.reclaimed;i++) {
        uint64_t before=freed;bool done=xhci_reclaim();
        assert(freed-before<=16 && done==xhci.reclaimed);
        assert_restart_refused(true);
    }
    assert(xhci.reclaimed && freed==expected && xhci.reclaimed_frames==expected);
    assert_restart_refused(true);
    assert(!d->input_context && !d->output_context && !d->descriptor_buffer && !d->ep0_ring);
    assert(!d->interrupt_buffer && !d->bulk_buffer && !d->trackpad.buffer && !network.ep[0].buffer);
    assert(!xhci.dcbaa && !xhci.event_ring && !xhci.command_ring && !xhci.erst);
    struct srv_device_info info;assert(xhci_device_snapshot(0,&info)>0 && (info.flags&SRV_DEVICE_FAILED));
    uint64_t before=freed;assert(xhci_reclaim() && xhci_stop() && freed==before);
    poll_hid_events_once();assert(freed==before); /* Freed-ring poison must not be read. */
    reclaim_check=false;
    puts("xhci-reclaim: PASS worker/poll/diagnostic/storage/PCI gates, bounded exact-once DMA free and tombstones");
}
static void test_retired_event_drain(void) {
    reset_test();
    static struct xhci_trb events[XHCI_EVENT_TRB_COUNT];memset(events,0,sizeof(events));
    xhci.event_ring=events;xhci.event_cycle=true;xhci.rt_base=0x2000;
    xhci.devices[0]=(struct xhci_device){.present=true,.slot_id=1,.hid_keyboard=true,
        .interrupt_failed=true,.interrupt_trb=0xa000,.interrupt_dci=3};
    events[0]=(struct xhci_trb){.parameter_low=0xa000,.status=1u<<24,
        .control=1|(32u<<10)|(1u<<24)|(3u<<16)};
    unsigned before=injected;
    flood_dequeues=0;event_flood=true;
    poll_hid_events_once();
    assert(flood_dequeues==XHCI_EVENT_TRB_COUNT && !xhci.event_dequeue && !xhci.event_cycle);
    poll_hid_events_once();event_flood=false;
    assert(flood_dequeues==2*XHCI_EVENT_TRB_COUNT && xhci.event_cycle);
    assert(xhci.transfer_events==2*XHCI_EVENT_TRB_COUNT && injected==before && !freed);
    assert(!xhci.devices[0].interrupt_enqueue && !xhci.devices[0].interrupt_pending);
    puts("xhci-event-retire: PASS bounded continuous stale-event drain across event-ring cycles");
}
static void test_interrupt_retirement(void) {
    for(unsigned residue=0;residue<2;residue++) {
        reset_test();
        static struct xhci_trb ring[XHCI_TRB_COUNT],events[XHCI_EVENT_TRB_COUNT];
        static uint8_t report[64];
        memset(ring,0,sizeof(ring));memset(events,0,sizeof(events));
        xhci.event_ring=events;xhci.event_cycle=true;xhci.db_base=0x1000;xhci.rt_base=0x2000;
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.slot_id=1,.hid_keyboard=true,
            .interrupt_ready=true,.interrupt_pending=true,.interrupt_dci=3,.interrupt_trb=0xa000,
            .interrupt_max_packet=8,.interrupt_ring=ring,.interrupt_ring_phys=(uint64_t)ring,
            .interrupt_buffer=report,.interrupt_cycle=true};
        d->keyboard_state.repeat_usage=4;
        struct xhci_trb error={.parameter_low=0xa000,.status=residue?((1u<<24)|9):(6u<<24),
            .control=(32u<<10)|(1u<<24)|(3u<<16)};
        dispatch_transfer_event(&error);
        assert(!d->interrupt_ready && !d->keyboard_state.repeat_usage);
        unsigned before=injected;
        poll_hid_events_once();
        error.status=1u<<24;report[2]=4;dispatch_transfer_event(&error);
        assert(d->interrupt_errors==1 && !d->interrupt_enqueue && !d->interrupt_pending && injected==before && !freed);
    }
    puts("xhci-interrupt-retire: PASS fatal completion/residue do not rearm or deliver late reports");
}
static void test_network_retirement(void) {
    for(unsigned callback=0;callback<3;callback++) {
        reset_test();test_time=0;test_time_step=0;
        static struct xhci_trb events[XHCI_EVENT_TRB_COUNT];memset(events,0,sizeof(events));
        xhci.event_ring=events;xhci.event_cycle=true;xhci.db_base=0x1000;xhci.rt_base=0x2000;
        xhci.port_count=1;xhci.op_base=0x100;
        *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=XHCI_PORTSC_CCS|XHCI_PORTSC_CSC;
        struct xhci_network n={.ready=true,.link={.known=true,.up=true}};
        struct xhci_device d={.present=true,.slot_id=1,.port_id=1,.network=&n};
        for(unsigned i=0;i<3;i++)n.ep[i].pending=true;
        uint8_t frame[64]={0};
        if(callback==0)assert(!network_send(&d,frame,sizeof(frame)));
        else if(callback==1)network_service(&d);
        else assert(!network_poll(&d,1,false));
        assert(n.failed && !n.ready && !n.count && !freed);
        for(unsigned i=0;i<3;i++)assert(n.ep[i].pending && !n.ep[i].hw.enqueue);
    }
    test_time_step=1000;
    puts("xhci-network-retire: PASS provider callbacks observe replacement before admitting work");
}
static unsigned retirement_reads;
static bool retirement_read(struct block_device *d,uint64_t offset,void *data,uint64_t length) {
    (void)d;(void)offset;retirement_reads++;memset(data,0x5a,length);return true;
}
static void test_storage_retirement(void) {
    reset_test();block_init();retirement_reads=0;
    struct block_device *disk=block_register("usb-retirement",512,16,retirement_read,NULL,NULL);
    assert(disk);
    struct block_device alias={.used=true,.backing=disk,.block_size=512,.block_count=16,.read=retirement_read};
    struct block_device *healthy=block_register("healthy",512,16,retirement_read,NULL,NULL);
    assert(healthy);
    uint64_t claim=block_claim_mount(disk);assert(claim);
    uint8_t data[512];
    assert(block_read(&alias,0,data,sizeof(data)) && retirement_reads==1);
    assert(block_read(healthy,0,data,sizeof(data)) && retirement_reads==2);
    assert(block_read(disk,0,data,sizeof(data)) && retirement_reads==3);
    assert(block_read(disk,0,data,sizeof(data)) && retirement_reads==3); /* Cached. */
    struct xhci_device *d=&xhci.devices[0];
    *d=(struct xhci_device){.present=true,.slot_id=1,.port_id=1};
    d->storage.block=disk;xhci.port_count=1;xhci.op_base=0x100;
    assert(device_removed(d) && d->storage.failed);
    memset(data,0xcc,sizeof(data));
    assert(!block_read(disk,0,data,sizeof(data)) && !block_device_online(disk));
    for(unsigned i=0;i<sizeof(data);i++)assert(data[i]==0xcc);
    assert(!block_read(&alias,0,data,sizeof(data)) && !block_device_online(&alias));
    assert(block_read(healthy,0,data,sizeof(data)) && retirement_reads==3);
    assert(block_claim_release(claim) && !block_claim_release(claim));
    assert(disk->used && d->storage.block==disk && !freed);
    puts("xhci-storage-retire: PASS cached provider refuses reads after root removal, identity retained");
}
static void test_device_record_lifetime(void) {
    reset_test();
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++) {
        struct xhci_device *d=allocate_device_record();
        assert(d==&xhci.devices[i]);
        /* A failed address/configure may still own this slot and DMA. */
        *d=(struct xhci_device){.present=true,.slot_id=(uint8_t)(i+1),
            .input_context_phys=0x10000+i*PMM_FRAME_SIZE};
    }
    assert(!xhci.addressed_devices && !allocate_device_record() && xhci.device_record_full==1);
    for(unsigned i=0;i<XHCI_MAX_DEVICES;i++)assert(xhci.devices[i].slot_id==i+1 &&
        xhci.devices[i].input_context_phys==0x10000+i*PMM_FRAME_SIZE);
    puts("xhci-record: PASS failed enumeration retains identities through table exhaustion");
}
static void test_removal_lifetime(void) {
    for(unsigned changed=0;changed<3;changed++) {
        reset_test();
        static struct xhci_trb ring[XHCI_TRB_COUNT],events[XHCI_EVENT_TRB_COUNT];
        static uint8_t report[64];
        memset(ring,0,sizeof(ring));memset(events,0,sizeof(events));
        xhci.port_count=1;xhci.op_base=0x100;xhci.rt_base=0x2000;xhci.db_base=0x1000;
        xhci.event_ring=events;xhci.event_cycle=true;
        struct xhci_device *d=&xhci.devices[0];
        *d=(struct xhci_device){.present=true,.configured=true,.slot_id=1,.port_id=1,
            .hid_keyboard=true,.interrupt_ready=true,.interrupt_pending=true,
            .interrupt_trb=0xa000,.interrupt_dci=3,.interrupt_max_packet=8,
            .interrupt_ring=ring,.interrupt_ring_phys=(uint64_t)ring,.interrupt_cycle=true,
            .interrupt_buffer=report};
        if(changed==1) *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=XHCI_PORTSC_CCS|XHCI_PORTSC_CSC;
        if(changed==2)d->parent_slot_id=2; /* Child behind a removed root hub. */
        d->keyboard_state.repeat_usage=4;
        d->trackpad.ready=d->trackpad.pending=true;
        d->trackpad.pointer.buttons=1;trackpad_pointer_enabled=true;
        unsigned canceled=pointer_canceled;
        unsigned before=injected;
        poll_hid_events_once(); /* Root CCS is clear: removal before any rearm. */
        assert(!d->interrupt_ready && !d->keyboard_state.repeat_usage && !d->bulk_failed);
        assert(!d->trackpad.ready && d->trackpad.pending && pointer_canceled==canceled+1);
        assert(d->interrupt_pending && !d->interrupt_enqueue && !freed);
        *(uint32_t *)(registers+xhci.op_base+XHCI_OP_PORTS)=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;
        report[2]=4;
        struct xhci_trb late={.parameter_low=0xa000,.status=1u<<24,
            .control=(32u<<10)|(1u<<24)|(3u<<16)};
        dispatch_transfer_event(&late);poll_hid_events_once();
        assert(!d->interrupt_ready && !d->interrupt_enqueue && injected==before && !freed);
        /* Reconnection cannot allocate another slot or overwrite this identity. */
        xhci.ports[0].portsc=XHCI_PORTSC_CCS|XHCI_PORTSC_PED;
        uint64_t allocations=pool_used;
        assert(!enumerate_port(0) && pool_used==allocations && d->slot_id==1);
    }
    trackpad_pointer_enabled=false;
    puts("xhci-removal: PASS no rearm, late reports, missed disconnect/change, root-hub loss and reconnect refusal");
}
static void test_control_failure(void) {
    reset_test();
    static struct xhci_trb ring[XHCI_TRB_COUNT],events[XHCI_EVENT_TRB_COUNT],saved[XHCI_TRB_COUNT];
    memset(ring,0,sizeof(ring));memset(events,0,sizeof(events));
    xhci.event_ring=events;xhci.event_cycle=true;xhci.rt_base=0x2000;xhci.db_base=0x1000;
    for(unsigned error=0;error<2;error++) {
        memset(ring,0,sizeof(ring));memset(events,0,sizeof(events));
        xhci.event_dequeue=0;xhci.event_cycle=true;
        struct xhci_device d={.slot_id=1,.ep0_ring=ring,.ep0_ring_phys=(uint64_t)ring,.ep0_cycle=true};
        uint64_t pointer=(uint64_t)ring+32;
        if(error)events[0]=(struct xhci_trb){.parameter_low=(uint32_t)pointer,
            .parameter_high=(uint32_t)(pointer>>32),.status=6u<<24,.control=1|(32u<<10)|(1u<<24)|(1u<<16)};
        struct usb_setup_packet setup={.request=USB_REQ_GET_STATUS,.length=4};
        assert(!control_transfer(&d,&setup,0x10000,4,true));
        uint64_t enqueue=d.ep0_enqueue;memcpy(saved,ring,sizeof(saved));
        for(unsigned i=0;i<XHCI_TRB_COUNT;i++) {
            assert(!control_transfer(&d,&setup,0x10000,4,true));
            assert(d.ep0_enqueue==enqueue && !memcmp(saved,ring,sizeof(saved)));
        }
        assert(d.ep0_enqueue==enqueue && !memcmp(saved,ring,sizeof(saved)) && !freed);
    }
    puts("xhci-control: PASS timeout and failed EP0 cannot reuse ring without endpoint recovery");
}
static unsigned init_discoveries;
uint64_t pci_device_count(void) { init_discoveries++;return 0; }
const struct pci_device *pci_device_at(uint64_t i) { (void)i;return NULL; }
static void assert_restart_refused(bool terminal) {
    static struct xhci_state saved;saved=xhci;
    unsigned discoveries=init_discoveries,writes=halt_writes;
    uint64_t allocated=pool_used,released=freed,providers=network_count;
    bool worker=xhci_poll_worker_started,probe=xhci_input_probe_active;
    xhci_init();
    if(terminal)assert(!start_controller() && !reset_controller());
    assert(!memcmp(&saved,&xhci,sizeof(saved)) && discoveries==init_discoveries && halt_writes==writes &&
        pool_used==allocated && freed==released && network_count==providers &&
        xhci_poll_worker_started==worker && xhci_input_probe_active==probe);
}
static void test_failed_init_restart(void) {
    reset_test();xhci.present=true;xhci.stage="firmware-handoff-failed";
    assert_restart_refused(false);
    xhci.controller_owned=true;xhci.stage="partial-runtime-allocation";
    void *frame=NULL;assert(alloc_frame_zero(&xhci.command_ring_phys,&frame));xhci.command_ring=frame;
    assert_restart_refused(false);
    puts("xhci-restart: PASS failed handoff/partial allocation preserve one-attempt identity and backing");
}
static void test_repeated_init(void) {
    xhci_init();assert(init_discoveries==1);
    xhci.present=xhci.operational=true;xhci.command_ring_phys=0xa000;
    xhci.command_enqueue=7;xhci.event_dequeue=9;
    xhci.devices[0].present=true;xhci.devices[0].bulk_failed=true;
    xhci_poll_worker_started=true;
    static struct xhci_state saved;saved=xhci;
    uint64_t allocations=pool_used,releases=freed;
    xhci_init();
    assert(init_discoveries==1 && !memcmp(&saved,&xhci,sizeof(saved)));
    assert(pool_used==allocations && freed==releases && xhci_poll_worker_started);
    xhci_poll_worker_started=false; /* Restore only the host model. */
    puts("xhci-init: PASS repeated entry preserves modeled controller/DMA/quarantine/worker state");
}
int main(int argc, char **argv) {
    assert(argc == 4);
    test_repeated_init();
    test_failed_init_restart();
    test_terminal_stop();
    test_slot_retirement();
    test_hub_route_depth();
    test_nested_tt();
    test_shared_tt_cleanup();
    test_hid_cancellation();
    test_short_hid_recovery();
    test_class_recovery_refusal();
    test_interrupt_recovery();
    test_ep0_recovery();
    test_hub_changes();
    test_start_reset_boundaries();
    test_backing_reclaim();
    test_retired_event_drain();
    test_network_retirement();
    test_interrupt_retirement();
    test_storage_retirement();
    test_device_record_lifetime();
    test_command_identity();
    test_removal_lifetime();
    test_control_failure();
    test_ep0_packet();
    test_network_transport();
    test_device_inventory();
    test_storage();
    test_service_gap_edges();
    test_service_ring_results();
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
