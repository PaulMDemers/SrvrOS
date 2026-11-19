#ifndef SRVROS_INTEL_BDW_EDP_SOURCE_H
#define SRVROS_INTEL_BDW_EDP_SOURCE_H

#include <srvros/intel_bdw_edp.h>

#define BDW_EDP_PHY_ENTRIES 9u

struct bdw_edp_source_image {
    const void *backing;
    uint64_t epoch, mapped_bytes, backing_hash;
    uint32_t cdclk_khz, ddi_lane_bits, reference_crc;
    uint32_t phy[BDW_EDP_PHY_ENTRIES][2];
};

enum bdw_edp_source_error {
    BDW_EDP_SOURCE_NONE,
    BDW_EDP_SOURCE_IMAGE,
    BDW_EDP_SOURCE_OWNER,
    BDW_EDP_SOURCE_RESET_STATE,
    BDW_EDP_SOURCE_CLOCK,
    BDW_EDP_SOURCE_REGISTER,
    BDW_EDP_SOURCE_ORDER,
    BDW_EDP_SOURCE_UNDERRUN,
    BDW_EDP_SOURCE_FRAME,
    BDW_EDP_SOURCE_PANEL,
};

struct bdw_edp_source_report {
    struct bdw_edp_report panel;
    enum bdw_edp_source_error source_error;
    uint32_t offset, word, frame_first, frame_last;
    uint32_t captured_crc[2];
    unsigned source_writes, verified_frames, crc_checks;
    bool crc_retained;
};
struct bdw_edp_baseline_report;

struct bdw_edp_source_io {
    void *context;
    /* Verify the exact pre-reset board PHY values, original X-tiled primary
     * at GGTT offset 0 with its full extent and tail, and current reset epoch.
     * Maintain exclusive powered MMIO, clocks, mapping and render exclusion.
     * The owner admits the LP panel workaround, PWM, fixed 2us WM0 latency,
     * one-pipe FIFO/DDB, and no competing pipe/plane, PSR, FBC, IPS or LP
     * watermark mode. It admits the sticky pipe-A underrun latch unmasked
     * in IMR and disabled in IER with no handler acknowledging events.
     * This guard must work with the panel either off or on. */
    bool (*held)(void *, const struct bdw_edp_source_image *);
    bool (*read)(void *, uint32_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint32_t, uint32_t);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
    /* Compute the live backing fingerprint with the same full PTE census
     * used before reset. The owner also keeps the reference pixels intact
     * across reset and excludes all framebuffer writers. */
    bool (*backing_hash)(void *, const struct bdw_edp_source_image *, uint64_t *);
};

/* Host-testable source adapter for the fixed Air7,2 replay. The caller
 * supplies already-restored LCPLL/CDCLK, an original backing pin and the
 * stable pre-reset primary CRC for the reference pixels. The provider must
 * keep the captured reference and backing under the same live owner across
 * reset. A report from a previous boot cannot grant that authority. It
 * retains the pin regardless of outcome. No reset, mapping or global clock
 * writes. The only intended native caller is a terminal recovery diagnostic. */
enum bdw_edp_result intel_bdw_edp_source_restore(const struct bdw_edp_source_io *,
                                                const struct bdw_edp_source_image *,
                                                const struct bdw_edp_baseline_report *,
                                                struct bdw_edp_source_report *);

#endif
