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
/* Called only inside the publisher's write callback. The device owner must
 * pin its original CPU root and its exclusive UC register alias across the
 * callback. A successful store includes raw posting readback. The separate
 * flush and reader callbacks remain mandatory for publication and reuse. */
struct vmm_reserved_guard;
/* request acquires the hardware block for this publication exactly once.
 * held_epoch checks the same registered request using CPU state. Zero means
 * no block. read is bounded, guarded MMIO under the original display owner.
 * release is permitted only after restoration, invalidation and retirement. */
struct intel_bdw_scanout_traffic {
    uint64_t (*held_epoch)(const struct scanout_transfer *,const struct scanout_publication *);
    bool (*read)(const struct scanout_transfer *,uint32_t offset,uint32_t *);
    /* Read low SCRATCH1 (0xb11c) for slices 0 and 1 in that order. If steering is used, own it and restore it. */
    bool (*read_l3_flush_policy)(const struct scanout_transfer *, uint32_t words[2]);
    enum intel_gt_write (*request)(const struct scanout_transfer *,const struct scanout_publication *,uint64_t *epoch);
    enum intel_gt_write (*release)(const struct scanout_transfer *,const struct scanout_publication *,uint64_t epoch);
};
bool intel_bdw_scanout_store_test_page(struct scanout_memory *, const struct scanout_memory_token *,
                                      struct scanout_transfer *, const struct scanout_transfer_token *,
                                      struct intel_bdw_scanout_traffic, const struct vmm_reserved_guard *, uint64_t root, uint64_t alias, uint32_t index, uint64_t value);

/* Return the CPU aperture pin after scanout_memory_close retired this exact
 * publication. Until then, including on uncertain writes, parent close fails. */
bool intel_bdw_scanout_retire_test_page(const struct scanout_memory *,const struct scanout_memory_token *);

#endif
