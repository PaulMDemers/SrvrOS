#ifndef SRVROS_INTEL_BDW_PCI_HOLD_H
#define SRVROS_INTEL_BDW_PCI_HOLD_H

#include <srvros/intel_gt_access.h>

#define BDW_PCI_HOLD_LIMIT_US 2000000u
#define BDW_PCI_HOLD_TP_LIMIT_US 10000u
#define BDW_PCI_HOLD_SETTLE_US 100u
#define BDW_PCI_HOLD_SAMPLES 5u

struct bdw_pci_hold_sample {
    uint32_t command, af;
};

#define BDW_PCI_RC6_INTERVAL_US 2000u

struct bdw_pci_rc6_sample {
    uint64_t begin_us, end_us; /* Relative to entry into this PCI hold. */
    uint32_t core_status, residency, wake_ack;
};

struct bdw_pci_hold_report {
    struct bdw_pci_rc6_sample rc6[2];
    unsigned rc6_count;
    struct bdw_pci_hold_sample sample[BDW_PCI_HOLD_SAMPLES];
    uint64_t elapsed_us, settle_us, first_clear_us;
    unsigned samples, writes, polls;
    bool cpu_copy, backing, complete, retained;
};

struct bdw_pci_hold_io {
    void *context;
    /* The original CPU display/backing exclusion. Guard and config
     * callbacks must work with memory decode closed and must never use MMIO. */
    bool (*held)(void *);
    bool (*sample)(void *, struct bdw_pci_hold_sample *);
    enum intel_gt_write (*command)(void *, uint16_t before, uint16_t after);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
    bool (*read_rc6)(void *, struct bdw_pci_rc6_sample *); /* Only at Command 0x403. */
    bool (*cpu_copy)(void *); /* Only CPU-owned ordinary RAM at Command 0x400. */
    bool (*backing)(void *);  /* Full live backing comparison at Command 0x402. */
};

/* After verified display shutdown, inhibit BME, close decode, prove the
 * CPU copy independently of the aperture, and reopen MEM before BME.
 * AF TP only reports non-posted transactions. No reset or DMA-drain grant. */
bool intel_bdw_pci_hold(const struct bdw_pci_hold_io *, struct bdw_pci_hold_report *);

#endif
