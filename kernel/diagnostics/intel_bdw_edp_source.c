#include <srvros/intel_bdw_edp_source.h>
#include <srvros/intel_bdw_edp_baseline.h>

#define LCPLL_CTL 0x130040u
#define LCPLL_LOCK (1u << 30)
#define LCPLL_FREQ_MASK (3u << 26)
/* Disabled/unavailable clocks, alternate reference, power-down and FCLK. */
#define LCPLL_BAD (0x80000000u | (3u << 28) | (15u << 22) | (1u << 21) | (1u << 19))
#define FUSE_STRAP 0x42014u
#define FUSE_CDCLK_LIMIT (1u << 24)
#define CDCLK_FREQ 0x46200u
#define PORT_CLOCK_A 0x46100u
#define PORT_CLOCK_NONE 0xe0000000u
#define PORT_CLOCK_LCPLL_1350 0x20000000u

#define DDI_BUF_A 0x64000u
#define DDI_BUF_ENABLE (1u << 31)
#define DDI_BUF_IDLE (1u << 7)
#define DDI_LANE_CONFIG ((1u << 16) | (1u << 4))
#define DDI_BUF_SELECT (0xfu << 24)
#define DDI_BUF_WIDTH (7u << 1)
#define DDI_PHY_A 0x64e00u
#define DP_TP_A 0x64040u
#define DP_TP_SST ((1u << 31) | (1u << 18))
#define DP_TP_TPS1 0u
#define DP_TP_TPS2 (1u << 8)
#define DP_TP_IDLE (2u << 8)
#define DP_TP_NORMAL (3u << 8)
#define DP_TP_ENABLE (1u << 31)

#define EDP_DDI 0x6f400u
#define EDP_CONF 0x7f008u
#define TRANS_ENABLE (1u << 31)
#define TRANS_ACTIVE (1u << 30)
#define EDP_DDI_SST_6BPC_A 0x82200000u
#define PIPE_A_FRAME 0x70040u
#define PRI_A_CTL 0x70180u
#define PRI_A_STRIDE 0x70188u
#define PRI_A_SURF 0x7019cu
#define PRI_A_OFFSET 0x701a4u
#define PRI_A_SURFLIVE 0x701acu
#define PRI_A_XRGB_TILED 0x98000400u
#define PRI_STRIDE_BYTES 6144u
/* 900 rows rounded to an X-tile boundary, then the 128-PTE fetch tail. */
#define PRI_MAPPED_BYTES (904ull * PRI_STRIDE_BYTES + 128ull * 4096)

#define PIPE_A_IMR 0x44404u
#define PIPE_A_IIR 0x44408u
#define PIPE_A_IER 0x4440cu
#define FIFO_UNDERRUN (1u << 31)
#define PP_STATUS 0xc7200u
#define PP_CONTROL 0xc7204u
#define PP_POWER_STABLE_MASK 0xb8000000u
#define PP_POWER_STABLE_ON 0x80000000u
#define PP_BACKLIGHT (1u << 2)
#define PP_OFF_PENDING 0xb0000000u /* Panel on or power sequence in progress. */
#define PRI_GAMMA_ENABLE (1u << 30)
#define PRI_ENABLE (1u << 31)

#define SOURCE_STAGE_US 25000u
#define FRAME_CHECK_US 60000u
#define DISABLE_LIMIT_US 800000u
#define DISABLE_TWO_FRAMES_US 34000u
#define BACKLIGHT_OFF_US 250000u
/* The inherited PPS times its own T9 (250 ms) and T10 (74 ms) on
 * shutdown. T12 can remain active after the rail is off and is waited
 * separately by panel_power before the original path can be powered up. */
#define PANEL_OFF_US 350000u
#define MODE_PAIR(high, low) ((((high) - 1u) << 16) | ((low) - 1u))
#define PIXEL_KHZ 88890ull
#define LINK_SYMBOL_KHZ 270000ull
#define DATA_N 0x800000u
#define LINK_N 0x80000u
#define DATA_M ((uint32_t)(PIXEL_KHZ * 18 * DATA_N / (LINK_SYMBOL_KHZ * 8)))
#define LINK_M ((uint32_t)(PIXEL_KHZ * LINK_N / LINK_SYMBOL_KHZ))

struct source_attempt {
    struct bdw_edp_source_io io;
    struct bdw_edp_source_image image;
    struct bdw_edp_source_report *report;
    enum bdw_edp_stage stage;
    uint64_t last, phase_start;
    uint32_t budget;
    bool require_panel, keep_timings;
    uint32_t gamma;
};

static bool reject(struct source_attempt *a, enum bdw_edp_source_error error)
{
    if (!a->report->source_error)
        a->report->source_error = error;
    return false;
}

static bool held(void *context)
{
    struct source_attempt *a = context;
    return a->io.held(a->io.context, &a->image) ||
           reject(a, BDW_EDP_SOURCE_OWNER);
}

static bool link_held(void *context)
{
    struct source_attempt *a = context;
    uint32_t status;
    if (!held(a))
        return false;
    bool ready = a->io.read(a->io.context, PP_STATUS, &status) &&
                 (status & PP_POWER_STABLE_MASK) == PP_POWER_STABLE_ON;
    return held(a) && (ready || reject(a, BDW_EDP_SOURCE_PANEL));
}

static bool check(struct source_attempt *a)
{
    if (!(a->require_panel ? link_held(a) : held(a)))
        return false;
    uint64_t now = a->io.now(a->io.context);
    if (!now || now < a->last)
        return reject(a, BDW_EDP_SOURCE_CLOCK);
    a->last = now;
    return (!a->budget || now - a->phase_start < a->budget) ||
           reject(a, BDW_EDP_SOURCE_CLOCK);
}

static bool start_phase(struct source_attempt *a, uint32_t budget)
{
    a->budget = 0;
    if (!check(a))
        return false;
    a->phase_start = a->last;
    a->budget = budget;
    return true;
}

static uint64_t clock_now(void *context)
{
    struct source_attempt *a = context;
    return a->io.now(a->io.context);
}

static void delay(void *context, uint32_t us)
{
    struct source_attempt *a = context;
    a->io.delay(a->io.context, us);
}

static bool wait(struct source_attempt *a, uint32_t us)
{
    if (!check(a))
        return false;
    uint64_t since = a->last;
    delay(a, us);
    return check(a) && (a->last - since >= us || reject(a, BDW_EDP_SOURCE_CLOCK));
}

static bool raw_read(void *context, uint32_t offset, uint32_t *value)
{
    struct source_attempt *a = context;
    return a->io.read(a->io.context, offset, value) &&
           (offset == PIPE_A_FRAME || *value != UINT32_MAX);
}

static enum intel_gt_write raw_write(void *context, uint32_t offset, uint32_t value)
{
    struct source_attempt *a = context;
    return a->io.write(a->io.context, offset, value);
}

static bool load(struct source_attempt *a, uint32_t offset, uint32_t *value)
{
    a->report->offset = offset;
    if (!check(a))
        return false;
    if (!raw_read(a, offset, value))
        return reject(a, BDW_EDP_SOURCE_REGISTER);
    a->report->word = *value;
    return check(a);
}

static bool expect(struct source_attempt *a, uint32_t offset, uint32_t mask,
                   uint32_t value, enum bdw_edp_source_error error)
{
    uint32_t word;
    return load(a, offset, &word) && ((word & mask) == value || reject(a, error));
}

static bool store(struct source_attempt *a, uint32_t offset, uint32_t value,
                  uint32_t readback_mask)
{
    a->report->offset = offset;
    a->report->word = value;
    if (!check(a))
        return false;
    enum intel_gt_write status = raw_write(a, offset, value);
    if (status != GT_WRITE_NONE)
        ++a->report->source_writes;
    if (status != GT_WRITE_DONE)
        return reject(a, BDW_EDP_SOURCE_REGISTER);
    if (!check(a))
        return false;
    return expect(a, offset, readback_mask, value & readback_mask, BDW_EDP_SOURCE_REGISTER);
}

static bool valid_image(const struct bdw_edp_source_image *image)
{
    if (!image || !image->backing || !image->epoch || !image->backing_hash ||
        !image->reference_crc || image->reference_crc == UINT32_MAX ||
        image->mapped_bytes < PRI_MAPPED_BYTES || (image->mapped_bytes & 4095u) ||
        image->mapped_bytes > 256ull * 1024 * 1024 ||
        (image->ddi_lane_bits & ~DDI_LANE_CONFIG) ||
        (image->cdclk_khz != 337500 && image->cdclk_khz != 450000 &&
         image->cdclk_khz != 540000 && image->cdclk_khz != 675000))
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        if (!(image->phy[i][0] & 0xffffffu) || !(image->phy[i][1] & 31u) ||
            (image->phy[i][0] & ~0x80ffffffu) || (image->phy[i][1] & ~0x001f001fu))
            return false;
    return true;
}

static bool reference_matches(const struct bdw_edp_source_image *image,
                              const struct bdw_edp_baseline_report *ref)
{
    if (!ref || ref->status != BDW_EDP_BASELINE_OK || ref->reason || ref->retained ||
        !ref->restored || ref->events != BDW_EDP_CRC_FRAMES || ref->writes < 8 ||
        (ref->poll_iir & (BDW_EDP_CRC_IRQS | FIFO_UNDERRUN)) != BDW_EDP_CRC_DONE ||
        ref->sample.port != PORT_CLOCK_LCPLL_1350 ||
        (ref->sample.buffer & (DDI_BUF_ENABLE | DDI_BUF_IDLE | DDI_LANE_CONFIG | DDI_BUF_WIDTH)) !=
        (DDI_BUF_ENABLE | image->ddi_lane_bits) ||
        ref->sample.backing_hash != image->backing_hash || ref->crc[2] != image->reference_crc ||
        ref->crc[3] != image->reference_crc)
        return false;
    for (unsigned i = 1; i < BDW_EDP_CRC_FRAMES; ++i) {
        uint32_t gap = ref->frame[i] - ref->frame[i - 1];
        if (!gap || gap > 2)
            return false;
    }
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i) {
        if (image->phy[i][0] != ref->sample.phy[i][0] || image->phy[i][1] != ref->sample.phy[i][1])
            return false;
    }
    return true;
}

static bool clock_ready(struct source_attempt *a)
{
    const uint32_t rate[] = {450000, 540000, 337500, 675000};
    uint32_t pll, fuse;
    if (!load(a, LCPLL_CTL, &pll) || !load(a, FUSE_STRAP, &fuse))
        return false;
    uint32_t effective = fuse & FUSE_CDCLK_LIMIT ? 450000 : rate[(pll & LCPLL_FREQ_MASK) >> 26];
    return (((pll & (LCPLL_LOCK | LCPLL_BAD)) == LCPLL_LOCK &&
             effective == a->image.cdclk_khz) || reject(a, BDW_EDP_SOURCE_CLOCK)) &&
           expect(a, CDCLK_FREQ, UINT32_MAX, (effective + 500) / 1000 - 1, BDW_EDP_SOURCE_CLOCK);
}

static bool underrun_clear(struct source_attempt *a)
{
    return expect(a, PIPE_A_IMR, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN) &&
           expect(a, PIPE_A_IER, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN) &&
           expect(a, PIPE_A_IIR, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN);
}

static bool reset_state(struct source_attempt *a)
{
    const uint32_t disabled = DDI_BUF_ENABLE | DDI_BUF_IDLE | DDI_LANE_CONFIG;
    return clock_ready(a) && underrun_clear(a) &&
           expect(a, PORT_CLOCK_A, UINT32_MAX, PORT_CLOCK_NONE, BDW_EDP_SOURCE_RESET_STATE) &&
           expect(a, DDI_BUF_A, disabled, DDI_BUF_IDLE | a->image.ddi_lane_bits, BDW_EDP_SOURCE_RESET_STATE) &&
           expect(a, DP_TP_A, DP_TP_ENABLE, 0, BDW_EDP_SOURCE_RESET_STATE) &&
           expect(a, EDP_DDI, TRANS_ENABLE, 0, BDW_EDP_SOURCE_RESET_STATE) &&
           expect(a, EDP_CONF, TRANS_ENABLE | TRANS_ACTIVE, 0, BDW_EDP_SOURCE_RESET_STATE) &&
           expect(a, PRI_A_CTL, TRANS_ENABLE, 0, BDW_EDP_SOURCE_RESET_STATE);
}

static bool prepare(struct source_attempt *a)
{
    if (!reset_state(a) || !store(a, PORT_CLOCK_A, PORT_CLOCK_LCPLL_1350, UINT32_MAX))
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        for (unsigned word = 0; word < 2; ++word)
            if (!store(a, DDI_PHY_A + 8 * i + 4 * word, a->image.phy[i][word], UINT32_MAX))
                return false;
    return true;
}

static bool training(struct source_attempt *a, enum bdw_edp_stage stage,
                     unsigned swing, unsigned emphasis)
{
    if (swing > 2 || emphasis > 3 - swing)
        return reject(a, BDW_EDP_SOURCE_ORDER);
    unsigned index = (swing == 0 ? 0 : swing == 1 ? 4 : 7) + emphasis;
    uint32_t pattern = stage == BDW_EDP_CLOCK_RECOVERY ? DP_TP_TPS1 : DP_TP_TPS2;
    if (!store(a, DP_TP_A, DP_TP_SST | pattern, UINT32_MAX))
        return false;
    uint32_t buffer = DDI_BUF_ENABLE | a->image.ddi_lane_bits | (index << 24);
    const uint32_t config = DDI_BUF_ENABLE | DDI_BUF_SELECT | DDI_LANE_CONFIG | DDI_BUF_WIDTH;
    if (!store(a, DDI_BUF_A, buffer, config))
        return false;
    if (a->stage == BDW_EDP_PREPARE_SOURCE && !wait(a, 520))
        return false;
    return expect(a, DDI_BUF_A, config | DDI_BUF_IDLE, buffer, BDW_EDP_SOURCE_REGISTER);
}

static bool configure_pipe(struct source_attempt *a)
{
    static const struct {
        uint32_t offset, value;
    } mode[] = {
        {0x6001c, MODE_PAIR(1440, 900)},     /* PIPE A source, width then height */
        {0x70030, 0x50},                   /* 6 bpc, spatial dither */
        {0x6f000, MODE_PAIR(1600, 1440)},   /* HTOTAL */
        {0x6f004, MODE_PAIR(1600, 1440)},   /* HBLANK */
        {0x6f008, MODE_PAIR(1520, 1488)},   /* HSYNC */
        {0x6f00c, MODE_PAIR(926, 900)},     /* VTOTAL */
        {0x6f010, MODE_PAIR(926, 900)},     /* VBLANK */
        {0x6f014, MODE_PAIR(909, 903)},     /* VSYNC */
        {0x6f030, (63u << 25) | DATA_M},   /* 64-symbol TU and data M1 */
        {0x6f034, DATA_N},
        {0x6f040, LINK_M},
        {0x6f044, LINK_N},
        {0x6f410, 1},                     /* MSA synchronous, full-range 6-bpc RGB */
        {0x45100, 14u << 16},             /* 2 us WM0, primary only */
        {0x45270, 144},                   /* 18 us line, IPS disabled */
    };
    if (!expect(a, DP_TP_A, UINT32_MAX, DP_TP_SST | DP_TP_IDLE, BDW_EDP_SOURCE_ORDER))
        return false;
    for (unsigned i = 0; i < sizeof(mode) / sizeof(mode[0]); ++i)
        if (!store(a, mode[i].offset, mode[i].value, UINT32_MAX))
            return false;
    /* Enable the eDP transcoder, then arm plane A on the original mapping.
     * The gamma LUT survives an isolated cycle and is bypassed after reset. */
    return store(a, EDP_DDI, EDP_DDI_SST_6BPC_A, UINT32_MAX) &&
           store(a, EDP_CONF, TRANS_ENABLE, ~TRANS_ACTIVE) &&
           store(a, PRI_A_STRIDE, PRI_STRIDE_BYTES, UINT32_MAX) &&
           store(a, PRI_A_OFFSET, 0, UINT32_MAX) &&
           store(a, PRI_A_CTL, PRI_A_XRGB_TILED | a->gamma, UINT32_MAX) &&
           store(a, PRI_A_SURF, 0, UINT32_MAX);
}

static bool apply_stage(struct source_attempt *a, enum bdw_edp_stage stage,
                        unsigned swing, unsigned emphasis)
{
    switch (stage) {
    case BDW_EDP_PREPARE_SOURCE:
        return a->stage == BDW_EDP_PROBE && prepare(a);
    case BDW_EDP_CLOCK_RECOVERY:
        return (a->stage == BDW_EDP_PREPARE_SOURCE || a->stage == stage) &&
               training(a, stage, swing, emphasis);
    case BDW_EDP_EQUALIZE:
        return (a->stage == BDW_EDP_CLOCK_RECOVERY || a->stage == stage) &&
               training(a, stage, swing, emphasis);
    case BDW_EDP_IDLE:
        return a->stage == BDW_EDP_EQUALIZE &&
               store(a, DP_TP_A, DP_TP_SST | DP_TP_IDLE, UINT32_MAX);
    case BDW_EDP_SCANOUT:
        return a->stage == BDW_EDP_IDLE && configure_pipe(a);
    case BDW_EDP_NORMAL:
        return a->stage == BDW_EDP_SCANOUT &&
               store(a, DP_TP_A, DP_TP_SST | DP_TP_NORMAL, UINT32_MAX);
    default:
        return false;
    }
}

static enum intel_gt_write set_source(void *context, enum bdw_edp_stage stage,
                                     unsigned swing, unsigned emphasis)
{
    struct source_attempt *a = context;
    unsigned writes = a->report->source_writes;
    a->require_panel = true;
    if (start_phase(a, SOURCE_STAGE_US) && apply_stage(a, stage, swing, emphasis)) {
        a->stage = stage;
        a->budget = 0;
        return GT_WRITE_DONE;
    }
    reject(a, BDW_EDP_SOURCE_ORDER);
    return writes != a->report->source_writes ? GT_WRITE_UNCERTAIN : GT_WRITE_NONE;
}

static bool scanout_matches(struct source_attempt *a)
{
    return clock_ready(a) &&
           expect(a, PORT_CLOCK_A, UINT32_MAX, PORT_CLOCK_LCPLL_1350, BDW_EDP_SOURCE_FRAME) &&
           expect(a, DDI_BUF_A, DDI_BUF_ENABLE | DDI_BUF_IDLE | DDI_LANE_CONFIG | DDI_BUF_WIDTH,
                  DDI_BUF_ENABLE | a->image.ddi_lane_bits, BDW_EDP_SOURCE_FRAME) &&
           expect(a, DP_TP_A, UINT32_MAX, DP_TP_SST | DP_TP_NORMAL, BDW_EDP_SOURCE_FRAME) &&
           expect(a, EDP_DDI, UINT32_MAX, EDP_DDI_SST_6BPC_A, BDW_EDP_SOURCE_FRAME) &&
           expect(a, EDP_CONF, TRANS_ENABLE | TRANS_ACTIVE, TRANS_ENABLE | TRANS_ACTIVE, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_CTL, UINT32_MAX, PRI_A_XRGB_TILED | a->gamma, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_STRIDE, UINT32_MAX, PRI_STRIDE_BYTES, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_OFFSET, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_SURF, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_SURFLIVE, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           underrun_clear(a);
}

static bool crc_snapshot(void *context, struct bdw_edp_baseline_sample *sample)
{
    struct source_attempt *a = context;
    if (!scanout_matches(a) ||
        !load(a, PORT_CLOCK_A, &sample->port) ||
        !load(a, DDI_BUF_A, &sample->buffer) ||
        !load(a, DP_TP_A, &sample->transport) ||
        !load(a, LCPLL_CTL, &sample->lcpll) ||
        !load(a, CDCLK_FREQ, &sample->cdclk))
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i) {
        for (unsigned j = 0; j < 2; ++j) {
            if (!load(a, DDI_PHY_A + 8 * i + 4 * j, &sample->phy[i][j]) ||
                sample->phy[i][j] != a->image.phy[i][j])
                return false;
        }
    }
    return a->io.backing_hash(a->io.context, &a->image, &sample->backing_hash) &&
           sample->backing_hash == a->image.backing_hash;
}

static bool read_live(void *context)
{
    struct source_attempt *a = context;
    if (a->stage != BDW_EDP_NORMAL)
        return reject(a, BDW_EDP_SOURCE_ORDER);
    uint32_t frame;
    if (!start_phase(a, FRAME_CHECK_US) || !load(a, PIPE_A_FRAME, &frame))
        return false;
    a->report->frame_first = frame;
    do {
        if (!wait(a, 1000) || !underrun_clear(a) || !load(a, PIPE_A_FRAME, &frame))
            return false;
    } while ((uint32_t)(frame - a->report->frame_first) < 2);
    a->report->frame_last = frame;
    if ((uint32_t)(frame - a->report->frame_first) > 4)
        return reject(a, BDW_EDP_SOURCE_FRAME);
    if (!scanout_matches(a) || a->report->crc_checks >= 2)
        return false;

    /* The fresh-frame wait above keeps its 60 ms bound. CRC runs under its
     * own capture and retirement bounds and the enclosing PPS deadline. */
    a->budget = 0;
    const struct bdw_edp_baseline_io io = {
        a, link_held, crc_snapshot, raw_read, raw_write, clock_now, delay,
    };
    struct bdw_edp_baseline_report crc;
    bool captured = intel_bdw_edp_baseline(&io, &crc);
    a->report->crc_retained |= crc.retained;
    a->report->captured_crc[a->report->crc_checks++] = crc.crc[2];
    if (!captured || crc.crc[2] != a->image.reference_crc)
        return reject(a, BDW_EDP_SOURCE_FRAME);
    if (!scanout_matches(a))
        return false;
    a->report->verified_frames += (uint32_t)(frame - a->report->frame_first);
    a->budget = 0;
    return true;
}

static enum bdw_edp_result restore(const struct bdw_edp_source_io *io,
                                                const struct bdw_edp_source_image *image,
                                                const struct bdw_edp_baseline_report *reference,
                                                struct bdw_edp_source_report *report,
                                                bool keep_timings, uint32_t gamma)
{
    if (!report)
        return BDW_EDP_INVALID;
    *report = (struct bdw_edp_source_report){.panel.result = BDW_EDP_INVALID};
    if (!image || !io || !io->held || !io->read || !io->write ||
        !io->now || !io->delay || !io->backing_hash) {
        report->source_error = BDW_EDP_SOURCE_IMAGE;
        return report->panel.result;
    }

    struct source_attempt a = {.io = *io, .image = *image, .report = report, .gamma = gamma};
    if (!valid_image(&a.image) || !reference_matches(&a.image, reference)) {
        report->source_error = BDW_EDP_SOURCE_IMAGE;
        return report->panel.result;
    }
    if (!start_phase(&a, SOURCE_STAGE_US) || !reset_state(&a)) {
        report->panel.result = report->source_error == BDW_EDP_SOURCE_OWNER
            ? BDW_EDP_OWNERSHIP : BDW_EDP_IO;
        return report->panel.result;
    }
    a.budget = 0;
    const struct bdw_edp_panel_io panel = {
        .link = {&a, link_held, raw_read, raw_write, clock_now, delay, set_source},
        .panel_held = held,
        .scanout_live = read_live,
        .keep_timings = keep_timings,
    };
    intel_bdw_edp_panel_restore(&panel, a.image.cdclk_khz, &report->panel);
    if (report->source_error == BDW_EDP_SOURCE_OWNER)
        report->panel.result = BDW_EDP_OWNERSHIP;
    return report->panel.result;
}

enum bdw_edp_result intel_bdw_edp_source_restore(const struct bdw_edp_source_io *io,
                                                const struct bdw_edp_source_image *image,
                                                const struct bdw_edp_baseline_report *reference,
                                                struct bdw_edp_source_report *report)
{
    return restore(io, image, reference, report, false, 0);
}

static bool backing_preserved(struct source_attempt *a)
{
    uint64_t hash;
    return check(a) && a->io.backing_hash(a->io.context, &a->image, &hash) && check(a) &&
           (hash == a->image.backing_hash || reject(a, BDW_EDP_SOURCE_IMAGE));
}

static bool poll_value(struct source_attempt *a, uint32_t offset,
                       uint32_t mask, uint32_t expected, uint32_t budget,
                       struct bdw_edp_panel_off_report *off)
{
    if (off)
        off->outcome = BDW_EDP_PANEL_OFF_INTERRUPTED;
    if (!check(a))
        return false;
    uint64_t begin = a->last;
    for (;;) {
        uint32_t word;
        bool read = load(a, offset, &word);
        uint64_t elapsed = a->last - begin;
        if (off) {
            off->elapsed_us = elapsed;
            if (read) {
                if (!off->samples)
                    off->first = word;
                ++off->samples;
                off->last = word;
            }
        }
        if (!read)
            return false;
        if (elapsed >= budget) {
            if (off)
                off->outcome = BDW_EDP_PANEL_OFF_TIMEOUT;
            return reject(a, BDW_EDP_SOURCE_CLOCK);
        }
        if ((word & mask) == expected) {
            if (off)
                off->outcome = BDW_EDP_PANEL_OFF_COMPLETE;
            return true;
        }
        if (!wait(a, 100)) {
            if (off)
                off->elapsed_us = a->last - begin;
            return false;
        }
    }
}

static bool primary_off(struct source_attempt *a, uint32_t ctl)
{
    uint32_t initial, frame;
    if (!store(a, PRI_A_CTL, ctl & ~PRI_ENABLE, 0) ||
        !store(a, PRI_A_SURF, 0, UINT32_MAX) ||
        !load(a, PIPE_A_FRAME, &initial))
        return false;
    uint64_t begin = a->last;
    do {
        if (!wait(a, 100) || !load(a, PIPE_A_FRAME, &frame))
            return false;
        if (a->last - begin >= DISABLE_TWO_FRAMES_US)
            return reject(a, BDW_EDP_SOURCE_CLOCK);
    } while ((uint32_t)(frame - initial) < 2);
    return ((uint32_t)(frame - initial) <= 3 || reject(a, BDW_EDP_SOURCE_FRAME)) &&
           expect(a, PRI_A_CTL, UINT32_MAX, ctl & ~PRI_ENABLE, BDW_EDP_SOURCE_FRAME);
}

static bool active_path(struct source_attempt *a, const struct bdw_edp_baseline_report *reference,
                        uint32_t *primary, uint32_t *buffer, uint32_t *transport)
{
    if (!start_phase(a, DISABLE_LIMIT_US) || !clock_ready(a) ||
        !backing_preserved(a) ||
        !expect(a, PORT_CLOCK_A, UINT32_MAX, PORT_CLOCK_LCPLL_1350, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, EDP_DDI, UINT32_MAX, EDP_DDI_SST_6BPC_A, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, EDP_CONF, UINT32_MAX, TRANS_ENABLE | TRANS_ACTIVE, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, PRI_A_STRIDE, UINT32_MAX, PRI_STRIDE_BYTES, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, PRI_A_OFFSET, UINT32_MAX, 0, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, PRI_A_SURF, UINT32_MAX, 0, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, PRI_A_SURFLIVE, UINT32_MAX, 0, BDW_EDP_SOURCE_RESET_STATE) ||
        !expect(a, PP_STATUS, PP_POWER_STABLE_MASK, PP_POWER_STABLE_ON, BDW_EDP_SOURCE_PANEL) ||
        !expect(a, PP_CONTROL, UINT32_MAX, 7, BDW_EDP_SOURCE_PANEL) ||
        !load(a, PRI_A_CTL, primary) || !load(a, DDI_BUF_A, buffer) || !load(a, DP_TP_A, transport))
        return false;
    return ((*primary & ~PRI_GAMMA_ENABLE) == PRI_A_XRGB_TILED &&
            *buffer == reference->sample.buffer && *transport == reference->sample.transport &&
            (*transport & ~0x40u) == (DP_TP_SST | DP_TP_NORMAL)) ||
           reject(a, BDW_EDP_SOURCE_RESET_STATE);
}

/* An all-masked IMR is valid. While we poll, IER stays disabled and IIR
 * underrun is never acknowledged. Distinguish a possibly issued write
 * from a refused write and require exact posted readback. */
static bool fifo_mask(struct source_attempt *a, struct bdw_edp_cycle_report *r, bool closing)
{
    uint32_t before;
    if (!check(a) || !a->io.read(a->io.context, PIPE_A_IMR, &before) || !check(a) ||
        !expect(a, PIPE_A_IER, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN) ||
        !expect(a, PIPE_A_IIR, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN))
        return reject(a, BDW_EDP_SOURCE_UNDERRUN);
    if (!closing)
        r->mask_before = before;
    else if (before != r->mask_live && before != r->mask_before)
        return reject(a, BDW_EDP_SOURCE_UNDERRUN);

    uint32_t target = closing ? r->mask_before : before & ~FIFO_UNDERRUN;
    if (!closing)
        r->mask_live = target;
    if (target != before) {
        if (!check(a))
            return false;
        enum intel_gt_write status = raw_write(a, PIPE_A_IMR, target);
        if (status != GT_WRITE_NONE)
            ++r->mask_writes;
        if (status != GT_WRITE_DONE)
            return reject(a, BDW_EDP_SOURCE_UNDERRUN);
    }
    uint32_t observed;
    if (!check(a) || !a->io.read(a->io.context, PIPE_A_IMR, &observed) ||
        observed != target || !check(a) ||
        !expect(a, PIPE_A_IER, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN) ||
        !expect(a, PIPE_A_IIR, FIFO_UNDERRUN, 0, BDW_EDP_SOURCE_UNDERRUN))
        return reject(a, BDW_EDP_SOURCE_UNDERRUN);
    if (closing)
        r->mask_after = observed;
    return true;
}

static bool disable_path(struct source_attempt *a, struct bdw_edp_cycle_report *r,
                         uint32_t primary, uint32_t buffer, uint32_t transport)
{
    r->stage = BDW_EDP_CYCLE_BACKLIGHT;
    if (!store(a, PP_CONTROL, 7 & ~PP_BACKLIGHT, UINT32_MAX) ||
        !wait(a, BACKLIGHT_OFF_US) ||
        !expect(a, PP_CONTROL, UINT32_MAX, 3, BDW_EDP_SOURCE_PANEL))
        return false;
    r->stage = BDW_EDP_CYCLE_PRIMARY;
    if (!primary_off(a, primary))
        return false;
    r->stage = BDW_EDP_CYCLE_TRANSCODER;
    if (!store(a, EDP_CONF, 0, TRANS_ENABLE) ||
        !poll_value(a, EDP_CONF, TRANS_ENABLE | TRANS_ACTIVE, 0, DISABLE_TWO_FRAMES_US, 0) ||
        !store(a, EDP_DDI, EDP_DDI_SST_6BPC_A & ~(TRANS_ENABLE | (7u << 24) | (7u << 28)),
               UINT32_MAX))
        return false;
    r->stage = BDW_EDP_CYCLE_PORT;
    if (!store(a, DDI_BUF_A, buffer & ~DDI_BUF_ENABLE, DDI_BUF_ENABLE) ||
        !store(a, DP_TP_A, transport & ~DP_TP_ENABLE, DP_TP_ENABLE) ||
        !poll_value(a, DDI_BUF_A, DDI_BUF_ENABLE | DDI_BUF_IDLE, DDI_BUF_IDLE, 1000u, 0))
        return false;
    r->stage = BDW_EDP_CYCLE_PANEL;
    if (!store(a, PP_CONTROL, 0, UINT32_MAX) ||
        !poll_value(a, PP_STATUS, PP_OFF_PENDING, 0, PANEL_OFF_US, &r->panel_off) ||
        !store(a, PORT_CLOCK_A, PORT_CLOCK_NONE, UINT32_MAX))
        return false;
    r->stage = BDW_EDP_CYCLE_DARK;
    if (!reset_state(a) || !backing_preserved(a))
        return false;
    r->dark_hash = a->image.backing_hash;
    return true;
}

bool intel_bdw_edp_source_cycle(const struct bdw_edp_source_io *io,
                                const struct bdw_edp_source_image *image,
                                const struct bdw_edp_baseline_report *reference,
                                bdw_edp_dark_handoff handoff,
                                struct bdw_edp_cycle_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_cycle_report){0};
    if (!io || !io->held || !io->read || !io->write || !io->now || !io->delay ||
        !io->backing_hash || !valid_image(image) || !reference_matches(image, reference)) {
        r->shutdown.source_error = BDW_EDP_SOURCE_IMAGE;
        return false;
    }
    struct source_attempt a = {.io = *io, .image = *image, .report = &r->shutdown};
    uint32_t primary, buffer, transport;
    if (!active_path(&a, reference, &primary, &buffer, &transport))
        return false;

    a.gamma = primary & PRI_GAMMA_ENABLE;
    bool opened = fifo_mask(&a, r, false);
    bool restored = false;
    if (opened && disable_path(&a, r, primary, buffer, transport)) {
        enum bdw_edp_dark_result state = handoff ?
            handoff(io->context, image) : BDW_EDP_DARK_UNCHANGED;
        if (state != BDW_EDP_DARK_UNCHANGED && state != BDW_EDP_DARK_RESET_RECOVERED) {
            r->retained = true;
            return false;
        }
        bool keep = state == BDW_EDP_DARK_UNCHANGED;
        if (!keep && (!start_phase(&a, SOURCE_STAGE_US) || !backing_preserved(&a))) {
            r->retained = true;
            return false;
        }
        r->stage = BDW_EDP_CYCLE_RESTORE;
        a.report = &r->restore;
        if (!keep)
            a.gamma = 0;
        restored = restore(io, image, reference, &r->restore, keep, a.gamma) == BDW_EDP_OK &&
                   start_phase(&a, SOURCE_STAGE_US) && scanout_matches(&a) && backing_preserved(&a) &&
                   expect(&a, PP_CONTROL, UINT32_MAX, 7, BDW_EDP_SOURCE_PANEL);
    }
    if ((opened || r->mask_writes) && !r->restore.crc_retained) {
        struct bdw_edp_source_report cleanup = {0};
        a.report = &cleanup;
        r->mask_restored = start_phase(&a, SOURCE_STAGE_US) && fifo_mask(&a, r, true);
        r->mask_error = cleanup.source_error;
    }
    r->retained = (r->mask_writes && !r->mask_restored) ||
                  (r->shutdown.source_writes && !restored);
    if (!restored || !r->mask_restored)
        return false;
    r->retained = false;
    r->stage = BDW_EDP_CYCLE_COMPLETE;
    return true;
}
