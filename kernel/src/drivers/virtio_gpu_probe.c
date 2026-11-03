/* Bounded QEMU-only transport and opt-in presentation, VirtIO 1.2 sections 4.1
 * and 5.7. Explicit kernel-CR3 mappings bypass the persistent VMM registry. The
 * boot diagnostic never changes scanout; the separate primary-VGA owner does.
 * Neither path installs interrupts or workers. */
#include <srvros/virtio_gpu_probe.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

#define VGPU_VIRTUAL 0xffffc003ff000000ull
#define VGPU_MAX_PAGES 3
#define VGPU_POLL_LIMIT 1000000u
#define VGPU_WAIT_US 2000000ull
#define VGPU_MAX_DIMENSION 16384u
#define VGPU_QUEUE_SIZE 8u
#define VGPU_FENCE 0x5352565247505501ull
#define VGPU_COMMON_SIZE 56u
#define VGPU_STATUS 20u

struct region { uint8_t bar; uint32_t offset, length; };
struct transport {
    const struct pci_device *pci;
    struct region regions[5];
    uint8_t cfg_cap;
    uint32_t multiplier;
    uint64_t bar_base[6], bar_size[6];
    uint64_t pages[VGPU_MAX_PAGES];
    unsigned page_count;
    uint64_t cr3, common, notify;
    uint16_t original_command;
    bool cooperative;
};
struct descriptor { uint64_t address; uint32_t length; uint16_t flags, next; };
struct gpu_header {
    uint32_t type, flags;
    uint64_t fence;
    uint32_t context;
    uint8_t ring_index, padding[3];
};
struct display_mode { uint32_t x, y, width, height, enabled, flags; };
struct display_response { struct gpu_header header; struct display_mode modes[16]; };
struct dma_layout {
    struct descriptor descriptors[VGPU_QUEUE_SIZE];
    uint16_t available_flags, available_index, available_ring[VGPU_QUEUE_SIZE];
    uint16_t available_event;
    /* Explicit padding keeps the used ring 4-byte aligned. */
    uint16_t padding;
    uint16_t used_flags, used_index;
    struct { uint32_t id, length; } used_ring[VGPU_QUEUE_SIZE];
    uint16_t used_event;
};
_Static_assert(sizeof(struct gpu_header) == 24, "GPU command layout");
_Static_assert(sizeof(struct display_response) == 408, "display response layout");
_Static_assert(__builtin_offsetof(struct dma_layout, used_flags) % 4 == 0,
    "used ring alignment");
static bool probe_attempted;
static void presentation_checkpoint(void);

/* Tests replace individual MMIO accesses, not the transport state machine. */
#ifdef VGPU_PROBE_HOST_TEST
extern uint32_t vgpu_test_mmio_read(uint64_t physical, unsigned width);
extern void vgpu_test_mmio_write(uint64_t physical, unsigned width, uint32_t value);
extern void vgpu_test_pause(void);
#define probe_pause() vgpu_test_pause()
#else
#define probe_pause() __asm__ volatile("pause" ::: "memory")
#endif
static void barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static void checkpoint(struct transport *t) {
    if (!t->cooperative) return;
    presentation_checkpoint();
    if (vmm_current_address_space() != t->cr3) vmm_switch_address_space(t->cr3);
}
/* The reference clock runs without timer IRQs. Keep a finite iteration limit
 * too, so a missing/stopped reference cannot turn a diagnostic into a hang. */
static bool expired(unsigned iteration, bool timed, uint64_t started) {
    return timed && (iteration & 255u) == 0 &&
        timer_monotonic_us() - started >= VGPU_WAIT_US;
}
static void zero(void *memory, unsigned bytes) {
    unsigned char *p = memory;
    for (unsigned i = 0; i < bytes; ++i) p[i] = 0;
}
#ifndef VGPU_PROBE_HOST_TEST
static uint64_t virtual_for(struct transport *t, uint64_t physical) {
    for (unsigned i = 0; i < t->page_count; ++i)
        if (t->pages[i] == (physical & ~4095ull))
            return VGPU_VIRTUAL + i * 4096 + (physical & 4095);
    return 0; /* Only validated/mapped fields reach MMIO accessors. */
}
#endif
static uint32_t mmio_read(struct transport *t, uint64_t physical, unsigned width) {
#ifdef VGPU_PROBE_HOST_TEST
    (void)t;
    return vgpu_test_mmio_read(physical, width);
#else
    uint64_t v = virtual_for(t, physical);
    if (width == 1) return *(volatile uint8_t *)v;
    if (width == 2) return *(volatile uint16_t *)v;
    return *(volatile uint32_t *)v;
#endif
}
static void mmio_write(struct transport *t, uint64_t physical, unsigned width, uint32_t value) {
#ifdef VGPU_PROBE_HOST_TEST
    (void)t;
    vgpu_test_mmio_write(physical, width, value);
#else
    uint64_t v = virtual_for(t, physical);
    if (width == 1) *(volatile uint8_t *)v = (uint8_t)value;
    else if (width == 2) *(volatile uint16_t *)v = (uint16_t)value;
    else *(volatile uint32_t *)v = value;
#endif
}
static uint32_t common_read(struct transport *t, unsigned offset, unsigned width) {
    return mmio_read(t, t->common + offset, width);
}
static void common_write(struct transport *t, unsigned offset, unsigned width, uint32_t value) {
    mmio_write(t, t->common + offset, width, value);
}
static void common_address(struct transport *t, unsigned offset, uint64_t value) {
    common_write(t, offset, 4, (uint32_t)value);
    common_write(t, offset + 4, 4, (uint32_t)(value >> 32));
}

static bool memory_bar(const struct pci_device *p, unsigned index, uint64_t *base) {
    /* Walk from BAR0 so the upper half of a 64-bit BAR is never a BAR. */
    for (unsigned i = 0; i < 6; ++i) {
        uint32_t low = pci_read_config32(p, (uint8_t)(0x10 + 4 * i));
        bool wide = !(low & 1) && (low & 6) == 4;
        if (i == index) {
            if ((low & 1) || ((low & 6) != 0 && !wide) || (wide && i == 5)) return false;
            *base = low & ~15ull;
            if (wide) *base |= (uint64_t)pci_read_config32(p, (uint8_t)(0x14 + 4 * i)) << 32;
            return *base != 0 && *base < (1ull << 52);
        }
        if (wide) ++i;
    }
    return false;
}

static bool discover(uint64_t framebuffer, struct transport *t) {
    const struct pci_device *primary = 0;
    unsigned targets = 0, primaries = 0;
    for (uint64_t i = 0; i < pci_device_count(); ++i) {
        const struct pci_device *p = pci_device_at(i);
        if (!p) continue;
        if (p->vendor_id == 0x1af4 && p->device_id == 0x1050 &&
            p->class_code == 3 && p->subclass == 0x80 && (p->header_type & 0x7f) == 0) {
            t->pci = p;
            ++targets;
        }
        if (p->vendor_id == 0x1234 && p->device_id == 0x1111 &&
            p->class_code == 3 && p->subclass == 0 && (p->header_type & 0x7f) == 0) {
            primary = p;
            ++primaries;
        }
    }
    uint64_t primary_base = 0;
    return targets == 1 && primaries == 1 && framebuffer &&
        memory_bar(primary, 0, &primary_base) && primary_base == framebuffer;
}

static bool capabilities(struct transport *t) {
    const struct pci_device *p = t->pci;
    uint8_t occupied[256] = {0};
    bool types[6] = {0};
    if (!(pci_read_config16(p, 6) & 0x10)) return false;
    unsigned at = pci_read_config8(p, 0x34);
    unsigned count = 0;
    while (at) {
        if (++count > 48 || at < 0x40 || at > 0xfc || (at & 3) || occupied[at]) return false;
        unsigned next = pci_read_config8(p, (uint8_t)(at + 1));
        unsigned length = 2;
        if (pci_read_config8(p, (uint8_t)at) == 9) {
            length = pci_read_config8(p, (uint8_t)(at + 2));
            unsigned type = pci_read_config8(p, (uint8_t)(at + 3));
            if (length < 16 || at + length > 256 || type < 1 || type > 5 || types[type]) return false;
            types[type] = true;
            if (type == 5) {
                if (length < 20) return false;
                t->cfg_cap = (uint8_t)at;
            } else {
                struct region *r = &t->regions[type];
                r->bar = pci_read_config8(p, (uint8_t)(at + 4));
                r->offset = pci_read_config32(p, (uint8_t)(at + 8));
                r->length = pci_read_config32(p, (uint8_t)(at + 12));
                uint64_t base;
                if (r->bar >= 6 || !memory_bar(p, r->bar, &base) || !r->length ||
                    (uint64_t)r->offset + r->length > 0x100000000ull) return false;
                if (type == 1 && (r->length < VGPU_COMMON_SIZE || (r->offset & 3))) return false;
                if (type == 2) {
                    if (length < 20 || r->length < 2 || (r->offset & 1)) return false;
                    t->multiplier = pci_read_config32(p, (uint8_t)(at + 16));
                    if (t->multiplier & 1) return false;
                }
            }
        }
        for (unsigned i = at; i < at + length; ++i) {
            if (occupied[i]) return false;
            occupied[i] = 1;
        }
        at = next;
    }
    if (!types[1] || !types[2] || !types[5]) return false;
    for (unsigned a = 1; a <= 4; ++a)
        for (unsigned b = a + 1; b <= 4; ++b) {
            struct region *x = &t->regions[a], *y = &t->regions[b];
            if (x->length && y->length && x->bar == y->bar &&
                (uint64_t)x->offset < (uint64_t)y->offset + y->length &&
                (uint64_t)y->offset < (uint64_t)x->offset + x->length) return false;
        }
    return true;
}

/* PCI_CFG is mandatory, and lets us check ownership before disabling decode or
 * touching BAR size. Restore all selector fields even on a refusal. No data write. */
static bool inactive_via_pci(struct transport *t) {
    const struct pci_device *p = t->pci;
    uint8_t at = t->cfg_cap;
    uint8_t saved_bar = pci_read_config8(p, at + 4);
    uint32_t saved_offset = pci_read_config32(p, at + 8);
    uint32_t saved_length = pci_read_config32(p, at + 12);
    pci_write_config8(p, at + 4, t->regions[1].bar);
    pci_write_config32(p, at + 12, 1);
    pci_write_config32(p, at + 8, t->regions[1].offset + VGPU_STATUS);
    bool selected = pci_read_config8(p, at + 4) == t->regions[1].bar &&
        pci_read_config32(p, at + 12) == 1 &&
        pci_read_config32(p, at + 8) == t->regions[1].offset + VGPU_STATUS;
    uint8_t status = selected ? pci_read_config8(p, at + 16) : 0xff;
    pci_write_config32(p, at + 8, saved_offset);
    pci_write_config32(p, at + 12, saved_length);
    pci_write_config8(p, at + 4, saved_bar);
    return selected && status == 0 && pci_read_config8(p, at + 4) == saved_bar &&
        pci_read_config32(p, at + 8) == saved_offset &&
        pci_read_config32(p, at + 12) == saved_length;
}

static bool size_bars(struct transport *t, uint64_t framebuffer) {
    const struct pci_device *p = t->pci;
    for (unsigned region = 1; region <= 4; ++region) {
        struct region *r = &t->regions[region];
        if (!r->length) continue;
        unsigned bar = r->bar;
        if (!t->bar_size[bar]) {
            uint64_t base;
            if (!memory_bar(p, bar, &base) || base == framebuffer) return false;
            uint8_t at = (uint8_t)(0x10 + 4 * bar);
            uint32_t low = pci_read_config32(p, at);
            bool wide = (low & 6) == 4;
            uint32_t high = wide ? pci_read_config32(p, at + 4) : 0;
            pci_write_config32(p, at, 0xffffffffu);
            if (wide) pci_write_config32(p, at + 4, 0xffffffffu);
            uint32_t mask_low = pci_read_config32(p, at);
            uint32_t mask_high = wide ? pci_read_config32(p, at + 4) : 0xffffffffu;
            if (wide) pci_write_config32(p, at + 4, high);
            pci_write_config32(p, at, low);
            if (pci_read_config32(p, at) != low ||
                (wide && pci_read_config32(p, at + 4) != high)) return false;
            uint64_t mask = ((uint64_t)mask_high << 32) | (mask_low & ~15u);
            uint64_t size = ~mask + 1;
            if (!size || (size & (size - 1)) || size < 16 ||
                (base & (size - 1)) || base + size < base || base + size > (1ull << 52) ||
                (framebuffer >= base && framebuffer < base + size)) return false;
            for (unsigned i = 0; i < 6; ++i)
                if (t->bar_size[i] && base < t->bar_base[i] + t->bar_size[i] &&
                    t->bar_base[i] < base + size) return false;
            t->bar_base[bar] = base;
            t->bar_size[bar] = size;
        }
        if ((uint64_t)r->offset + r->length > t->bar_size[bar]) return false;
    }
    t->common = t->bar_base[t->regions[1].bar] + t->regions[1].offset;
    return true;
}

static bool map_range(struct transport *t, uint64_t physical, unsigned length) {
    uint64_t last = (physical + length - 1) & ~4095ull;
    for (uint64_t page = physical & ~4095ull; page <= last; page += 4096) {
        bool exists = false;
        for (unsigned i = 0; i < t->page_count; ++i) if (t->pages[i] == page) exists = true;
        if (exists) continue;
        if (t->page_count == VGPU_MAX_PAGES) return false;
        uint64_t virtual = VGPU_VIRTUAL + t->page_count * 4096;
        uint64_t previous;
        /* Do not overwrite another owner, even at our reserved scratch address. */
        if (vmm_virt_to_phys_in_space(t->cr3, virtual, &previous) ||
            !vmm_map_page_in_space(t->cr3, virtual, page, VMM_PAGE_PRESENT |
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) return false;
        t->pages[t->page_count++] = page;
    }
    return true;
}
static bool unmap(struct transport *t) {
    bool ok = true;
    while (t->page_count) {
        --t->page_count;
        if (!vmm_unmap_page_in_space(t->cr3, VGPU_VIRTUAL + t->page_count * 4096, 0)) ok = false;
    }
    return ok;
}
static bool reset(struct transport *t) {
    common_write(t, VGPU_STATUS, 1, 0);
    bool timed = timer_reference_ready();
    uint64_t started = timed ? timer_monotonic_us() : 0;
    for (unsigned i = 0; i < VGPU_POLL_LIMIT; ++i) {
        if (common_read(t, VGPU_STATUS, 1) == 0) { barrier(); return true; }
        if (expired(i, timed, started)) break;
        if ((i & 255u) == 0) checkpoint(t);
        probe_pause();
    }
    return false;
}

static enum virtio_gpu_probe_status queue_start(struct transport *t, uint64_t *dma) {
    common_write(t, VGPU_STATUS, 1, 1); /* ACKNOWLEDGE */
    common_write(t, VGPU_STATUS, 1, 3); /* DRIVER */
    common_write(t, 0, 4, 1);
    if (!(common_read(t, 4, 4) & 1)) return VGPU_PROBE_FEATURES_REJECTED;
    common_write(t, 8, 4, 0);
    common_write(t, 12, 4, 0);
    common_write(t, 8, 4, 1);
    common_write(t, 12, 4, 1); /* Only VIRTIO_F_VERSION_1. */
    common_write(t, VGPU_STATUS, 1, 11);
    if (common_read(t, VGPU_STATUS, 1) != 11) return VGPU_PROBE_FEATURES_REJECTED;
    if (common_read(t, 18, 2) < 1) return VGPU_PROBE_QUEUE_REJECTED;
    common_write(t, 16, 2, 0xffff); /* No MSI-X config vector. */
    common_write(t, 22, 2, 0);
    if (common_read(t, 28, 2) || common_read(t, 24, 2) < VGPU_QUEUE_SIZE)
        return VGPU_PROBE_QUEUE_REJECTED;
    common_write(t, 24, 2, VGPU_QUEUE_SIZE);
    common_write(t, 26, 2, 0xffff);
    if (common_read(t, 24, 2) != VGPU_QUEUE_SIZE || common_read(t, 26, 2) != 0xffff ||
        common_read(t, 16, 2) != 0xffff) return VGPU_PROBE_QUEUE_REJECTED;
    uint64_t offset = (uint64_t)common_read(t, 30, 2) * t->multiplier;
    struct region *notify = &t->regions[2];
    if (offset + 2 > notify->length) return VGPU_PROBE_QUEUE_REJECTED;
    t->notify = t->bar_base[notify->bar] + notify->offset + offset;
    if (!map_range(t, t->notify, 2)) return VGPU_PROBE_MAPPING_FAILED;
    for (unsigned i = 0; i < 3; ++i) {
        dma[i] = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if (!dma[i]) return VGPU_PROBE_NO_MEMORY;
        zero(pmm_phys_to_virt(dma[i]), 4096);
        checkpoint(t);
    }
    struct dma_layout *q = pmm_phys_to_virt(dma[0]);
    q->available_flags = 1; /* VIRTQ_AVAIL_F_NO_INTERRUPT */
    common_address(t, 32, dma[0]);
    common_address(t, 40, dma[0] + __builtin_offsetof(struct dma_layout, available_flags));
    common_address(t, 48, dma[0] + __builtin_offsetof(struct dma_layout, used_flags));
    barrier();
    common_write(t, 28, 2, 1);
    if (common_read(t, 28, 2) != 1) return VGPU_PROBE_QUEUE_REJECTED;
    pci_write_config16(t->pci, 4, (t->original_command & ~1u) | 6u | 0x400u);
    if ((pci_read_config16(t->pci, 4) & 0x406) != 0x406) return VGPU_PROBE_QUEUE_REJECTED;
    common_write(t, VGPU_STATUS, 1, 15); /* DRIVER_OK */
    if (common_read(t, VGPU_STATUS, 1) != 15) return VGPU_PROBE_FEATURES_REJECTED;
    return VGPU_PROBE_OK;
}

struct queue_cursor { uint16_t available, used; uint64_t fence; };

/* One descriptor chain at a time. A slot and its DMA are reused only after a
 * matching used element and matching GPU fence have both been observed. */
static enum virtio_gpu_probe_status command(struct transport *t, uint64_t *dma,
    struct queue_cursor *cursor, unsigned request_length, unsigned response_length,
    uint32_t response_type) {
    if (request_length < sizeof(struct gpu_header) || request_length > 4096 ||
        response_length < sizeof(struct gpu_header) || response_length > 4096 ||
        cursor->fence == UINT64_MAX) return VGPU_PROBE_REFUSED;
    struct dma_layout *q = pmm_phys_to_virt(dma[0]);
    if (*(volatile uint16_t *)&q->used_index != cursor->used ||
        common_read(t, VGPU_STATUS, 1) != 15) return VGPU_PROBE_BAD_RESPONSE;
    struct gpu_header *request = pmm_phys_to_virt(dma[1]);
    request->flags = 1;
    request->fence = ++cursor->fence;
    request->context = 0;
    request->ring_index = 0;
    request->padding[0] = request->padding[1] = request->padding[2] = 0;
    zero(pmm_phys_to_virt(dma[2]), response_length);
    q->descriptors[0] = (struct descriptor){dma[1], request_length, 1, 1};
    q->descriptors[1] = (struct descriptor){dma[2], response_length, 2, 0};
    q->available_ring[cursor->available % VGPU_QUEUE_SIZE] = 0;
    barrier();
    ++cursor->available;
    *(volatile uint16_t *)&q->available_index = cursor->available;
    barrier();
    mmio_write(t, t->notify, 2, 0);
    bool timed = timer_reference_ready();
    uint64_t started = timed ? timer_monotonic_us() : 0;
    unsigned poll;
    for (poll = 0; poll < VGPU_POLL_LIMIT; ++poll) {
        if (*(volatile uint16_t *)&q->used_index != cursor->used) break;
        if (common_read(t, VGPU_STATUS, 1) != 15) return VGPU_PROBE_BAD_RESPONSE;
        if (expired(poll, timed, started)) return VGPU_PROBE_TIMEOUT;
        if ((poll & 255u) == 0) checkpoint(t);
        probe_pause();
    }
    if (poll == VGPU_POLL_LIMIT) return VGPU_PROBE_TIMEOUT;
    barrier();
    unsigned slot = cursor->used % VGPU_QUEUE_SIZE;
    uint16_t expected = (uint16_t)(cursor->used + 1u);
    if (*(volatile uint16_t *)&q->used_index != expected || q->used_ring[slot].id != 0 ||
        q->used_ring[slot].length != response_length) return VGPU_PROBE_BAD_RESPONSE;
    struct gpu_header response = *(struct gpu_header *)pmm_phys_to_virt(dma[2]);
    if (response.type != response_type || response.flags != 1 ||
        response.fence != cursor->fence || response.context || response.ring_index)
        return VGPU_PROBE_BAD_RESPONSE;
    cursor->used = expected;
    checkpoint(t);
    return VGPU_PROBE_OK;
}

static enum virtio_gpu_probe_status display_info(struct transport *t, uint64_t *dma,
    struct queue_cursor *cursor, struct virtio_gpu_probe_result *out, bool primary) {
    struct gpu_header *request = pmm_phys_to_virt(dma[1]);
    zero(request, sizeof(*request));
    request->type = 0x100;
    enum virtio_gpu_probe_status status = command(t, dma, cursor, sizeof(*request),
        sizeof(struct display_response), 0x1101);
    if (status != VGPU_PROBE_OK) return status;
    struct display_response response = *(struct display_response *)pmm_phys_to_virt(dma[2]);
    if (primary && response.modes[0].enabled != 1) return VGPU_PROBE_BAD_RESPONSE;
    bool found = false;
    for (unsigned i = 0; i < 16; ++i) {
        struct display_mode *m = &response.modes[i];
        if (m->enabled > 1) return VGPU_PROBE_BAD_RESPONSE;
        if (!m->enabled) continue;
        if (!m->width || !m->height || m->width > VGPU_MAX_DIMENSION ||
            m->height > VGPU_MAX_DIMENSION || m->x > UINT32_MAX - m->width ||
            m->y > UINT32_MAX - m->height || m->flags) return VGPU_PROBE_BAD_RESPONSE;
        if (!found) { out->width = m->width; out->height = m->height; found = true; }
    }
    if (!found) return VGPU_PROBE_BAD_RESPONSE;
    out->fence_ok = true;
    return VGPU_PROBE_OK;
}

static enum virtio_gpu_probe_status transact(struct transport *t, uint64_t *dma,
    struct virtio_gpu_probe_result *out) {
    enum virtio_gpu_probe_status status = queue_start(t, dma);
    if (status != VGPU_PROBE_OK) return status;
    struct queue_cursor cursor = {.fence = VGPU_FENCE - 1};
    return display_info(t, dma, &cursor, out, false);
}

bool virtio_gpu_probe_run(uint64_t framebuffer, struct virtio_gpu_probe_result *out) {
    if (!out) return false;
    *out = (struct virtio_gpu_probe_result){.status = VGPU_PROBE_REFUSED};
    if (probe_attempted) return false;
    probe_attempted = true;
    struct transport t = {0};
    t.cr3 = vmm_kernel_address_space();
    if (!t.cr3 || vmm_current_address_space() != t.cr3) return false;
    out->status = VGPU_PROBE_UNAVAILABLE;
    if (!discover(framebuffer, &t)) return false;
    out->status = VGPU_PROBE_BAD_CAPABILITY;
    if (!capabilities(&t)) return false;
    out->status = VGPU_PROBE_REFUSED;
    if (!inactive_via_pci(&t)) return false;
    t.original_command = pci_read_config16(t.pci, 4);
    pci_write_config16(t.pci, 4, t.original_command & ~6u);
    out->status = VGPU_PROBE_BAD_BAR;
    if ((pci_read_config16(t.pci, 4) & 6) || !size_bars(&t, framebuffer)) goto restore_command;
    out->status = VGPU_PROBE_MAPPING_FAILED;
    if (!map_range(&t, t.common, VGPU_COMMON_SIZE)) goto restore_command;
    pci_write_config16(t.pci, 4, (t.original_command & ~5u) | 2u | 0x400u);
    if ((pci_read_config16(t.pci, 4) & 0x406) != 0x402) goto restore_command;
    /* A second ownership check closes the decode/mapping interval. */
    out->status = VGPU_PROBE_REFUSED;
    if (common_read(&t, VGPU_STATUS, 1) != 0) goto restore_command;
    uint64_t dma[3] = {0};
    out->status = transact(&t, dma, out);
    out->reset_ok = reset(&t);
    pci_write_config16(t.pci, 4, t.original_command & ~6u);
    if (!out->reset_ok) {
        out->status = VGPU_PROBE_RESET_FAILED;
        out->quarantined = dma[0] || dma[1] || dma[2];
        /* No proven reset: never free potentially device-owned DMA, and leave
         * bus mastering/decode disabled instead of restoring a live command. */
        unmap(&t);
        return false;
    }
    for (unsigned i = 0; i < 3; ++i) if (dma[i]) pmm_free_frame(dma[i]);
restore_command:
    pci_write_config16(t.pci, 4, t.original_command);
    if (pci_read_config16(t.pci, 4) != t.original_command) out->status = VGPU_PROBE_REFUSED;
    if (!unmap(&t) && out->status == VGPU_PROBE_OK) out->status = VGPU_PROBE_MAPPING_FAILED;
    return out->status == VGPU_PROBE_OK;
}

const char *virtio_gpu_probe_status_name(enum virtio_gpu_probe_status status) {
    static const char *const names[] = {
        "unavailable", "ok", "refused", "bad-capability", "bad-bar", "mapping-failed",
        "no-memory", "features-rejected", "queue-rejected", "timeout", "bad-response", "reset-failed"
    };
    return (unsigned)status < sizeof(names) / sizeof(names[0]) ? names[status] : "unknown";
}

#define VGPU_BACKING_LIMIT (32ull * 1024 * 1024)
struct gpu_rect { uint32_t x, y, width, height; };
struct create_2d_command {
    struct gpu_header header;
    uint32_t resource, format, width, height;
};
struct attach_command {
    struct gpu_header header;
    uint32_t resource, entries;
    uint64_t address;
    uint32_t length, padding;
};
struct transfer_command {
    struct gpu_header header;
    struct gpu_rect rect;
    uint64_t offset;
    uint32_t resource, padding;
};
struct scanout_command {
    struct gpu_header header;
    struct gpu_rect rect;
    uint32_t scanout, resource;
};
struct flush_command {
    struct gpu_header header;
    struct gpu_rect rect;
    uint32_t resource, padding;
};
_Static_assert(sizeof(struct create_2d_command) == 40, "create 2D layout");
_Static_assert(sizeof(struct attach_command) == 48, "one-entry attachment layout");
_Static_assert(sizeof(struct transfer_command) == 56, "transfer layout");
_Static_assert(sizeof(struct scanout_command) == 48, "scanout layout");
_Static_assert(sizeof(struct flush_command) == 48, "flush layout");

static struct {
    bool attempted, prepared, blocked, busy, active, owned, command_changed, shown;
    const struct pci_device *pci;
    uint64_t framebuffer, bytes, backing_frames;
    uint32_t width, height, front;
    uint64_t dma[3], backing[2];
    struct transport transport;
    struct queue_cursor cursor;
    struct virtio_gpu_probe_result result;
    struct gpu_rect pending[2];
    struct virtio_gpu_present_stats stats;
    void (*checkpoint_fn)(void *);
    void *checkpoint_arg;
} presentation;

static void presentation_checkpoint(void) {
    if (presentation.checkpoint_fn) presentation.checkpoint_fn(presentation.checkpoint_arg);
}
static void zero_cooperatively(struct transport *t, void *memory, uint64_t bytes) {
    uint8_t *p = memory;
    while (bytes) {
        unsigned chunk = bytes > 65536 ? 65536 : (unsigned)bytes;
        zero(p, chunk);
        p += chunk;
        bytes -= chunk;
        checkpoint(t);
    }
}

static bool presentation_enter(void) {
    return !__atomic_test_and_set(&presentation.busy, __ATOMIC_ACQUIRE);
}
static void presentation_leave(void) {
    __atomic_clear(&presentation.busy, __ATOMIC_RELEASE);
}
static bool kernel_context(void) {
    uint64_t kernel = vmm_kernel_address_space();
    return kernel && vmm_current_address_space() == kernel;
}

bool virtio_gpu_present_prepare(uint64_t framebuffer, uint32_t width, uint32_t height) {
    if (!presentation_enter()) return false;
    if (presentation.attempted) { presentation_leave(); return false; }
    presentation.attempted = true;
    presentation.result.status = VGPU_PROBE_REFUSED;
    uint64_t bytes = (uint64_t)width * height * 4;
    if (!kernel_context() || !framebuffer || !width || !height ||
        width > VGPU_MAX_DIMENSION || height > VGPU_MAX_DIMENSION || bytes > VGPU_BACKING_LIMIT)
        goto refused;
    presentation.result.status = VGPU_PROBE_UNAVAILABLE;
    unsigned vga_count = 0;
    const struct pci_device *candidate = 0;
    for (uint64_t i = 0; i < pci_device_count(); ++i) {
        const struct pci_device *p = pci_device_at(i);
        if (!p || p->class_code != 3 || p->subclass != 0) continue;
        ++vga_count;
        if (p->vendor_id == 0x1af4 && p->device_id == 0x1050 && (p->header_type & 0x7f) == 0)
            candidate = p;
    }
    uint64_t base = 0;
    if (vga_count != 1 || !candidate || !memory_bar(candidate, 0, &base) || base != framebuffer)
        goto refused;
    struct transport t = {.pci = candidate, .cr3 = vmm_kernel_address_space()};
    presentation.result.status = VGPU_PROBE_BAD_CAPABILITY;
    if (!capabilities(&t)) goto refused;
    /* Read-only admission. In particular, no VGA register or BAR writes and no
     * PCI_CFG selector writes occur until the explicit runtime begin. */
    presentation.pci = candidate;
    presentation.framebuffer = framebuffer;
    presentation.width = width;
    presentation.height = height;
    presentation.bytes = bytes;
    presentation.backing_frames = (bytes + 4095) / 4096;
    presentation.prepared = true;
    presentation.result = (struct virtio_gpu_probe_result){.status = VGPU_PROBE_OK,
        .width = width, .height = height};
    presentation_leave();
    return true;
refused:
    presentation_leave();
    return false;
}

/* Called with the sole presentation owner. Reset tears down all host resources
 * before any backing or command memory is freed. On QEMU virtio-vga this also
 * returns scanout to the inherited VGA register configuration (never rewritten
 * by this driver). No RESOURCE_UNREF path may bypass the reset proof. */
static bool presentation_stop(void) {
    struct transport *t = &presentation.transport;
    bool ok = true;
    if (presentation.owned) {
        presentation.result.reset_ok = reset(t);
        pci_write_config16(t->pci, 4, t->original_command & ~6u);
        if (!presentation.result.reset_ok) {
            presentation.result.status = VGPU_PROBE_RESET_FAILED;
            presentation.result.quarantined = presentation.dma[0] || presentation.dma[1] ||
                presentation.dma[2] || presentation.backing[0] || presentation.backing[1];
            presentation.blocked = true;
            presentation.active = presentation.owned = false;
            presentation.command_changed = false;
            unmap(t);
            return false;
        }
        for (unsigned i = 0; i < 2; ++i) {
            if (presentation.backing[i]) pmm_free_frame_range(presentation.backing[i], presentation.backing_frames);
            presentation.backing[i] = 0;
            checkpoint(t);
        }
        for (unsigned i = 0; i < 3; ++i) {
            if (presentation.dma[i]) pmm_free_frame(presentation.dma[i]);
            presentation.dma[i] = 0;
            checkpoint(t);
        }
    }
    presentation.active = presentation.owned = presentation.shown = false;
    if (presentation.command_changed) {
        pci_write_config16(t->pci, 4, t->original_command);
        if (pci_read_config16(t->pci, 4) != t->original_command) {
            presentation.result.status = VGPU_PROBE_REFUSED;
            ok = false;
        }
        presentation.command_changed = false;
    }
    if (!unmap(t)) { presentation.result.status = VGPU_PROBE_MAPPING_FAILED; ok = false; }
    if (!ok) presentation.blocked = true;
    return ok;
}

static bool presentation_command(unsigned bytes) {
    presentation.result.fence_ok = false;
    presentation.result.status = command(&presentation.transport, presentation.dma,
        &presentation.cursor, bytes, sizeof(struct gpu_header), 0x1100);
    if (presentation.result.status != VGPU_PROBE_OK) return false;
    presentation.result.fence_ok = true;
    return true;
}

bool virtio_gpu_present_begin(void) {
    if (!presentation_enter()) return false;
    if (!presentation.prepared || presentation.blocked || presentation.active || !kernel_context()) {
        presentation_leave(); return false;
    }
    presentation.result = (struct virtio_gpu_probe_result){.status = VGPU_PROBE_REFUSED,
        .width = presentation.width, .height = presentation.height};
    presentation.stats = (struct virtio_gpu_present_stats){0};
    presentation.pending[0] = presentation.pending[1] =
        (struct gpu_rect){0, 0, presentation.width, presentation.height};
    presentation.transport = (struct transport){.pci = presentation.pci,
        .cr3 = vmm_kernel_address_space(), .cooperative = true};
    presentation.cursor = (struct queue_cursor){.fence = VGPU_FENCE - 1};
    struct transport *t = &presentation.transport;
    uint64_t framebuffer = 0;
    if (!memory_bar(t->pci, 0, &framebuffer) || framebuffer != presentation.framebuffer) goto failed;
    presentation.result.status = VGPU_PROBE_BAD_CAPABILITY;
    if (!capabilities(t)) goto failed;
    presentation.result.status = VGPU_PROBE_REFUSED;
    if (!inactive_via_pci(t)) goto failed;
    t->original_command = pci_read_config16(t->pci, 4);
    presentation.command_changed = true;
    pci_write_config16(t->pci, 4, t->original_command & ~6u);
    presentation.result.status = VGPU_PROBE_BAD_BAR;
    if ((pci_read_config16(t->pci, 4) & 6) || !size_bars(t, presentation.framebuffer)) goto failed;
    presentation.result.status = VGPU_PROBE_MAPPING_FAILED;
    if (!map_range(t, t->common, VGPU_COMMON_SIZE)) goto failed;
    pci_write_config16(t->pci, 4, (t->original_command & ~5u) | 2u | 0x400u);
    if ((pci_read_config16(t->pci, 4) & 0x406) != 0x402) goto failed;
    presentation.result.status = VGPU_PROBE_REFUSED;
    if (common_read(t, VGPU_STATUS, 1) != 0) goto failed;
    presentation.owned = true;
    presentation.result.status = queue_start(t, presentation.dma);
    if (presentation.result.status != VGPU_PROBE_OK) goto failed;
    struct virtio_gpu_probe_result display = {0};
    presentation.result.status = display_info(t, presentation.dma, &presentation.cursor, &display, true);
    if (presentation.result.status != VGPU_PROBE_OK) goto failed;
    for (unsigned i = 0; i < 2; ++i) {
        presentation.backing[i] = pmm_alloc_frames_tagged(presentation.backing_frames, PMM_FRAME_DMA);
        if (!presentation.backing[i]) { presentation.result.status = VGPU_PROBE_NO_MEMORY; goto failed; }
        checkpoint(t);
        zero_cooperatively(t, pmm_phys_to_virt(presentation.backing[i]), presentation.backing_frames * 4096);
    }
    for (unsigned i = 0; i < 2; ++i) {
        struct create_2d_command *create = pmm_phys_to_virt(presentation.dma[1]);
        zero(create, sizeof(*create));
        create->header.type = 0x101;
        create->resource = i + 1;
        create->format = 2; /* B8G8R8X8_UNORM: inherited 32-bit BGRX pixels. */
        create->width = presentation.width;
        create->height = presentation.height;
        if (!presentation_command(sizeof(*create))) goto failed;
        struct attach_command *attach = pmm_phys_to_virt(presentation.dma[1]);
        zero(attach, sizeof(*attach));
        attach->header.type = 0x106;
        attach->resource = i + 1;
        attach->entries = 1;
        attach->address = presentation.backing[i];
        attach->length = (uint32_t)presentation.bytes;
        if (!presentation_command(sizeof(*attach))) goto failed;
    }
    presentation.front = 0;
    presentation.active = true;
    presentation_leave();
    return true;
failed:
    presentation_stop();
    presentation_leave();
    return false;
}

void virtio_gpu_present_damage(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!kernel_context() || !presentation_enter()) return;
    if (!presentation.active || presentation.blocked || !width || !height ||
        x >= presentation.width || y >= presentation.height) goto done;
    if (width > presentation.width - x) width = presentation.width - x;
    if (height > presentation.height - y) height = presentation.height - y;
    for (unsigned i = 0; i < 2; ++i) {
        struct gpu_rect *r = &presentation.pending[i];
        if (!r->width) { *r = (struct gpu_rect){x, y, width, height}; continue; }
        uint32_t left = x < r->x ? x : r->x;
        uint32_t top = y < r->y ? y : r->y;
        uint32_t right = x + width > r->x + r->width ? x + width : r->x + r->width;
        uint32_t bottom = y + height > r->y + r->height ? y + height : r->y + r->height;
        *r = (struct gpu_rect){left, top, right - left, bottom - top};
    }
done:
    presentation_leave();
}

bool virtio_gpu_present_frame(const void *framebuffer, uint64_t pitch) {
    if (!presentation_enter()) return false;
    if (!presentation.active || presentation.blocked || !kernel_context()) {
        presentation_leave(); return false;
    }
    uint64_t row_bytes = (uint64_t)presentation.width * 4;
    /* The parent validates the inherited mapping and pixel layout. Bound its
     * span again here before pointer arithmetic or reading the VGA framebuffer. */
    if (!framebuffer || ((uintptr_t)framebuffer & 3) || (pitch & 3) || pitch < row_bytes ||
        pitch > VGPU_BACKING_LIMIT || pitch > UINT64_MAX / presentation.height ||
        pitch * presentation.height > VGPU_BACKING_LIMIT ||
        (uintptr_t)framebuffer > UINTPTR_MAX - pitch * presentation.height) {
        presentation.result.status = VGPU_PROBE_REFUSED;
        goto failed;
    }
    /* The front resource already represents staging if it has no pending
     * writes. The other resource may still be stale (or never initialized), so
     * a no-op must not flip to it merely because it has outstanding damage. */
    if (presentation.shown && !presentation.pending[presentation.front].width) {
        ++presentation.stats.skipped_frames;
        presentation_leave();
        return true;
    }
    unsigned next = presentation.shown ? 1u - presentation.front : 0;
    struct gpu_rect damage = presentation.pending[next];
    if (!damage.width || !damage.height) { presentation.result.status = VGPU_PROBE_REFUSED; goto failed; }
    uint32_t *destination = pmm_phys_to_virt(presentation.backing[next]);
    unsigned budget = 65536 / sizeof(uint32_t);
    for (uint32_t y = damage.y; y < damage.y + damage.height; ++y) {
        const volatile uint32_t *source = (const volatile uint32_t *)((const uint8_t *)framebuffer + y * pitch);
        for (uint32_t x = damage.x; x < damage.x + damage.width;) {
            unsigned chunk = damage.x + damage.width - x;
            if (chunk > budget) chunk = budget;
            for (unsigned i = 0; i < chunk; ++i) destination[(uint64_t)y * presentation.width + x + i] = source[x + i];
            x += chunk;
            budget -= chunk;
            if (!budget) { checkpoint(&presentation.transport); budget = 65536 / sizeof(uint32_t); }
        }
    }
    if (budget != 65536 / sizeof(uint32_t)) checkpoint(&presentation.transport);
    uint64_t copied_bytes = (uint64_t)damage.width * damage.height * 4;
    presentation.stats.copied_bytes += copied_bytes;
    barrier();
    struct gpu_rect rect = {0, 0, presentation.width, presentation.height};
    struct transfer_command *transfer = pmm_phys_to_virt(presentation.dma[1]);
    zero(transfer, sizeof(*transfer));
    transfer->header.type = 0x105;
    transfer->rect = damage;
    transfer->offset = ((uint64_t)damage.y * presentation.width + damage.x) * 4;
    transfer->resource = next + 1;
    if (!presentation_command(sizeof(*transfer))) goto failed;
    presentation.stats.transferred_bytes += copied_bytes;
    struct scanout_command *scanout = pmm_phys_to_virt(presentation.dma[1]);
    zero(scanout, sizeof(*scanout));
    scanout->header.type = 0x103;
    scanout->rect = rect;
    scanout->resource = next + 1;
    if (!presentation_command(sizeof(*scanout))) goto failed;
    struct flush_command *flush = pmm_phys_to_virt(presentation.dma[1]);
    zero(flush, sizeof(*flush));
    flush->header.type = 0x104;
    flush->rect = rect;
    flush->resource = next + 1;
    if (!presentation_command(sizeof(*flush))) goto failed;
    presentation.pending[next] = (struct gpu_rect){0};
    presentation.front = next;
    presentation.shown = true;
    ++presentation.stats.presented_frames;
    presentation_leave();
    return true;
failed:
    presentation_stop();
    presentation_leave();
    return false;
}

bool virtio_gpu_present_end(void) {
    if (!presentation_enter()) return false;
    if (!kernel_context() || presentation.blocked) { presentation_leave(); return false; }
    bool ok = presentation_stop();
    presentation_leave();
    return ok;
}
bool virtio_gpu_present_available(void) { return presentation.prepared && !presentation.blocked; }
bool virtio_gpu_present_active(void) { return presentation.active; }
const struct virtio_gpu_probe_result *virtio_gpu_present_result(void) { return &presentation.result; }
void virtio_gpu_present_get_stats(struct virtio_gpu_present_stats *out) {
    if (out) *out = presentation.stats;
}
void virtio_gpu_present_set_checkpoint(void (*fn)(void *), void *arg) {
    if (!presentation_enter()) return;
    presentation.checkpoint_fn = fn;
    presentation.checkpoint_arg = arg;
    presentation_leave();
}
