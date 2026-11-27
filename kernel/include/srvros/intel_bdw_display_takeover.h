#ifndef SRVROS_INTEL_BDW_DISPLAY_TAKEOVER_H
#define SRVROS_INTEL_BDW_DISPLAY_TAKEOVER_H

#include <srvros/intel_bdw_boot_scanout.h>
#include <srvros/intel_bdw_edp_source.h>
#include <srvros/intel_bdw_flr.h>

struct bdw_display_takeover_grant {
    struct bdw_boot_page_binding binding;
    struct scanout_interval *driver_range;
    uint64_t scratch;
    uint64_t private_backing, private_marker[2];
};

struct bdw_display_takeover_reset {
    struct bdw_flr_io io;
    struct bdw_flr_pci_image pci;
    struct bdw_flr_drain *drain;
};

struct bdw_display_takeover_io {
    struct bdw_edp_source_io display;
    struct bdw_boot_page_io page;
    /* Prepare the existing reset provider. Read-only, including refusal. */
    bool (*prepare_reset)(void *, const struct bdw_edp_source_image *, const struct bdw_flr_report *,
                          struct bdw_display_takeover_reset *);
    /* Reestablish clock, well, PPS and PWM under the original dark owner. */
    bool (*recover)(void *, const struct bdw_edp_source_image *, const struct bdw_flr_report *);
    /* Supply a driver-owned vacant page while dark, for this exact completed
     * reset, under its owner and CPU root and in a newer epoch. The output is
     * grant data, not a page source. Keep the range, both frames and original
     * display backing alive until clean return or machine recovery. */
    bool (*range)(void *, const struct bdw_edp_source_image *, const struct bdw_flr_report *,
                  struct bdw_display_takeover_grant *);
};

enum bdw_display_takeover_state {
    BDW_TAKEOVER_NEW, BDW_TAKEOVER_RUNNING, BDW_TAKEOVER_REFUSED,
    BDW_TAKEOVER_READY, BDW_TAKEOVER_RETAINED,
};

struct bdw_display_takeover {
    const struct bdw_display_takeover *object;
    struct bdw_edp_cycle_report cycle;
    struct bdw_flr_report reset;
    struct scanout_interval recovery_loan;
    struct bdw_boot_page_source seed_source, private_source;
    struct bdw_boot_page seed, page;
    enum bdw_display_takeover_state state;
};

/* One use, with zero-initialized storage outside the stack. Keep the caller's
 * original display lease throughout. A retained result keeps an extra pin.
 * A ready result leaves the verified driver scratch mapping resident. */
bool intel_bdw_display_takeover(struct bdw_display_takeover *, const struct display_access_token *,
                                const struct bdw_display_takeover_io *,
                                const struct bdw_edp_source_image *, const struct bdw_edp_baseline_report *);

#endif
