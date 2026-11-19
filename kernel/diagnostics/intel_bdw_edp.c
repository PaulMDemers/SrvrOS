#include <srvros/intel_bdw_edp.h>

#define AUX_CTL 0x64010u
#define AUX_DATA 0x64014u
#define AUX_BUSY (1u << 31)
#define AUX_DONE (1u << 30)
#define AUX_INTERRUPT_DISABLE (1u << 29)
#define AUX_TIMEOUT_ERROR (1u << 28)
#define AUX_TIMEOUT_600 (1u << 26)
#define AUX_RECEIVE_ERROR (1u << 25)
#define AUX_PRECHARGE_6 (3u << 16)
#define AUX_EVENTS (AUX_DONE | AUX_TIMEOUT_ERROR | AUX_RECEIVE_ERROR)
#define AUX_TIMEOUT_US 10000u
#define AUX_DEFER_US 500u
#define AUX_ATTEMPTS 7u
#define LINK_TIMEOUT_US 150000u
#define PANEL_TIMEOUT_US 1600000u
#define PANEL_BACKLIGHT_DELAY_US 210000u

#define PP_STATUS 0xc7200u
#define PP_CONTROL 0xc7204u
#define PP_ON_DELAYS 0xc7208u
#define PP_OFF_DELAYS 0xc720cu
#define PP_DIVISOR 0xc7210u
#define PP_ON (1u << 31)
#define PP_SEQUENCING (3u << 28)
#define PP_POWERING_UP (1u << 28)
#define PP_CYCLING (1u << 27)
#define PP_STATE (PP_ON | PP_SEQUENCING | PP_CYCLING)
#define PP_TARGET_ON 1u
#define PP_POWER_DOWN_ON_RESET 2u
#define PP_BACKLIGHT 4u

#define PP_ON_TIMING ((2000u << 16) | 1u)
#define PP_OFF_TIMING ((740u << 16) | 1u)
#define PP_CYCLE_TIMING ((1199u << 8) | 6u)

#define DP_LINK_BW_SET 0x100u
#define DP_TRAINING_PATTERN_SET 0x102u
#define DP_TRAINING_LANE0_SET 0x103u
#define DP_DOWNSPREAD_CTRL 0x107u
#define DP_EDP_CONFIGURATION_SET 0x10au
#define DP_LANE0_1_STATUS 0x202u
#define DP_SET_POWER 0x600u
#define DP_LINK_BW_2_7 0x0au
#define DP_ONE_LANE_ENHANCED 0x81u

struct edp_attempt {
    struct bdw_edp_io io;
    struct bdw_edp_report *report;
    bool (*panel_held)(void *);
    uint64_t start, last, link_start, aux_start;
    uint32_t aux_ctl;
};

static bool fail(struct edp_attempt *a, enum bdw_edp_result result)
{
    a->report->result = result;
    return false;
}

static bool check(struct edp_attempt *a)
{
    if ((a->panel_held && !a->panel_held(a->io.context)) ||
        (a->link_start && !a->io.held(a->io.context)))
        return fail(a, BDW_EDP_OWNERSHIP);
    uint64_t now = a->io.now(a->io.context);
    if (!now || now < a->last ||
        (a->panel_held && now - a->start >= PANEL_TIMEOUT_US) ||
        (a->link_start && now - a->link_start >= LINK_TIMEOUT_US))
        return fail(a, BDW_EDP_CLOCK);
    if (a->aux_start && now - a->aux_start >= AUX_TIMEOUT_US)
        return fail(a, BDW_EDP_AUX_TIMEOUT);
    a->last = now;
    return true;
}

static bool delay(struct edp_attempt *a, uint32_t us)
{
    if (!check(a))
        return false;
    uint64_t start = a->last;
    a->io.delay(a->io.context, us);
    return check(a) && (a->last - start >= us || fail(a, BDW_EDP_CLOCK));
}

static bool read_reg(struct edp_attempt *a, uint32_t offset, uint32_t *value)
{
    if (!check(a))
        return false;
    if (!a->io.read(a->io.context, offset, value))
        return fail(a, BDW_EDP_IO);
    return check(a);
}

static bool wrote(struct edp_attempt *a, enum intel_gt_write result)
{
    a->report->effects_possible |= result != GT_WRITE_NONE;
    if (result != GT_WRITE_DONE)
        return fail(a, BDW_EDP_IO);
    return check(a);
}

static bool write_reg(struct edp_attempt *a, uint32_t offset, uint32_t value)
{
    return check(a) && wrote(a, a->io.write(a->io.context, offset, value));
}

static bool source(struct edp_attempt *a, enum bdw_edp_stage stage)
{
    a->report->stage = stage;
    return check(a) && wrote(a, a->io.source(a->io.context, stage,
                                           a->report->swing, a->report->emphasis));
}

static bool aux_message(struct edp_attempt *a, const uint8_t *tx, unsigned tx_size,
                        uint8_t *rx, unsigned *rx_size)
{
    uint32_t status;
    if (!read_reg(a, AUX_CTL, &status))
        return false;
    a->report->aux_status = status;
    if (status & AUX_BUSY)
        return fail(a, BDW_EDP_AUX_BUSY);

    for (unsigned i = 0; i < tx_size; i += 4) {
        uint32_t word = 0;
        for (unsigned j = 0; j < 4 && i + j < tx_size; ++j)
            word |= (uint32_t)tx[i + j] << (24 - 8 * j);
        if (!write_reg(a, AUX_DATA + i, word))
            return false;
    }

    uint32_t control = a->aux_ctl | (tx_size << 20);
    if (!write_reg(a, AUX_CTL, control | AUX_BUSY | AUX_EVENTS))
        return false;
    for (;;) {
        if (!read_reg(a, AUX_CTL, &status))
            return false;
        a->report->aux_status = status;
        if (!(status & AUX_BUSY))
            break;
        if (!delay(a, 10))
            return false;
    }

    *rx_size = (status >> 20) & 31u;
    bool received = (status & AUX_EVENTS) == AUX_DONE && *rx_size &&
                    *rx_size <= 20;
    if (received) {
        for (unsigned i = 0; i < *rx_size; i += 4) {
            uint32_t word;
            if (!read_reg(a, AUX_DATA + i, &word))
                return false;
            for (unsigned j = 0; j < 4 && i + j < *rx_size; ++j)
                rx[i + j] = (uint8_t)(word >> (24 - 8 * j));
        }
    }

    /* The RX data is valid while DONE is set. Ack only once BUSY has cleared. */
    uint32_t cleared;
    if (!write_reg(a, AUX_CTL, control | AUX_EVENTS) ||
        !read_reg(a, AUX_CTL, &cleared))
        return false;
    if (cleared & (AUX_BUSY | AUX_EVENTS))
        return fail(a, BDW_EDP_IO);
    if (status & AUX_TIMEOUT_ERROR)
        return fail(a, BDW_EDP_AUX_TIMEOUT);
    return received || fail(a, BDW_EDP_AUX_REPLY);
}

static bool dpcd(struct edp_attempt *a, uint32_t address,
                 uint8_t *read, const uint8_t *write, unsigned size)
{
    uint8_t tx[20], rx[20];
    tx[0] = (read ? 0x90u : 0x80u) | (uint8_t)(address >> 16);
    tx[1] = (uint8_t)(address >> 8);
    tx[2] = (uint8_t)address;
    tx[3] = (uint8_t)(size - 1);
    if (write)
        for (unsigned i = 0; i < size; ++i)
            tx[4 + i] = write[i];

    a->report->aux_address = address;
    if (!check(a))
        return false;
    a->aux_start = a->last;
    for (unsigned attempt = 0; attempt < AUX_ATTEMPTS; ++attempt) {
        unsigned received;
        ++a->report->aux_attempts;
        if (!aux_message(a, tx, read ? 4 : 4 + size, rx, &received))
            return false;
        /* DP NACK, short ACK and invalid replies terminate. Only an
         * unambiguous native DEFER permits replaying the same transaction. */
        unsigned reply = rx[0] >> 4;
        if (reply == 0 && received == (read ? 1 + size : 1)) {
            a->aux_start = 0;
            if (read)
                for (unsigned i = 0; i < size; ++i)
                    read[i] = rx[1 + i];
            return true;
        }
        if (reply != 2 || received != 1)
            return fail(a, BDW_EDP_AUX_REPLY);
        if (attempt + 1 < AUX_ATTEMPTS && !delay(a, AUX_DEFER_US))
            return false;
    }
    return fail(a, BDW_EDP_AUX_DEFER);
}

static uint8_t lane_settings(const struct bdw_edp_report *r)
{
    /* The Broadwell DP/eDP translation tables top out at swing level 2. */
    return (uint8_t)(r->swing | (r->swing == 2 ? 4u : 0u) |
                     (r->emphasis << 3) | (r->emphasis == 3 ? 32u : 0u));
}

static void adjust(struct bdw_edp_report *r)
{
    unsigned swing = r->status[4] & 3u;
    unsigned emphasis = (r->status[4] >> 2) & 3u;
    if (swing > 3 - emphasis)
        swing = 3 - emphasis;
    r->swing = swing > 2 ? 2 : swing;
    r->emphasis = emphasis;
}

static bool equalized(const uint8_t *status)
{
    return (status[0] & 7u) == 7u && (status[2] & 1u);
}

static bool sample_link(struct edp_attempt *a, uint32_t wait_us)
{
    return delay(a, wait_us) &&
           dpcd(a, DP_LANE0_1_STATUS, a->report->status, 0, 6);
}

static bool train(struct edp_attempt *a, enum bdw_edp_stage stage)
{
    struct bdw_edp_report *r = a->report;
    bool eq = stage == BDW_EDP_EQUALIZE;
    unsigned limit = eq ? 5 : 10;
    uint8_t pattern[2] = {eq ? 0x22 : 0x21, lane_settings(r)};
    if (!source(a, stage) || !dpcd(a, DP_TRAINING_PATTERN_SET, 0, pattern, 2))
        return false;

    unsigned same_voltage = 1;
    for (unsigned i = 0; i < limit; ++i) {
        if (!sample_link(a, eq ? 400 : 100))
            return false;
        bool recovered = r->status[0] & 1u;
        if (eq ? equalized(r->status) : recovered)
            return true;
        if ((eq && !recovered) || (!eq && (r->swing == 2 || same_voltage == 5)) ||
            i + 1 == limit)
            break;
        unsigned previous_swing = r->swing;
        adjust(r);
        same_voltage = r->swing == previous_swing ? same_voltage + 1 : 1;
        uint8_t lanes = lane_settings(r);
        if (!source(a, stage) || !dpcd(a, DP_TRAINING_LANE0_SET, 0, &lanes, 1))
            return false;
    }
    return fail(a, BDW_EDP_TRAINING);
}

static bool recover(struct edp_attempt *a)
{
    struct bdw_edp_report *r = a->report;
    if (!dpcd(a, 0, r->caps, 0, sizeof(r->caps)))
        return false;
    if (r->caps[0] != 0x11 || r->caps[1] != DP_LINK_BW_2_7 ||
        r->caps[2] != DP_ONE_LANE_ENHANCED || (r->caps[5] & 1u) ||
        !(r->caps[6] & 1u) || r->caps[14])
        return fail(a, BDW_EDP_CAPS);

    if (!source(a, BDW_EDP_PREPARE_SOURCE))
        return false;
    r->stage = BDW_EDP_POWER;
    const uint8_t d0 = 1;
    if (!dpcd(a, DP_SET_POWER, 0, &d0, 1) || !delay(a, 2000))
        return false;

    r->stage = BDW_EDP_CONFIGURE;
    const uint8_t mode[] = {0, 1}; /* fixed MSA timing, 8b/10b, no SSC */
    const uint8_t link[] = {DP_LINK_BW_2_7, DP_ONE_LANE_ENHANCED};
    const uint8_t normal = 0; /* standard scrambler and framing */
    if (!dpcd(a, DP_DOWNSPREAD_CTRL, 0, mode, sizeof(mode)) ||
        !dpcd(a, DP_EDP_CONFIGURATION_SET, 0, &normal, 1) ||
        !dpcd(a, DP_LINK_BW_SET, 0, link, sizeof(link)) ||
        !train(a, BDW_EDP_CLOCK_RECOVERY) || !train(a, BDW_EDP_EQUALIZE) ||
        !source(a, BDW_EDP_IDLE) || !source(a, BDW_EDP_SCANOUT) ||
        !dpcd(a, DP_TRAINING_PATTERN_SET, 0, &normal, 1) ||
        !source(a, BDW_EDP_NORMAL))
        return false;

    uint8_t configured[3];
    if (!dpcd(a, DP_LINK_BW_SET, configured, 0, sizeof(configured)) ||
        !sample_link(a, 400))
        return false;
    if (configured[0] != link[0] || configured[1] != link[1] ||
        configured[2] || !equalized(r->status))
        return fail(a, BDW_EDP_TRAINING);
    r->stage = BDW_EDP_LINK_READY;
    return true;
}

static bool begin(struct edp_attempt *a, const struct bdw_edp_io *io,
                  uint32_t cdclk_khz, struct bdw_edp_report *report,
                  bool (*panel_held)(void *))
{
    *report = (struct bdw_edp_report){.result = BDW_EDP_INVALID};
    if (!io || !io->held || !io->read || !io->write || !io->now ||
        !io->delay || !io->source ||
        (cdclk_khz != 337500 && cdclk_khz != 450000 &&
         cdclk_khz != 540000 && cdclk_khz != 675000))
        return false;

    *a = (struct edp_attempt){.io = *io, .report = report, .panel_held = panel_held,
        .aux_ctl = AUX_INTERRUPT_DISABLE | AUX_TIMEOUT_600 | AUX_PRECHARGE_6 |
                   ((cdclk_khz + 1000) / 2000)};
    bool (*held)(void *) = panel_held ? panel_held : a->io.held;
    if (!held(a->io.context))
        return fail(a, BDW_EDP_OWNERSHIP);
    a->start = a->last = a->io.now(a->io.context);
    if (!a->start)
        return fail(a, BDW_EDP_CLOCK);
    report->result = BDW_EDP_OK;
    return true;
}

enum bdw_edp_result intel_bdw_edp_recover(const struct bdw_edp_io *io,
                                         uint32_t cdclk_khz,
                                         struct bdw_edp_report *report)
{
    if (!report)
        return BDW_EDP_INVALID;
    struct edp_attempt a;
    if (begin(&a, io, cdclk_khz, report, 0)) {
        a.link_start = a.start;
        recover(&a);
    }
    return report->result;
}

static bool panel_status(struct edp_attempt *a, uint32_t pending, uint32_t ready)
{
    for (;;) {
        if (!read_reg(a, PP_STATUS, &a->report->panel_status))
            return false;
        uint32_t status = a->report->panel_status & PP_STATE;
        if (status == ready)
            return true;
        /* Panel power can become true before the power-up sequence is idle. */
        if (status != pending && !(pending == PP_POWERING_UP && status == (PP_ON | pending)))
            return fail(a, BDW_EDP_PANEL_STATE);
        if (!delay(a, 1000))
            return false;
    }
}

static bool panel_write(struct edp_attempt *a, uint32_t offset, uint32_t value)
{
    uint32_t posted;
    if (!write_reg(a, offset, value) || !read_reg(a, offset, &posted))
        return false;
    if (offset == PP_CONTROL)
        a->report->panel_control = posted;
    return posted == value || fail(a, BDW_EDP_PANEL_STATE);
}

static bool panel_power(struct edp_attempt *a)
{
    struct bdw_edp_report *r = a->report;
    if (!read_reg(a, PP_CONTROL, &r->panel_control))
        return false;
    if (r->panel_control)
        return fail(a, BDW_EDP_PANEL_STATE);
    if (!panel_status(a, PP_CYCLING, 0))
        return false;

    /* PPS tick = 100 us from the 24 MHz PCH reference. Program the 500 ms
     * minimum cycle (+1 encoding). Use software backlight timers, including
     * the enclosing owner's 250 ms T9 before any later power-off. */
    r->panel_stage = BDW_EDP_PANEL_DELAYS;
    if (!panel_write(a, PP_ON_DELAYS, PP_ON_TIMING) ||
        !panel_write(a, PP_OFF_DELAYS, PP_OFF_TIMING) ||
        !panel_write(a, PP_DIVISOR, PP_CYCLE_TIMING))
        return false;

    r->panel_stage = BDW_EDP_PANEL_POWERING;
    return panel_write(a, PP_CONTROL, PP_TARGET_ON | PP_POWER_DOWN_ON_RESET) &&
           delay(a, 200000) && panel_status(a, PP_POWERING_UP, PP_ON);
}

static bool scanout_live(struct edp_attempt *a, bool (*live)(void *))
{
    if (!check(a))
        return false;
    if (!live(a->io.context))
        return fail(a, BDW_EDP_SCANOUT_FAILED);
    return check(a);
}

enum bdw_edp_result intel_bdw_edp_panel_restore(const struct bdw_edp_panel_io *io,
                                               uint32_t cdclk_khz,
                                               struct bdw_edp_report *report)
{
    if (!report)
        return BDW_EDP_INVALID;
    if (!io || !io->panel_held || !io->scanout_live) {
        *report = (struct bdw_edp_report){.result = BDW_EDP_INVALID};
        return report->result;
    }

    bool (*live)(void *) = io->scanout_live;
    struct edp_attempt a;
    if (!begin(&a, &io->link, cdclk_khz, report, io->panel_held) || !panel_power(&a))
        return report->result;
    report->panel_stage = BDW_EDP_PANEL_TRAINING;
    a.link_start = a.last;
    if (!recover(&a))
        return report->result;
    a.link_start = 0;

    report->panel_stage = BDW_EDP_PANEL_CHECK_SCANOUT;
    if (!scanout_live(&a, live) || !delay(&a, PANEL_BACKLIGHT_DELAY_US) ||
        !scanout_live(&a, live) || !read_reg(&a, PP_STATUS, &report->panel_status) ||
        !read_reg(&a, PP_CONTROL, &report->panel_control))
        return report->result;
    if ((report->panel_status & PP_STATE) != PP_ON ||
        report->panel_control != (PP_TARGET_ON | PP_POWER_DOWN_ON_RESET)) {
        report->result = BDW_EDP_PANEL_STATE;
        return report->result;
    }

    report->panel_stage = BDW_EDP_PANEL_LIGHTING;
    if (panel_write(&a, PP_CONTROL, PP_TARGET_ON | PP_POWER_DOWN_ON_RESET | PP_BACKLIGHT))
        report->panel_stage = BDW_EDP_PANEL_READY;
    return report->result;
}
