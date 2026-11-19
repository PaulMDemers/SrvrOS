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
