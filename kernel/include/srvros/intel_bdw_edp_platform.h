#ifndef SRVROS_INTEL_BDW_EDP_PLATFORM_H
#define SRVROS_INTEL_BDW_EDP_PLATFORM_H

#include <srvros/intel_gt_access.h>

#define BDW_EDP_WELL_DRIVER 0x45404u
#define BDW_EDP_WELL_REQUEST 0x80000000u
#define BDW_EDP_WELL_ON 0x40000000u

enum bdw_edp_platform_word {
    BDW_EDP_PLAT_BIOS, BDW_EDP_PLAT_DRIVER, BDW_EDP_PLAT_KVMR, BDW_EDP_PLAT_DEBUG,
    BDW_EDP_PLAT_FUSE, BDW_EDP_PLAT_PLL, BDW_EDP_PLAT_CDCLK, BDW_EDP_PLAT_PCH_GATE,
    BDW_EDP_PLAT_PCH_CHICKEN, BDW_EDP_PLAT_PPS_STATUS, BDW_EDP_PLAT_PPS_CONTROL,
    BDW_EDP_PLAT_PPS_ON, BDW_EDP_PLAT_PPS_OFF, BDW_EDP_PLAT_PPS_DIV,
    BDW_EDP_PLAT_CPU_PWM, BDW_EDP_PLAT_CPU_DUTY, BDW_EDP_PLAT_PCH_PWM, BDW_EDP_PLAT_PCH_DUTY,
    BDW_EDP_PLAT_WORDS,
};

enum bdw_edp_platform_reason {
    BDW_EDP_PLAT_OK, BDW_EDP_PLAT_INPUT, BDW_EDP_PLAT_SNAPSHOT, BDW_EDP_PLAT_PROFILE,
    BDW_EDP_PLAT_ACQUIRE, BDW_EDP_PLAT_CHILD, BDW_EDP_PLAT_RELEASE,
};

struct bdw_edp_platform_report {
    uint32_t words[4][BDW_EDP_PLAT_WORDS];
    uint64_t begin_us, end_us;
    unsigned samples, writes, reason;
    bool acquired, released, retained;
};

struct bdw_edp_platform_io {
    void *context;
    bool (*held)(void *);
    bool (*read)(void *, uint32_t, uint32_t *);
    /* The only write is the driver request bit. The provider must perform
     * a fresh RMW, preserving other bits and never touching the BIOS well. */
    enum intel_gt_write (*request)(void *, bool);
    uint64_t (*now)(void *);
    /* Run CRC with the original CPU/framebuffer pin and the driver request
     * held. Set retained if child state cannot safely retire. */
    bool (*capture)(void *, bool *retained);
};

/* Isolated Air7,2 inherited-display trial. Snapshots cover endpoints before
 * acquire, while held around the capture, and after release. The BIOS request
 * remains asserted throughout. No clock, PPS, PWM or reset stores. A retained
 * report requires the caller to keep the original exclusion and aliases. */
bool intel_bdw_edp_platform_trial(const struct bdw_edp_platform_io *,
                                  struct bdw_edp_platform_report *);

#define BDW_EDP_PWM_CONTROL 0xc8250u
#define BDW_EDP_PWM_DATA 0xc8254u

struct bdw_edp_pwm_image {
    uint32_t control, data;
};

struct bdw_edp_pwm_io {
    void *context;
    bool (*held)(void *);
    bool (*read)(void *, uint32_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint32_t, uint32_t);
    uint64_t (*now)(void *);
};

struct bdw_edp_pwm_report {
    struct bdw_edp_pwm_image before, after;
    uint64_t elapsed_us;
    unsigned writes;
    bool complete, retained;
};

/* Restore the captured Air7,2 PWM from disabled state at a dark, owned
 * panel. The rehearsal first zeros duty while enabled and disables PWM.
 * The caller owns the live display lease and separately verifies backing,
 * link and CRC before requesting panel backlight. */
bool intel_bdw_edp_pwm_restore(const struct bdw_edp_pwm_io *, struct bdw_edp_pwm_image,
                                bool rehearse, struct bdw_edp_pwm_report *);

#define BDW_EDP_PPS_ON_DELAYS 0xc7208u
#define BDW_EDP_PPS_OFF_DELAYS 0xc720cu
#define BDW_EDP_PPS_DIVISOR 0xc7210u
#define BDW_EDP_PPS_ON_AIR72 0x007d0834u
#define BDW_EDP_PPS_OFF_AIR72 0x02e409c4u
#define BDW_EDP_PPS_DIV_AIR72 0x0004af06u

struct bdw_edp_pps_image {
    uint32_t on, off, divisor;
};

struct bdw_edp_pps_io {
    void *context;
    bool (*held)(void *);
    bool (*read)(void *, uint32_t, uint32_t *);
    enum intel_gt_write (*write)(void *, uint32_t, uint32_t);
    uint64_t (*now)(void *);
    void (*delay)(void *, uint32_t);
};

struct bdw_edp_pps_report {
    struct bdw_edp_pps_image before, reset, after;
    uint32_t initial_status, idle_status;
    uint64_t wait_us, program_us;
    unsigned samples, writes;
    bool complete, retained;
};

/* With the original display owner and route disabled, wait for T12, then
 * restore the captured Air7,2 timing words from their LPT-LP reset state.
 * A rehearsal establishes that state by clearing only the two timing words
 * with all power requests off. Never clear the reference divider or T12. */
bool intel_bdw_edp_pps_restore(const struct bdw_edp_pps_io *, struct bdw_edp_pps_image,
                               bool rehearse, struct bdw_edp_pps_report *);

#endif
