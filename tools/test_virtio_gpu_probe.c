/* Isolated transport tests: compile with -I kernel/include and the existing
 * build/limine-protocol/include. No guest, shared build outputs or real I/O. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VGPU_PROBE_HOST_TEST
#include "../kernel/src/drivers/virtio_gpu_probe.c"

#define FB 0xe0000000ull
#define FRAME0 0x100000ull
#define BACKING0 0x1000000ull
static struct pci_device devices[3];
static uint8_t config[3][256], mmio[0x4000];
static unsigned device_count, primary_writes, bar_writes, command_writes;
static unsigned allocated, freed, map_count, unmap_count, reset_writes, cfg_reads;
static unsigned fail_alloc, fail_map, free_order, pauses;
static uint64_t mapped[VGPU_MAX_PAGES], current_cr3, bar_base;
static uint8_t frames[3][4096];
static bool missing_version, reject_features, reject_driver, reject_queue;
static bool timeout_mode, stuck_reset, selector_reject, collision;
static bool fail_restore, fail_unmap, reference_clock;
static uint64_t clock_us, clock_step;
static unsigned bad_response;
static bool presenting, frame_live[3], backing_live[2];
static uint8_t *backing_memory[2];
static uint64_t backing_sizes[2], last_fence;
static unsigned backing_allocated, backing_freed, fail_backing;
static unsigned commands, command_types[128], fail_command, host_scanout;
static bool created[2], attached[2];
static unsigned checkpoint_count;
static uint32_t *host_pixels[2], *host_front_snapshot;
static const uint32_t *oracle_staging;
static uint64_t oracle_pitch;
static unsigned oracle_frames, transfers;
static struct gpu_rect last_transfer;

static void host_images_reset(void) {
    for (unsigned i = 0; i < 2; ++i) { free(host_pixels[i]); host_pixels[i] = NULL; }
    free(host_front_snapshot); host_front_snapshot = NULL;
}

static uint32_t get(const uint8_t *p, unsigned offset, unsigned width) {
    uint32_t value = 0; memcpy(&value, p + offset, width); return value;
}
static void put(uint8_t *p, unsigned offset, unsigned width, uint32_t value) {
    memcpy(p + offset, &value, width);
}
static unsigned device_index(const struct pci_device *p) {
    assert(p >= devices && p < devices + 3); return (unsigned)(p - devices);
}
uint64_t pci_device_count(void) { return device_count; }
const struct pci_device *pci_device_at(uint64_t i) { return i < device_count ? &devices[i] : NULL; }
uint8_t pci_read_config8(const struct pci_device *p, uint8_t offset) {
    unsigned i = device_index(p);
    if (i == 1 && offset == 0x94) {
        ++cfg_reads;
        assert(get(config[i], 0x88, 1) == 4);
        assert(get(config[i], 0x90, 4) == 1);
        uint32_t common = get(config[i], 0x48, 4);
        assert(get(config[i], 0x8c, 4) == common + VGPU_STATUS);
        assert(presenting || (!bar_writes && !command_writes)); /* Ownership before mutation. */
        return mmio[common + VGPU_STATUS];
    }
    return (uint8_t)get(config[i], offset, 1);
}
uint16_t pci_read_config16(const struct pci_device *p, uint8_t offset) {
    return (uint16_t)get(config[device_index(p)], offset, 2);
}
uint32_t pci_read_config32(const struct pci_device *p, uint8_t offset) {
    unsigned i = device_index(p);
    uint32_t value = get(config[i], offset, 4);
    if (i == 1 && value == 0xffffffffu) {
        if (offset == 0x20) return 0xffffc000u | (bar_base > UINT32_MAX ? 4u : 0u);
        if (offset == 0x24 && bar_base > UINT32_MAX) return 0xffffffffu;
    }
    return value;
}
static void config_write(const struct pci_device *p, uint8_t offset, unsigned width, uint32_t value) {
    unsigned i = device_index(p);
    if (i == 0) ++primary_writes;
    assert(i == 1);
    if (offset == 4) {
        ++command_writes; assert(width == 2);
        if (fail_restore && reset_writes && value == 2) return;
    }
    if (offset >= 0x10 && offset < 0x28) {
        ++bar_writes;
        assert(cfg_reads && !(get(config[i], 4, 2) & 6));
        assert(width == 4);
        if (presenting) assert(offset >= 0x20); /* Never size or write framebuffer BAR0. */
    }
    assert(offset != 0x94); /* PCI_CFG is read-only data use. */
    if (selector_reject && offset == 0x90 && value == 1) return;
    put(config[i], offset, width, value);
}
void pci_write_config8(const struct pci_device *p, uint8_t at, uint8_t v) { config_write(p, at, 1, v); }
void pci_write_config16(const struct pci_device *p, uint8_t at, uint16_t v) { config_write(p, at, 2, v); }
void pci_write_config32(const struct pci_device *p, uint8_t at, uint32_t v) { config_write(p, at, 4, v); }
uint64_t vmm_kernel_address_space(void) { return 0x1000; }
uint64_t vmm_current_address_space(void) { return current_cr3; }
void vmm_switch_address_space(uint64_t cr3) { current_cr3 = cr3; }
bool vmm_virt_to_phys_in_space(uint64_t cr3, uint64_t virtual, uint64_t *physical) {
    assert(cr3 == 0x1000 && virtual >= VGPU_VIRTUAL);
    if (collision) { *physical = 0xabc000; return true; }
    return false;
}
bool vmm_map_page_in_space(uint64_t cr3, uint64_t virtual, uint64_t physical, uint64_t flags) {
    assert(cr3 == 0x1000 && virtual == VGPU_VIRTUAL + (map_count - unmap_count) * 4096);
    assert(flags == (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE));
    if (fail_map && map_count + 1 == fail_map) return false;
    assert(map_count - unmap_count < VGPU_MAX_PAGES);
    mapped[map_count - unmap_count] = physical;
    ++map_count;
    return true;
}
bool vmm_unmap_page_in_space(uint64_t cr3, uint64_t virtual, uint64_t *physical) {
    (void)physical;
    assert(cr3 == 0x1000 && virtual == VGPU_VIRTUAL + (map_count - unmap_count - 1) * 4096);
    ++unmap_count;
    return !fail_unmap;
}
bool timer_reference_ready(void) { return reference_clock; }
uint64_t timer_monotonic_us(void) { clock_us += clock_step; return clock_us; }
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag == PMM_FRAME_DMA);
    if (fail_alloc && allocated + 1 == fail_alloc) return 0;
    for (unsigned i = 0; i < 3; ++i) if (!frame_live[i]) {
        frame_live[i] = true; ++allocated; return FRAME0 + i * 4096;
    }
    assert(!"too many live queue frames"); return 0;
}
uint64_t pmm_alloc_frames_tagged(uint64_t count, enum pmm_frame_tag tag) {
    assert(presenting && count && tag == PMM_FRAME_DMA);
    if (fail_backing && backing_allocated + 1 == fail_backing) return 0;
    for (unsigned i = 0; i < 2; ++i) if (!backing_live[i]) {
        backing_memory[i] = malloc(count * 4096);
        assert(backing_memory[i]);
        memset(backing_memory[i], 0xa5, count * 4096);
        backing_sizes[i] = count * 4096;
        backing_live[i] = true; ++backing_allocated;
        return BACKING0 + i * VGPU_BACKING_LIMIT;
    }
    assert(!"too many live resource backings"); return 0;
}
void *pmm_phys_to_virt(uint64_t physical) {
    if (physical >= BACKING0) {
        unsigned index = (unsigned)((physical - BACKING0) / VGPU_BACKING_LIMIT);
        unsigned offset = (unsigned)((physical - BACKING0) % VGPU_BACKING_LIMIT);
        assert(index < 2 && backing_live[index] && offset < backing_sizes[index]);
        return backing_memory[index] + offset;
    }
    assert(physical >= FRAME0 && physical < FRAME0 + 3 * 4096);
    return frames[(physical - FRAME0) / 4096] + (physical & 4095);
}
static void free_safe(void) {
    assert(reset_writes && mmio[get(config[1], 0x48, 4) + VGPU_STATUS] == 0);
    assert(!(get(config[1], 4, 2) & 6));
}
void pmm_free_frame(uint64_t physical) {
    free_safe();
    unsigned i = (unsigned)((physical - FRAME0) / 4096);
    assert(i < 3 && frame_live[i]);
    frame_live[i] = false; ++free_order; ++freed;
}
void pmm_free_frame_range(uint64_t physical, uint64_t count) {
    free_safe();
    unsigned i = (unsigned)((physical - BACKING0) / VGPU_BACKING_LIMIT);
    assert(i < 2 && backing_live[i] && backing_sizes[i] == count * 4096);
    assert(host_scanout == 0); /* Reset retired scanout before backing free. */
    free(backing_memory[i]); backing_memory[i] = NULL;
    backing_live[i] = false; ++backing_freed;
}
static unsigned mmio_offset(uint64_t physical, unsigned width) {
    assert(get(config[1], 4, 2) & 2);
    assert(physical >= bar_base && physical + width <= bar_base + sizeof(mmio));
    bool found = false;
    for (unsigned i = 0; i < map_count - unmap_count; ++i)
        if (mapped[i] == (physical & ~4095ull)) found = true;
    assert(found && !(physical & (width - 1)));
    return (unsigned)(physical - bar_base);
}
uint32_t vgpu_test_mmio_read(uint64_t physical, unsigned width) {
    unsigned offset = mmio_offset(physical, width);
    unsigned common = get(config[1], 0x48, 4);
    if (offset == common + 4) return get(mmio, common, 4) == 1 && !missing_version ? 1 : 0;
    return get(mmio, offset, width);
}
static void resource_command(struct gpu_header *request) {
    switch (request->type) {
    case 0x100: break;
    case 0x101: {
        struct create_2d_command *r = (void *)request;
        assert(r->resource >= 1 && r->resource <= 2 && !created[r->resource - 1]);
        assert(r->format == 2 && r->width == presentation.width && r->height == presentation.height);
        host_pixels[r->resource - 1] = malloc((size_t)r->width * r->height * 4);
        assert(host_pixels[r->resource - 1]);
        memset(host_pixels[r->resource - 1], 0xcd, (size_t)r->width * r->height * 4);
        created[r->resource - 1] = true;
        break;
    }
    case 0x106: {
        struct attach_command *r = (void *)request;
        assert(r->resource >= 1 && r->resource <= 2 && created[r->resource - 1]);
        assert(!attached[r->resource - 1] && r->entries == 1 && r->length == presentation.bytes);
        assert(r->address == BACKING0 + (r->resource - 1) * VGPU_BACKING_LIMIT);
        attached[r->resource - 1] = true;
        break;
    }
    case 0x105: {
        struct transfer_command *r = (void *)request;
        assert(r->resource >= 1 && r->resource <= 2 && attached[r->resource - 1]);
        assert(r->resource != host_scanout && r->rect.width && r->rect.height);
        assert(r->rect.x < presentation.width && r->rect.width <= presentation.width - r->rect.x);
        assert(r->rect.y < presentation.height && r->rect.height <= presentation.height - r->rect.y);
        assert(r->offset == ((uint64_t)r->rect.y * presentation.width + r->rect.x) * 4);
        if (host_scanout && host_front_snapshot)
            assert(!memcmp(backing_memory[host_scanout - 1], host_front_snapshot, presentation.bytes));
        for (unsigned y = 0; y < r->rect.height; ++y) {
            uint64_t destination = (uint64_t)(r->rect.y + y) * presentation.width + r->rect.x;
            const uint8_t *source = backing_memory[r->resource - 1] + r->offset + (uint64_t)y * presentation.width * 4;
            memcpy(host_pixels[r->resource - 1] + destination, source, r->rect.width * 4);
        }
        last_transfer = r->rect;
        ++transfers;
        break;
    }
    case 0x103: {
        struct scanout_command *r = (void *)request;
        assert(r->resource >= 1 && r->resource <= 2 && attached[r->resource - 1]);
        assert(r->resource != host_scanout && r->scanout == 0);
        assert(!r->rect.x && !r->rect.y && r->rect.width == presentation.width && r->rect.height == presentation.height);
        host_scanout = r->resource;
        break;
    }
    case 0x104: {
        struct flush_command *r = (void *)request;
        assert(r->resource == host_scanout);
        assert(!r->rect.x && !r->rect.y && r->rect.width == presentation.width && r->rect.height == presentation.height);
        if (oracle_staging) {
            for (unsigned y = 0; y < presentation.height; ++y)
                assert(!memcmp(host_pixels[host_scanout - 1] + (uint64_t)y * presentation.width,
                    (const uint8_t *)oracle_staging + y * oracle_pitch, presentation.width * 4));
            ++oracle_frames;
        }
        if (!host_front_snapshot) host_front_snapshot = malloc(presentation.bytes);
        assert(host_front_snapshot);
        memcpy(host_front_snapshot, backing_memory[host_scanout - 1], presentation.bytes);
        break;
    }
    default: assert(!"unexpected GPU command");
    }
}
static void complete(void) {
    assert((get(config[1], 4, 2) & 0x406) == 0x406);
    unsigned c = get(config[1], 0x48, 4);
    assert(get(mmio, c + VGPU_STATUS, 1) == 15);
    assert(get(mmio, c + 24, 2) == VGPU_QUEUE_SIZE && get(mmio, c + 28, 2) == 1);
    assert(get(mmio, c + 32, 4) == FRAME0 && get(mmio, c + 36, 4) == 0);
    assert(get(mmio, c + 40, 4) == FRAME0 + __builtin_offsetof(struct dma_layout, available_flags));
    assert(get(mmio, c + 48, 4) == FRAME0 + __builtin_offsetof(struct dma_layout, used_flags));
    struct dma_layout *q = (void *)frames[0];
    struct gpu_header *request = (void *)frames[1];
    assert(q->descriptors[0].address == FRAME0 + 4096 && q->descriptors[0].length >= 24);
    assert(q->descriptors[0].flags == 1 && q->descriptors[0].next == 1);
    assert(q->descriptors[1].address == FRAME0 + 8192);
    assert(q->descriptors[1].length == (request->type == 0x100 ? 408 : 24));
    assert(q->descriptors[1].flags == 2 && q->available_flags == 1);
    assert(q->available_index == (uint16_t)(q->used_index + 1));
    assert(q->available_ring[q->used_index % VGPU_QUEUE_SIZE] == 0);
    assert(request->flags == 1 && request->fence == (last_fence ? last_fence + 1 : VGPU_FENCE));
    last_fence = request->fence;
    if (commands < 128) command_types[commands] = request->type;
    ++commands;
    if (timeout_mode) return;
    struct display_response *r = (void *)frames[2];
    r->header.type = request->type == 0x100 ? 0x1101 : 0x1100;
    r->header.flags = 1; r->header.fence = request->fence;
    if (presenting) resource_command(request);
    r->modes[0] = (struct display_mode){0, 0, 1280, 800, 1, 0};
    unsigned slot = q->used_index % VGPU_QUEUE_SIZE;
    q->used_ring[slot].id = 0;
    q->used_ring[slot].length = q->descriptors[1].length;
    ++q->used_index;
    if (fail_command && commands == fail_command) r->header.fence++;
    switch (bad_response) {
    case 1: ++q->used_index; break;
    case 2: q->used_ring[slot].id = 1; break;
    case 3: q->used_ring[slot].length--; break;
    case 4: r->header.type = 0x1200; break;
    case 5: r->header.fence++; break;
    case 6: r->header.flags = 0; break;
    case 7: r->modes[0].width = 0; break;
    case 8: r->modes[0].x = UINT32_MAX; break;
    case 9: r->modes[0].enabled = 0; break;
    case 10: r->modes[1].enabled = 2; break;
    case 11: q->used_ring[slot].length++; break;
    case 12: r->header.context = 1; break;
    case 13: r->modes[0].flags = 1; break;
    case 14: r->modes[0].width = VGPU_MAX_DIMENSION + 1; break;
    case 15: r->modes[0].height = VGPU_MAX_DIMENSION + 1; break;
    default: break;
    }
}
void vgpu_test_mmio_write(uint64_t physical, unsigned width, uint32_t value) {
    unsigned offset = mmio_offset(physical, width);
    unsigned common = get(config[1], 0x48, 4);
    if (offset == common + VGPU_STATUS) {
        assert(width == 1);
        if (value == 0) {
            ++reset_writes;
            if (stuck_reset) return;
            put(mmio, common + 24, 2, 256);
            put(mmio, common + 28, 2, 0);
            memset(created, 0, sizeof(created)); memset(attached, 0, sizeof(attached));
            host_images_reset();
            host_scanout = 0; last_fence = 0;
        }
        if (reject_features && value == 11) value = 3;
        if (reject_driver && value == 15) value = 11;
    }
    if (offset == common + 28 && reject_queue && value == 1) return;
    put(mmio, offset, width, value);
    uint64_t notify = get(config[1], 0x58, 4) +
        (uint64_t)get(mmio, common + 30, 2) * get(config[1], 0x60, 4);
    if (offset == notify) { assert(width == 2 && value == 0); complete(); }
}
void vgpu_test_pause(void) { ++pauses; }
static void cap(unsigned at, unsigned next, unsigned length, unsigned type,
    uint32_t offset, uint32_t region_length) {
    put(config[1], at, 1, 9); put(config[1], at + 1, 1, next);
    put(config[1], at + 2, 1, length); put(config[1], at + 3, 1, type);
    put(config[1], at + 4, 1, 4); put(config[1], at + 8, 4, offset);
    put(config[1], at + 12, 4, region_length);
}
static void setup(void) {
    host_images_reset();
    oracle_staging = NULL; oracle_pitch = 0; oracle_frames = transfers = 0;
    last_transfer = (struct gpu_rect){0};
    for (unsigned i = 0; i < 2; ++i) {
        free(backing_memory[i]); backing_memory[i] = NULL;
    }
    memset(&presentation, 0, sizeof(presentation));
    memset(frame_live, 0, sizeof(frame_live)); memset(backing_live, 0, sizeof(backing_live));
    memset(created, 0, sizeof(created)); memset(attached, 0, sizeof(attached));
    memset(command_types, 0, sizeof(command_types));
    presenting = false; last_fence = 0;
    backing_allocated = backing_freed = fail_backing = commands = fail_command = host_scanout = checkpoint_count = 0;
    memset(config, 0, sizeof(config)); memset(mmio, 0, sizeof(mmio));
    memset(devices, 0, sizeof(devices)); memset(frames, 0xa5, sizeof(frames));
    memset(mapped, 0, sizeof(mapped));
    device_count = 2; current_cr3 = 0x1000; bar_base = 0x100000000ull;
    devices[0] = (struct pci_device){.vendor_id=0x1234,.device_id=0x1111,.class_code=3};
    devices[1] = (struct pci_device){.vendor_id=0x1af4,.device_id=0x1050,.class_code=3,.subclass=0x80};
    put(config[0], 0x10, 4, FB);
    put(config[1], 4, 2, 2); put(config[1], 6, 2, 0x10); put(config[1], 0x34, 1, 0x40);
    put(config[1], 0x20, 4, 4); put(config[1], 0x24, 4, 1);
    cap(0x40, 0x50, 16, 1, 0, 56);
    cap(0x50, 0x64, 20, 2, 0x3000, 0x1000); put(config[1], 0x60, 4, 4);
    cap(0x64, 0x74, 16, 3, 0x1000, 1);
    cap(0x74, 0x84, 16, 4, 0x2000, 16);
    cap(0x84, 0, 20, 5, 0x12345678, 0xabcdef00);
    put(config[1], 0x88, 1, 0); /* Saved selector has no valid region on purpose. */
    put(mmio, 18, 2, 2); put(mmio, 24, 2, 256);
    primary_writes = bar_writes = command_writes = allocated = freed = map_count = unmap_count = 0;
    reset_writes = cfg_reads = fail_alloc = fail_map = free_order = pauses = bad_response = 0;
    missing_version = reject_features = reject_driver = reject_queue = false;
    timeout_mode = stuck_reset = selector_reject = collision = false;
    fail_restore = fail_unmap = reference_clock = false;
    clock_us = clock_step = 0;
    probe_attempted = false;
}
static struct virtio_gpu_probe_result run(enum virtio_gpu_probe_status expected) {
    struct virtio_gpu_probe_result out;
    bool ok = virtio_gpu_probe_run(FB, &out);
    if (out.status != expected) fprintf(stderr, "expected %s got %s\n",
        virtio_gpu_probe_status_name(expected), virtio_gpu_probe_status_name(out.status));
    assert(out.status == expected && ok == (expected == VGPU_PROBE_OK));
    assert(primary_writes == 0 && unmap_count == map_count);
    assert(get(config[1], 0x88, 1) == 0 && get(config[1], 0x8c, 4) == 0x12345678);
    assert(get(config[1], 0x90, 4) == 0xabcdef00);
    assert(freed == (out.quarantined ? 0 : allocated));
    return out;
}
static void unchanged(void) { assert(!bar_writes && !command_writes && !allocated && !map_count); }
static void presentation_tests(void);
static void damage_tests(void);
int main(void) {
    setup(); struct virtio_gpu_probe_result out = run(VGPU_PROBE_OK);
    assert(out.width == 1280 && out.height == 800 && out.fence_ok && out.reset_ok && !out.quarantined);
    assert(map_count == 2 && allocated == 3 && freed == 3 && get(config[1], 4, 2) == 2);
    assert(get(config[1], 0x20, 4) == 4 && get(config[1], 0x24, 4) == 1);
    unsigned writes = command_writes;
    assert(!virtio_gpu_probe_run(FB, &out) && out.status == VGPU_PROBE_REFUSED && command_writes == writes);
    setup(); bar_base = 0xd0000000; put(config[1], 0x20, 4, bar_base); put(config[1], 0x24, 4, 0);
    run(VGPU_PROBE_OK); assert(bar_writes == 2);
    setup(); put(config[1], 0x58, 4, 128); put(config[1], 0x5c, 4, 2);
    run(VGPU_PROBE_OK); assert(map_count == 1); /* Shared MMIO page deduplicated. */
    setup(); put(config[1], 0x48, 4, 0xff0); put(config[1], 0x6c, 4, 0x2000);
    put(config[1], 0x7c, 4, 0x2400); put(mmio, 0xff0 + 18, 2, 2); put(mmio, 0xff0 + 24, 2, 256);
    run(VGPU_PROBE_OK); assert(map_count == 3);
    setup(); device_count = 1; run(VGPU_PROBE_UNAVAILABLE); unchanged();
    setup(); devices[2] = devices[1]; device_count = 3; run(VGPU_PROBE_UNAVAILABLE); unchanged();
    setup(); devices[1].subclass = 0; run(VGPU_PROBE_UNAVAILABLE); unchanged();
    setup(); put(config[0], 0x10, 4, FB + 4096); run(VGPU_PROBE_UNAVAILABLE); unchanged();
    setup(); current_cr3 = 0x2000; run(VGPU_PROBE_REFUSED); unchanged();
    setup(); mmio[20] = 15; run(VGPU_PROBE_REFUSED); unchanged(); assert(cfg_reads == 1);
    setup(); selector_reject = true; run(VGPU_PROBE_REFUSED); unchanged(); assert(!cfg_reads);
    setup(); put(config[1], 0x85, 1, 0x40); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x41, 1, 0x51); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x41, 1, 0x20); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x42, 1, 15); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x82, 1, 250); /* This is inside DEVICE_CFG length, no parser role. */
    put(config[1], 0x86, 1, 250); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x53, 1, 1); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x53, 1, 0); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x54, 1, 5); run(VGPU_PROBE_BAD_CAPABILITY); unchanged(); /* Upper BAR half. */
    setup(); put(config[1], 0x54, 1, 6); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x20, 4, 1); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x48, 4, 2); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x4c, 4, 55); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x58, 4, 0xfffffff0); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x60, 4, 3); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x58, 4, 16); run(VGPU_PROBE_BAD_CAPABILITY); unchanged();
    setup(); put(config[1], 0x75, 1, 0); run(VGPU_PROBE_BAD_CAPABILITY); unchanged(); /* Missing PCI_CFG. */
    setup(); put(config[1], 0x58, 4, 0x4000); run(VGPU_PROBE_BAD_BAR); assert(!map_count);
    setup(); bar_base = FB; put(config[1], 0x20, 4, FB); put(config[1], 0x24, 4, 0);
    run(VGPU_PROBE_BAD_BAR); assert(!bar_writes);
    setup(); collision = true; run(VGPU_PROBE_MAPPING_FAILED); assert(!map_count);
    setup(); fail_map = 1; run(VGPU_PROBE_MAPPING_FAILED); assert(!map_count);
    setup(); fail_map = 2; out = run(VGPU_PROBE_MAPPING_FAILED); assert(out.reset_ok);
    for (unsigned i = 1; i <= 3; ++i) { setup(); fail_alloc = i; out = run(VGPU_PROBE_NO_MEMORY); assert(out.reset_ok); }
    setup(); missing_version = true; out = run(VGPU_PROBE_FEATURES_REJECTED); assert(out.reset_ok);
    setup(); reject_features = true; run(VGPU_PROBE_FEATURES_REJECTED);
    setup(); reject_driver = true; run(VGPU_PROBE_FEATURES_REJECTED);
    setup(); put(mmio, 18, 2, 0); run(VGPU_PROBE_QUEUE_REJECTED);
    setup(); put(mmio, 24, 2, 4); run(VGPU_PROBE_QUEUE_REJECTED);
    setup(); put(mmio, 28, 2, 1); run(VGPU_PROBE_QUEUE_REJECTED);
    setup(); put(mmio, 30, 2, 0xffff); run(VGPU_PROBE_QUEUE_REJECTED);
    setup(); reject_queue = true; run(VGPU_PROBE_QUEUE_REJECTED);
    for (unsigned i = 1; i <= 15; ++i) { setup(); bad_response = i; out = run(VGPU_PROBE_BAD_RESPONSE); assert(out.reset_ok && !out.fence_ok); }
    setup(); timeout_mode = true; out = run(VGPU_PROBE_TIMEOUT); assert(out.reset_ok && pauses == VGPU_POLL_LIMIT);
    setup(); timeout_mode = reference_clock = true; clock_step = 100000;
    out = run(VGPU_PROBE_TIMEOUT); assert(out.reset_ok && pauses < VGPU_POLL_LIMIT && clock_us >= VGPU_WAIT_US);
    setup(); timeout_mode = reference_clock = true; /* Stopped reference still terminates. */
    out = run(VGPU_PROBE_TIMEOUT); assert(out.reset_ok && pauses == VGPU_POLL_LIMIT);
    setup(); fail_restore = true; out = run(VGPU_PROBE_REFUSED); assert(out.reset_ok && out.fence_ok);
    setup(); fail_unmap = true; out = run(VGPU_PROBE_MAPPING_FAILED); assert(out.reset_ok && out.fence_ok);
    setup(); stuck_reset = true; out = run(VGPU_PROBE_RESET_FAILED);
    assert(out.fence_ok && !out.reset_ok && out.quarantined && allocated == 3 && freed == 0);
    assert(!(get(config[1], 4, 2) & 6) && pauses == VGPU_POLL_LIMIT);
    setup(); timeout_mode = stuck_reset = true; out = run(VGPU_PROBE_RESET_FAILED);
    assert(out.quarantined && !out.fence_ok && freed == 0);
    assert(strcmp(virtio_gpu_probe_status_name((enum virtio_gpu_probe_status)99), "unknown") == 0);
    puts("virtio-gpu probe: PCI ownership/capability/BAR, explicit kernel mappings, queue/fence, bounded timeout and reset-before-free tests passed");
    presentation_tests();
    damage_tests();
    return 0;
}

static void setup_primary(void) {
    setup(); presenting = true;
    devices[0].class_code = 1;
    devices[1].subclass = 0;
    put(config[1], 0x10, 4, FB);
}
static void prepare_small(void) {
    setup_primary();
    assert(virtio_gpu_present_prepare(FB, 4, 3));
    assert(virtio_gpu_present_available() && !virtio_gpu_present_active());
    assert(!command_writes && !bar_writes && !cfg_reads && !allocated && !backing_allocated);
}
static void retired_cleanly(void) {
    assert(!virtio_gpu_present_active() && virtio_gpu_present_available());
    assert(virtio_gpu_present_result()->reset_ok && !virtio_gpu_present_result()->quarantined);
    assert(allocated == freed && backing_allocated == backing_freed && map_count == unmap_count);
    assert(!host_scanout && get(config[1], 4, 2) == 2 && get(config[1], 0x10, 4) == FB);
    assert(virtio_gpu_present_end());
}
static unsigned checkpoint_pixels;
static void service_checkpoint(void *arg) {
    assert(arg == &checkpoint_count && current_cr3 == 0x1000 && presentation.busy);
    ++checkpoint_count;
    assert(!virtio_gpu_present_end()); /* No reentrant resource retirement. */
    assert(!virtio_gpu_present_begin());
    assert(!virtio_gpu_present_frame(NULL, 0));
    unsigned pixels = 0;
    for (unsigned i = 0; i < 2; ++i) if (backing_live[i]) {
        uint32_t *p = (void *)backing_memory[i];
        for (uint64_t n = 0; n < backing_sizes[i] / 4; ++n) if (p[n] == 0x11223344u) ++pixels;
    }
    if (pixels > checkpoint_pixels) assert(pixels - checkpoint_pixels <= 65536 / 4);
    checkpoint_pixels = pixels;
    current_cr3 = 0x2000; /* Driver must restore kernel CR3 on callback return. */
}
static void presentation_tests(void) {
    uint32_t image[15];
    for (unsigned i = 0; i < 15; ++i) image[i] = 0x1000 + i;
    prepare_small();
    assert(!virtio_gpu_present_prepare(FB, 4, 3)); /* Boot admission is once. */
    assert(virtio_gpu_present_begin() && virtio_gpu_present_active());
    assert(commands == 5 && !host_scanout && allocated == 3 && backing_allocated == 2);
    const unsigned initial[] = {0x100, 0x101, 0x106, 0x101, 0x106};
    assert(!memcmp(command_types, initial, sizeof(initial)));
    assert(virtio_gpu_present_frame(image, 20) && host_scanout == 1);
    assert(commands == 8 && command_types[5] == 0x105 && command_types[6] == 0x103 && command_types[7] == 0x104);
    uint32_t original[12];
    memcpy(original, backing_memory[0], sizeof(original));
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < 4; ++x) assert(original[y * 4 + x] == image[y * 5 + x]);
    for (unsigned i = 0; i < 15; ++i) image[i] += 0x2000;
    virtio_gpu_present_damage(0, 0, 4, 3);
    assert(virtio_gpu_present_frame(image, 20) && host_scanout == 2);
    assert(!memcmp(original, backing_memory[0], sizeof(original))); /* Current resource was never overwritten. */
    unsigned maps = map_count, allocations = allocated, backings = backing_allocated;
    struct dma_layout *q = (void *)frames[0];
    presentation.cursor.used = presentation.cursor.available = 65534;
    q->used_index = q->available_index = 65534;
    virtio_gpu_present_damage(0, 0, 4, 3);
    assert(virtio_gpu_present_frame(image, 20) && host_scanout == 1);
    assert(presentation.cursor.used == 1 && presentation.cursor.available == 1); /* 16-bit ring wrap. */
    for (unsigned i = 0; i < 12; ++i) {
        virtio_gpu_present_damage(0, 0, 4, 3);
        assert(virtio_gpu_present_frame(image, 20));
    }
    assert(map_count == maps && allocated == allocations && backing_allocated == backings);
    assert(virtio_gpu_present_end()); retired_cleanly();
    assert(virtio_gpu_present_begin() && !host_scanout); /* Full clean restart. */
    assert(virtio_gpu_present_frame(image, 20));
    assert(virtio_gpu_present_end()); retired_cleanly();

    setup_primary(); assert(!virtio_gpu_present_prepare(FB + 4096, 4, 3)); unchanged();
    setup_primary(); assert(!virtio_gpu_present_prepare(FB, 0, 3)); unchanged();
    setup_primary(); assert(!virtio_gpu_present_prepare(FB, 16384, 16384)); unchanged();
    setup_primary(); devices[0].class_code = 3; assert(!virtio_gpu_present_prepare(FB, 4, 3)); unchanged();
    setup_primary(); put(config[1], 0x43, 1, 0); assert(!virtio_gpu_present_prepare(FB, 4, 3)); unchanged();
    prepare_small(); mmio[20] = 15; assert(!virtio_gpu_present_begin()); unchanged();
    assert(!virtio_gpu_present_active() && virtio_gpu_present_end());
    prepare_small(); current_cr3 = 0x2000; assert(!virtio_gpu_present_begin()); unchanged();
    current_cr3 = 0x1000;
    for (unsigned i = 1; i <= 3; ++i) {
        prepare_small(); fail_alloc = i; assert(!virtio_gpu_present_begin()); retired_cleanly();
    }
    for (unsigned i = 1; i <= 2; ++i) {
        prepare_small(); fail_backing = i; assert(!virtio_gpu_present_begin()); retired_cleanly();
        assert(virtio_gpu_present_result()->status == VGPU_PROBE_NO_MEMORY);
    }
    for (unsigned i = 1; i <= 8; ++i) {
        prepare_small(); fail_command = i;
        if (i <= 5) assert(!virtio_gpu_present_begin());
        else { assert(virtio_gpu_present_begin()); assert(!virtio_gpu_present_frame(image, 20)); }
        assert(virtio_gpu_present_result()->status == VGPU_PROBE_BAD_RESPONSE);
        retired_cleanly();
    }
    prepare_small(); assert(virtio_gpu_present_begin());
    presentation.cursor.fence = UINT64_MAX;
    assert(!virtio_gpu_present_frame(image, 20) && commands == 5);
    assert(virtio_gpu_present_result()->status == VGPU_PROBE_REFUSED); retired_cleanly();
    prepare_small(); assert(virtio_gpu_present_begin());
    q = (void *)frames[0]; ++q->used_index; /* Unsolicited used element before reuse. */
    assert(!virtio_gpu_present_frame(image, 20)); retired_cleanly();
    prepare_small(); assert(virtio_gpu_present_begin());
    assert(!virtio_gpu_present_frame(image, 12)); retired_cleanly();
    prepare_small(); assert(virtio_gpu_present_begin());
    current_cr3 = 0x2000;
    assert(!virtio_gpu_present_frame(image, 20) && !virtio_gpu_present_end() && virtio_gpu_present_active());
    current_cr3 = 0x1000; assert(virtio_gpu_present_end()); retired_cleanly();
    prepare_small(); assert(virtio_gpu_present_begin()); fail_restore = true;
    assert(!virtio_gpu_present_end() && !virtio_gpu_present_available() && !virtio_gpu_present_end());
    assert(virtio_gpu_present_result()->reset_ok && allocated == freed && backing_allocated == backing_freed);
    prepare_small(); assert(virtio_gpu_present_begin()); fail_unmap = true;
    assert(!virtio_gpu_present_end() && !virtio_gpu_present_available() && !virtio_gpu_present_begin());
    prepare_small(); assert(virtio_gpu_present_begin()); stuck_reset = true;
    assert(!virtio_gpu_present_end() && !virtio_gpu_present_available());
    assert(virtio_gpu_present_result()->quarantined && !freed && !backing_freed);
    assert(!virtio_gpu_present_end() && !virtio_gpu_present_begin() && !virtio_gpu_present_active());
    prepare_small(); assert(virtio_gpu_present_begin()); timeout_mode = stuck_reset = true;
    assert(!virtio_gpu_present_frame(image, 20));
    assert(virtio_gpu_present_result()->quarantined && !freed && !backing_freed && !virtio_gpu_present_end());

    setup_primary(); checkpoint_pixels = 0;
    assert(virtio_gpu_present_prepare(FB, 256, 128));
    virtio_gpu_present_set_checkpoint(service_checkpoint, &checkpoint_count);
    assert(virtio_gpu_present_begin() && checkpoint_count >= 14 && current_cr3 == 0x1000);
    uint32_t *large = malloc(256 * 128 * 4); assert(large);
    for (unsigned i = 0; i < 256 * 128; ++i) large[i] = 0x11223344;
    unsigned checkpoints_before = checkpoint_count;
    assert(virtio_gpu_present_frame(large, 256 * 4));
    assert(checkpoint_count >= checkpoints_before + 5 && current_cr3 == 0x1000);
    timeout_mode = true; checkpoints_before = checkpoint_count;
    virtio_gpu_present_damage(0, 0, 256, 128);
    assert(!virtio_gpu_present_frame(large, 256 * 4));
    assert(checkpoint_count > checkpoints_before + 100 && current_cr3 == 0x1000);
    retired_cleanly(); free(large);
    virtio_gpu_present_set_checkpoint(NULL, NULL);
    setup();
    puts("virtio-gpu present: read-only admission, resource ordering, inactive backing, reusable/wrapping fenced queue, restart, callback slices and failure quarantine passed");
}

static void paint_damage(uint32_t *image, unsigned stride, unsigned rows, unsigned columns,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    /* Independent whole-surface oracle: membership arithmetic is 64-bit, with
     * no reuse of the driver's rectangle union or clipping implementation. */
    for (unsigned row = 0; row < rows; ++row)
        for (unsigned column = 0; column < columns; ++column)
            if (column >= x && (uint64_t)column < (uint64_t)x + width &&
                row >= y && (uint64_t)row < (uint64_t)y + height)
                image[row * stride + column] = color;
    virtio_gpu_present_damage(x, y, width, height);
}
static struct virtio_gpu_present_stats statistics(void) {
    struct virtio_gpu_present_stats stats;
    virtio_gpu_present_get_stats(&stats);
    return stats;
}
static void damage_tests(void) {
    enum { WIDTH = 32, HEIGHT = 24, STRIDE = 36 };
    uint32_t image[HEIGHT * STRIDE];
    for (unsigned i = 0; i < HEIGHT * STRIDE; ++i) image[i] = 0x01000000u + i;
    setup_primary();
    assert(virtio_gpu_present_prepare(FB, WIDTH, HEIGHT) && virtio_gpu_present_begin());
    oracle_staging = image; oracle_pitch = STRIDE * 4;
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    struct virtio_gpu_present_stats stats = statistics();
    assert(stats.presented_frames == 1 && !stats.skipped_frames);
    assert(stats.copied_bytes == WIDTH * HEIGHT * 4 && stats.transferred_bytes == stats.copied_bytes);
    unsigned commands_before = commands;
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    assert(commands == commands_before && host_scanout == 1 && statistics().skipped_frames == 1);
    /* Resource 2 is still unknown; no-op did not expose its poison image. */
    assert(host_pixels[1][0] == 0xcdcdcdcdu);
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 2, 3, 2, 2, 0x10203040);
    assert(virtio_gpu_present_frame(image, oracle_pitch) && host_scanout == 2);
    assert(last_transfer.x == 0 && last_transfer.y == 0 && last_transfer.width == WIDTH && last_transfer.height == HEIGHT);
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 20, 12, 3, 2, 0x50607080);
    assert(virtio_gpu_present_frame(image, oracle_pitch) && host_scanout == 1);
    /* Buffer 1 must catch up with both disjoint writes since its last use. */
    assert(last_transfer.x == 2 && last_transfer.y == 3 && last_transfer.width == 21 && last_transfer.height == 11);
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 7, 6, 2, 3, 0x90a0b0c0);
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    /* Moving an object dirties the old footprint as well as the new one. */
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 7, 6, 2, 3, 0x01010101);
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 10, 8, 2, 3, 0x90a0b0c0);
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    paint_damage(image, STRIDE, HEIGHT, WIDTH, WIDTH - 1, HEIGHT - 1, UINT32_MAX, UINT32_MAX, 0xeeff0011);
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    commands_before = commands;
    virtio_gpu_present_damage(UINT32_MAX, 0, UINT32_MAX, UINT32_MAX);
    virtio_gpu_present_damage(0, UINT32_MAX, UINT32_MAX, UINT32_MAX);
    virtio_gpu_present_damage(WIDTH, HEIGHT, 1, 1);
    virtio_gpu_present_damage(0, 0, 0, UINT32_MAX);
    virtio_gpu_present_damage(0, 0, UINT32_MAX, 0);
    assert(virtio_gpu_present_frame(image, oracle_pitch) && commands == commands_before);
    /* The public full-damage fallback is required for unknown or bulk writers. */
    paint_damage(image, STRIDE, HEIGHT, WIDTH, 0, 0, UINT32_MAX, UINT32_MAX, 0xa1b2c3d4);
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    assert(last_transfer.width == WIDTH && last_transfer.height == HEIGHT);
    uint32_t random = 0x78abcdef;
    for (unsigned i = 0; i < 100; ++i) {
        random = random * 1664525u + 1013904223u;
        uint32_t x = 3 + random % 12;
        uint32_t y = 2 + ((random >> 8) % 8);
        uint32_t width = 1 + ((random >> 16) % 4);
        uint32_t height = 1 + ((random >> 24) % 4);
        paint_damage(image, STRIDE, HEIGHT, WIDTH, x, y, width, height, random);
        if ((i % 7) == 0)
            paint_damage(image, STRIDE, HEIGHT, WIDTH, 4, 3, 2, 2, random ^ 0xdeadbeef);
        assert(virtio_gpu_present_frame(image, oracle_pitch));
    }
    stats = statistics();
    assert(stats.presented_frames == oracle_frames && stats.presented_frames == 107);
    assert(stats.skipped_frames == 2 && stats.copied_bytes == stats.transferred_bytes);
    assert(stats.copied_bytes < stats.presented_frames * WIDTH * HEIGHT * 4 / 2);
    assert(transfers == stats.presented_frames && allocated == 3 && backing_allocated == 2 && map_count == 2);
    virtio_gpu_present_get_stats(NULL);
    assert(virtio_gpu_present_end()); retired_cleanly();
    assert(virtio_gpu_present_begin());
    stats = statistics();
    assert(!stats.presented_frames && !stats.skipped_frames && !stats.copied_bytes && !stats.transferred_bytes);
    assert(virtio_gpu_present_frame(image, oracle_pitch));
    assert(statistics().copied_bytes == WIDTH * HEIGHT * 4);
    assert(virtio_gpu_present_end()); retired_cleanly();

    for (unsigned failed_step = 1; failed_step <= 3; ++failed_step) {
        uint32_t small[8 * 8];
        for (unsigned i = 0; i < 64; ++i) small[i] = 0x10000000 + i;
        setup_primary();
        assert(virtio_gpu_present_prepare(FB, 8, 8) && virtio_gpu_present_begin());
        oracle_staging = small; oracle_pitch = 8 * 4;
        assert(virtio_gpu_present_frame(small, oracle_pitch));
        paint_damage(small, 8, 8, 8, 1, 1, 1, 1, 0x12345678);
        assert(virtio_gpu_present_frame(small, oracle_pitch));
        paint_damage(small, 8, 8, 8, 2, 2, 1, 1, 0x87654321);
        stats = statistics();
        fail_command = commands + failed_step;
        assert(!virtio_gpu_present_frame(small, oracle_pitch));
        assert(presentation.pending[0].width && presentation.pending[1].width);
        assert(statistics().presented_frames == stats.presented_frames);
        assert(statistics().copied_bytes == stats.copied_bytes + 16);
        assert(statistics().transferred_bytes == stats.transferred_bytes + (failed_step == 1 ? 0 : 16));
        retired_cleanly();
        fail_command = 0;
        assert(virtio_gpu_present_begin());
        assert(!statistics().copied_bytes && !statistics().presented_frames);
        assert(virtio_gpu_present_frame(small, oracle_pitch));
        assert(statistics().copied_bytes == 8 * 8 * 4);
        assert(virtio_gpu_present_end()); retired_cleanly();
    }
    setup();
    puts("virtio-gpu damage: full-staging host-image oracle passed alternating/disjoint/moved/edge/no-op updates, clipped overflow, failure/restart, and reduced copy/transfer bytes");
}
