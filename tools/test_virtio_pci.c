/* Standalone: cc -std=c11 -Wall -Wextra -Werror -Ikernel/include
 * -Ibuild/limine-protocol/include tools/test_virtio_pci.c -o /tmp/test-virtio-pci */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VIRTIO_PCI_HOST_TEST
#include "../kernel/src/drivers/virtio_pci.c"

#define BASE 0xf0000000ull
#define DMA_BASE 0x100000ull
#define DMA_FRAMES 72u
#define COMMON 0x0000u
#define NOTIFY 0x1000u
#define ISR 0x2000u
#define CONFIG 0x3000u
static struct pci_device pci_devices[8];
static uint8_t cfg[8][256], mmio[8][0x4000];
static _Alignas(16) uint8_t frames[DMA_FRAMES][4096];
static bool live[DMA_FRAMES];
static uint64_t mapped[8][VPCI_MAX_PAGES];
static uint16_t qsizes[8][3], qenabled[8][3], qvectors[8][3];
static uint64_t qaddresses[8][3][3], feature_offer, feature_accept[8];
static uint64_t current_cr3;
static unsigned allocs, frees, maps, resets, notifications, command_writes, bar_writes, preflights;
static unsigned firmware_resets;
static bool firmware_reset_allowed;
static unsigned fail_alloc, fail_map, notify_offset, generation_reads;
static bool collision, reject_selector, reject_features, reject_start, stuck_reset;
static bool reject_size, reject_enable, mutate_generation, reject_disable;
static bool reference_clock;
static uint64_t clock_us;
static unsigned pauses;
static uint32_t get(const uint8_t *p, unsigned at, unsigned width) {
    uint32_t value = 0; memcpy(&value, p + at, width); return value;
}
static void put(uint8_t *p, unsigned at, unsigned width, uint32_t value) {
    memcpy(p + at, &value, width);
}
static unsigned pindex(const struct pci_device *p) {
    assert(p >= pci_devices && p < pci_devices + 8); return (unsigned)(p - pci_devices);
}
uint8_t pci_read_config8(const struct pci_device *p, uint8_t at) {
    unsigned d = pindex(p);
    if (at == 0x98 + 16) {
        ++preflights;
        assert(get(cfg[d], 0x98 + 4, 1) == 4);
        assert(get(cfg[d], 0x98 + 12, 4) == 1);
        assert(get(cfg[d], 0x98 + 8, 4) == COMMON + VPCI_STATUS);
        return mmio[d][VPCI_STATUS];
    }
    return (uint8_t)get(cfg[d], at, 1);
}
uint16_t pci_read_config16(const struct pci_device *p, uint8_t at) { return (uint16_t)get(cfg[pindex(p)], at, 2); }
uint32_t pci_read_config32(const struct pci_device *p, uint8_t at) {
    uint32_t value = get(cfg[pindex(p)], at, 4);
    if (at == 0x20 && value == 0xffffffffu) return 0xffffc000;
    return value;
}
static void config_write(const struct pci_device *p, unsigned at, unsigned width, uint32_t value) {
    unsigned d = pindex(p);
    if (at == 0xa8) {
        assert(firmware_reset_allowed && pci_devices[d].device_id == 0x1042 && width == 1 && !value);
        assert(!bar_writes && !command_writes && !allocs && !maps);
        assert(get(cfg[d], 0x9c, 1) == 4 && get(cfg[d], 0xa0, 4) == VPCI_STATUS && get(cfg[d], 0xa4, 4) == 1);
        ++firmware_resets;
        if (!stuck_reset) { mmio[d][VPCI_STATUS] = 0; memset(qenabled[d], 0, sizeof(qenabled[d])); }
        return;
    }
    if (at == 4) {
        ++command_writes;
        assert(preflights && width == 2);
        if (reject_disable && !(value & 6)) return;
    }
    if (at >= 0x10 && at < 0x28) {
        ++bar_writes;
        assert(preflights && !(get(cfg[d], 4, 2) & 6) && width == 4);
    }
    if (reject_selector && at == 0xa4 && value == 1) return;
    put(cfg[d], at, width, value);
}
void pci_write_config8(const struct pci_device *p, uint8_t at, uint8_t v) { config_write(p, at, 1, v); }
void pci_write_config16(const struct pci_device *p, uint8_t at, uint16_t v) { config_write(p, at, 2, v); }
void pci_write_config32(const struct pci_device *p, uint8_t at, uint32_t v) { config_write(p, at, 4, v); }
uint64_t vmm_kernel_address_space(void) { return 0x1000; }
uint64_t vmm_current_address_space(void) { return current_cr3; }
bool vmm_virt_to_phys_in_space(uint64_t cr3, uint64_t va, uint64_t *pa) {
    assert(cr3 == 0x1000 && va >= VPCI_VIRTUAL && va < VPCI_VIRTUAL + sizeof(mapped) / 8 * 4096);
    unsigned page = (unsigned)((va - VPCI_VIRTUAL) / 4096);
    *pa = mapped[page / VPCI_MAX_PAGES][page % VPCI_MAX_PAGES];
    return collision || *pa;
}
bool vmm_map_page_in_space(uint64_t cr3, uint64_t va, uint64_t pa, uint64_t flags) {
    assert(cr3 == 0x1000 && current_cr3 == cr3);
    assert(flags == (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE));
    if (fail_map && maps + 1 == fail_map) return false;
    unsigned page = (unsigned)((va - VPCI_VIRTUAL) / 4096);
    assert(page < 8 * VPCI_MAX_PAGES && !mapped[page / VPCI_MAX_PAGES][page % VPCI_MAX_PAGES]);
    mapped[page / VPCI_MAX_PAGES][page % VPCI_MAX_PAGES] = pa;
    ++maps; return true;
}
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA && current_cr3 == 0x1000);
    if (fail_alloc && allocs + 1 == fail_alloc) return 0;
    for (unsigned i = 0; i < DMA_FRAMES; ++i) if (!live[i]) {
        live[i] = true; ++allocs; memset(frames[i], 0xa5, 4096); return DMA_BASE + i * 4096;
    }
    return 0;
}
void *pmm_phys_to_virt(uint64_t physical) {
    assert(physical >= DMA_BASE && physical < DMA_BASE + DMA_FRAMES * 4096);
    unsigned i = (unsigned)((physical - DMA_BASE) / 4096);
    assert(live[i]); return frames[i] + (physical & 4095);
}
void pmm_free_frame(uint64_t physical) {
    assert(resets && current_cr3 == 0x1000);
    unsigned i = (unsigned)((physical - DMA_BASE) / 4096);
    assert(i < DMA_FRAMES && live[i]);
    /* Every queue allocation is retired only after its device reset and decode disable. */
    bool owner = false;
    for (unsigned d = 0; d < 8; ++d) for (unsigned q = 0; q < 3; ++q)
        for (unsigned part = 0; part < 3; ++part) if (devices[d].queues[q].dma[part] == physical) {
            assert(!mmio[d][VPCI_STATUS] && !(get(cfg[d], 4, 2) & 6)); owner = true;
        }
    assert(owner); live[i] = false; ++frees;
}
static unsigned mmio_index(uint64_t physical, unsigned width, unsigned *offset) {
    assert(current_cr3 == 0x1000 && physical >= BASE && physical < BASE + 8 * 0x10000);
    unsigned d = (unsigned)((physical - BASE) / 0x10000);
    *offset = (unsigned)((physical - BASE) % 0x10000);
    assert(*offset + width <= 0x4000 && !(physical & (width - 1)) && (get(cfg[d], 4, 2) & 2));
    bool found = false;
    for (unsigned s = 0; s < 8; ++s) for (unsigned i = 0; i < VPCI_MAX_PAGES; ++i)
        if (mapped[s][i] == (physical & ~4095ull)) found = true;
    assert(found); return d;
}
uint32_t virtio_pci_test_mmio_read(uint64_t physical, unsigned width) {
    unsigned at, d = mmio_index(physical, width, &at);
    unsigned q = get(mmio[d], 22, 2);
    if (at == 4) return get(mmio[d], 0, 4) == 0 ? (uint32_t)feature_offer : (uint32_t)(feature_offer >> 32);
    if (at == 21 && mutate_generation) return ++generation_reads;
    if (at >= 24 && at <= 30) assert(q < 3);
    if (at == 24) return qsizes[d][q];
    if (at == 26) return qvectors[d][q];
    if (at == 28) return qenabled[d][q];
    if (at == 30) return notify_offset ? notify_offset : q;
    return get(mmio[d], at, width);
}
void virtio_pci_test_mmio_write(uint64_t physical, unsigned width, uint32_t value) {
    unsigned at, d = mmio_index(physical, width, &at);
    unsigned q = get(mmio[d], 22, 2);
    if (at >= NOTIFY && at < ISR) {
        assert(width == 2 && mmio[d][VPCI_STATUS] == 15 && value < 3);
        assert(get(cfg[d], 4, 2) & 4); ++notifications; return;
    }
    if (at == 12) {
        if (get(mmio[d], 8, 4) == 0) feature_accept[d] = (feature_accept[d] & 0xffffffff00000000ull) | value;
        else feature_accept[d] = (feature_accept[d] & 0xffffffffull) | ((uint64_t)value << 32);
    }
    if (at == VPCI_STATUS) {
        if (!value) {
            ++resets;
            if (stuck_reset) return;
            memset(qenabled[d], 0, sizeof(qenabled[d]));
        }
        if (value == 11 && reject_features) value = 3;
        if (value == 15 && reject_start) value = 11;
    }
    if (at == 24) { if (!reject_size) qsizes[d][q] = (uint16_t)value; return; }
    if (at == 26) { qvectors[d][q] = (uint16_t)value; return; }
    if (at == 28) { if (!reject_enable) qenabled[d][q] = (uint16_t)value; return; }
    if (at >= 32 && at < 56) {
        unsigned part = (at - 32) / 8;
        if (at & 4) qaddresses[d][q][part] = (qaddresses[d][q][part] & 0xffffffffull) | ((uint64_t)value << 32);
        else qaddresses[d][q][part] = (qaddresses[d][q][part] & 0xffffffff00000000ull) | value;
    }
    put(mmio[d], at, width, value);
}
bool timer_reference_ready(void) { return reference_clock; }
uint64_t timer_monotonic_us(void) { clock_us += 10000; return clock_us; }
void virtio_pci_test_pause(void) { ++pauses; }
static void capability(unsigned d, unsigned at, unsigned next, unsigned type, unsigned offset, unsigned len) {
    put(cfg[d], at, 1, 9); put(cfg[d], at + 1, 1, next);
    put(cfg[d], at + 2, 1, type == 2 || type == 5 ? 20 : 16);
    put(cfg[d], at + 3, 1, type); put(cfg[d], at + 4, 1, 4);
    put(cfg[d], at + 8, 4, offset); put(cfg[d], at + 12, 4, len);
    if (type == 2) put(cfg[d], at + 16, 4, 4);
}
static void fixture(void) {
    memset(devices, 0, sizeof(devices)); memset(cfg, 0, sizeof(cfg)); memset(mmio, 0, sizeof(mmio));
    memset(mapped, 0, sizeof(mapped)); memset(live, 0, sizeof(live));
    memset(qenabled, 0, sizeof(qenabled)); memset(qvectors, 0, sizeof(qvectors));
    memset(qaddresses, 0, sizeof(qaddresses)); memset(feature_accept, 0, sizeof(feature_accept));
    allocs = frees = maps = resets = notifications = command_writes = bar_writes = preflights = 0;
    fail_alloc = fail_map = notify_offset = generation_reads = 0;
    firmware_resets = 0; firmware_reset_allowed = false;
    collision = reject_selector = reject_features = reject_start = stuck_reset = false;
    reject_size = reject_enable = mutate_generation = reject_disable = false;
    reference_clock = false; clock_us = 0; pauses = 0;
    current_cr3 = 0x1000; feature_offer = VIRTIO_PCI_VERSION_1 | 0xffffff;
    for (unsigned d = 0; d < 8; ++d) {
        pci_devices[d] = (struct pci_device){.device = (uint8_t)d, .vendor_id = 0x1af4, .device_id = 0x1052};
        put(cfg[d], 4, 2, 2); put(cfg[d], 6, 2, 0x10); put(cfg[d], 0x34, 1, 0x40);
        put(cfg[d], 0x20, 4, (uint32_t)(BASE + d * 0x10000));
        capability(d, 0x40, 0x50, 1, COMMON, 56);
        capability(d, 0x50, 0x64, 2, NOTIFY, 4096);
        capability(d, 0x64, 0x74, 3, ISR, 1);
        capability(d, 0x74, 0x98, 4, CONFIG, 136);
        capability(d, 0x98, 0, 5, 0, 0);
        put(mmio[d], 18, 2, 3);
        for (unsigned q = 0; q < 3; ++q) qsizes[d][q] = 256;
    }
}
static struct virtio_pci_device *open_device(unsigned d) {
    struct virtio_pci_device *t = NULL;
    assert(virtio_pci_open(&pci_devices[d], 18, 0, 0, 136, &t));
    assert(t && virtio_pci_features(t) == VIRTIO_PCI_VERSION_1 && feature_accept[d] == VIRTIO_PCI_VERSION_1);
    return t;
}
static struct virtio_pci_device *running(unsigned size) {
    struct virtio_pci_device *t = open_device(0);
    assert(virtio_pci_queue_setup(t, 0, (uint16_t)size));
    assert(virtio_pci_start(t)); return t;
}
static uint16_t submit(struct virtio_pci_device *t, unsigned size) {
    struct virtio_pci_buffer b = {0x200000, size, true}; uint16_t token;
    assert(virtio_pci_submit(t, 0, &b, 1, &token)); return token;
}
static void complete(struct virtio_pci_device *t, uint32_t id, uint32_t length) {
    struct queue *q = &t->queues[0];
    uint16_t *used = pmm_phys_to_virt(q->dma[2]);
    struct used_element *ring = (void *)(used + 2);
    ring[used[1] % q->size] = (struct used_element){id, length}; ++used[1];
}
static void clean_stop(struct virtio_pci_device *t) {
    unsigned before = frees;
    assert(virtio_pci_stop(t) && !virtio_pci_quarantined(t) && !virtio_pci_active(t));
    assert(virtio_pci_stop(t) && frees >= before);
}
static void test_success(void) {
    fixture(); struct virtio_pci_device *t = open_device(0);
    assert(!virtio_pci_queue_setup(t, 3, 8)); assert(!virtio_pci_queue_setup(t, 0, 7));
    assert(!virtio_pci_queue_setup(t, 0, 512));
    assert(virtio_pci_queue_setup(t, 0, 256));
    uint16_t token = submit(t, 8); assert(!notifications);
    assert(virtio_pci_queue_setup(t, 1, 8)); assert(virtio_pci_start(t) && notifications == 1);
    struct virtio_pci_completion c;
    assert(!virtio_pci_poll(t, 0, &c)); complete(t, token, 8);
    assert(virtio_pci_poll(t, 0, &c) == 1 && c.token == token && c.length == 8);
    assert(!virtio_pci_poll(t, 0, &c));
    assert(qaddresses[0][0][0] == t->queues[0].dma[0]);
    assert(qaddresses[0][0][1] == t->queues[0].dma[1]);
    assert(qaddresses[0][0][2] == t->queues[0].dma[2]);
    unsigned retained_maps = maps;
    clean_stop(t); assert(allocs == frees && maps == retained_maps);
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t));
}
static void test_refusals(void) {
    struct virtio_pci_device *t;
    fixture(); current_cr3 = 0x2000;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!command_writes && !maps);
    fixture(); mmio[0][VPCI_STATUS] = 15;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t));
    assert(!command_writes && !bar_writes && !resets && !maps && preflights == 1);
    fixture(); reject_selector = true;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!command_writes && !bar_writes);
    fixture(); collision = true;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!command_writes && !bar_writes && !preflights);
    fixture(); pci_devices[0].device_id = 0x1000;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!preflights);
    fixture(); assert(!virtio_pci_open(&pci_devices[0], 18, 1ull << 29, 0, 0, &t));
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 1, 0, &t)); assert(!preflights);
}
static void test_caps(void) {
    struct virtio_pci_device *t;
    /* Cycle, overlapping capability, bad notify size, upper-half BAR, region
     * overlap, missing ISR, missing PCI_CFG, cross-BAR span, >4096 config. */
    const unsigned offsets[] = {0x99, 0x41, 0x52, 0x44, 0x7c, 0x67, 0x77, 0x4c, 0x80};
    const unsigned widths[] =  {1, 1, 1, 1, 4, 1, 1, 4, 4};
    const uint32_t values[] =  {0x40, 0x44, 16, 5, 0, 4, 5, 0x5000, 4097};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        fixture(); put(cfg[0], offsets[i], widths[i], values[i]);
        assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t));
        assert(!allocs && !resets);
    }
    fixture(); put(cfg[0], 0x4c, 4, 0x5000);
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!allocs);
}
static void test_failures(void) {
    struct virtio_pci_device *t;
    fixture(); feature_offer = 0;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(resets == 2 && !allocs);
    fixture(); reject_features = true;
    assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(resets == 2);
    for (unsigned i = 1; i <= 3; ++i) {
        fixture(); fail_map = i;
        assert(!virtio_pci_open(&pci_devices[0], 18, 0, 0, 0, &t)); assert(!allocs);
    }
    fixture(); t = open_device(0); fail_map = maps + 1;
    assert(!virtio_pci_queue_setup(t, 0, 8)); clean_stop(t); assert(!allocs);
    for (unsigned i = 1; i <= 3; ++i) {
        fixture(); t = open_device(0); fail_alloc = i;
        assert(!virtio_pci_queue_setup(t, 0, 8)); assert(!frees); clean_stop(t);
        assert(allocs == i - 1 && frees == allocs && resets == 2);
    }
    fixture(); t = open_device(0); notify_offset = 2048;
    assert(!virtio_pci_queue_setup(t, 0, 8)); clean_stop(t); assert(!allocs);
    fixture(); t = open_device(0); reject_size = true;
    assert(!virtio_pci_queue_setup(t, 0, 8)); clean_stop(t); assert(!allocs);
    fixture(); t = open_device(0); reject_enable = true;
    assert(!virtio_pci_queue_setup(t, 0, 8)); clean_stop(t); assert(allocs == frees && allocs == 3);
    fixture(); t = open_device(0); assert(virtio_pci_queue_setup(t, 0, 8)); reject_start = true;
    assert(!virtio_pci_start(t)); clean_stop(t); assert(allocs == frees);
}
static void test_queue_integrity(void) {
    struct virtio_pci_completion c;
    for (unsigned mode = 0; mode < 6; ++mode) {
        fixture(); struct virtio_pci_device *t = running(8); uint16_t token = submit(t, 8);
        if (mode == 0) complete(t, 8, 8);
        if (mode == 1) complete(t, token + 1, 8);
        if (mode == 2) complete(t, token, 9);
        if (mode == 3) { submit(t, 8); complete(t, token, 8); complete(t, token, 8); }
        if (mode == 4) { complete(t, token, 8); assert(virtio_pci_poll(t, 0, &c) == 1); complete(t, token, 8); }
        if (mode == 5) { t->queues[0].next[token] = token; complete(t, token, 8); }
        assert(virtio_pci_poll(t, 0, &c) == -1);
        assert(virtio_pci_get_status(t) == VIRTIO_PCI_BAD_COMPLETION && !frees);
        clean_stop(t); assert(allocs == frees);
    }
    fixture(); struct virtio_pci_device *t = running(8);
    struct virtio_pci_buffer b[3] = {{0x200000, 16, false}, {0x201000, 512, true}, {0x202000, 1, true}};
    uint16_t token;
    assert(virtio_pci_submit(t, 0, b, 3, &token));
    complete(t, token, 513); assert(virtio_pci_poll(t, 0, &c) == 1);
    for (unsigned i = 0; i < 8; ++i) assert(t->queues[0].owner[i] == VPCI_NONE);
    b[0].device_write = true; b[1].device_write = false;
    assert(!virtio_pci_submit(t, 0, b, 3, &token));
    b[0].physical = UINT64_MAX - 1; assert(!virtio_pci_submit(t, 0, b, 1, &token));
    b[0].physical = 0x200000; b[0].length = 0; assert(!virtio_pci_submit(t, 0, b, 1, &token));
    for (unsigned i = 0; i < 8; ++i) submit(t, 8);
    b[0].length = 8; assert(!virtio_pci_submit(t, 0, b, 1, &token));
    assert(virtio_pci_get_status(t) == VIRTIO_PCI_OK); clean_stop(t);
}
static void test_wrap(void) {
    fixture(); struct virtio_pci_device *t = running(8);
    struct virtio_pci_completion c;
    for (unsigned i = 0; i < 65540; ++i) {
        uint16_t token = submit(t, 8); complete(t, token, 8);
        assert(virtio_pci_poll(t, 0, &c) == 1 && c.token == token && c.length == 8);
    }
    assert(t->queues[0].available == 4 && t->queues[0].used == 4); clean_stop(t);
}
static void test_config_and_context(void) {
    fixture(); struct virtio_pci_device *t = open_device(0);
    put(mmio[0], CONFIG, 4, 0x12345678); put(mmio[0], CONFIG + 4, 4, 0x9abcdef0);
    uint8_t b; uint16_t s; uint32_t u; uint64_t v;
    assert(virtio_pci_config_read(t, 0, &b, 1) && b == 0x78);
    assert(virtio_pci_config_read16(t, 0, &s) && s == 0x5678);
    assert(virtio_pci_config_read32(t, 0, &u) && u == 0x12345678);
    assert(virtio_pci_config_read64(t, 0, &v) && v == 0x9abcdef012345678ull);
    assert(!virtio_pci_config_read32(t, 1, &u)); assert(!virtio_pci_config_read(t, 136, &b, 1));
    assert(!virtio_pci_config_read(t, UINT32_MAX, &b, 4));
    assert(virtio_pci_config_write8(t, 1, 9) && mmio[0][CONFIG + 1] == 9);
    assert(!virtio_pci_config_write8(t, 136, 9));
    current_cr3 = 0x2000;
    assert(!virtio_pci_queue_setup(t, 0, 8) && !virtio_pci_start(t) && !virtio_pci_stop(t));
    assert(!virtio_pci_config_read(t, 0, &b, 1) && !virtio_pci_config_write8(t, 0, 1));
    current_cr3 = 0x1000; mutate_generation = true;
    assert(!virtio_pci_config_read64(t, 0, &v) && generation_reads == 32);
    assert(virtio_pci_get_status(t) == VIRTIO_PCI_BAD_CONFIG); clean_stop(t);
}
static void test_quarantine(void) {
    fixture(); struct virtio_pci_device *t = running(8); submit(t, 8); stuck_reset = true;
    assert(!virtio_pci_stop(t) && virtio_pci_quarantined(t) && !frees);
    assert(!(get(cfg[0], 4, 2) & 6)); assert(!virtio_pci_stop(t) && !frees);
    assert(virtio_pci_get_status(t) == VIRTIO_PCI_RESET_FAILED && !virtio_pci_active(t));
    struct virtio_pci_buffer b = {0x200000, 8, true}; uint16_t token;
    assert(!virtio_pci_submit(t, 0, &b, 1, &token));
    fixture(); t = running(8); stuck_reset = true; reference_clock = true;
    assert(!virtio_pci_stop(t) && virtio_pci_quarantined(t) && !frees);
    assert(pauses < VPCI_RESET_POLLS && clock_us >= 100000);
    fixture(); t = open_device(0); fail_alloc = 2;
    assert(!virtio_pci_queue_setup(t, 0, 8)); stuck_reset = true;
    assert(!virtio_pci_stop(t) && !frees && allocs == 1 && virtio_pci_quarantined(t));
}
static void test_many_devices(void) {
    fixture(); struct virtio_pci_device *t[8];
    for (unsigned d = 0; d < 8; ++d) {
        t[d] = open_device(d);
        assert(virtio_pci_queue_setup(t[d], 0, 8)); assert(virtio_pci_start(t[d]));
        uint16_t token = submit(t[d], 8); complete(t[d], token, 8);
        struct virtio_pci_completion c; assert(virtio_pci_poll(t[d], 0, &c) == 1);
    }
    for (unsigned d = 0; d < 8; ++d) clean_stop(t[d]);
    assert(allocs == frees && allocs == 24);
    fixture(); struct virtio_pci_device *a = open_device(0), *b;
    put(cfg[1], 0x20, 4, BASE);
    assert(!virtio_pci_open(&pci_devices[1], 18, 0, 0, 0, &b)); clean_stop(a);
}
static void test_firmware_handoff(void) {
    struct virtio_pci_device *t, *second;
    struct virtio_pci_admission admission;
    fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = 15;
    assert(!virtio_pci_open(&pci_devices[0], 2, 0, 0, 8, &t));
    virtio_pci_get_admission(t, &admission);
    assert(admission.status == 15 && admission.selector_ok && !admission.firmware_reset);
    assert(!bar_writes && !command_writes && !allocs && !firmware_resets);
    /* Even a refused generic claim cannot be retried as a firmware adoption. */
    firmware_reset_allowed = true;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &second));
    assert(!firmware_resets);

    fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = 15;
    firmware_reset_allowed = true; qenabled[0][0] = 1;
    assert(virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t));
    virtio_pci_get_admission(t, &admission);
    assert(admission.status == 15 && admission.selector_ok && admission.firmware_reset && admission.reset_ok);
    assert(firmware_resets == 1 && bar_writes && !allocs);
    assert(get(cfg[0], 0x9c, 1) == 4 && !get(cfg[0], 0xa0, 4) && !get(cfg[0], 0xa4, 4));
    assert(virtio_pci_queue_setup(t, 0, 8) && virtio_pci_start(t));
    unsigned before = preflights;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &second));
    assert(preflights == before && firmware_resets == 1); clean_stop(t);

    fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = 15;
    firmware_reset_allowed = stuck_reset = reference_clock = true;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t));
    virtio_pci_get_admission(t, &admission);
    assert(admission.status == 15 && admission.firmware_reset && !admission.reset_ok);
    assert(virtio_pci_quarantined(t) && virtio_pci_get_status(t) == VIRTIO_PCI_RESET_FAILED);
    assert(firmware_resets == 1 && !bar_writes && !maps && !allocs && !frees && !(get(cfg[0], 4, 2) & 6));
    assert(get(cfg[0], 0x9c, 1) == 4 && !get(cfg[0], 0xa0, 4) && !get(cfg[0], 0xa4, 4));
    assert(!virtio_pci_stop(t));

    const uint8_t refused_statuses[] = {1, 3, 11, 0x4f, 0x8f, 255};
    for (unsigned i = 0; i < sizeof(refused_statuses); ++i) {
        fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = refused_statuses[i];
        firmware_reset_allowed = true;
        assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t));
        assert(!firmware_resets && !bar_writes && !allocs && !command_writes);
    }
    fixture(); firmware_reset_allowed = true;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t)); assert(!preflights);
    fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = 15; firmware_reset_allowed = true;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 137, &t)); assert(!preflights && !firmware_resets);
    fixture(); pci_devices[0].device_id = 0x1042; mmio[0][VPCI_STATUS] = 15;
    firmware_reset_allowed = reject_selector = true;
    assert(!virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t));
    assert(!firmware_resets && !bar_writes && !allocs);
    /* A different BDF cannot reset a known kernel owner through an alias. */
    fixture(); struct virtio_pci_device *owner = running(8);
    pci_devices[1].device_id = 0x1042; mmio[1][VPCI_STATUS] = 15;
    put(cfg[1], 0x20, 4, BASE); firmware_reset_allowed = true;
    unsigned reads_before = preflights, bars_before = bar_writes, commands_before = command_writes;
    assert(!virtio_pci_open_boot_block(&pci_devices[1], 0, 0, 8, &t));
    assert(!firmware_resets && preflights == reads_before && bar_writes == bars_before && command_writes == commands_before);
    assert(virtio_pci_active(owner) && mmio[0][VPCI_STATUS] == 15); clean_stop(owner);
    /* Unequal bases overlapping an existing BAR are also refused pre-write. */
    fixture(); owner = running(8);
    pci_devices[1].device_id = 0x1042; mmio[1][VPCI_STATUS] = 15;
    put(cfg[1], 0x20, 4, BASE + 0x2000); firmware_reset_allowed = true;
    reads_before = preflights; bars_before = bar_writes; commands_before = command_writes;
    assert(!virtio_pci_open_boot_block(&pci_devices[1], 0, 0, 8, &t));
    assert(!firmware_resets && preflights == reads_before && bar_writes == bars_before && command_writes == commands_before);
    assert(virtio_pci_active(owner)); clean_stop(owner);
    fixture(); pci_devices[0].device_id = 0x1042;
    assert(virtio_pci_open_boot_block(&pci_devices[0], 0, 0, 8, &t));
    virtio_pci_get_admission(t, &admission);
    assert(!admission.status && admission.selector_ok && !admission.firmware_reset); clean_stop(t);
}
int main(void) {
    test_success(); test_refusals(); test_caps(); test_failures();
    test_queue_integrity(); test_wrap(); test_config_and_context(); test_quarantine(); test_many_devices(); test_firmware_handoff();
    puts("virtio-pci: ownership, caps, features, queue integrity, wrap, config, faults and quarantine passed");
    return 0;
}
