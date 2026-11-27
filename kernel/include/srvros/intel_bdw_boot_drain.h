#ifndef SRVROS_INTEL_BDW_BOOT_DRAIN_H
#define SRVROS_INTEL_BDW_BOOT_DRAIN_H

#include <srvros/intel_bdw_boot_scanout.h>

enum bdw_boot_drain_state {
    BDW_DRAIN_NEW, BDW_DRAIN_ADMITTING, BDW_DRAIN_WAITING, BDW_DRAIN_RELEASING,
    BDW_DRAIN_REFUSED, BDW_DRAIN_RETAINED, BDW_DRAIN_RETURNED,
};

struct bdw_boot_drain_io {
    void *ctx;
    /* Existing boot owner, root and reset epoch, PCI command 3, sealed engine
     * ports, excluded firmware/proxy, and maintained L3 bank policy. */
    bool (*parent)(void *, const struct bdw_boot_page_binding *);
    uint64_t (*now)(void *);
    void (*pause)(void *);
    bool (*read)(void *, uint32_t offset, uint32_t *);
    bool (*read_pte)(void *, uint32_t index, uint64_t *);
    /* Request hardware control. GT_WRITE_NONE guarantees no action. The
     * ticket is live only while this provider owns the actual request. */
    enum intel_gt_write (*request)(void *, uint64_t *ticket);
    uint64_t (*held)(void *);
    /* DONE relinquishes this ticket. The arbiter can still be reopening. */
    enum intel_gt_write (*release)(void *, uint64_t ticket);
};

struct bdw_boot_drain {
    const struct bdw_boot_drain *object;
    struct bdw_boot_page_binding binding;
    struct bdw_boot_drain_io io;
    struct scanout_interval interval;
    uint64_t begin, last, release_begin, ticket;
    unsigned state, polls, return_polls;
};

/* Serialized on the boot CPU. Use persistent zero-initialized storage.
 * A retained outcome keeps the interval and display pin through recovery.
 * A returned outcome describes a completed trial without ongoing authority. */
bool intel_bdw_boot_drain_run(struct bdw_boot_drain *, const struct bdw_boot_page_binding *,
                             const struct bdw_boot_drain_io *);

#endif
