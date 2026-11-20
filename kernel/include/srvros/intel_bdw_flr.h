#ifndef SRVROS_INTEL_BDW_FLR_H
#define SRVROS_INTEL_BDW_FLR_H

#include <srvros/intel_vtd_drain.h>

struct bdw_flr_pci_image {
    uint32_t bar[4];
    uint32_t subsystem;
};

enum bdw_flr_stage {
    BDW_FLR_ADMISSION, BDW_FLR_WAIT, BDW_FLR_RESOURCES, BDW_FLR_REOPEN, BDW_FLR_COMPLETE,
};

struct bdw_flr_report {
    enum bdw_flr_stage stage;
    unsigned stores, ready_polls;
    uint64_t elapsed_us;
    bool reset_issued, retained;
};

/* These are the original one-use objects registered to the held requester.
 * The separate remapping unit remains accessible with IGD memory decode off. */
struct bdw_flr_drain {
    struct intel_vtd_drain_source *source;
    struct intel_vtd_drain *result;
    struct intel_vtd_drain_io io;
    /* Retire and verify the temporary DRHD mapping after completion, while
     * retaining the parent requester hold and the original source identity. */
    bool (*retire_alias)(void *, const struct intel_vtd_drain_source *, const struct intel_vtd_drain *);
};

struct bdw_flr_io {
    void *context;
    /* This guard must use only CPU state while configuration is unavailable.
     * It keeps the original backing and all other PCI/display clients excluded. */
    bool (*held)(void *);
    /* Verify the registered drain objects, original epoch and BAR reservations.
     * Independently exclude new requests and account for pipelines, caches,
     * prefetch and paths outside the graphics remapper. Maintain that ownership
     * through reset and decode reopen, including inside the drain's own guard. */
    bool (*admitted)(void *, const struct bdw_flr_pci_image *, const struct bdw_flr_drain *);
    bool (*read)(void *, uint8_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint8_t offset, unsigned bytes, uint32_t value);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
};

/* Start with IO/MEM/BME and MSI disabled. Access the IGD via config only.
 * Run a fresh graphics DRHD drain under the original hold before AFCTL.
 * Reopen the exact original memory BARs with bus mastering and interrupts
 * disabled. The caller still owns clocks, backing and display recovery.
 * A possibly issued reset is never retried or unwound here. */
bool intel_bdw_flr(const struct bdw_flr_io *, const struct bdw_flr_pci_image *,
                   const struct bdw_flr_drain *, struct bdw_flr_report *);
#endif
