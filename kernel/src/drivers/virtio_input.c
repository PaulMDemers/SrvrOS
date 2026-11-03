/* VirtIO 1.2 5.8; QEMU v11.1.2 hw/input/virtio-input-hid.c and Linux
 * input-event-codes.h define the admitted identities and evdev event numbers.
 * No IRQ registration, worker creation, tablet conversion or LED feedback. */
#include <srvros/virtio_input.h>
#include <srvros/console.h>
#include <srvros/virtio_pci.h>
#include <srvros/keyboard.h>
#include <srvros/mouse.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>
#include <limits.h>

#define INPUT_DEVICES 4u
#define EVENT_POOL 64u
#define REPORT_LIMIT 128u
#define REPORT_TIMEOUT_US 250000ull
#define WHEEL_PIXELS 48
#define BITMAP_BYTES 128u
#define TOKEN_NONE UINT16_MAX

struct input_event { uint16_t type, code; uint32_t value; };
_Static_assert(sizeof(struct input_event) == 8, "VirtIO input event layout");
_Static_assert(INPUT_DEVICES <= MOUSE_VIRTIO_SOURCES, "independent mouse ownership");
enum profile { PROFILE_NONE, PROFILE_KEYBOARD, PROFILE_MOUSE };
struct input_device {
    struct virtio_pci_device *transport;
    enum profile profile;
    bool active, quarantined;
    uint64_t dma, report_started;
    unsigned index, report_count;
    uint16_t token_slot[VIRTIO_PCI_MAX_QUEUE_SIZE];
    uint8_t keys[BITMAP_BYTES], relative[BITMAP_BYTES], buttons;
    struct input_event report[REPORT_LIMIT];
    struct keyboard_usage_state keyboard;
    const char *stage;
    uint8_t name_size;
};
static struct input_device inputs[INPUT_DEVICES];
static unsigned input_count;
static bool attempted, busy;
static struct virtio_input_stats statistics;

static void clear(void *memory, unsigned length) {
    uint8_t *p = memory;
    for (unsigned i = 0; i < length; ++i) p[i] = 0;
}
static bool enter(void) {
    uint64_t kernel = vmm_kernel_address_space();
    return kernel && vmm_current_address_space() == kernel && !__atomic_test_and_set(&busy, __ATOMIC_ACQUIRE);
}
static void leave(void) { __atomic_clear(&busy, __ATOMIC_RELEASE); }
static bool bit(const uint8_t *bitmap, unsigned code) {
    return code < BITMAP_BYTES * 8 && (bitmap[code / 8] & (1u << (code & 7)));
}
static bool select_config(struct input_device *d, uint8_t select, uint8_t subsel, uint8_t *size) {
    uint8_t header[3];
    return virtio_pci_config_write8(d->transport, 0, select) &&
        virtio_pci_config_write8(d->transport, 1, subsel) &&
        virtio_pci_config_read(d->transport, 0, header, sizeof(header)) &&
        (!header[2] || (header[0] == select && header[1] == subsel)) &&
        header[2] <= 128 && (*size = header[2], true);
}
static bool config_array(struct input_device *d, uint8_t select, uint8_t subsel, uint8_t *data, uint8_t *size) {
    uint8_t header[3];
    clear(data, 128);
    if (!select_config(d, select, subsel, size)) return false;
    if (*size && !virtio_pci_config_read(d->transport, 8, data, *size)) return false;
    return virtio_pci_config_read(d->transport, 0, header, sizeof(header)) &&
        header[2] == *size && (!*size || (header[0] == select && header[1] == subsel));
}
static bool name_is(const uint8_t *name, uint8_t size, const char *expected) {
    unsigned i = 0;
    while (expected[i]) { if (i >= size || name[i] != (uint8_t)expected[i]) return false; ++i; }
    return size == i + 1 && !name[i];
}
static bool identify(struct input_device *d) {
    uint8_t data[128], size;
    d->stage = "name";
    if (!config_array(d, 1, 0, data, &size)) return false;
    d->name_size = size;
    bool keyboard = name_is(data, size, "QEMU Virtio Keyboard");
    bool mouse = name_is(data, size, "QEMU Virtio Mouse");
    if (!keyboard && !mouse) return false;
    d->profile = keyboard ? PROFILE_KEYBOARD : PROFILE_MOUSE;
    d->stage = "ids";
    uint16_t bus, vendor, product, version;
    if (!select_config(d, 3, 0, &size) || size != 8 ||
        !virtio_pci_config_read16(d->transport, 8, &bus) ||
        !virtio_pci_config_read16(d->transport, 10, &vendor) ||
        !virtio_pci_config_read16(d->transport, 12, &product) ||
        !virtio_pci_config_read16(d->transport, 14, &version) ||
        bus != 6 || vendor != 0x0627 || product != (keyboard ? 1 : 2) || version != (keyboard ? 1 : 2)) return false;
    d->stage = "key-bits";
    if (!config_array(d, 0x11, 1, d->keys, &size) || !size) return false;
    d->stage = "relative-bits";
    if (!config_array(d, 0x11, 2, d->relative, &size)) return false;
    d->stage = "absolute-bits";
    if (!config_array(d, 0x11, 3, data, &size) || size) return false; /* No ABS/tablet. */
    d->stage = "profile";
    if (keyboard) {
        if (!bit(d->keys, 1) || !bit(d->keys, 28) || !bit(d->keys, 30) ||
            !bit(d->keys, 29) || !bit(d->keys, 42) || !bit(d->keys, 58) ||
            bit(d->relative, 0) || bit(d->relative, 1)) return false;
        d->profile = PROFILE_KEYBOARD;
    } else {
        if (!bit(d->keys, 272) || !bit(d->relative, 0) || !bit(d->relative, 1)) return false;
        d->profile = PROFILE_MOUSE;
    }
    return true;
}

/* Evdev key codes are not USB usages. Keep this translation explicit; unknown
 * multimedia/vendor keys are ignored, never truncated into another key. */
static uint8_t key_usage(uint16_t code) {
    static const uint8_t usages[128] = {
        [1]=0x29,[2]=0x1e,[3]=0x1f,[4]=0x20,[5]=0x21,[6]=0x22,[7]=0x23,
        [8]=0x24,[9]=0x25,[10]=0x26,[11]=0x27,[12]=0x2d,[13]=0x2e,[14]=0x2a,[15]=0x2b,
        [16]=0x14,[17]=0x1a,[18]=0x08,[19]=0x15,[20]=0x17,[21]=0x1c,[22]=0x18,[23]=0x0c,
        [24]=0x12,[25]=0x13,[26]=0x2f,[27]=0x30,[28]=0x28,[29]=0xe0,[30]=0x04,[31]=0x16,
        [32]=0x07,[33]=0x09,[34]=0x0a,[35]=0x0b,[36]=0x0d,[37]=0x0e,[38]=0x0f,[39]=0x33,
        [40]=0x34,[41]=0x35,[42]=0xe1,[43]=0x31,[44]=0x1d,[45]=0x1b,[46]=0x06,[47]=0x19,
        [48]=0x05,[49]=0x11,[50]=0x10,[51]=0x36,[52]=0x37,[53]=0x38,[54]=0xe5,[55]=0x55,
        [56]=0xe2,[57]=0x2c,[58]=0x39,[59]=0x3a,[60]=0x3b,[61]=0x3c,[62]=0x3d,[63]=0x3e,
        [64]=0x3f,[65]=0x40,[66]=0x41,[67]=0x42,[68]=0x43,[69]=0x53,[70]=0x47,[71]=0x5f,
        [72]=0x60,[73]=0x61,[74]=0x56,[75]=0x5c,[76]=0x5d,[77]=0x5e,[78]=0x57,[79]=0x59,
        [80]=0x5a,[81]=0x5b,[82]=0x62,[83]=0x63,[86]=0x64,[87]=0x44,[88]=0x45,[96]=0x58,
        [97]=0xe4,[98]=0x54,[99]=0x46,[100]=0xe6,[102]=0x4a,[103]=0x52,[104]=0x4b,[105]=0x50,
        [106]=0x4f,[107]=0x4d,[108]=0x51,[109]=0x4e,[110]=0x49,[111]=0x4c,[117]=0x67,
        [119]=0x48,[125]=0xe3,[126]=0xe7,[127]=0x65,
    };
    return code < sizeof(usages) ? usages[code] : 0;
}
static int32_t bounded(int64_t value) {
    return value > INT32_MAX ? INT32_MAX : value < INT32_MIN ? INT32_MIN : (int32_t)value;
}
static bool retire(struct input_device *d, bool failure) {
    if (failure) ++statistics.failed;
    if (d->active) {
        --statistics.devices;
        if (d->profile == PROFILE_KEYBOARD) { --statistics.keyboards; keyboard_release_usage(&d->keyboard); }
        else if (d->profile == PROFILE_MOUSE) { --statistics.mice; mouse_cancel_virtio(d->index); }
    }
    d->active = false;
    d->report_count = 0;
    if (!d->transport) return true;
    if (!virtio_pci_stop(d->transport)) {
        if (!d->quarantined) ++statistics.quarantined;
        d->quarantined = true;
        return false; /* Event DMA remains pinned even after a valid completion. */
    }
    if (d->dma) pmm_free_frame(d->dma);
    d->dma = 0;
    return true;
}
static bool populate(struct input_device *d, unsigned slot) {
    struct virtio_pci_buffer buffer = {d->dma + slot * sizeof(struct input_event), sizeof(struct input_event), true};
    uint16_t token;
    if (!virtio_pci_submit(d->transport, 0, &buffer, 1, &token)) return false;
    if (token >= VIRTIO_PCI_MAX_QUEUE_SIZE || d->token_slot[token] != TOKEN_NONE) return false;
    d->token_slot[token] = (uint16_t)slot;
    return true;
}
static bool begin_device(struct input_device *d, const struct pci_device *pci) {
    for (unsigned i = 0; i < VIRTIO_PCI_MAX_QUEUE_SIZE; ++i) d->token_slot[i] = TOKEN_NONE;
    d->stage = "open";
    /* QEMU advertises only largest supported payload + 8, not the maximum
     * 136-byte C union. Each later payload read is separately range checked. */
    if (!virtio_pci_open(pci, 18, 0, 0, 16, &d->transport)) return false;
    if (!identify(d) || pci->subclass != (d->profile == PROFILE_KEYBOARD ? 0 : 2)) return false;
    d->stage = "eventq";
    if (!virtio_pci_queue_setup(d->transport, 0, EVENT_POOL)) return false;
    d->stage = "statusq";
    if (!virtio_pci_queue_setup(d->transport, 1, 8)) return false;
    d->stage = "memory";
    d->dma = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
    if (!d->dma) return false;
    clear(pmm_phys_to_virt(d->dma), 4096);
    d->stage = "populate";
    for (unsigned slot = 0; slot < EVENT_POOL; ++slot) if (!populate(d, slot)) return false;
    d->stage = "start";
    if (!virtio_pci_start(d->transport)) return false;
    d->stage = "ready";
    d->active = true;
    ++statistics.devices;
    if (d->profile == PROFILE_KEYBOARD) ++statistics.keyboards;
    else ++statistics.mice;
    return true;
}

bool virtio_input_init(void) {
    if (!enter()) return false;
    if (!attempted) {
        attempted = true;
        for (uint64_t i = 0; i < pci_device_count() && input_count < INPUT_DEVICES; ++i) {
            const struct pci_device *pci = pci_device_at(i);
            if (!pci || pci->vendor_id != 0x1af4 || pci->device_id != 0x1052 ||
                pci->class_code != 9 || (pci->subclass != 0 && pci->subclass != 2) ||
                (pci->header_type & 0x7f) != 0) continue;
            struct input_device *d = &inputs[input_count];
            d->index = input_count++;
            if (!begin_device(d, pci)) {
                console_printf("virtio-input: init-failed pci=%u:%u.%u stage=%s profile=%u name-size=%u transport=%s\n",
                    (uint64_t)pci->bus, (uint64_t)pci->device, (uint64_t)pci->function,
                    d->stage, (uint64_t)d->profile, (uint64_t)d->name_size,
                    d->transport ? virtio_pci_status_name(virtio_pci_get_status(d->transport)) : "unclaimed");
                retire(d, true);
            }
        }
    }
    bool ready = statistics.devices != 0;
    leave();
    return ready;
}
static void commit_report(struct input_device *d) {
    if (d->profile == PROFILE_KEYBOARD) {
        for (unsigned i = 0; i < d->report_count; ++i) {
            struct input_event *e = &d->report[i];
            if (e->type == 1 && e->value != 2) {
                uint8_t usage = key_usage(e->code);
                if (usage) keyboard_inject_usage(usage, e->value != 0, &d->keyboard);
            }
        }
    } else {
        int64_t dx = 0, dy = 0, wx = 0, wy = 0;
        uint8_t edges[REPORT_LIMIT];
        unsigned edge_count = 0;
        for (unsigned i = 0; i < d->report_count; ++i) {
            struct input_event *e = &d->report[i];
            int32_t value = (int32_t)e->value;
            if (e->type == 2) {
                if (e->code == 0) dx += value;
                if (e->code == 1) dy += value;
                if (e->code == 6) wx += (int64_t)value * WHEEL_PIXELS;
                if (e->code == 8) wy -= (int64_t)value * WHEEL_PIXELS;
            } else if (e->type == 1 && e->code >= 272 && e->code <= 274 && e->value != 2) {
                uint8_t mask = (uint8_t)(1u << (e->code - 272));
                uint8_t buttons = e->value ? d->buttons | mask : d->buttons & (uint8_t)~mask;
                if (buttons != d->buttons) { d->buttons = buttons; edges[edge_count++] = buttons; }
            }
        }
        /* Preserve even press+release in one report. Movement is committed once
         * at the report's final position, before any remaining button edges. */
        mouse_inject_virtio(d->index, bounded(dx), bounded(dy), bounded(wx), bounded(wy),
            edge_count ? edges[0] : d->buttons);
        for (unsigned i = 1; i < edge_count; ++i) mouse_inject_virtio(d->index, 0, 0, 0, 0, edges[i]);
    }
    d->report_count = 0;
    ++statistics.reports;
}
static bool stage_event(struct input_device *d, struct input_event e, uint64_t now) {
    if (e.type == 0) {
        if (e.code == 3) return false; /* SYN_DROPPED: no safe pressed-state query. */
        if (e.code == 0) {
            if (e.value) return false;
            commit_report(d);
            return true;
        }
    }
    if (d->report_count == REPORT_LIMIT) return false;
    if (!d->report_count) d->report_started = now;
    /* Count ignored events as well, so an unrecognized stream without SYN is
     * bounded. Unsupported events are otherwise ignored as required by 5.8.6. */
    if (e.type == 1 && bit(d->keys, e.code)) {
        if (e.value > 2) return false;
    } else if (e.type != 2 || d->profile != PROFILE_MOUSE || !bit(d->relative, e.code)) {
        e.type = UINT16_MAX;
    }
    d->report[d->report_count++] = e;
    return true;
}
void virtio_input_poll(void) {
    if (!enter()) return;
    uint64_t now = timer_monotonic_us();
    for (unsigned i = 0; i < input_count; ++i) {
        struct input_device *d = &inputs[i];
        if (!d->active) continue;
        bool empty = false;
        for (unsigned n = 0; n < EVENT_POOL; ++n) {
            struct virtio_pci_completion completion;
            int result = virtio_pci_poll(d->transport, 0, &completion);
            if (!result) { empty = true; break; }
            if (result < 0 || completion.token >= VIRTIO_PCI_MAX_QUEUE_SIZE ||
                d->token_slot[completion.token] == TOKEN_NONE || completion.length != sizeof(struct input_event)) {
                retire(d, true); break;
            }
            unsigned slot = d->token_slot[completion.token];
            d->token_slot[completion.token] = TOKEN_NONE;
            struct input_event *events = pmm_phys_to_virt(d->dma);
            struct input_event event = events[slot];
            ++statistics.events;
            if (!stage_event(d, event, now) || !populate(d, slot)) { retire(d, true); break; }
        }
        /* A delayed worker may find a valid SYN already queued. Consume it
         * before declaring a stalled report; a nonempty stream is still bounded
         * by REPORT_LIMIT even when its terminator never arrives. */
        if (d->active && empty && d->report_count && now - d->report_started >= REPORT_TIMEOUT_US)
            retire(d, true);
        if (d->active && d->profile == PROFILE_KEYBOARD && !d->report_count)
            keyboard_repeat_usage(&d->keyboard, now);
    }
    leave();
}
bool virtio_input_ready(void) { return statistics.devices != 0; }
bool virtio_input_stop(void) {
    if (!enter()) return false;
    bool ok = true;
    for (unsigned i = 0; i < input_count; ++i) {
        if (inputs[i].quarantined) ok = false;
        else if (inputs[i].transport && !retire(&inputs[i], false)) ok = false;
    }
    leave();
    return ok;
}
void virtio_input_get_stats(struct virtio_input_stats *out) { if (out) *out = statistics; }
