#pragma once
#include <stdbool.h>
#include <stdint.h>

struct hwb_result;

struct intel_hsw_ggtt_page {
    uint32_t index;
    uint32_t mmio_offset;
    uint32_t original;
    uint32_t replacement;
};

/* Haswell GGTT DWORD: address bits 38:32 are in bits 10:4; bit 11
 * belongs to cache control. The caller interprets the present bit. */
static inline uint64_t intel_hsw_ggtt_address(uint32_t pte) {
    return (pte & 0xfffff000ull) | ((pte & 0x7f0ull) << 28);
}

/* Plan one private aperture page from an intact hwb_capture result and its
 * separately sampled slot word. dma_address is a device address independent
 * of the CPU address. The first experiment models only low-32-bit addresses
 * and the Haswell no-cache encoding. This function neither reserves an
 * interval nor authorizes MMIO, DMA, cache policy, or rollback. On refusal the
 * output is unchanged. */
bool intel_hsw_ggtt_plan_page(const struct hwb_result *backing, uint32_t gpu_offset,
                              uint32_t original, uint64_t dma_address,
                              struct intel_hsw_ggtt_page *page);

/* Geometry only. No permission or lifetime follows. */
bool intel_hsw_ggtt_candidate(const struct hwb_result *backing, uint32_t gpu_offset,
                             uint32_t *mmio_offset);

#define HSW_CANDIDATE_GPU 0x01000000u
#define HSW_CANDIDATE_MMIO 0x204000u
#define HSW_BOOT_SLOT_GPU 0x04001000u
#define HSW_BOOT_SLOT_MMIO 0x210004u
#define HSW_CANDIDATE_LIMIT_US 100000u
#define HSW_CANDIDATE_GAP_US 20000u
