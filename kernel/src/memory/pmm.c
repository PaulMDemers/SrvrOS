#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/pmm.h>

#include <stddef.h>

#define PMM_MAX_TRACKED_FRAMES (2 * 1024 * 1024)
#define PMM_TAG_COUNT 8
#define PMM_POISON_FREE 0xcc

static uint8_t *bitmap;
static uint8_t frame_tags[PMM_MAX_TRACKED_FRAMES];
static uint64_t tag_counts[PMM_TAG_COUNT];
static uint64_t bitmap_bytes;
static uint64_t frame_count;
static uint64_t free_count;
static uint64_t hhdm;

#define PMM_DEFAULT_MIN_PHYSICAL 0x100000ull

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

static bool bitmap_get(uint64_t frame) {
    return (bitmap[frame / 8] & (uint8_t)(1u << (frame % 8))) != 0;
}

static bool tag_valid(enum pmm_frame_tag tag) {
    return (uint64_t)tag < PMM_TAG_COUNT;
}

static void set_frame_tag(uint64_t frame, enum pmm_frame_tag tag) {
    if (frame >= frame_count || frame >= PMM_MAX_TRACKED_FRAMES || !tag_valid(tag)) {
        return;
    }

    uint8_t old_tag = frame_tags[frame];
    if (old_tag < PMM_TAG_COUNT && tag_counts[old_tag] > 0) {
        tag_counts[old_tag]--;
    }
    frame_tags[frame] = (uint8_t)tag;
    tag_counts[tag]++;
}

static void bitmap_set(uint64_t frame, bool used) {
    uint8_t mask = (uint8_t)(1u << (frame % 8));
    bool was_used = bitmap_get(frame);

    if (used) {
        bitmap[frame / 8] |= mask;
        if (!was_used && free_count > 0) {
            free_count--;
        }
    } else {
        bitmap[frame / 8] &= (uint8_t)~mask;
        if (was_used) {
            free_count++;
        }
    }
}

static void mark_range(uint64_t base, uint64_t length, bool used) {
    uint64_t start;
    uint64_t end;

    if (used) {
        start = align_down(base, PMM_FRAME_SIZE);
        end = align_up(base + length, PMM_FRAME_SIZE);
    } else {
        start = align_up(base, PMM_FRAME_SIZE);
        end = align_down(base + length, PMM_FRAME_SIZE);
    }

    for (uint64_t address = start; address < end; address += PMM_FRAME_SIZE) {
        uint64_t frame = address / PMM_FRAME_SIZE;
        if (frame < frame_count) {
            bitmap_set(frame, used);
            set_frame_tag(frame, used ? PMM_FRAME_RESERVED : PMM_FRAME_FREE);
        }
    }
}

static bool is_managed_type(uint64_t type) {
    return type == LIMINE_MEMMAP_USABLE ||
        type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
        type == LIMINE_MEMMAP_ACPI_NVS ||
        type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
        type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES ||
        type == LIMINE_MEMMAP_FRAMEBUFFER;
}

static uint64_t find_highest_managed_address(struct limine_memmap_response *memmap) {
    uint64_t highest = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (!is_managed_type(entry->type)) {
            continue;
        }

        uint64_t end = entry->base + entry->length;
        if (end > highest) {
            highest = end;
        }
    }

    return highest;
}

static uint64_t find_bitmap_location(struct limine_memmap_response *memmap, uint64_t size) {
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t base = align_up(entry->base, PMM_FRAME_SIZE);
        uint64_t end = entry->base + entry->length;
        if (base < end && end - base >= size) {
            return base;
        }
    }

    return 0;
}

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    if (memmap == NULL || hhdm_offset == 0) {
        console_write("pmm: missing Limine memory data\n");
        halt_forever();
    }

    hhdm = hhdm_offset;
    frame_count = align_up(find_highest_managed_address(memmap), PMM_FRAME_SIZE) / PMM_FRAME_SIZE;
    bitmap_bytes = align_up(frame_count, 8) / 8;

    uint64_t bitmap_physical = find_bitmap_location(memmap, bitmap_bytes);
    if (bitmap_physical == 0) {
        console_write("pmm: no usable range can hold the bitmap\n");
        halt_forever();
    }

    bitmap = (uint8_t *)(bitmap_physical + hhdm);

    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        bitmap[i] = 0xff;
    }
    free_count = 0;
    for (uint64_t i = 0; i < PMM_TAG_COUNT; i++) {
        tag_counts[i] = 0;
    }
    for (uint64_t i = 0; i < frame_count && i < PMM_MAX_TRACKED_FRAMES; i++) {
        frame_tags[i] = PMM_FRAME_RESERVED;
        tag_counts[PMM_FRAME_RESERVED]++;
    }
    if (frame_count > PMM_MAX_TRACKED_FRAMES) {
        console_printf("pmm: tag tracking capped frames=%u tracked=%u\n",
            frame_count,
            (uint64_t)PMM_MAX_TRACKED_FRAMES);
    }

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            mark_range(entry->base, entry->length, false);
        }
    }

    mark_range(bitmap_physical, bitmap_bytes, true);

    console_printf("pmm: frames total=%u free=%u used=%u bitmap=%x bytes=%u\n",
        frame_count,
        free_count,
        pmm_used_frames(),
        bitmap_physical,
        bitmap_bytes);
}

uint64_t pmm_alloc_frame(void) {
    return pmm_alloc_frame_tagged(PMM_FRAME_GENERAL);
}

uint64_t pmm_alloc_frames(uint64_t count) {
    return pmm_alloc_frames_tagged(count, PMM_FRAME_GENERAL);
}

uint64_t pmm_alloc_frames_above(uint64_t count, uint64_t minimum_physical_address) {
    return pmm_alloc_frames_above_tagged(count, minimum_physical_address, PMM_FRAME_GENERAL);
}

uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    return pmm_alloc_frames_tagged(1, tag);
}

uint64_t pmm_alloc_frames_tagged(uint64_t count, enum pmm_frame_tag tag) {
    return pmm_alloc_frames_above_tagged(count, PMM_DEFAULT_MIN_PHYSICAL, tag);
}

uint64_t pmm_alloc_frames_above_tagged(uint64_t count,
    uint64_t minimum_physical_address,
    enum pmm_frame_tag tag) {
    if (count == 0 || count > free_count) {
        return 0;
    }
    if (!tag_valid(tag) || tag == PMM_FRAME_FREE) {
        console_printf("pmm: invalid alloc tag=%u count=%u\n", (uint64_t)tag, count);
        halt_forever();
    }

    uint64_t flags = irq_save();
    uint64_t run_start = 0;
    uint64_t run_length = 0;
    uint64_t minimum_frame = align_up(minimum_physical_address, PMM_FRAME_SIZE) / PMM_FRAME_SIZE;

    for (uint64_t frame = minimum_frame; frame < frame_count; frame++) {
        if (!bitmap_get(frame)) {
            if (run_length == 0) {
                run_start = frame;
            }
            run_length++;

            if (run_length == count) {
                for (uint64_t i = 0; i < count; i++) {
                    bitmap_set(run_start + i, true);
                    set_frame_tag(run_start + i, tag);
                }
                uint64_t physical = run_start * PMM_FRAME_SIZE;
                irq_restore(flags);
                return physical;
            }
        } else {
            run_length = 0;
        }
    }

    irq_restore(flags);
    return 0;
}

void pmm_free_frame(uint64_t physical_address) {
    pmm_free_frame_range(physical_address, 1);
}

void pmm_free_frame_range(uint64_t physical_address, uint64_t count) {
    uint64_t frame = physical_address / PMM_FRAME_SIZE;
    if ((physical_address % PMM_FRAME_SIZE) != 0 ||
        count == 0 ||
        frame >= frame_count ||
        count > frame_count - frame) {
        console_printf("pmm: invalid free address=%x count=%u\n",
            physical_address,
            count);
        halt_forever();
    }

    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < count; i++) {
        if (!bitmap_get(frame + i)) {
            console_printf("pmm: double free frame=%u\n", frame + i);
            halt_forever();
        }
        enum pmm_frame_tag tag = frame + i < PMM_MAX_TRACKED_FRAMES ?
            (enum pmm_frame_tag)frame_tags[frame + i] :
            PMM_FRAME_RESERVED;
        if (tag == PMM_FRAME_RESERVED) {
            console_printf("pmm: free reserved frame=%u phys=%x\n",
                frame + i,
                (frame + i) * PMM_FRAME_SIZE);
            halt_forever();
        }
        if (tag == PMM_FRAME_HEAP) {
            console_printf("pmm: free heap frame=%u phys=%x\n",
                frame + i,
                (frame + i) * PMM_FRAME_SIZE);
            halt_forever();
        }
        uint8_t *bytes = pmm_phys_to_virt((frame + i) * PMM_FRAME_SIZE);
        for (uint64_t j = 0; j < PMM_FRAME_SIZE; j++) {
            bytes[j] = PMM_POISON_FREE;
        }
        bitmap_set(frame + i, false);
        set_frame_tag(frame + i, PMM_FRAME_FREE);
    }
    irq_restore(flags);
}

uint64_t pmm_total_frames(void) {
    return frame_count;
}

uint64_t pmm_free_frames(void) {
    return free_count;
}

uint64_t pmm_used_frames(void) {
    return frame_count - free_count;
}

uint64_t pmm_tagged_frames(enum pmm_frame_tag tag) {
    return tag_valid(tag) ? tag_counts[tag] : 0;
}

const char *pmm_tag_name(enum pmm_frame_tag tag) {
    switch (tag) {
    case PMM_FRAME_FREE:
        return "free";
    case PMM_FRAME_RESERVED:
        return "reserved";
    case PMM_FRAME_GENERAL:
        return "general";
    case PMM_FRAME_HEAP:
        return "heap";
    case PMM_FRAME_PAGE_TABLE:
        return "page-table";
    case PMM_FRAME_USER:
        return "user";
    case PMM_FRAME_KERNEL_STACK:
        return "kernel-stack";
    case PMM_FRAME_DMA:
        return "dma";
    default:
        return "unknown";
    }
}

void pmm_print_status(void) {
    console_printf("pmm: total=%u free=%u used=%u frames\n",
        pmm_total_frames(),
        pmm_free_frames(),
        pmm_used_frames());
    for (uint64_t i = 0; i < PMM_TAG_COUNT; i++) {
        console_printf("pmm: %s=%u\n",
            pmm_tag_name((enum pmm_frame_tag)i),
            tag_counts[i]);
    }
}

uint64_t pmm_hhdm_offset(void) {
    return hhdm;
}

void *pmm_phys_to_virt(uint64_t physical_address) {
    return (void *)(physical_address + hhdm);
}
