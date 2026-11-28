#ifndef SRVROS_INTEL_BDW_EDP_BASELINE_H
#define SRVROS_INTEL_BDW_EDP_BASELINE_H

#include <srvros/intel_bdw_edp_source.h>

#define BDW_EDP_CRC_CTL 0x60050u
#define BDW_EDP_CRC_RES 0x60064u
#define BDW_EDP_FRAME 0x70040u
#define BDW_EDP_PIPE_IMR 0x44404u
#define BDW_EDP_PIPE_IIR 0x44408u
#define BDW_EDP_PIPE_IER 0x4440cu
#define BDW_EDP_FIFO_UNDERRUN (1u << 31)
#define BDW_EDP_CRC_ENABLE 0x80000000u
#define BDW_EDP_CRC_DONE 0x10000000u
#define BDW_EDP_CRC_ERROR 0x20000000u
#define BDW_EDP_CRC_IRQS (BDW_EDP_CRC_DONE | BDW_EDP_CRC_ERROR)
#define BDW_EDP_CRC_FRAMES 4u
#define BDW_EDP_BACKING_PAGES 1484u

enum bdw_edp_baseline_status {
    BDW_EDP_BASELINE_REFUSED, BDW_EDP_BASELINE_OK, BDW_EDP_BASELINE_INCOMPLETE,
};

struct bdw_edp_baseline_sample {
    uint32_t port, buffer, transport, lcpll, cdclk;
    uint32_t phy[BDW_EDP_PHY_ENTRIES][2];
    uint64_t backing_hash;
};

struct bdw_edp_baseline_report {
    enum bdw_edp_baseline_status status;
    struct bdw_edp_baseline_sample sample;
    uint32_t frame[BDW_EDP_CRC_FRAMES], crc[BDW_EDP_CRC_FRAMES];
    uint32_t original_mask, poll_iir;
    unsigned events, writes, reason;
    bool restored, retained;
};

/* Each snapshot verifies the original powered route and all reserved primary
 * mappings, including the 128-entry fetch tail. The provider must own the
 * immutable software/kernel/IRQ epoch and forbid framebuffer writers while
 * held. Read/write are ordered exact Broadwell MMIO. An unreadable or
 * unexpectedly changed control register leaves the lease retained. */
struct bdw_edp_baseline_io {
    void *context;
    bool (*held)(void *);
    bool (*snapshot)(void *, struct bdw_edp_baseline_sample *);
    bool (*read)(void *, uint32_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint32_t, uint32_t);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
};

bool intel_bdw_edp_baseline(const struct bdw_edp_baseline_io *, struct bdw_edp_baseline_report *);
bool intel_bdw_edp_baseline_same(const struct bdw_edp_baseline_report *,
                                 const struct bdw_edp_baseline_report *);

#ifdef SRVROS_INTEL_EDP_BASELINE
bool intel_edp_baseline_probe(bool *retained, bool display_cycle, bool flr_preflight,
                             bool dark_well, bool pci_hold, bool dark_pwm, bool dark_pps, bool live_ggtt);
#endif
#endif
