#ifndef SRVROS_INTEL_BDW_FLR_H
#define SRVROS_INTEL_BDW_FLR_H

#include <srvros/intel_gt_access.h>

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

struct bdw_flr_io {
    void *context;
    /* This guard must use only CPU state while configuration is unavailable.
     * It keeps the original backing and all other PCI/display clients excluded. */
    bool (*held)(void *);
    /* Prove requester and memory retirement independently of AF TP and bind
     * the saved BAR spans to reservations for this exact PCI function. */
    bool (*admitted)(void *, const struct bdw_flr_pci_image *);
    bool (*read)(void *, uint8_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint8_t offset, unsigned bytes, uint32_t value);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
};

/* Start with IO/MEM/BME and MSI disabled. Use config accesses only.
 * Reopen the exact original memory BARs with bus mastering and interrupts
 * disabled. The caller still owns clocks, backing and display recovery.
 * A possibly issued reset is never retried or unwound here. */
bool intel_bdw_flr(const struct bdw_flr_io *, const struct bdw_flr_pci_image *,
                   struct bdw_flr_report *);
#endif
