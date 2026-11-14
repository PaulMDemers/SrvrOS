#ifndef SRVROS_SCANOUT_TRANSFER_H
#define SRVROS_SCANOUT_TRANSFER_H
#include <srvros/scanout_handoff.h>

/* Expanded trusted-adapter transfer, separate from the legacy16-entry native
 * observation. CPU0 serialized, not a hardware/IOMMU ownership mechanism.
 * No native provider supplies these assurances or clears the obligations.
 * Own this large object in static/heap storage, never a kernel stack. */
#define SCANOUT_TRANSFER_PAGES 4096u
#define SCANOUT_TRANSFER_DEADLINE_US 100000u
enum scanout_transfer_state {TRANSFER_EMPTY,TRANSFER_CAPTURING,TRANSFER_ACTIVE,TRANSFER_CLOSING,TRANSFER_CLOSED,TRANSFER_FAULT};
enum scanout_transfer_error {TRANSFER_OK,TRANSFER_INPUT,TRANSFER_ASSURANCE,TRANSFER_EXCLUSION,TRANSFER_CLOCK,TRANSFER_PROVIDER,TRANSFER_SPACE};
enum scanout_transfer_phase {TRANSFER_ADMIT,TRANSFER_LIVE,TRANSFER_RETIRE};
struct scanout_transfer_manifest {
    uint64_t mmio,aperture,aperture_bytes,fence_begin,fence_end,source_epoch;
    uint32_t first,pages,assurances,obligations;
    /* Exactly pages raw entries, including padding, copied before callbacks.
     * Entry values do not imply availability or authorize freeing targets. */
    const uint64_t *original;
};
struct scanout_transfer_io {
    void *ctx;
    /* Read-only, bounded, same-boot source/firmware/engine/DMA/cache ownership
     * validation. ADMIT/RETIRE also require every exact original PTE; LIVE
     * validates maintained ownership while children may have published PTEs.
     * A real provider must independently establish every claimed assurance
     * and completion obligation, not echo these fields back as proof. */
    bool (*revalidate)(void *,const struct scanout_transfer_manifest *,enum scanout_transfer_phase);
    uint64_t (*now)(void *);
};
struct scanout_transfer_pool {uint64_t epoch;};
struct scanout_transfer;
struct scanout_transfer_token {const struct scanout_transfer *object;uint64_t epoch,owner;};
struct scanout_transfer {
    const struct scanout_transfer *object;
    uint64_t epoch,owner,arena_epoch,begin_us,last_us;
    struct display_access_token exclusion;
    struct scanout_transfer_manifest manifest;
    struct scanout_transfer_io io;
    struct scanout_space space;
    uint64_t original[SCANOUT_TRANSFER_PAGES];
    uint32_t state,error;
    bool busy,pinned;
};
/* Zero-init/address-sealed. Reuse only after clean CLOSED, with a fresh epoch.
 * Missing assurances/any open obligation refuses before provider or arena.
 * Captures inputs and pins CPU exclusion before validation. Clean read-only
 * admission failures unwind; loss of exclusion retains the pin until reset. */
bool scanout_transfer_open(struct scanout_transfer *,struct scanout_transfer_pool *,
    struct scanout_space_pool *,const struct display_access_token *,
    const struct scanout_transfer_manifest *,struct scanout_transfer_io,
    struct scanout_transfer_token *);
/* A live check failure closes reservation admission and latches FAULT. No retry
 * or reuse after lost ownership; all existing leases and backing stay pinned. */
bool scanout_transfer_check(struct scanout_transfer *,const struct scanout_transfer_token *);
/* Exact captured entry lookup, not fresh hardware proof. Permitted while
 * closing so a still-owned child's restoration can use its original vector. */
bool scanout_transfer_original(const struct scanout_transfer *,const struct scanout_transfer_token *,uint32_t index,uint64_t *);
/* Stop reservations, then drain children before RETIRE validation. Pending
 * readers are retryable; failed provider/clock/exclusion retains a terminal
 * fault. Only a full original-state proof returns this owner's exclusion pin. */
bool scanout_transfer_close(struct scanout_transfer *,const struct scanout_transfer_token *);
#endif
