#include <srvros/intel_bdw_edp_platform.h>

/* Broadwell global well and Lynx Point-LP panel/backlight register block. */
static const uint32_t offsets[BDW_EDP_PLAT_WORDS] = {
    0x45400, BDW_EDP_WELL_DRIVER, 0x45408, 0x4540c,
    0x42014, 0x130040, 0x46200, 0xc2020, 0xc2004,
    0xc7200, 0xc7204, 0xc7208, 0xc720c, 0xc7210,
    0x48250, 0x48254, 0xc8250, 0xc8254,
};

#define PLATFORM_TOTAL_US 600000u
#define PLATFORM_VECTOR_US 25000u

struct platform_attempt {
    const struct bdw_edp_platform_io *io;
    struct bdw_edp_platform_report *report;
    uint64_t last;
};

static bool tick(struct platform_attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    if (!a->report->begin_us)
        a->report->begin_us = now;
    a->last = now;
    a->report->end_us = now;
    return now - a->report->begin_us < PLATFORM_TOTAL_US;
}

static bool sample(struct platform_attempt *a)
{
    if (!tick(a))
        return false;
    uint64_t begin = a->last;
    uint32_t *words = a->report->words[a->report->samples];
    for (unsigned i = 0; i < BDW_EDP_PLAT_WORDS; ++i) {
        if (!a->io->read(a->io->context, offsets[i], &words[i]) ||
            words[i] == UINT32_MAX || !tick(a) || a->last - begin >= PLATFORM_VECTOR_US)
            return false;
    }
    ++a->report->samples;
    return a->last > begin;
}

static bool initial_profile(const uint32_t *w)
{
    uint32_t period = w[BDW_EDP_PLAT_PCH_DUTY] >> 16;
    uint32_t duty = w[BDW_EDP_PLAT_PCH_DUTY] & 0xffffu;
    return w[BDW_EDP_PLAT_BIOS] == (BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON) &&
           w[BDW_EDP_PLAT_DRIVER] == BDW_EDP_WELL_ON &&
           w[BDW_EDP_PLAT_KVMR] == BDW_EDP_WELL_ON &&
           w[BDW_EDP_PLAT_DEBUG] == BDW_EDP_WELL_ON &&
           !(w[BDW_EDP_PLAT_FUSE] & (1u << 24)) &&
           w[BDW_EDP_PLAT_PLL] == 0x4c000000u && w[BDW_EDP_PLAT_CDCLK] == 674u &&
           (w[BDW_EDP_PLAT_PPS_STATUS] & 0xb800000fu) == 0x80000008u &&
           (w[BDW_EDP_PLAT_PPS_CONTROL] & 0xdu) == 5u &&
           !(w[BDW_EDP_PLAT_CPU_PWM] & (1u << 31)) &&
           (w[BDW_EDP_PLAT_PCH_PWM] & (1u << 31)) &&
           period && duty <= period;
}

static bool unchanged(const struct bdw_edp_platform_report *r, bool claimed)
{
    const uint32_t *w = r->words[r->samples - 1];
    for (unsigned i = 0; i < BDW_EDP_PLAT_WORDS; ++i) {
        uint32_t expected = r->words[0][i];
        if (claimed && i == BDW_EDP_PLAT_DRIVER)
            expected |= BDW_EDP_WELL_REQUEST;
        if (w[i] != expected)
            return false;
    }
    return true;
}

bool intel_bdw_edp_platform_trial(const struct bdw_edp_platform_io *io,
                                  struct bdw_edp_platform_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_platform_report){.reason = BDW_EDP_PLAT_INPUT};
    if (!io || !io->held || !io->read || !io->request || !io->now || !io->capture)
        return false;
    struct platform_attempt a = {io, r, 0};
    r->reason = BDW_EDP_PLAT_SNAPSHOT;
    if (!sample(&a))
        return false;
    r->reason = BDW_EDP_PLAT_PROFILE;
    if (!initial_profile(r->words[0]))
        return false;

    r->reason = BDW_EDP_PLAT_ACQUIRE;
    if (!tick(&a))
        return false;
    enum intel_gt_write result = io->request(io->context, true);
    if (result == GT_WRITE_NONE)
        return false;
    ++r->writes;
    r->retained = true;
    if (result != GT_WRITE_DONE || !sample(&a) || !unchanged(r, true))
        return false;
    r->acquired = true;

    r->reason = BDW_EDP_PLAT_CHILD;
    bool child_uncertain = true;
    bool captured = io->capture(io->context, &child_uncertain);
    if (child_uncertain)
        return false;
    r->reason = BDW_EDP_PLAT_RELEASE;
    if (!sample(&a) || !unchanged(r, true) || !tick(&a))
        return false;
    result = io->request(io->context, false);
    if (result != GT_WRITE_NONE)
        ++r->writes;
    if (result != GT_WRITE_DONE || !sample(&a) || !unchanged(r, false))
        return false;
    r->retained = false;
    r->released = true;
    r->reason = captured ? BDW_EDP_PLAT_OK : BDW_EDP_PLAT_CHILD;
    return captured;
}

#define PWM_ENABLED 0xc0000000u /* PCH PWM enable and duty override. */
#define PWM_OVERRIDE 0x40000000u
#define PWM_PERIOD 0x0ad90000u
#define PWM_RESTORE_US 25000u

struct pwm_attempt {
    const struct bdw_edp_pwm_io *io;
    struct bdw_edp_pwm_report *report;
    uint64_t begin, last;
};

static bool pwm_held(struct pwm_attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    if (!a->begin)
        a->begin = now;
    a->last = now;
    a->report->elapsed_us = now - a->begin;
    return a->report->elapsed_us < PWM_RESTORE_US;
}

static bool pwm_expect(struct pwm_attempt *a, uint32_t reg, uint32_t mask, uint32_t expected)
{
    uint32_t word;
    return pwm_held(a) && a->io->read(a->io->context, reg, &word) &&
           (word & mask) == expected && pwm_held(a);
}

static bool pwm_dark(struct pwm_attempt *a)
{
    return pwm_expect(a, 0xc7204, UINT32_MAX, 0) && /* Panel and backlight requests clear. */
           pwm_expect(a, 0xc7200, 0xb0000000u, 0) && /* T12 may still be active. */
           pwm_expect(a, 0x48250, 0x80000000u, 0) && /* CPU PWM disabled. */
           pwm_expect(a, 0xc2004, UINT32_MAX, 0x24);  /* Original polarity/clock profile. */
}

static bool pwm_store(struct pwm_attempt *a, uint32_t reg, uint32_t word)
{
    if (!pwm_dark(a))
        return false;
    enum intel_gt_write status = a->io->write(a->io->context, reg, word);
    if (status != GT_WRITE_NONE) {
        ++a->report->writes;
        a->report->retained = true;
    }
    return status == GT_WRITE_DONE && pwm_expect(a, reg, UINT32_MAX, word);
}

static bool pwm_sample(struct pwm_attempt *a, struct bdw_edp_pwm_image *image)
{
    return pwm_held(a) && a->io->read(a->io->context, BDW_EDP_PWM_CONTROL, &image->control) &&
           pwm_held(a) && a->io->read(a->io->context, BDW_EDP_PWM_DATA, &image->data) && pwm_held(a);
}

bool intel_bdw_edp_pwm_restore(const struct bdw_edp_pwm_io *io, struct bdw_edp_pwm_image saved,
                                bool rehearse, struct bdw_edp_pwm_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_pwm_report){0};
    if (!io || !io->held || !io->read || !io->write || !io->now ||
        saved.control != PWM_ENABLED || (saved.data & 0xffff0000u) != PWM_PERIOD ||
        (saved.data & 0xffffu) > (PWM_PERIOD >> 16))
        return false;
    struct pwm_attempt a = {.io = io, .report = r};
    if (!pwm_dark(&a) || !pwm_sample(&a, &r->before))
        return false;
    if (rehearse) {
        if (r->before.control != saved.control || r->before.data != saved.data ||
            !pwm_store(&a, BDW_EDP_PWM_DATA, PWM_PERIOD) ||
            !pwm_store(&a, BDW_EDP_PWM_CONTROL, PWM_OVERRIDE))
            return false;
    } else if ((r->before.control && r->before.control != PWM_OVERRIDE) ||
               (r->before.data && r->before.data != PWM_PERIOD && r->before.data != saved.data)) {
        return false;
    }
    if (!pwm_store(&a, BDW_EDP_PWM_DATA, PWM_PERIOD) ||
        !pwm_store(&a, BDW_EDP_PWM_CONTROL, saved.control) ||
        !pwm_store(&a, BDW_EDP_PWM_DATA, saved.data) ||
        !pwm_sample(&a, &r->after) || r->after.control != saved.control ||
        r->after.data != saved.data || !pwm_dark(&a))
        return false;
    r->complete = true;
    r->retained = false;
    return true;
}

#define PPS_WAIT_US 525000u
#define PPS_PROGRAM_US 25000u
#define PPS_CYCLE_ACTIVE (1u << 27)
#define PPS_STATUS_NOT_OFF 0xf0000000u

struct pps_attempt {
    const struct bdw_edp_dark_io *io;
    struct bdw_edp_pps_report *report;
    uint64_t begin, last;
    unsigned limit;
};

static bool pps_tick(struct pps_attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    if (!a->begin)
        a->begin = now;
    a->last = now;
    if (a->limit == PPS_WAIT_US)
        a->report->wait_us = now - a->begin;
    else
        a->report->program_us = now - a->begin;
    return now - a->begin < a->limit;
}

static bool pps_read(struct pps_attempt *a, uint32_t offset, uint32_t *value)
{
    return pps_tick(a) && a->io->read(a->io->context, offset, value) && pps_tick(a);
}

static bool pps_status(struct pps_attempt *a, uint32_t *status)
{
    uint32_t control, gate;
    return pps_read(a, 0xc7204, &control) && !control &&
           pps_read(a, 0xc2020, &gate) && gate == 0x1000u &&
           pps_read(a, 0xc7200, status) &&
           !(*status & PPS_STATUS_NOT_OFF);
}

static bool pps_idle(struct pps_attempt *a)
{
    uint32_t status;
    return pps_status(a, &status) && !(status & PPS_CYCLE_ACTIVE);
}

static bool pps_wait_idle(struct pps_attempt *a)
{
    uint32_t status;
    if (!pps_status(a, &status))
        return false;
    a->report->initial_status = status;
    while (status & PPS_CYCLE_ACTIVE) {
        uint64_t before = a->last;
        a->io->delay(a->io->context, 1000);
        if (!pps_tick(a) || a->last - before < 1000 || !pps_status(a, &status))
            return false;
    }
    a->report->idle_status = status;
    return true;
}

static bool pps_sample(struct pps_attempt *a, struct bdw_edp_pps_image *image)
{
    if (!pps_read(a, BDW_EDP_PPS_ON_DELAYS, &image->on) ||
        !pps_read(a, BDW_EDP_PPS_OFF_DELAYS, &image->off) ||
        !pps_read(a, BDW_EDP_PPS_DIVISOR, &image->divisor))
        return false;
    ++a->report->samples;
    return true;
}

static bool pps_store(struct pps_attempt *a, uint32_t offset, uint32_t value)
{
    if (!pps_idle(a))
        return false;
    enum intel_gt_write result = a->io->write(a->io->context, offset, value);
    if (result != GT_WRITE_NONE) {
        ++a->report->writes;
        a->report->retained = true;
    }
    uint32_t posted;
    return result == GT_WRITE_DONE && pps_read(a, offset, &posted) && posted == value;
}

static bool pps_equal(struct bdw_edp_pps_image a, struct bdw_edp_pps_image b)
{
    return a.on == b.on && a.off == b.off && a.divisor == b.divisor;
}

bool intel_bdw_edp_pps_restore(const struct bdw_edp_dark_io *io, struct bdw_edp_pps_image saved,
                               bool rehearse, struct bdw_edp_pps_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_pps_report){0};
    const struct bdw_edp_pps_image original = {
        BDW_EDP_PPS_ON_AIR72, BDW_EDP_PPS_OFF_AIR72, BDW_EDP_PPS_DIV_AIR72,
    };
    const struct bdw_edp_pps_image reset = {.divisor = BDW_EDP_PPS_DIV_AIR72};
    if (!io || !io->held || !io->read || !io->write || !io->now || !io->delay ||
        !pps_equal(saved, original))
        return false;
    struct pps_attempt a = {.io = io, .report = r, .limit = PPS_WAIT_US};
    if (!pps_wait_idle(&a))
        return false;
    a.begin = a.last;
    a.limit = PPS_PROGRAM_US;
    if (!pps_sample(&a, &r->before) || !pps_equal(r->before, rehearse ? saved : reset))
        return false;
    if (rehearse && (!pps_store(&a, BDW_EDP_PPS_ON_DELAYS, 0) ||
                     !pps_store(&a, BDW_EDP_PPS_OFF_DELAYS, 0)))
        return false;
    if (!pps_sample(&a, &r->reset) || !pps_equal(r->reset, reset) ||
        !pps_store(&a, BDW_EDP_PPS_ON_DELAYS, saved.on) ||
        !pps_store(&a, BDW_EDP_PPS_OFF_DELAYS, saved.off) ||
        !pps_store(&a, BDW_EDP_PPS_DIVISOR, saved.divisor) ||
        !pps_sample(&a, &r->after) || !pps_equal(r->after, saved) || !pps_idle(&a))
        return false;
    r->complete = true;
    r->retained = false;
    return true;
}

#define REOPEN_TOTAL_US 25000u
#define REOPEN_WELL_US 1000u
#define AIR72_PLL 0x4c000000u
#define AIR72_FUSE 0x20100042u
#define AIR72_CDCLK 674u
#define CDCLK_DEFAULT 449u
#define AIR72_LP_GATE 0x1000u
#define WELL_OWNED (BDW_EDP_WELL_REQUEST | BDW_EDP_WELL_ON)

struct reopen_attempt {
    const struct bdw_edp_dark_io *io;
    struct bdw_edp_reopen_report *r;
    uint64_t begin, last;
};

static bool reopen_tick(struct reopen_attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    a->last = now;
    a->r->elapsed_us = now - a->begin;
    return a->r->elapsed_us < REOPEN_TOTAL_US;
}

static bool reopen_read(struct reopen_attempt *a, uint32_t reg, uint32_t *word)
{
    return reopen_tick(a) && a->io->read(a->io->context, reg, word) &&
           *word != UINT32_MAX && reopen_tick(a);
}

static bool reopen_expect(struct reopen_attempt *a, uint32_t reg, uint32_t expected)
{
    uint32_t word;
    return reopen_read(a, reg, &word) && word == expected;
}

static bool reopen_dark(struct reopen_attempt *a)
{
    uint32_t status;
    return reopen_expect(a, 0xc7204, 0) && reopen_read(a, 0xc7200, &status) &&
           !(status & PPS_STATUS_NOT_OFF);
}

static bool reopen_write(struct reopen_attempt *a, uint32_t reg, uint32_t word)
{
    uint32_t driver = reg == BDW_EDP_WELL_DRIVER ? a->r->driver_before : WELL_OWNED;
    if (!reopen_expect(a, BDW_EDP_WELL_DRIVER, driver) ||
        !reopen_expect(a, BDW_EDP_LCPLL_CTL, AIR72_PLL) || !reopen_dark(a))
        return false;
    enum intel_gt_write result = a->io->write(a->io->context, reg, word);
    if (result != GT_WRITE_NONE) {
        ++a->r->writes;
        a->r->retained = true;
    }
    return result == GT_WRITE_DONE;
}

static bool reopen_foreign_requests_match(struct reopen_attempt *a, uint32_t bios_request)
{
    return reopen_expect(a, 0x45400, bios_request | BDW_EDP_WELL_ON) &&
           reopen_expect(a, 0x45408, BDW_EDP_WELL_ON) &&
           reopen_expect(a, 0x4540c, BDW_EDP_WELL_ON);
}

static bool reopen_well_ready(struct reopen_attempt *a, uint32_t bios_request)
{
    uint64_t start = a->last;
    for (;;) {
        uint32_t *driver = &a->r->driver_after;
        if (!reopen_read(a, BDW_EDP_WELL_DRIVER, driver) || a->last - start >= REOPEN_WELL_US)
            return false;
        if (*driver == WELL_OWNED)
            return reopen_foreign_requests_match(a, bios_request);
        if (*driver != BDW_EDP_WELL_REQUEST)
            return false;
        uint64_t before = a->last;
        a->io->delay(a->io->context, 10);
        if (!reopen_tick(a) || a->last - before < 10)
            return false;
    }
}

bool intel_bdw_edp_reopen_resources(const struct bdw_edp_dark_io *io, struct bdw_edp_reopen_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_reopen_report){0};
    if (!io || !io->held || !io->read || !io->write || !io->now || !io->delay || !io->held(io->context))
        return false;
    uint64_t now = io->now(io->context);
    if (!now)
        return false;
    struct reopen_attempt a = {.io = io, .r = r, .begin = now, .last = now};
    uint32_t bios;
    if (!reopen_read(&a, BDW_EDP_LCPLL_CTL, &r->pll) || r->pll != AIR72_PLL ||
        !reopen_expect(&a, 0x42014, AIR72_FUSE) || !reopen_dark(&a) ||
        !reopen_read(&a, 0x45400, &bios) || (bios != WELL_OWNED && bios != 0) ||
        !reopen_read(&a, BDW_EDP_WELL_DRIVER, &r->driver_before) ||
        r->driver_before != (bios & BDW_EDP_WELL_ON) ||
        !reopen_expect(&a, 0x45408, r->driver_before) ||
        !reopen_expect(&a, 0x4540c, r->driver_before))
        return false;
    r->phase = BDW_EDP_REOPEN_WELL;
    if (!reopen_write(&a, BDW_EDP_WELL_DRIVER, BDW_EDP_WELL_REQUEST) ||
        !reopen_well_ready(&a, bios & BDW_EDP_WELL_REQUEST))
        return false;
    r->phase = BDW_EDP_REOPEN_CLOCK;
    if (!reopen_expect(&a, BDW_EDP_LCPLL_CTL, AIR72_PLL) ||
        !reopen_read(&a, BDW_EDP_CDCLK_FREQ, &r->cdclk_before) ||
        (r->cdclk_before != AIR72_CDCLK &&
         (r->cdclk_before != CDCLK_DEFAULT || !reopen_write(&a, BDW_EDP_CDCLK_FREQ, AIR72_CDCLK))) ||
        !reopen_expect(&a, BDW_EDP_CDCLK_FREQ, AIR72_CDCLK))
        return false;
    r->phase = BDW_EDP_REOPEN_GATE;
    if (!reopen_read(&a, BDW_EDP_LP_CLOCK_GATE, &r->gate_before) ||
        (r->gate_before != AIR72_LP_GATE &&
         (r->gate_before != 0 || !reopen_write(&a, BDW_EDP_LP_CLOCK_GATE, AIR72_LP_GATE))) ||
        !reopen_expect(&a, BDW_EDP_LP_CLOCK_GATE, AIR72_LP_GATE) ||
        !reopen_expect(&a, BDW_EDP_LCPLL_CTL, AIR72_PLL) ||
        !reopen_expect(&a, BDW_EDP_WELL_DRIVER, WELL_OWNED) ||
        !reopen_foreign_requests_match(&a, bios & BDW_EDP_WELL_REQUEST) || !reopen_dark(&a))
        return false;
    r->phase = BDW_EDP_REOPEN_READY;
    r->retained = false;
    return true;
}
