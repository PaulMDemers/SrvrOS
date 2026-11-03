/* Modern split-ring PCI transport, VirtIO 1.2 sections 2.7, 3.1 and 4.1.
 * https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html
 * Explicit kernel page tables only; all calls are externally serialized. */
#include <srvros/virtio_pci.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

#define VPCI_VIRTUAL 0xffffc003fe000000ull
#define VPCI_MAX_PAGES 16u
#define VPCI_COMMON_SIZE 56u
#define VPCI_STATUS 20u
#define VPCI_RESET_POLLS 1000000u
#define VPCI_NONE 0xffffu
struct region { uint8_t bar; uint32_t offset, length; };
struct descriptor { uint64_t address; uint32_t length; uint16_t flags, next; };
struct used_element { uint32_t id, length; };
_Static_assert(sizeof(struct descriptor) == 16, "split descriptor layout");
_Static_assert(sizeof(struct used_element) == 8, "used element layout");
struct queue {
    uint64_t dma[3], notify;
    uint16_t size, available, used, outstanding;
    bool enabled;
    uint16_t owner[VIRTIO_PCI_MAX_QUEUE_SIZE], next[VIRTIO_PCI_MAX_QUEUE_SIZE];
    uint16_t count[VIRTIO_PCI_MAX_QUEUE_SIZE];
    uint32_t writable[VIRTIO_PCI_MAX_QUEUE_SIZE];
};
struct virtio_pci_device {
    const struct pci_device *pci;
    struct region regions[5];
    uint8_t cfg_cap;
    struct virtio_pci_admission admission;
    uint32_t multiplier;
    uint64_t bar_base[6], bar_size[6];
    uint64_t pages[VPCI_MAX_PAGES];
    unsigned page_count, slot;
    uint64_t cr3, common, features;
    uint16_t original_command;
    bool claimed, command_changed, owned, active, closed, quarantined;
    enum virtio_pci_status status;
    struct queue queues[VIRTIO_PCI_MAX_QUEUES];
};
static struct virtio_pci_device devices[VIRTIO_PCI_MAX_DEVICES];
#ifdef VIRTIO_PCI_HOST_TEST
extern uint32_t virtio_pci_test_mmio_read(uint64_t physical, unsigned width);
extern void virtio_pci_test_mmio_write(uint64_t physical, unsigned width, uint32_t value);
extern void virtio_pci_test_pause(void);
#define transport_pause() virtio_pci_test_pause()
#else
#define transport_pause() __asm__ volatile("pause" ::: "memory")
#endif
static void barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static void zero(void *memory, unsigned bytes) {
    unsigned char *p = memory;
    for (unsigned i = 0; i < bytes; ++i) p[i] = 0;
}
static bool known(const struct virtio_pci_device *t) {
    for (unsigned i = 0; i < VIRTIO_PCI_MAX_DEVICES; ++i)
        if (t == &devices[i]) return t->claimed;
    return false;
}
static bool context(const struct virtio_pci_device *t) {
    return known(t) && t->cr3 && vmm_current_address_space() == t->cr3 &&
        t->cr3 == vmm_kernel_address_space();
}
static bool usable(const struct virtio_pci_device *t) {
    return context(t) && t->owned && !t->closed && !t->quarantined && t->status == VIRTIO_PCI_OK;
}
static bool fail(struct virtio_pci_device *t, enum virtio_pci_status status) {
    t->status = status; return false;
}
static uint64_t virtual_base(const struct virtio_pci_device *t) {
    return VPCI_VIRTUAL + (uint64_t)t->slot * VPCI_MAX_PAGES * 4096;
}
#ifndef VIRTIO_PCI_HOST_TEST
static uint64_t virtual_for(struct virtio_pci_device *t, uint64_t physical) {
    for (unsigned i = 0; i < t->page_count; ++i)
        if (t->pages[i] == (physical & ~4095ull))
            return virtual_base(t) + i * 4096 + (physical & 4095);
    return 0; /* Only validated/mapped fields reach MMIO accessors. */
}
#endif
static uint32_t mmio_read(struct virtio_pci_device *t, uint64_t physical, unsigned width) {
#ifdef VIRTIO_PCI_HOST_TEST
    (void)t;
    return virtio_pci_test_mmio_read(physical, width);
#else
    uint64_t v = virtual_for(t, physical);
    if (width == 1) return *(volatile uint8_t *)v;
    if (width == 2) return *(volatile uint16_t *)v;
    return *(volatile uint32_t *)v;
#endif
}
static void mmio_write(struct virtio_pci_device *t, uint64_t physical, unsigned width, uint32_t value) {
#ifdef VIRTIO_PCI_HOST_TEST
    (void)t;
    virtio_pci_test_mmio_write(physical, width, value);
#else
    uint64_t v = virtual_for(t, physical);
    if (width == 1) *(volatile uint8_t *)v = (uint8_t)value;
    else if (width == 2) *(volatile uint16_t *)v = (uint16_t)value;
    else *(volatile uint32_t *)v = value;
#endif
}
static uint32_t common_read(struct virtio_pci_device *t, unsigned offset, unsigned width) {
    return mmio_read(t, t->common + offset, width);
}
static void common_write(struct virtio_pci_device *t, unsigned offset, unsigned width, uint32_t value) {
    mmio_write(t, t->common + offset, width, value);
}
static void common_address(struct virtio_pci_device *t, unsigned offset, uint64_t value) {
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

static bool capabilities(struct virtio_pci_device *t) {
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
                t->bar_base[r->bar] = base; /* Read-only admission identity. */
                if (type == 1 && (r->length < VPCI_COMMON_SIZE || (r->offset & 3))) return false;
                if (type == 4 && r->length > 4096) return false;
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
    if (!types[1] || !types[2] || !types[3] || !types[5]) return false;
    for (unsigned a = 1; a <= 4; ++a)
        for (unsigned b = a + 1; b <= 4; ++b) {
            struct region *x = &t->regions[a], *y = &t->regions[b];
            if (x->length && y->length && x->bar == y->bar &&
                (uint64_t)x->offset < (uint64_t)y->offset + y->length &&
                (uint64_t)y->offset < (uint64_t)x->offset + x->length) return false;
        }
    return true;
}

/* Handoff must reject aliases before its PCI_CFG status write, not merely
 * before mapping. Known BAR extents cover active owners; equal read-only bases
 * and common ranges also protect previously refused/quarantined reservations. */
static bool aliases_claimed(struct virtio_pci_device *t) {
    const struct region *common = &t->regions[1];
    uint64_t begin = t->bar_base[common->bar] + common->offset;
    uint64_t end = begin + common->length;
    for (unsigned d = 0; d < VIRTIO_PCI_MAX_DEVICES; ++d) {
        const struct virtio_pci_device *other = &devices[d];
        if (other == t || !other->claimed) continue;
        for (unsigned a = 0; a < 6; ++a) for (unsigned b = 0; b < 6; ++b) {
            if (t->bar_base[a] && t->bar_base[a] == other->bar_base[b]) return true;
            if (other->bar_size[b] && begin < other->bar_base[b] + other->bar_size[b] &&
                other->bar_base[b] < end) return true;
        }
        const struct region *r = &other->regions[1];
        if (r->length && r->bar < 6 && other->bar_base[r->bar]) {
            uint64_t prior = other->bar_base[r->bar] + r->offset;
            if (begin < prior + r->length && prior < end) return true;
        }
    }
    return false;
}

/* PCI_CFG checks ownership before disabling decode or touching BAR size. The
 * explicit boot-block handoff alone may write status zero. It cannot adopt an
 * existing kernel BDF (checked before here), and never reads/frees firmware DMA.
 * SeaBIOS leaves modern block status 15 after boot-device discovery:
 * https://github.com/coreboot/seabios/blob/master/src/hw/virtio-blk.c
 * The VM boot caller guarantees firmware no longer runs. Reset proof through
 * PCI_CFG (VirtIO 1.2 section 4.1.4.9) precedes BAR sizing and our DMA ownership. */
static bool inactive_via_pci(struct virtio_pci_device *t, bool boot_block) {
    const struct pci_device *p = t->pci;
    uint8_t at = t->cfg_cap;
    uint8_t saved_bar = pci_read_config8(p, at + 4);
    uint32_t saved_offset = pci_read_config32(p, at + 8);
    uint32_t saved_length = pci_read_config32(p, at + 12);
    pci_write_config8(p, at + 4, t->regions[1].bar);
    pci_write_config32(p, at + 12, 1);
    pci_write_config32(p, at + 8, t->regions[1].offset + VPCI_STATUS);
    bool selected = pci_read_config8(p, at + 4) == t->regions[1].bar &&
        pci_read_config32(p, at + 12) == 1 &&
        pci_read_config32(p, at + 8) == t->regions[1].offset + VPCI_STATUS;
    uint8_t status = selected ? pci_read_config8(p, at + 16) : 0xff;
    t->admission = (struct virtio_pci_admission){.status=status, .selector_ok=selected};
    if (selected && status == 15 && boot_block) {
        t->admission.firmware_reset = true;
        pci_write_config8(p, at + 16, 0);
        bool timed = timer_reference_ready();
        uint64_t started = timed ? timer_monotonic_us() : 0;
        for (unsigned i = 0; i < VPCI_RESET_POLLS; ++i) {
            status = pci_read_config8(p, at + 16);
            if (!status) { barrier(); t->admission.reset_ok = true; break; }
            if (timed && !(i & 255u) && timer_monotonic_us() - started >= 100000) break;
            transport_pause();
        }
    }
    pci_write_config32(p, at + 8, saved_offset);
    pci_write_config32(p, at + 12, saved_length);
    pci_write_config8(p, at + 4, saved_bar);
    return selected && status == 0 && pci_read_config8(p, at + 4) == saved_bar &&
        pci_read_config32(p, at + 8) == saved_offset &&
        pci_read_config32(p, at + 12) == saved_length;
}

static bool size_bars(struct virtio_pci_device *t) {
    const struct pci_device *p = t->pci;
    for (unsigned region = 1; region <= 4; ++region) {
        struct region *r = &t->regions[region];
        if (!r->length) continue;
        unsigned bar = r->bar;
        if (!t->bar_size[bar]) {
            uint64_t base;
            if (!memory_bar(p, bar, &base)) return false;
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
                (base & (size - 1)) || base + size < base || base + size > (1ull << 52)) return false;
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

static bool map_range(struct virtio_pci_device *t, uint64_t physical, unsigned length) {
    uint64_t last = (physical + length - 1) & ~4095ull;
    for (uint64_t page = physical & ~4095ull; page <= last; page += 4096) {
        bool exists = false;
        for (unsigned i = 0; i < t->page_count; ++i) if (t->pages[i] == page) exists = true;
        if (exists) continue;
        if (t->page_count == VPCI_MAX_PAGES) return false;
        uint64_t virtual = virtual_base(t) + t->page_count * 4096;
        uint64_t previous;
        /* Do not overwrite another owner, even at our reserved scratch address. */
        if (vmm_virt_to_phys_in_space(t->cr3, virtual, &previous) ||
            !vmm_map_page_in_space(t->cr3, virtual, page, VMM_PAGE_PRESENT |
                VMM_PAGE_WRITABLE | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE)) return false;
        t->pages[t->page_count++] = page;
    }
    return true;
}

static bool status_ok(struct virtio_pci_device *t) {
    uint8_t expected = t->active ? 15 : 11;
    if (common_read(t, VPCI_STATUS, 1) != expected)
        return fail(t, VIRTIO_PCI_FEATURES_REJECTED);
    return true;
}
static bool reset(struct virtio_pci_device *t) {
    common_write(t, VPCI_STATUS, 1, 0);
    bool timed = timer_reference_ready();
    uint64_t started = timed ? timer_monotonic_us() : 0;
    for (unsigned i = 0; i < VPCI_RESET_POLLS; ++i) {
        if (common_read(t, VPCI_STATUS, 1) == 0) { barrier(); return true; }
        /* Only teardown spins: cap failure latency without yielding while the
         * serialized caller may have temporarily borrowed kernel CR3. */
        if (timed && !(i & 255u) && timer_monotonic_us() - started >= 100000) break;
        transport_pause();
    }
    return false;
}

bool virtio_pci_stop(struct virtio_pci_device *t) {
    if (!context(t) || t->quarantined) return false;
    if (t->closed) return true;
    bool stopped = !t->owned || reset(t);
    t->active = false;
    /* Decode is retained until the reset observation, then DMA is disabled. */
    if (t->command_changed) {
        pci_write_config16(t->pci, 4, t->original_command & ~6u);
        if (pci_read_config16(t->pci, 4) & 6) stopped = false;
    }
    if (!stopped) {
        t->quarantined = true;
        return fail(t, VIRTIO_PCI_RESET_FAILED);
    }
    for (unsigned q = 0; q < VIRTIO_PCI_MAX_QUEUES; ++q) {
        for (unsigned i = 0; i < 3; ++i) {
            if (t->queues[q].dma[i]) pmm_free_frame(t->queues[q].dma[i]);
            t->queues[q].dma[i] = 0;
        }
        t->queues[q].enabled = false;
    }
    t->owned = false;
    t->closed = true;
    if (t->command_changed) {
        pci_write_config16(t->pci, 4, t->original_command);
        if (pci_read_config16(t->pci, 4) != t->original_command)
            return fail(t, VIRTIO_PCI_REFUSED);
    }
    if (t->status == VIRTIO_PCI_OK) t->status = VIRTIO_PCI_STOPPED;
    return true;
}

static bool open_transport(const struct pci_device *pci, uint16_t type,
    uint64_t allowed, uint64_t required, uint32_t config_min,
    struct virtio_pci_device **out, bool boot_block) {
    if (!out) return false;
    *out = 0;
    uint64_t cr3 = vmm_kernel_address_space();
    /* Only device-specific low feature bits and VERSION_1 are implemented. */
    const uint64_t supported = 0xffffffull | VIRTIO_PCI_VERSION_1;
    if (!pci || !cr3 || vmm_current_address_space() != cr3 ||
        (type != 1 && type != 2 && type != 4 && type != 18) || (boot_block && type != 2) ||
        pci->vendor_id != 0x1af4 || pci->device_id != 0x1040u + type ||
        (pci->header_type & 0x7f) || config_min > 4096 ||
        (allowed & ~supported) || (required & ~(allowed | VIRTIO_PCI_VERSION_1))) return false;
    struct virtio_pci_device *t = 0;
    for (unsigned i = 0; i < VIRTIO_PCI_MAX_DEVICES; ++i) {
        if (devices[i].claimed && devices[i].pci->bus == pci->bus &&
            devices[i].pci->device == pci->device && devices[i].pci->function == pci->function)
            return false;
        if (!devices[i].claimed && !t) t = &devices[i];
    }
    if (!t) return false;
    t->admission.status = 255;
    t->claimed = true; t->slot = (unsigned)(t - devices); t->pci = pci; t->cr3 = cr3;
    *out = t;
    /* Collision preflight covers the entire lifetime reservation before PCI writes. */
    for (unsigned i = 0; i < VPCI_MAX_PAGES; ++i) {
        uint64_t prior;
        if (vmm_virt_to_phys_in_space(cr3, virtual_base(t) + i * 4096, &prior)) {
            t->closed = true; return fail(t, VIRTIO_PCI_MAPPING_FAILED);
        }
    }
    if (!capabilities(t) || config_min > t->regions[4].length) {
        t->closed = true; return fail(t, VIRTIO_PCI_BAD_CAPABILITY);
    }
    if (boot_block && aliases_claimed(t)) {
        t->closed = true; return fail(t, VIRTIO_PCI_REFUSED);
    }
    t->original_command = pci_read_config16(pci, 4);
    if (!inactive_via_pci(t, boot_block)) {
        t->closed = true;
        if (t->admission.firmware_reset && !t->admission.reset_ok) {
            /* Firmware DMA retirement is uncertain. Do not size any BAR or
             * allocate memory; disable DMA/decode and reserve the BDF forever. */
            pci_write_config16(pci, 4, t->original_command & ~6u);
            t->quarantined = true;
            return fail(t, VIRTIO_PCI_RESET_FAILED);
        }
        return fail(t, VIRTIO_PCI_REFUSED);
    }
    t->command_changed = true;
    pci_write_config16(pci, 4, t->original_command & ~6u);
    t->status = VIRTIO_PCI_BAD_BAR;
    if ((pci_read_config16(pci, 4) & 6) || !size_bars(t)) goto failed;
    /* Even distinct BDFs may alias a physical BAR. Refuse a second MMIO owner. */
    for (unsigned d = 0; d < VIRTIO_PCI_MAX_DEVICES; ++d) {
        const struct virtio_pci_device *other = &devices[d];
        if (other == t || !other->claimed) continue;
        for (unsigned a = 0; a < 6; ++a) for (unsigned b = 0; b < 6; ++b)
            if (t->bar_size[a] && other->bar_size[b] &&
                t->bar_base[a] < other->bar_base[b] + other->bar_size[b] &&
                other->bar_base[b] < t->bar_base[a] + t->bar_size[a]) goto failed;
    }
    t->status = VIRTIO_PCI_MAPPING_FAILED;
    if (!map_range(t, t->common, VPCI_COMMON_SIZE)) goto failed;
    struct region *isr = &t->regions[3], *cfg = &t->regions[4];
    if (!map_range(t, t->bar_base[isr->bar] + isr->offset, 1)) goto failed;
    if (cfg->length && !map_range(t, t->bar_base[cfg->bar] + cfg->offset, cfg->length)) goto failed;
    pci_write_config16(pci, 4, (t->original_command & ~5u) | 2u | 0x400u);
    if ((pci_read_config16(pci, 4) & 0x407) != 0x402) goto failed;
    t->status = VIRTIO_PCI_REFUSED;
    if (common_read(t, VPCI_STATUS, 1) != 0) goto failed;
    t->owned = true;
    /* Initialization starts with a proven reset even when ownership preflight
     * observed zero; never inherit queue/configuration state from firmware. */
    t->status = VIRTIO_PCI_RESET_FAILED;
    if (!reset(t)) goto failed;
    common_write(t, VPCI_STATUS, 1, 1);
    common_write(t, VPCI_STATUS, 1, 3);
    common_write(t, 0, 4, 0);
    uint64_t offered = common_read(t, 4, 4);
    common_write(t, 0, 4, 1);
    offered |= (uint64_t)common_read(t, 4, 4) << 32;
    required |= VIRTIO_PCI_VERSION_1;
    t->status = VIRTIO_PCI_FEATURES_REJECTED;
    if ((offered & required) != required) goto failed;
    t->features = offered & (allowed | VIRTIO_PCI_VERSION_1);
    common_write(t, 8, 4, 0);
    common_write(t, 12, 4, (uint32_t)t->features);
    common_write(t, 8, 4, 1);
    common_write(t, 12, 4, (uint32_t)(t->features >> 32));
    common_write(t, VPCI_STATUS, 1, 11);
    if (common_read(t, VPCI_STATUS, 1) != 11) goto failed;
    common_write(t, 16, 2, 0xffff);
    if (common_read(t, 16, 2) != 0xffff) goto failed;
    t->status = VIRTIO_PCI_OK;
    return true;
failed:
    (void)virtio_pci_stop(t);
    return false;
}

bool virtio_pci_open(const struct pci_device *pci, uint16_t type,
    uint64_t allowed, uint64_t required, uint32_t config_min,
    struct virtio_pci_device **out) {
    return open_transport(pci, type, allowed, required, config_min, out, false);
}
bool virtio_pci_open_boot_block(const struct pci_device *pci,
    uint64_t allowed, uint64_t required, uint32_t config_min,
    struct virtio_pci_device **out) {
    return open_transport(pci, 2, allowed, required, config_min, out, true);
}
void virtio_pci_get_admission(const struct virtio_pci_device *t, struct virtio_pci_admission *out) {
    if (out) *out = known(t) ? t->admission : (struct virtio_pci_admission){.status=255};
}

bool virtio_pci_queue_setup(struct virtio_pci_device *t, uint16_t index, uint16_t size) {
    if (!usable(t) || t->active || index >= VIRTIO_PCI_MAX_QUEUES || !size ||
        size > VIRTIO_PCI_MAX_QUEUE_SIZE || (size & (size - 1))) return false;
    struct queue *q = &t->queues[index];
    if (q->enabled || q->dma[0] || !status_ok(t)) return false;
    if (index >= common_read(t, 18, 2)) return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    common_write(t, 22, 2, index);
    if (common_read(t, 28, 2) || common_read(t, 24, 2) < size)
        return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    common_write(t, 24, 2, size);
    common_write(t, 26, 2, 0xffff);
    if (common_read(t, 24, 2) != size || common_read(t, 26, 2) != 0xffff)
        return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    uint64_t offset = (uint64_t)common_read(t, 30, 2) * t->multiplier;
    struct region *notify = &t->regions[2];
    if (offset + 2 > notify->length) return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    q->notify = t->bar_base[notify->bar] + notify->offset + offset;
    if (!map_range(t, q->notify, 2)) return fail(t, VIRTIO_PCI_MAPPING_FAILED);
    for (unsigned i = 0; i < 3; ++i) {
        q->dma[i] = pmm_alloc_frame_tagged(PMM_FRAME_DMA);
        if (!q->dma[i]) return fail(t, VIRTIO_PCI_NO_MEMORY);
        zero(pmm_phys_to_virt(q->dma[i]), 4096);
    }
    q->size = size;
    for (unsigned i = 0; i < size; ++i) q->owner[i] = q->next[i] = VPCI_NONE;
    *(uint16_t *)pmm_phys_to_virt(q->dma[1]) = 1; /* NO_INTERRUPT */
    common_address(t, 32, q->dma[0]);
    common_address(t, 40, q->dma[1]);
    common_address(t, 48, q->dma[2]);
    barrier();
    common_write(t, 28, 2, 1);
    if (common_read(t, 28, 2) != 1) return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    q->enabled = true;
    return true;
}

bool virtio_pci_start(struct virtio_pci_device *t) {
    if (!usable(t) || t->active || !status_ok(t)) return false;
    bool any = false;
    for (unsigned i = 0; i < VIRTIO_PCI_MAX_QUEUES; ++i) any |= t->queues[i].enabled;
    if (!any) return false;
    pci_write_config16(t->pci, 4, (t->original_command & ~1u) | 6u | 0x400u);
    if ((pci_read_config16(t->pci, 4) & 0x407) != 0x406)
        return fail(t, VIRTIO_PCI_QUEUE_REJECTED);
    common_write(t, VPCI_STATUS, 1, 15);
    if (common_read(t, VPCI_STATUS, 1) != 15) return fail(t, VIRTIO_PCI_FEATURES_REJECTED);
    t->active = true;
    barrier();
    for (unsigned i = 0; i < VIRTIO_PCI_MAX_QUEUES; ++i)
        if (t->queues[i].enabled && t->queues[i].outstanding)
            mmio_write(t, t->queues[i].notify, 2, i);
    return true;
}

/* Check the complete visible used batch before releasing any head. This catches
 * duplicate pending IDs before a caller can re-submit the first completed head. */
static bool pending_valid(struct virtio_pci_device *t, struct queue *q, uint16_t *pending) {
    volatile uint16_t *used = pmm_phys_to_virt(q->dma[2]);
    uint16_t delta = (uint16_t)(used[1] - q->used);
    barrier();
    if (delta > q->outstanding || delta > q->size) return fail(t, VIRTIO_PCI_BAD_COMPLETION);
    bool seen[VIRTIO_PCI_MAX_QUEUE_SIZE] = {0};
    volatile struct used_element *ring = (volatile void *)(used + 2);
    for (unsigned i = 0; i < delta; ++i) {
        unsigned slot = (uint16_t)(q->used + i) % q->size;
        uint32_t id = ring[slot].id, length = ring[slot].length;
        if (id >= q->size || seen[id] || q->owner[id] != id || !q->count[id] ||
            length > q->writable[id]) return fail(t, VIRTIO_PCI_BAD_COMPLETION);
        seen[id] = true;
    }
    *pending = delta;
    return true;
}

bool virtio_pci_submit(struct virtio_pci_device *t, uint16_t index,
    const struct virtio_pci_buffer *buffers, uint16_t count, uint16_t *token) {
    if (!usable(t) || index >= VIRTIO_PCI_MAX_QUEUES || !buffers || !token || !count)
        return false;
    struct queue *q = &t->queues[index];
    if (!q->enabled || count > q->size || !status_ok(t)) return false;
    uint16_t pending;
    if (!pending_valid(t, q, &pending)) return false;
    uint16_t chosen[VIRTIO_PCI_MAX_QUEUE_SIZE], found = 0;
    for (uint16_t i = 0; i < q->size && found < count; ++i)
        if (q->owner[i] == VPCI_NONE) chosen[found++] = i;
    if (found != count) return false;
    uint64_t writable = 0, total = 0;
    bool writes = false;
    for (unsigned i = 0; i < count; ++i) {
        const struct virtio_pci_buffer *b = &buffers[i];
        if (!b->physical || !b->length || b->physical >= (1ull << 52) ||
            b->length > (1ull << 52) - b->physical || (writes && !b->device_write)) return false;
        total += b->length;
        if (total > UINT32_MAX) return false;
        if (b->device_write) { writes = true; writable += b->length; }
    }
    uint16_t head = chosen[0];
    struct descriptor *desc = pmm_phys_to_virt(q->dma[0]);
    for (unsigned i = 0; i < count; ++i) {
        uint16_t id = chosen[i], next = i + 1 < count ? chosen[i + 1] : VPCI_NONE;
        q->owner[id] = head; q->next[id] = next;
        desc[id] = (struct descriptor){ buffers[i].physical, buffers[i].length,
            (uint16_t)((buffers[i].device_write ? 2 : 0) | (next != VPCI_NONE ? 1 : 0)),
            (uint16_t)(next == VPCI_NONE ? 0 : next) };
    }
    q->count[head] = count; q->writable[head] = (uint32_t)writable;
    volatile uint16_t *avail = pmm_phys_to_virt(q->dma[1]);
    avail[2 + q->available % q->size] = head;
    ++q->outstanding;
    barrier();
    avail[1] = ++q->available;
    barrier();
    *token = head;
    if (t->active) mmio_write(t, q->notify, 2, index);
    return true;
}

int virtio_pci_poll(struct virtio_pci_device *t, uint16_t index,
    struct virtio_pci_completion *completion) {
    if (!usable(t) || !t->active || index >= VIRTIO_PCI_MAX_QUEUES || !completion) return -1;
    struct queue *q = &t->queues[index];
    if (!q->enabled || !status_ok(t)) return -1;
    uint16_t pending;
    if (!pending_valid(t, q, &pending)) return -1;
    if (!pending) return 0;
    volatile uint16_t *used = pmm_phys_to_virt(q->dma[2]);
    volatile struct used_element *ring = (volatile void *)(used + 2);
    uint32_t head = ring[q->used % q->size].id, length = ring[q->used % q->size].length;
    /* Revalidate the selected element before indexing even if a device violates
     * used-ring ownership after publishing. Never trust a device-provided link. */
    if (head >= q->size || q->owner[head] != head || !q->count[head] ||
        length > q->writable[head]) { fail(t, VIRTIO_PCI_BAD_COMPLETION); return -1; }
    bool seen[VIRTIO_PCI_MAX_QUEUE_SIZE] = {0};
    uint16_t ids[VIRTIO_PCI_MAX_QUEUE_SIZE], id = (uint16_t)head, count = q->count[head];
    if (count > q->size) { fail(t, VIRTIO_PCI_BAD_COMPLETION); return -1; }
    for (unsigned i = 0; i < count; ++i) {
        if (id >= q->size || seen[id] || q->owner[id] != head) {
            fail(t, VIRTIO_PCI_BAD_COMPLETION); return -1;
        }
        seen[id] = true; ids[i] = id; id = q->next[id];
    }
    if (id != VPCI_NONE) { fail(t, VIRTIO_PCI_BAD_COMPLETION); return -1; }
    for (unsigned i = 0; i < count; ++i) { q->owner[ids[i]] = VPCI_NONE; q->next[ids[i]] = VPCI_NONE; }
    q->count[head] = 0; q->writable[head] = 0;
    ++q->used; --q->outstanding;
    *completion = (struct virtio_pci_completion){(uint16_t)head, length};
    return 1;
}

static bool config_access(struct virtio_pci_device *t, uint32_t offset, uint32_t length) {
    if (!usable(t) || !length || offset > t->regions[4].length ||
        length > t->regions[4].length - offset) return false;
    return status_ok(t);
}
static bool config_read_width(struct virtio_pci_device *t, uint32_t offset,
    void *dst, uint32_t length, unsigned width) {
    if (!dst || (offset & (width - 1)) || (length & (width - 1)) ||
        !config_access(t, offset, length)) return false;
    uint64_t physical = t->bar_base[t->regions[4].bar] + t->regions[4].offset + offset;
    if (physical & (width - 1)) return false;
    for (unsigned retry = 0; retry < 16; ++retry) {
        uint8_t generation = common_read(t, 21, 1);
        barrier();
        for (uint32_t i = 0; i < length; i += width) {
            uint32_t value = mmio_read(t, physical + i, width);
            for (unsigned j = 0; j < width; ++j) ((uint8_t *)dst)[i + j] = (uint8_t)(value >> (8 * j));
        }
        barrier();
        if (generation == common_read(t, 21, 1)) return true;
    }
    return fail(t, VIRTIO_PCI_BAD_CONFIG);
}
bool virtio_pci_config_read(struct virtio_pci_device *t, uint32_t off, void *dst, uint32_t len) {
    return config_read_width(t, off, dst, len, 1);
}
bool virtio_pci_config_read16(struct virtio_pci_device *t, uint32_t off, uint16_t *value) {
    return config_read_width(t, off, value, 2, 2);
}
bool virtio_pci_config_read32(struct virtio_pci_device *t, uint32_t off, uint32_t *value) {
    return config_read_width(t, off, value, 4, 4);
}
bool virtio_pci_config_read64(struct virtio_pci_device *t, uint32_t off, uint64_t *value) {
    return config_read_width(t, off, value, 8, 4);
}
bool virtio_pci_config_write8(struct virtio_pci_device *t, uint32_t off, uint8_t value) {
    if (!config_access(t, off, 1)) return false;
    mmio_write(t, t->bar_base[t->regions[4].bar] + t->regions[4].offset + off, 1, value);
    barrier();
    return true;
}
bool virtio_pci_quarantined(const struct virtio_pci_device *t) { return known(t) && t->quarantined; }
bool virtio_pci_active(const struct virtio_pci_device *t) { return known(t) && t->active && t->status == VIRTIO_PCI_OK; }
uint64_t virtio_pci_features(const struct virtio_pci_device *t) { return known(t) ? t->features : 0; }
enum virtio_pci_status virtio_pci_get_status(const struct virtio_pci_device *t) { return known(t) ? t->status : VIRTIO_PCI_REFUSED; }
const char *virtio_pci_status_name(enum virtio_pci_status status) {
    static const char *const names[] = {"ok", "refused", "bad-capability", "bad-bar", "mapping-failed",
        "no-memory", "features-rejected", "queue-rejected", "bad-completion", "bad-config", "reset-failed", "stopped"};
    return (unsigned)status < sizeof(names) / sizeof(names[0]) ? names[status] : "unknown";
}
