#ifndef SRVROS_INTEL_HSW_COMPLETION_PAGE_H
#define SRVROS_INTEL_HSW_COMPLETION_PAGE_H

#include <srvros/intel_hsw_ggtt.h>

enum hsw_completion_state { HSW_COMPLETION_NEW, HSW_COMPLETION_REFUSED,
                            HSW_COMPLETION_RETAINED, HSW_COMPLETION_RETURNED };

struct hsw_completion_binding {
    uint64_t boot_epoch, dma_address;
    uint32_t gpu_offset;
};

struct hsw_completion_sample {
    uint32_t pte, pga, hwstam, imr;
};

/* Trusted provider. Its ticket covers the exact device, boot epoch, vacant
 * slot, pinned private backing and inherited Pipe A surface. Valid checks
 * maintained firmware exclusion and absence of competing engine commands.
 * Data and register callbacks operate only on the supplied binding. A false
 * store can leave the old or new value installed. The caller keeps every pin
 * and the ticket reserved on RETAINED. */
struct hsw_completion_io {
    void *ctx;
    bool (*valid)(void *, uint64_t, const struct hsw_completion_binding *);
    uint64_t (*now)(void *);
    bool (*read)(void *, const struct hsw_completion_binding *, struct hsw_completion_sample *);
    bool (*zero_and_verify)(void *, const struct hsw_completion_binding *);
    bool (*write_pte)(void *, const struct hsw_completion_binding *, uint32_t);
    bool (*write_pga)(void *, const struct hsw_completion_binding *, uint32_t);
    bool (*translation_visible)(void *, const struct hsw_completion_binding *, uint32_t);
    bool (*readers_retired)(void *, const struct hsw_completion_binding *);
};

struct hsw_completion_page {
    enum hsw_completion_state state;
    const char *reason;
    struct hsw_completion_binding binding;
    struct intel_hsw_ggtt_page page;
    uint64_t ticket, begin, last;
    unsigned zero_attempts, pte_attempts, pga_attempts;
};

/* One-shot on the boot CPU. Both masks stay closed. The transaction publishes
 * and restores a page without admitting an engine command or a range loan. */
bool intel_hsw_completion_page_run(struct hsw_completion_page *, const struct hwb_result *,
                                   const struct hsw_completion_binding *, const struct hsw_completion_io *, uint64_t);

#endif
