#ifndef SRVROS_INTEL_BDW_SCANOUT_MAPPING_H
#define SRVROS_INTEL_BDW_SCANOUT_MAPPING_H

#include <srvros/scanout_memory.h>
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
/* held_epoch returns a nonzero generation only while this transfer's own GPM
 * block is continuously held and its fresh engine/L3 flush acknowledgements
 * have been verified. It checks the owner's CPU-side state without MMIO or
 * waiting. The lender retains it through restored translation and
 * reader retirement. read uses that same transfer's bounded guarded MMIO. */
struct intel_bdw_scanout_traffic {
    uint64_t (*held_epoch)(const struct scanout_transfer *);
    bool (*read)(const struct scanout_transfer *,uint32_t offset,uint32_t *);
};
bool intel_bdw_scanout_store_test_page(struct scanout_memory *, const struct scanout_memory_token *,
                                      struct scanout_transfer *, const struct scanout_transfer_token *,
                                      struct intel_bdw_scanout_traffic, const struct vmm_reserved_guard *, uint64_t root, uint64_t alias, uint32_t index, uint64_t value);

/* Return the CPU aperture pin after scanout_memory_close retired this exact
 * publication. Until then, including on uncertain writes, parent close fails. */
bool intel_bdw_scanout_retire_test_page(const struct scanout_memory *,const struct scanout_memory_token *);

#endif
