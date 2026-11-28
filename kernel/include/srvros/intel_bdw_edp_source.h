#ifndef SRVROS_INTEL_BDW_EDP_SOURCE_H
#define SRVROS_INTEL_BDW_EDP_SOURCE_H

#include <srvros/intel_bdw_edp.h>

#define BDW_EDP_PHY_ENTRIES 9u

struct bdw_edp_source_image {
    const void *backing;
    uint64_t epoch, mapped_bytes, backing_hash;
    uint64_t backing_physical; /* First page of the contiguous pinned fetch extent, including its tail. */
    uint32_t cdclk_khz, ddi_lane_bits, reference_crc;
    uint32_t pch_pwm_data; /* Required pre-reset sample for a reset-capable cycle. */
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
     * For the isolated cycle only, the adapter manages the mask itself.
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

enum bdw_edp_cycle_stage {
    BDW_EDP_CYCLE_ADMISSION, BDW_EDP_CYCLE_BACKLIGHT, BDW_EDP_CYCLE_PRIMARY,
    BDW_EDP_CYCLE_TRANSCODER, BDW_EDP_CYCLE_PORT, BDW_EDP_CYCLE_PANEL,
    BDW_EDP_CYCLE_DARK, BDW_EDP_CYCLE_RESTORE, BDW_EDP_CYCLE_COMPLETE,
};

enum bdw_edp_panel_off_outcome {
    BDW_EDP_PANEL_OFF_UNSTARTED, BDW_EDP_PANEL_OFF_INTERRUPTED,
    BDW_EDP_PANEL_OFF_TIMEOUT, BDW_EDP_PANEL_OFF_COMPLETE,
};

struct bdw_edp_panel_off_report {
    enum bdw_edp_panel_off_outcome outcome;
    unsigned samples;
    uint32_t first, last;
    uint64_t elapsed_us;
};

enum bdw_edp_dark_result {
    BDW_EDP_DARK_REFUSED,
    BDW_EDP_DARK_UNCHANGED,
    BDW_EDP_DARK_RESET_RECOVERED,
};

struct bdw_edp_cycle_report {
    enum bdw_edp_cycle_stage stage;
    enum bdw_edp_dark_result dark_result;
    struct bdw_edp_source_report shutdown, restore;
    struct bdw_edp_panel_off_report panel_off;
    enum bdw_edp_source_error mask_error;
    uint32_t mask_before, mask_live, mask_after;
    uint64_t dark_hash;
    unsigned mask_writes;
    bool mask_restored;
    bool retained;
};

/* Runs under the same display and backing lease after shutdown. A recovered
 * reset must return with config/MMIO decoding reestablished, the original
 * backing verified, the original locked CDCLK, the original
 * PPS timing and PWM restored, and an owned all-masked Pipe A with IER and
 * IIR clear. The cycle verifies panel resources and re-arms underrun polling.
 * No register access follows a refused or uncertain handoff. */
typedef enum bdw_edp_dark_result (*bdw_edp_dark_handoff)(
    void *, const struct bdw_edp_source_image *);

/* Disable and restore the single inherited eDP path under one live owner.
 * The owner has already excluded all other display clients and proved
 * inactive VGA, display audio, PSR, FBC, IPS, panel fitter and other pipes.
 * The backing_hash callback must verify the original PTEs and fences with
 * the pipe and panel either on or off. The owner admits the intact PPS
 * delay programming and gamma LUT. Only the underrun IMR bit may change
 * around the cycle, with its interrupt delivery disabled. No PCI, GT,
 * LCPLL or GGTT writes.
 * Any failure after a possibly issued write keeps the original owner pinned. */
bool intel_bdw_edp_source_cycle(const struct bdw_edp_source_io *,
                                const struct bdw_edp_source_image *,
                                const struct bdw_edp_baseline_report *,
                                bdw_edp_dark_handoff, void *handoff_context,
                                struct bdw_edp_cycle_report *);

#endif
