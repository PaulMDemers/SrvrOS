#ifndef SRVROS_SCANOUT_SPACE_H
#define SRVROS_SCANOUT_SPACE_H
#include <stdbool.h>
#include <stdint.h>

/* CPU0-serialized software leases, not GGTT takeover or DMA authority.
 * All pages start unavailable. Only an audited owner may supply managed ranges;
 * an absent PTE or firmware-reserved physical range is NOT such a grant.
 * No native adapter currently supplies managed ranges or calls this core. */
#define SCANOUT_SPACE_PAGES 65536u
#define SCANOUT_SPACE_LEASES 32u
#define SCANOUT_SPACE_RANGES 64u
#define SCANOUT_SPACE_PTES_RESTORED 1u
#define SCANOUT_SPACE_TRANSLATIONS_RETIRED 2u
#define SCANOUT_SPACE_READERS_RETIRED 4u
#define SCANOUT_SPACE_RETIRE_ALL 7u
enum scanout_space_state {SPACE_EMPTY, SPACE_ACTIVE, SPACE_CLOSING, SPACE_CLOSED, SPACE_FAULT};
enum scanout_space_use {SPACE_UNUSED, SPACE_RESERVED, SPACE_EXPOSED, SPACE_RETIRED, SPACE_QUARANTINED};
struct scanout_space_range {uint32_t first, pages;}; /* Half-open page intervals. */
/* Both handoff APIs reserve from the same GPU address space. Callers serialize
 * on the boot CPU and retain the node until the display pin can be returned. */
struct scanout_interval {
    uint64_t mmio, aperture, aperture_bytes, fence_end;
    struct scanout_space_range pages;
    struct scanout_interval *next;
    bool firmware;
    const void *driver;
    const struct scanout_interval *lender;
    bool sealed;
};
/* Register the verified inherited display fence [0, fence_end) for the boot.
 * Matching registration is idempotent. No candidate lease or write authority
 * follows. Closing a child never releases or resizes the original fence. */
bool scanout_interval_protect_boot_framebuffer(uint64_t mmio, uint64_t aperture,
                                              uint64_t aperture_bytes, uint64_t fence_end);
bool scanout_interval_boot_framebuffer_matches(uint64_t mmio, uint64_t aperture,
                                              uint64_t aperture_bytes, uint64_t fence_end);
bool scanout_interval_claim(struct scanout_interval *, uint64_t mmio, uint64_t aperture,
                            uint64_t aperture_bytes, uint64_t fence_end, struct scanout_space_range);
/* Reserve a GPU-only command range using the verified GGTT table size. */
bool scanout_interval_claim_gpu_only(struct scanout_interval *, uint64_t mmio, uint64_t aperture,
                                     uint64_t bytes, uint64_t fence_end, uint64_t ggtt_bytes,
                                     struct scanout_space_range);
/* Imported firmware spans remain reserved until reboot. A caller holding the
 * original import object may reserve a contained range; other leases conflict. */
bool scanout_interval_import_firmware(struct scanout_interval *, uint64_t mmio, uint64_t aperture,
                                      uint64_t bytes, uint64_t fence_end, struct scanout_space_range);
bool scanout_interval_borrow(struct scanout_interval *, const struct scanout_interval *firmware,
                             uint64_t mmio, uint64_t aperture, uint64_t bytes, uint64_t fence_end,
                             struct scanout_space_range);
/* Register a range already granted to the boot driver for this aperture.
 * The registry excludes software competitors; the caller supplies actual
 * hardware ownership. The original range cannot be released with a loan live. */
bool scanout_interval_claim_driver(struct scanout_interval *, const void *driver,
                                   uint64_t mmio, uint64_t aperture, uint64_t bytes, uint64_t fence_end,
                                   struct scanout_space_range);
bool scanout_interval_borrow_driver(struct scanout_interval *, const struct scanout_interval *owner,
                                    const void *driver, struct scanout_space_range);
/* Commit the live driver's parent range for this boot before installing
 * scratch. It remains reserved even after the child returns. */
bool scanout_interval_seal_driver(const struct scanout_interval *loan,
                                  struct scanout_interval *owner, const void *driver);
bool scanout_interval_held(const struct scanout_interval *,uint64_t mmio,uint64_t aperture,
                           uint64_t bytes,uint64_t fence_end,struct scanout_space_range);
void scanout_interval_release(struct scanout_interval *);
struct scanout_space_entry {uint64_t serial; uint32_t first, pages, use;};
struct scanout_space {
    const struct scanout_space *object;
    uint64_t epoch, owner, serial;
    uint32_t state, pages;
    const uint64_t *originals;
    struct scanout_space_range original_range;
    uint64_t available[SCANOUT_SPACE_PAGES / 64];
    struct scanout_space_entry leases[SCANOUT_SPACE_LEASES];
};
struct scanout_space_pool {uint64_t epoch;};
struct scanout_space_token {
    const struct scanout_space *object;
    uint64_t epoch, serial;
    uint32_t slot, first, pages;
};
/* Preserve every inherited mapping and the FULL inherited fence, including
 * padding/unobserved allocations. Protected intervals override managed ranges.
 * Limits are intentionally restricted to the observed 256MiB CPU aperture. */
bool scanout_space_open(struct scanout_space *, struct scanout_space_pool *, uint64_t owner,
                        uint32_t pages, const struct scanout_space_range *managed, uint32_t managed_count,
                        const struct scanout_space_range *protected, uint32_t protected_count);
/* Import a single transferred interval. Its original owner keeps one immutable
 * entry per managed page live until all leases retire and the arena closes. */
bool scanout_space_open_transferred(struct scanout_space *, struct scanout_space_pool *, uint64_t owner,
                                    uint32_t pages, struct scanout_space_range managed,
                                    struct scanout_space_range protected, const uint64_t *originals);
bool scanout_space_reserve(struct scanout_space *, uint64_t owner, uint64_t epoch,
                           uint32_t pages, uint32_t align_pages, struct scanout_space_token *);
/* Compare an entry within the actual lease. A conflict with transferred
 * originals faults the arena and stops other leases from publishing. */
bool scanout_space_verify_original(struct scanout_space *, const struct scanout_space_token *,
                                   uint32_t offset, uint64_t value);
/* Must run BEFORE any PTE writer can observe the lease. Once exposed, release
 * requires independently proven restoration, translation and reader retirement. */
bool scanout_space_expose(struct scanout_space *, const struct scanout_space_token *);
/* A matching token/proof mask is necessary, not independent hardware proof.
 * Only a future audited adapter may attest the three retirement conditions. */
bool scanout_space_retire(struct scanout_space *, const struct scanout_space_token *, uint32_t proof);
bool scanout_space_quarantine(struct scanout_space *, const struct scanout_space_token *);
bool scanout_space_release(struct scanout_space *, const struct scanout_space_token *);
bool scanout_space_close(struct scanout_space *, uint64_t owner, uint64_t epoch);
#endif
