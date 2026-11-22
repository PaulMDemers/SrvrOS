#ifndef SRVROS_INTEL_BDW_SCANOUT_MAPPING_H
#define SRVROS_INTEL_BDW_SCANOUT_MAPPING_H

#include <srvros/scanout_memory.h>
#include <srvros/intel_gt_access.h>
#include <srvros/scanout_transfer.h>

/* Prepare an OS-owned BGRX surface inside an already transferred interval.
 * Publication I/O belongs to that provider and must maintain its guard,
 * cache policy, translation completion and reader retirement on every use.
 * Use scanout_memory_apply/close for publication and exact restoration.
 * If preparation fails after reserving pages, close the memory normally. */
bool intel_bdw_scanout_prepare(struct scanout_memory *, const struct scanout_memory_token *,
                              struct scanout_transfer *, const struct scanout_transfer_token *,
                              struct scanout_publish_pool *, struct scanout_publish_io);

/* Air7,2 one-page diagnostic. The caller holds the original display, a live
 * GPU transfer and a private page whose DMA address equals its physical address.
 * The returned mapping must never be armed as a display surface. */
bool intel_bdw_scanout_prepare_test_page(struct scanout_memory *, const struct scanout_memory_token *,
                                        struct scanout_transfer *, const struct scanout_transfer_token *,
                                        struct scanout_publish_pool *, struct scanout_publish_io);
/* One page from a separate driver-owned interval beyond the permanent import
 * of both firmware buffers. The caller supplies the actual import object and
 * a live hardware transfer for exactly that page. I/O must keep the original
 * plane active, protect the target and retire its translation on both writes.
 * An observed PTE (including zero) never grants this transfer. */
struct bdw_stolen_layout;
bool intel_bdw_scanout_prepare_owned_page(struct scanout_memory *, const struct scanout_memory_token *,
                                         struct scanout_transfer *, const struct scanout_transfer_token *,
                                         const struct scanout_interval *, const struct bdw_stolen_layout *,
                                         struct scanout_publish_pool *,
                                         struct scanout_publish_io);

/* Check one proposed compare-and-store inside the provider's write callback.
 * The provider separately holds the verified register alias and device access
 * through the store, invalidation and reader retirement. */
bool intel_bdw_scanout_owned_page_store_active(const struct scanout_memory *,
        const struct scanout_memory_token *, const struct scanout_transfer *,
        const struct scanout_interval *, const struct bdw_stolen_layout *, uint32_t index);
bool intel_bdw_scanout_owned_page_store_matches(const struct scanout_memory *,
        const struct scanout_memory_token *, struct scanout_transfer *, const struct scanout_transfer_token *,
        const struct scanout_interval *, const struct bdw_stolen_layout *, uint32_t index,
        uint64_t expected, uint64_t value);

/* Check the projected store against the original memory, range, framebuffer
 * fence and publication phase. Call from inside the publisher's writer. */
bool intel_bdw_scanout_page_store_active(const struct scanout_memory *, const struct scanout_memory_token *,
                                        const struct scanout_transfer *, uint32_t index);
/* Called only inside the publisher's write callback. The device owner must
 * pin its original CPU root and its exclusive UC register alias across the
 * callback. A successful store includes raw PTE readback and a posted System
 * Agent GGTT invalidation under that same traffic hold. The separate
 * flush and reader callbacks remain mandatory for publication and reuse. */
struct vmm_reserved_guard;
/* request acquires the hardware block for this publication exactly once.
 * held_epoch checks the same registered request using CPU state. Zero means
 * no block. read is bounded, guarded MMIO under the original display owner.
 * release is permitted only after restoration, invalidation and retirement. */
struct intel_bdw_scanout_traffic {
    uint64_t (*held_epoch)(const struct scanout_transfer *,const struct scanout_publication *);
    bool (*read)(const struct scanout_transfer *,uint32_t offset,uint32_t *);
    /* Under exclusive MCR steering ownership and GT forcewake, compare the
     * full current selector at 0xfdc with expected and change only bits
     * 27:24. Keep ownership and both slices powered through restoration.
     * A missing, changed or uncertain owner must refuse further writes. */
    enum intel_gt_write (*select_l3)(const struct scanout_transfer *, uint32_t expected,
                                     uint32_t selected);
    /* Issue one ordered UC DWORD 1 at MMIO+0x101008 only while this
     * exact publication and traffic epoch are held. No polling or retry. */
    enum intel_gt_write (*invalidate_ggtt)(const struct scanout_transfer *,
                                          const struct scanout_publication *, uint64_t epoch,
                                          uint32_t offset, uint32_t value);
    enum intel_gt_write (*request)(const struct scanout_transfer *,const struct scanout_publication *,uint64_t *epoch);
    enum intel_gt_write (*release)(const struct scanout_transfer *,const struct scanout_publication *,uint64_t epoch);
};
bool intel_bdw_scanout_store_test_page(struct scanout_memory *, const struct scanout_memory_token *,
                                      struct scanout_transfer *, const struct scanout_transfer_token *,
                                      struct intel_bdw_scanout_traffic, const struct vmm_reserved_guard *, uint64_t root, uint64_t alias, uint32_t index, uint64_t value);

/* alias is the aligned base of the one checked MMIO page. The writer derives
 * the QWord offset from the live transfer and keeps the firmware import held. */
bool intel_bdw_scanout_store_owned_page(struct scanout_memory *,const struct scanout_memory_token *,
        struct scanout_transfer *,const struct scanout_transfer_token *, const struct scanout_interval *,
        const struct bdw_stolen_layout *,struct intel_bdw_scanout_traffic,const struct vmm_reserved_guard *,
        uint64_t root,uint64_t alias,uint32_t index,uint64_t value);

/* Return the CPU aperture pin after scanout_memory_close retired this exact
 * publication. Until then, including on uncertain writes, parent close fails. */
bool intel_bdw_scanout_retire_test_page(const struct scanout_memory *,const struct scanout_memory_token *);

#endif
