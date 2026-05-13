#ifndef SRVROS_PMM_H
#define SRVROS_PMM_H

#include <limine.h>
#include <stdbool.h>
#include <stdint.h>

#define PMM_FRAME_SIZE 4096

enum pmm_frame_tag {
    PMM_FRAME_FREE = 0,
    PMM_FRAME_RESERVED,
    PMM_FRAME_GENERAL,
    PMM_FRAME_HEAP,
    PMM_FRAME_PAGE_TABLE,
    PMM_FRAME_USER,
    PMM_FRAME_KERNEL_STACK,
    PMM_FRAME_DMA,
};

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);
uint64_t pmm_alloc_frame(void);
uint64_t pmm_alloc_frames(uint64_t count);
uint64_t pmm_alloc_frames_above(uint64_t count, uint64_t minimum_physical_address);
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag);
uint64_t pmm_alloc_frames_tagged(uint64_t count, enum pmm_frame_tag tag);
uint64_t pmm_alloc_frames_above_tagged(uint64_t count,
    uint64_t minimum_physical_address,
    enum pmm_frame_tag tag);
void pmm_free_frame(uint64_t physical_address);
void pmm_free_frame_range(uint64_t physical_address, uint64_t count);
uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);
uint64_t pmm_used_frames(void);
uint64_t pmm_tagged_frames(enum pmm_frame_tag tag);
const char *pmm_tag_name(enum pmm_frame_tag tag);
void pmm_print_status(void);
uint64_t pmm_hhdm_offset(void);
void *pmm_phys_to_virt(uint64_t physical_address);

#endif
