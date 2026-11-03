/* Actual input protocol driver against deterministic PCI/queue/config/DMA fakes.
 * The transport has its own ring tests; keyboard and mouse sinks have independent
 * tests of their actual normalization and cross-source ownership. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/drivers/virtio_input.c"

#define FAKE_PHYSICAL 0x100000ull
#define FAKE_EVENTS 512u
struct queued_event { struct input_event event; uint32_t length; int token_override; };
struct virtio_pci_device {
    unsigned index, kind, opens, submits, stops, queue_sizes[2], sequence;
    bool started, stopped, reset_stuck, fail_open, fail_start, fail_queue, bad_config;
    unsigned fail_submit;
    uint8_t select, subsel;
    bool pending[64];
    uint64_t addresses[64];
    struct queued_event events[FAKE_EVENTS];
    unsigned head, count;
    int32_t axis_min[2], axis_max[2];
    uint8_t abs_size[2], missing_axes;
    bool generation_fail, header_changed, bad_version;
    unsigned absolute_reads;
};
static struct virtio_pci_device fake[4];
static struct pci_device pci_devices[4];
static unsigned pci_count;
static uint8_t frames[4][4096];
static bool frame_live[4];
static unsigned allocations, frees, allocation_fail;
static uint64_t clock_now, current_space;
struct key_record { unsigned source; uint8_t usage; bool down; };
static struct key_record key_records[1024];
static unsigned key_count, repeat_checks[4];
struct pointer_record { unsigned source; int32_t dx, dy, wx, wy; uint8_t buttons; bool cancel, absolute; };
static struct pointer_record pointer_records[1024];
static unsigned pointer_count;
static uint8_t pointer_buttons[4];
void console_printf(const char *format, ...) { assert(strstr(format, "virtio-input: init-failed")); }
enum virtio_pci_status virtio_pci_get_status(const struct virtio_pci_device *d) {
    return d->fail_open ? VIRTIO_PCI_BAD_CONFIG : VIRTIO_PCI_OK;
}
const char *virtio_pci_status_name(enum virtio_pci_status status) { (void)status; return "fake"; }

uint64_t vmm_kernel_address_space(void) { return 0x1000; }
uint64_t vmm_current_address_space(void) { return current_space; }
uint64_t timer_monotonic_us(void) { return clock_now; }
uint64_t pci_device_count(void) { return pci_count; }
const struct pci_device *pci_device_at(uint64_t i) { return i < pci_count ? &pci_devices[i] : NULL; }
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA);
    if (allocation_fail && allocations + 1 == allocation_fail) return 0;
    for (unsigned i = 0; i < 4; ++i) if (!frame_live[i]) {
        frame_live[i] = true; ++allocations;
        return FAKE_PHYSICAL + i * 4096;
    }
    assert(!"event pool bound exceeded"); return 0;
}
void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical >= FAKE_PHYSICAL && physical < FAKE_PHYSICAL + sizeof(frames));
    return (uint8_t *)frames + physical - FAKE_PHYSICAL;
}
void pmm_free_frame(uint64_t physical) {
    unsigned index = (unsigned)((physical - FAKE_PHYSICAL) / 4096);
    assert(index < 4 && frame_live[index]);
    /* No queue that ever referenced this memory may still own DMA. */
    for (unsigned dev = 0; dev < 4; ++dev)
        for (unsigned token = 0; token < 64; ++token)
            if (fake[dev].addresses[token] >= physical && fake[dev].addresses[token] < physical + 4096)
                assert(fake[dev].stopped && !fake[dev].pending[token]);
    frame_live[index] = false; ++frees;
}
bool virtio_pci_open(const struct pci_device *p, uint16_t type, uint64_t allowed,
    uint64_t required, uint32_t config_length, struct virtio_pci_device **out) {
    assert(type == 18 && !allowed && !required && config_length == 16 && current_space == 0x1000);
    unsigned index = (unsigned)(p - pci_devices); assert(index < 4);
    *out = &fake[index]; ++fake[index].opens;
    return !fake[index].fail_open;
}
bool virtio_pci_queue_setup(struct virtio_pci_device *d, uint16_t queue, uint16_t size) {
    assert(queue <= 1 && !d->started && size == (queue ? 8 : 64));
    d->queue_sizes[queue] = size;
    return !d->fail_queue;
}
bool virtio_pci_start(struct virtio_pci_device *d) {
    assert(d->queue_sizes[0] == 64 && d->queue_sizes[1] == 8);
    for (unsigned i = 0; i < 64; ++i) assert(d->pending[i]);
    if (d->fail_start) return false;
    d->started = true;
    return true;
}
bool virtio_pci_submit(struct virtio_pci_device *d, uint16_t queue,
    const struct virtio_pci_buffer *buffers, uint16_t count, uint16_t *token) {
    assert(queue == 0 && count == 1 && buffers[0].device_write && buffers[0].length == 8);
    assert(!d->stopped && current_space == 0x1000);
    if (++d->submits == d->fail_submit) return false;
    /* Deliberately permute tokens independently of physical event slots. */
    for (unsigned i = 0; i < 64; ++i) {
        unsigned candidate = (d->sequence++ * 17 + 3) % 64;
        if (d->pending[candidate]) continue;
        d->pending[candidate] = true;
        d->addresses[candidate] = buffers[0].physical;
        *token = (uint16_t)candidate;
        return true;
    }
    return false;
}
int virtio_pci_poll(struct virtio_pci_device *d, uint16_t queue, struct virtio_pci_completion *out) {
    assert(queue == 0 && d->started && !d->stopped && current_space == 0x1000);
    if (!d->count) return 0;
    struct queued_event e = d->events[d->head++ % FAKE_EVENTS]; --d->count;
    if (e.token_override == -2) return -1;
    unsigned token = d->head % 64;
    for (unsigned i = 0; i < 64 && !d->pending[token]; ++i) token = (token + 1) % 64;
    assert(d->pending[token]);
    memcpy(pmm_phys_to_virt(d->addresses[token]), &e.event, sizeof(e.event));
    d->pending[token] = false;
    out->token = e.token_override < 0 ? (uint16_t)token : (uint16_t)e.token_override;
    out->length = e.length;
    return 1;
}
bool virtio_pci_stop(struct virtio_pci_device *d) {
    ++d->stops; d->started = false;
    if (d->reset_stuck) return false;
    d->stopped = true;
    memset(d->pending, 0, sizeof(d->pending));
    return true;
}
static unsigned config_payload(struct virtio_pci_device *d, uint8_t *data) {
    memset(data, 0, 128);
    if (d->bad_config) return 129;
    if (d->select == 1 && d->subsel == 0) {
        const char *name = d->kind == 1 ? "QEMU Virtio Keyboard" : d->kind == 2 ? "QEMU Virtio Mouse" : "QEMU Virtio Tablet";
        unsigned size = (unsigned)strlen(name) + 1; memcpy(data, name, size); return size;
    }
    if (d->select == 3 && d->subsel == 0) {
        uint16_t ids[4] = {6, 0x0627, (uint16_t)d->kind, (uint16_t)(d->kind == 1 ? 1 : 2)};
        if (d->bad_version) ids[3] = 1;
        memcpy(data, ids, sizeof(ids)); return sizeof(ids);
    }
    if (d->select == 0x11 && d->subsel == 1) {
        if (d->kind == 1) { for (unsigned key = 1; key < 248; ++key) data[key / 8] |= 1u << (key & 7); return 31; }
        data[272 / 8] = 7; return 35;
    }
    if (d->select == 0x11 && d->subsel == 2 && d->kind == 2) {
        data[0] = 3 | (1u << 6); data[1] = 1; return 2;
    }
    if (d->kind == 3 && d->select == 0x11 && d->subsel == 2) {
        data[0] = 1u << 6; data[1] = 1; return 2;
    }
    if (d->kind == 3 && d->select == 0x11 && d->subsel == 3) {
        data[0] = 3 & ~d->missing_axes; return 1;
    }
    if (d->kind == 3 && d->select == 0x12 && d->subsel < 2) {
        memcpy(data, &d->axis_min[d->subsel], 4);
        memcpy(data + 4, &d->axis_max[d->subsel], 4);
        return d->abs_size[d->subsel];
    }
    return 0;
}
bool virtio_pci_config_write8(struct virtio_pci_device *d, uint32_t offset, uint8_t value) {
    assert(offset < 2);
    if (offset) d->subsel = value; else d->select = value;
    return true;
}
bool virtio_pci_config_read(struct virtio_pci_device *d, uint32_t offset, void *dst, uint32_t length) {
    uint8_t data[128]; unsigned size = config_payload(d, data);
    if (offset == 0) {
        assert(length == 3); uint8_t header[3] = {size ? d->select : 0, size ? d->subsel : 0, (uint8_t)size};
        if (d->select == 0x12 && d->header_changed && d->absolute_reads) header[1] ^= 1;
        memcpy(dst, header, 3); return true;
    }
    assert(offset == 8 && length <= size && length <= 128 && d->select != 3);
    assert(offset + length <= (d->kind == 1 ? 39u : 43u)); /* Actual compact configuration. */
    memcpy(dst, data, length); return true;
}
bool virtio_pci_config_read16(struct virtio_pci_device *d, uint32_t offset, uint16_t *value) {
    assert(d->select == 3 && !d->subsel && offset >= 8 && offset <= 14 && !(offset & 1));
    uint8_t data[128]; config_payload(d, data); memcpy(value, data + offset - 8, 2); return true;
}
bool virtio_pci_config_read64(struct virtio_pci_device *d, uint32_t offset, uint64_t *value) {
    assert(d->select == 0x12 && d->subsel < 2 && offset == 8);
    ++d->absolute_reads;
    if (d->generation_fail) return false;
    uint8_t data[128]; config_payload(d, data); memcpy(value, data, 8); return true;
}
static unsigned keyboard_source(struct keyboard_usage_state *state) {
    for (unsigned i = 0; i < 4; ++i) if (state == &inputs[i].keyboard) return i;
    assert(!"unknown keyboard state"); return 0;
}
void keyboard_inject_usage(uint8_t usage, bool down, struct keyboard_usage_state *state) {
    unsigned source = keyboard_source(state);
    uint8_t mask = (uint8_t)(1u << (usage & 7));
    if (((state->pressed[usage / 8] & mask) != 0) == down) return;
    assert(key_count < 1024);
    key_records[key_count++] = (struct key_record){source, usage, down};
    if (down) state->pressed[usage / 8] |= mask; else state->pressed[usage / 8] &= (uint8_t)~mask;
}
void keyboard_repeat_usage(struct keyboard_usage_state *state, uint64_t now) {
    assert(now == clock_now); ++repeat_checks[keyboard_source(state)];
}
void keyboard_release_usage(struct keyboard_usage_state *state) {
    for (unsigned i = 4; i < 256; ++i)
        if (state->pressed[i / 8] & (1u << (i & 7))) keyboard_inject_usage((uint8_t)i, false, state);
}
void mouse_inject_virtio(unsigned source, int32_t dx, int32_t dy, int32_t wx, int32_t wy, uint8_t buttons) {
    assert(source < 4);
    if (!dx && !dy && !wx && !wy && buttons == pointer_buttons[source]) return;
    assert(pointer_count < 1024);
    pointer_records[pointer_count++] = (struct pointer_record){source, dx, dy, wx, wy, buttons, false, false};
    pointer_buttons[source] = buttons;
}
void mouse_inject_virtio_absolute(unsigned source, int32_t x, int32_t y, int32_t wx, int32_t wy, uint8_t buttons) {
    assert(source < 4 && x >= 0 && x <= 65535 && y >= 0 && y <= 65535);
    assert(pointer_count < 1024);
    pointer_records[pointer_count++] = (struct pointer_record){source, x, y, wx, wy, buttons, false, true};
    pointer_buttons[source] = buttons;
}
void mouse_cancel_virtio(unsigned source) {
    assert(source < 4 && pointer_count < 1024);
    pointer_records[pointer_count++] = (struct pointer_record){.source = source, .cancel = true};
    pointer_buttons[source] = 0;
}
static void setup(unsigned devices) {
    memset(inputs, 0, sizeof(inputs)); input_count = 0; attempted = busy = false;
    memset(&statistics, 0, sizeof(statistics)); memset(fake, 0, sizeof(fake));
    memset(frames, 0xa5, sizeof(frames)); memset(frame_live, 0, sizeof(frame_live));
    memset(pci_devices, 0, sizeof(pci_devices)); memset(repeat_checks, 0, sizeof(repeat_checks));
    memset(pointer_buttons, 0, sizeof(pointer_buttons));
    allocations = frees = allocation_fail = key_count = pointer_count = 0;
    clock_now = 100; current_space = 0x1000; pci_count = devices;
    for (unsigned i = 0; i < 4; ++i) {
        pci_devices[i] = (struct pci_device){.vendor_id = 0x1af4, .device_id = 0x1052,
            .class_code = 9, .subclass = i % 2 ? 2 : 0};
        fake[i].index = i; fake[i].kind = i % 2 ? 2 : 1;
        fake[i].axis_max[0] = fake[i].axis_max[1] = 32767;
        fake[i].abs_size[0] = fake[i].abs_size[1] = 20;
    }
}
static void event(unsigned dev, unsigned type, unsigned code, int32_t value) {
    struct virtio_pci_device *d = &fake[dev]; assert(d->count < FAKE_EVENTS);
    d->events[(d->head + d->count++) % FAKE_EVENTS] = (struct queued_event){
        {(uint16_t)type, (uint16_t)code, (uint32_t)value}, 8, -1};
}
static void sync_event(unsigned dev) { event(dev, 0, 0, 0); }
static void stopped(void) {
    assert(virtio_input_stop() && !virtio_input_ready() && allocations == frees);
}
static void tablet_tests(void);
int main(void) {
    setup(0); assert(!virtio_input_init() && !allocations);
    setup(1); pci_devices[0].subclass = 0x80; assert(!virtio_input_init() && !allocations);
    setup(1); pci_devices[0].subclass = 2; assert(!virtio_input_init() && !allocations);
    setup(1); fake[0].kind = 3; assert(!virtio_input_init() && !allocations && fake[0].stops == 1);
    setup(1); fake[0].bad_config = true; assert(!virtio_input_init() && !allocations);
    setup(1); current_space = 0x2000; assert(!virtio_input_init()); current_space = 0x1000;
    assert(virtio_input_init()); stopped();
    setup(2); assert(virtio_input_init() && virtio_input_ready());
    struct virtio_input_stats stats; virtio_input_get_stats(&stats);
    assert(stats.devices == 2 && stats.keyboards == 1 && stats.mice == 1 && allocations == 2);
    assert(virtio_input_init() && allocations == 2 && fake[0].opens == 1);
    const unsigned codes[] = {42,30,31,32,33,34,35,36,37,38,54,125,126,103,111};
    const unsigned usages[] = {0xe1,4,0x16,7,9,10,11,13,14,15,0xe5,0xe3,0xe7,0x52,0x4c};
    for (unsigned i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) event(0, 1, codes[i], 1);
    virtio_input_poll(); assert(!key_count && inputs[0].report_count == 15 && !repeat_checks[0]);
    sync_event(0); virtio_input_poll(); assert(key_count == 15 && repeat_checks[0] == 1);
    for (unsigned i = 0; i < 15; ++i) assert(key_records[i].usage == usages[i] && key_records[i].down);
    event(0,1,30,2); sync_event(0); virtio_input_poll(); assert(key_count == 15); /* Software repeat only. */
    event(0,4,4,123); event(0,1,700,1); sync_event(0); virtio_input_poll(); assert(key_count == 15);
    event(1,2,0,7); event(1,2,1,-9); event(1,2,8,2); event(1,2,6,-1); event(1,1,272,1);
    virtio_input_poll(); assert(!pointer_count);
    sync_event(1); virtio_input_poll();
    assert(pointer_count == 1 && pointer_records[0].dx == 7 && pointer_records[0].dy == -9);
    assert(pointer_records[0].wx == -48 && pointer_records[0].wy == -96 && pointer_records[0].buttons == 1);
    event(1,1,272,0); event(1,1,273,1); event(1,1,273,0); sync_event(1); virtio_input_poll();
    assert(pointer_count == 4 && pointer_records[1].buttons == 0 && pointer_records[2].buttons == 2 && pointer_records[3].buttons == 0);
    event(1,2,0,INT32_MAX); event(1,2,0,INT32_MAX); event(1,2,1,INT32_MIN);
    event(1,2,8,INT32_MIN); event(1,2,6,INT32_MAX); sync_event(1); virtio_input_poll();
    assert(pointer_records[4].dx == INT32_MAX && pointer_records[4].dy == INT32_MIN);
    assert(pointer_records[4].wx == INT32_MAX && pointer_records[4].wy == INT32_MAX);
    current_space = 0x2000; event(0,1,30,0); sync_event(0); virtio_input_poll();
    assert(fake[0].count == 2 && !virtio_input_stop()); current_space = 0x1000;
    virtio_input_poll(); assert(!key_records[key_count-1].down);
    stopped(); assert(!virtio_input_init());

    setup(1); assert(virtio_input_init());
    for (unsigned i=0;i<100;i++) event(0,4,4,0);
    sync_event(0); virtio_input_poll(); assert(statistics.events == 64 && statistics.reports == 0);
    virtio_input_poll(); assert(statistics.events == 101 && statistics.reports == 1); stopped();
    setup(1); assert(virtio_input_init());
    for(unsigned i=0;i<129;i++) event(0,4,4,0);
    virtio_input_poll(); virtio_input_poll(); virtio_input_poll();
    assert(!virtio_input_ready() && statistics.failed == 1 && allocations == frees && !key_count);
    setup(3); assert(virtio_input_init());
    event(0,1,30,1);sync_event(0);event(2,1,30,1);sync_event(2);virtio_input_poll();assert(key_count==2);
    event(0,0,3,0);virtio_input_poll();assert(key_count==3 && key_records[2].source==0 && !key_records[2].down);
    assert(inputs[2].keyboard.pressed[0]&(1u<<4));assert(statistics.devices==2);stopped();
    setup(1); assert(virtio_input_init());event(0,1,30,1);sync_event(0);virtio_input_poll();
    event(0,1,31,1);virtio_input_poll();clock_now+=REPORT_TIMEOUT_US;virtio_input_poll();
    assert(statistics.failed==1 && key_count==2 && !key_records[1].down && !virtio_input_ready());
    setup(1); assert(virtio_input_init());event(0,1,30,1);virtio_input_poll();
    clock_now+=REPORT_TIMEOUT_US*2;sync_event(0);virtio_input_poll();
    assert(virtio_input_ready() && key_count==1 && key_records[0].down && !statistics.failed);stopped();
    setup(2); assert(virtio_input_init());event(1,1,272,1);sync_event(1);virtio_input_poll();
    event(1,1,273,3);sync_event(1);virtio_input_poll();
    assert(pointer_records[pointer_count-1].cancel && !inputs[1].active && inputs[0].active);stopped();
    for(unsigned failure=0;failure<6;failure++) {
        setup(1);
        if(failure==0) fake[0].fail_open=true;
        if(failure==1) fake[0].fail_queue=true;
        if(failure==2) allocation_fail=1;
        if(failure==3) fake[0].fail_submit=3;
        if(failure==4) fake[0].fail_start=true;
        if(failure==5) { fake[0].fail_submit=3;fake[0].reset_stuck=true; }
        assert(!virtio_input_init());
        assert(failure==5 ? (allocations==1 && !frees && !virtio_input_stop() && statistics.quarantined==1) : allocations==frees);
    }
    for(unsigned failure=0;failure<5;failure++) {
        setup(1);assert(virtio_input_init());event(0,1,30,1);sync_event(0);virtio_input_poll();
        event(0,1,30,0);
        struct queued_event *next=&fake[0].events[fake[0].head%FAKE_EVENTS];
        if(failure==0) next->length=7;
        if(failure==1) next->token_override=255;
        if(failure==2) next->token_override=-2;
        if(failure==3) fake[0].fail_submit=fake[0].submits+1;
        if(failure==4) { next->length=9;fake[0].reset_stuck=true; }
        virtio_input_poll();assert(!virtio_input_ready() && statistics.failed==1 && key_count==2 && !key_records[1].down);
        assert(failure==4 ? (!frees && statistics.quarantined==1 && !virtio_input_stop()) : allocations==frees);
    }
    virtio_input_get_stats(NULL);
    tablet_tests();
    puts("virtio-input: PASS identities/config, populated event queue, permuted token recycling, SYN batching/bounds, key mapping, relative scroll, source retirement and reset-before-free quarantine");
}

static void tablet_setup(void) {
    setup(1); fake[0].kind = 3; pci_devices[0].subclass = 0x80;
}
static struct pointer_record last_absolute(int32_t x, int32_t y, unsigned buttons) {
    assert(pointer_count);
    struct pointer_record p = pointer_records[pointer_count - 1];
    assert(p.absolute && !p.cancel && p.source == 0 && p.dx == x && p.dy == y && p.buttons == buttons);
    return p;
}
static void tablet_xy(int32_t x, int32_t y) {
    event(0,3,0,x); event(0,3,1,y); sync_event(0); virtio_input_poll();
}
static void tablet_tests(void) {
    tablet_setup(); assert(virtio_input_init());
    assert(statistics.devices == 1 && statistics.tablets == 1 && !statistics.mice && !statistics.keyboards);
    assert(fake[0].absolute_reads == 2);
    event(0,1,272,1); event(0,2,8,1); sync_event(0); virtio_input_poll();
    assert(!pointer_count && inputs[0].buttons == 1); /* Never click/scroll the old pointer. */
    event(0,3,0,0); sync_event(0); virtio_input_poll();
    assert(!pointer_count && inputs[0].axis_seen == 1);
    event(0,3,1,32767); sync_event(0); virtio_input_poll();
    struct pointer_record p = last_absolute(0,65535,1);
    assert(pointer_count == 1 && !p.wx && !p.wy); /* Retained held button, no old wheel. */
    event(0,1,272,0); sync_event(0); virtio_input_poll(); last_absolute(0,65535,0);
    tablet_xy(0,0); last_absolute(0,0,0);
    tablet_xy(32767,0); last_absolute(65535,0,0);
    tablet_xy(32767,32767); last_absolute(65535,65535,0);
    tablet_xy(0,32767); last_absolute(0,65535,0);
    event(0,3,0,16384); sync_event(0); virtio_input_poll(); last_absolute(32768,65535,0);
    event(0,3,1,8192); sync_event(0); virtio_input_poll(); last_absolute(32768,16384,0);
    tablet_xy(-123,INT32_MAX); last_absolute(0,65535,0); /* Out-of-range samples clamp. */
    unsigned before = pointer_count;
    event(0,3,2,456); sync_event(0); sync_event(0); virtio_input_poll();
    assert(pointer_count == before); /* Unknown axes/empty reports cannot reposition. */
    event(0,2,8,-2); event(0,2,6,1); sync_event(0); virtio_input_poll();
    p = last_absolute(0,65535,0); assert(p.wx == 48 && p.wy == 96);
    before = pointer_count;
    event(0,1,272,1); event(0,1,272,0); sync_event(0); virtio_input_poll();
    assert(pointer_count == before + 2 && pointer_records[before].buttons == 1);
    assert(pointer_records[before].absolute && pointer_records[before+1].absolute);
    last_absolute(0,65535,0); stopped(); assert(!statistics.tablets);

    tablet_setup(); assert(virtio_input_init());
    event(0,1,272,1); event(0,1,272,0); event(0,2,8,10); sync_event(0); virtio_input_poll();
    event(0,3,1,100); sync_event(0); virtio_input_poll(); assert(!pointer_count);
    stopped(); assert(!pointer_count && !inputs[0].buttons && !inputs[0].axis_seen);
    tablet_setup(); assert(virtio_input_init());
    event(0,1,272,1); event(0,1,272,0); sync_event(0); virtio_input_poll();
    tablet_xy(10,20); assert(pointer_count == 1); last_absolute(20,40,0); stopped();

    tablet_setup(); fake[0].axis_min[0] = -100; fake[0].axis_max[0] = 100;
    fake[0].axis_min[1] = -200; fake[0].axis_max[1] = -100;
    assert(virtio_input_init());
    tablet_xy(-100,-200); last_absolute(0,0,0);
    tablet_xy(0,-150); last_absolute(32767,32767,0);
    tablet_xy(100,-100); last_absolute(65535,65535,0); stopped();
    tablet_setup(); fake[0].axis_min[0] = INT32_MIN; fake[0].axis_max[0] = INT32_MAX;
    fake[0].axis_min[1] = -1; fake[0].axis_max[1] = 1;
    assert(virtio_input_init());
    tablet_xy(INT32_MIN,-1); last_absolute(0,0,0);
    tablet_xy(0,0); last_absolute(32767,32767,0);
    tablet_xy(INT32_MAX,1); last_absolute(65535,65535,0); stopped();

    for (unsigned failure = 0; failure < 11; ++failure) {
        tablet_setup();
        if (failure == 0) fake[0].axis_max[0] = 0;
        if (failure == 1) { fake[0].axis_min[1] = 100; fake[0].axis_max[1] = -100; }
        if (failure == 2) fake[0].missing_axes = 1;
        if (failure == 3) fake[0].missing_axes = 2;
        if (failure == 4) fake[0].abs_size[0] = 0;
        if (failure == 5) fake[0].abs_size[0] = 19;
        if (failure == 6) fake[0].abs_size[1] = 21;
        if (failure == 7) fake[0].generation_fail = true;
        if (failure == 8) fake[0].header_changed = true;
        if (failure == 9) fake[0].axis_max[0] = -1;
        if (failure == 10) fake[0].bad_version = true;
        assert(!virtio_input_init() && !allocations && statistics.failed == 1 && !statistics.tablets);
        assert(fake[0].stopped && !pointer_count);
    }

    setup(4); fake[2].kind = 3; pci_devices[2].subclass = 0x80;
    assert(virtio_input_init());
    assert(statistics.devices == 4 && statistics.keyboards == 1 && statistics.mice == 2 && statistics.tablets == 1);
    event(0,1,30,1); sync_event(0);
    event(1,2,0,5); event(1,2,1,-7); event(1,1,272,1); sync_event(1);
    event(2,3,0,32767); event(2,3,1,0); event(2,1,273,1); sync_event(2); virtio_input_poll();
    assert(key_count == 1 && key_records[0].usage == 4);
    assert(pointer_count == 2 && !pointer_records[0].absolute && pointer_records[0].source == 1);
    assert(pointer_records[0].dx == 5 && pointer_records[0].dy == -7 && pointer_records[0].buttons == 1);
    assert(pointer_records[1].absolute && pointer_records[1].source == 2 && pointer_records[1].dx == 65535);
    event(2,0,3,0); virtio_input_poll();
    assert(pointer_records[pointer_count-1].cancel && pointer_records[pointer_count-1].source == 2);
    assert(pointer_buttons[1] == 1 && statistics.devices == 3 && !statistics.tablets); stopped();

    for (unsigned stuck = 0; stuck < 2; ++stuck) {
        tablet_setup(); assert(virtio_input_init()); tablet_xy(1,2);
        event(0,1,272,1); sync_event(0); virtio_input_poll();
        event(0,3,0,3); fake[0].events[fake[0].head % FAKE_EVENTS].length = 7;
        fake[0].reset_stuck = stuck != 0; virtio_input_poll();
        assert(!virtio_input_ready() && !statistics.tablets && statistics.failed == 1);
        assert(pointer_records[pointer_count-1].cancel && !inputs[0].buttons && !inputs[0].axis_seen);
        assert(stuck ? (!frees && statistics.quarantined == 1 && !virtio_input_stop()) : frees == allocations);
    }
    puts("virtio-tablet: PASS exact profile, generation-checked signed bounds, corners/clipping, partial axes, safe first-position buttons/wheel, coexistence and retirement");
}
