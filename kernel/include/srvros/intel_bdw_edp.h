#ifndef SRVROS_INTEL_BDW_EDP_H
#define SRVROS_INTEL_BDW_EDP_H

#include <srvros/intel_gt_access.h>

enum bdw_edp_stage {
    BDW_EDP_PROBE,
    BDW_EDP_PREPARE_SOURCE,
    BDW_EDP_POWER,
    BDW_EDP_CONFIGURE,
    BDW_EDP_CLOCK_RECOVERY,
    BDW_EDP_EQUALIZE,
    BDW_EDP_IDLE,
    BDW_EDP_SCANOUT,
    BDW_EDP_NORMAL,
    BDW_EDP_LINK_READY,
};

enum bdw_edp_result {
    BDW_EDP_OK,
    BDW_EDP_INVALID,
    BDW_EDP_OWNERSHIP,
    BDW_EDP_CLOCK,
    BDW_EDP_IO,
    BDW_EDP_AUX_BUSY,
    BDW_EDP_AUX_TIMEOUT,
    BDW_EDP_AUX_REPLY,
    BDW_EDP_AUX_DEFER,
    BDW_EDP_CAPS,
    BDW_EDP_TRAINING,
    BDW_EDP_PANEL_STATE,
    BDW_EDP_SCANOUT_FAILED,
};

enum bdw_edp_panel_stage {
    BDW_EDP_PANEL_OFF,
    BDW_EDP_PANEL_DELAYS,
    BDW_EDP_PANEL_POWERING,
    BDW_EDP_PANEL_TRAINING,
    BDW_EDP_PANEL_CHECK_SCANOUT,
    BDW_EDP_PANEL_LIGHTING,
    BDW_EDP_PANEL_READY,
};

struct bdw_edp_io {
    void *context;
    bool (*held)(void *);
    bool (*read)(void *, uint32_t offset, uint32_t *);
    enum intel_gt_write (*write)(void *, uint32_t offset, uint32_t value);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t min_us);
    enum intel_gt_write (*source)(void *, enum bdw_edp_stage,
                                 unsigned swing, unsigned emphasis);
};

struct bdw_edp_report {
    enum bdw_edp_stage stage;
    enum bdw_edp_result result;
    enum bdw_edp_panel_stage panel_stage;
    uint32_t panel_status, panel_control;
    uint32_t aux_status, aux_address, aux_attempts;
    uint8_t caps[16], status[6];
    unsigned swing, emphasis;
    bool effects_possible;
};

/* Fixed MacBookAir7,2 SST profile. Run with the exclusive lifetime already
 * pinned, AUX A powered (including panel VDD) and CDCLK established. The
 * caller's backlight stays dark through the entire attempt. No reentry or
 * operation from a render loop. held() checks the live route, VDD, clock,
 * resource and original backing, including exclusion of competing AUX/PSR.
 * read/write are ordered DWORD accesses to the provider's admitted AUX A.
 *
 * source() admits and verifies each exact stage separately. PREPARE_SOURCE
 * routes the link clock and loads the safe PHY translations with DDI off.
 * For TPS1/TPS2
 * it programs source training and actual signal levels. IDLE emits idle
 * patterns. SCANOUT enables the prepared eDP transcoder and the original
 * primary backing with the source still sending idle. NORMAL selects normal
 * transmission. DONE means verified at that stage. NONE guarantees no write,
 * and UNCERTAIN includes any partially effective change. Provider callbacks
 * are bounded and cannot release the lifetime.
 *
 * The enclosing display owner retains the lifetime on failure.
 * effects_possible records whether this attempt may have issued a write.
 * No additional AUX, disable or retry is attempted here. Success covers
 * link status; verify live scanout before lighting the panel or treating
 * the first frame as restored.
 */
enum bdw_edp_result intel_bdw_edp_recover(const struct bdw_edp_io *,
                                         uint32_t cdclk_khz,
                                         struct bdw_edp_report *);

struct bdw_edp_panel_io {
    struct bdw_edp_io link;
    /* Pin the exact powered Wildcat Point-LP PPS (24 MHz reference,
     * documented LP clock-gate workaround satisfied) and the display route
     * from terminal reset through the final read. This guard also verifies
     * PWM was prepared, and no independent backlight/firmware owner exists. */
    bool (*panel_held)(void *);
    /* Independently verify advancing frames and the live original primary
     * backing, with no underrun. Bound the observation in the provider. */
    bool (*scanout_live)(void *);
    /* An isolated cycle leaves the admitted live PPS delays programmed. */
    bool keep_timings;
};

/* Start from the panel sequencer's reset/off state. The exact source provider
 * still owns clocks, DDI, the original mapping and scanout mode. This routine
 * powers the dark panel, trains the link, checks live scanout and enables
 * panel backlight permission. Report any failure with possibly effective
 * operations to the enclosing owner and retain that lifetime. */
enum bdw_edp_result intel_bdw_edp_panel_restore(const struct bdw_edp_panel_io *,
                                               uint32_t cdclk_khz,
                                               struct bdw_edp_report *);

#endif
