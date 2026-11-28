#ifndef SRVROS_INTEL_BDW_BOOT_SCANOUT_H
#define SRVROS_INTEL_BDW_BOOT_SCANOUT_H

#include <srvros/display_access.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/scanout_publish.h>

/* Proof is for one disjoint GGTT page, in the boot driver's reset epoch.
 * The original framebuffer stays pinned. Other display planes, cursor and
 * proxy must be unable to fetch from this page. PRI_SURF alone cannot prove it. */
enum bdw_boot_page_rights {
    BDW_PAGE_SCANOUT_PINNED = 1u << 0,
    BDW_PAGE_ENGINE_PORTS_SEALED = 1u << 1,
    /* Includes outstanding GPU writes, reads, prefetch and cache traffic
     * that may still reach the candidate. */
    BDW_PAGE_PRIOR_ACCESS_RETIRED = 1u << 2,
    BDW_PAGE_PROXY_EXCLUDED = 1u << 3,
    BDW_PAGE_FIRMWARE_EXCLUDED = 1u << 4,
    BDW_PAGE_ALIASES_PINNED = 1u << 5,
    BDW_PAGE_BACKING_VISIBLE = 1u << 6,
    /* Device paths must stay excluded independently of engine submission.
     * This does not witness completion of work already in flight. */
    BDW_PAGE_REQUESTERS_EXCLUDED = 1u << 7,
    BDW_PAGE_REQUIRED = (1u << 8) - 1,
};

struct bdw_boot_page;
struct bdw_boot_page_binding {
    const void *driver;
    uint64_t epoch, root, mmio, aperture, aperture_bytes, fence_end;
    struct display_access_token display;
    uint32_t index, plane, surface;
    uint64_t original, backing, marker[2];
};

/* The original driver owns this single-use source and its physical backing.
 * A provider must preserve both until clean return or machine recovery. */
struct bdw_boot_page_source {
    const struct bdw_boot_page_source *object;
    struct bdw_boot_page_binding binding;
    /* Optional boot driver loan. An inherited alias can be tested only when
     * the provider proves it aliases the pinned first framebuffer page. */
    struct scanout_interval *driver_range;
    const struct scanout_interval *firmware_range;
    uint64_t scratch, inherited_backing;
    struct bdw_boot_page *consumer;
    bool consumed;
};

struct bdw_boot_page_proof {
    const struct bdw_boot_page_source *source;
    const void *driver;
    const struct scanout_interval *driver_range;
    const struct scanout_interval *firmware_range;
    uint64_t scratch, inherited_backing;
    uint64_t epoch, root, at;
    uint32_t index, rights, plane, programmed, live;
};

struct bdw_boot_page_io {
    void *ctx;
    bool (*prove)(void *, const struct bdw_boot_page_source *, struct bdw_boot_page_proof *);
    uint64_t (*now)(void *);
    bool (*read_pte)(void *, uint32_t offset, uint64_t *);
    /* The owner keeps both RAM pages and all aliases pinned. The native
     * backend rechecks the live requester and completion proofs immediately
     * before the store, and orders and posts invalidation. */
    enum intel_gt_write (*exchange)(void *, uint32_t offset, uint64_t expected, uint64_t replacement);
    bool (*invalidate)(void *);
    /* Read the first and last QWORD via the admitted CPU aperture alias. */
    bool (*read_aperture)(void *, uint64_t physical, uint64_t words[2]);
};

enum bdw_boot_page_state {
    BDW_PAGE_NEW, BDW_PAGE_RUNNING, BDW_PAGE_REFUSED, BDW_PAGE_RETURNED, BDW_PAGE_RETAINED, BDW_PAGE_SEEDED,
};

struct bdw_boot_page {
    const struct bdw_boot_page *object;
    struct bdw_boot_page_source *source;
    struct bdw_boot_page_binding binding;
    const struct scanout_interval *driver_range;
    const struct scanout_interval *firmware_range;
    uint64_t scratch, inherited_backing;
    struct bdw_boot_page_io io;
    struct scanout_interval interval;
    const struct scanout_interval *outer_loan;
    struct scanout_space space;
    struct scanout_space_token range;
    struct scanout_publication publication;
    struct intel_bdw_ggtt_page entry;
    enum intel_gt_write seed_write;
    uint64_t begin, last, baseline[2];
    unsigned state, stores, invalidations, apertures;
};

/* Run on the boot CPU with submission disabled and zero-initialized driver
 * storage that survives a retained fault. A clean refusal permits the caller
 * to return its resources. Retention keeps the page, source, aliases and
 * display pin live even if the original PTE read back. */
bool intel_bdw_boot_page_run(struct bdw_boot_page *, struct bdw_boot_page_source *,
                             const struct bdw_boot_page_io *);
/* Requires an independently granted, never-published page with an exact zero
 * PTE. The scratch frame is the requested backing. A successful seed leaves
 * the parent and scratch resident for the boot. Faults after any possible
 * write retain the child and display pin as well. */
bool intel_bdw_boot_page_seed(struct bdw_boot_page *, struct bdw_boot_page_source *,
                              const struct bdw_boot_page_io *);
/* The composing owner keeps the exact registered driver or firmware loan alive and is
 * responsible for returning it. A retained child prohibits that return. */
bool intel_bdw_boot_page_in_loan(struct bdw_boot_page *, struct bdw_boot_page_source *,
                                const struct bdw_boot_page_io *, const struct scanout_interval *, bool seed);

#endif
