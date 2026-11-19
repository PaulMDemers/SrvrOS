#include <srvros/intel_bdw_edp_source.h>

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
#define PP_POWER_STABLE_MASK 0xb8000000u
#define PP_POWER_STABLE_ON 0x80000000u

#define SOURCE_STAGE_US 25000u
#define FRAME_CHECK_US 60000u
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
    bool require_panel;
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
    if (!now || now < a->last ||
        (a->budget && now - a->phase_start >= a->budget))
        return reject(a, BDW_EDP_SOURCE_CLOCK);
    a->last = now;
    return true;
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
    if (!image || !image->backing || !image->epoch ||
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
     * Bypass the inherited gamma LUT, whose contents were lost to reset. */
    return store(a, EDP_DDI, EDP_DDI_SST_6BPC_A, UINT32_MAX) &&
           store(a, EDP_CONF, TRANS_ENABLE, ~TRANS_ACTIVE) &&
           store(a, PRI_A_STRIDE, PRI_STRIDE_BYTES, UINT32_MAX) &&
           store(a, PRI_A_OFFSET, 0, UINT32_MAX) &&
           store(a, PRI_A_CTL, PRI_A_XRGB_TILED, UINT32_MAX) &&
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
           expect(a, DP_TP_A, UINT32_MAX, DP_TP_SST | DP_TP_NORMAL, BDW_EDP_SOURCE_FRAME) &&
           expect(a, EDP_CONF, TRANS_ENABLE | TRANS_ACTIVE, TRANS_ENABLE | TRANS_ACTIVE, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_CTL, UINT32_MAX, PRI_A_XRGB_TILED, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_STRIDE, UINT32_MAX, PRI_STRIDE_BYTES, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_OFFSET, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_SURF, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           expect(a, PRI_A_SURFLIVE, UINT32_MAX, 0, BDW_EDP_SOURCE_FRAME) &&
           underrun_clear(a);
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
    if (!scanout_matches(a))
        return false;

    if (!check(a))
        return false;
    if (!a->io.frame_matches(a->io.context, &a->image, a->report->frame_first, frame))
        return reject(a, BDW_EDP_SOURCE_FRAME);
    if (!scanout_matches(a))
        return false;
    a->report->verified_frames += (uint32_t)(frame - a->report->frame_first);
    a->budget = 0;
    return true;
}

enum bdw_edp_result intel_bdw_edp_source_restore(const struct bdw_edp_source_io *io,
                                                const struct bdw_edp_source_image *image,
                                                struct bdw_edp_source_report *report)
{
    if (!report)
        return BDW_EDP_INVALID;
    *report = (struct bdw_edp_source_report){.panel.result = BDW_EDP_INVALID};
    if (!image || !io || !io->held || !io->read || !io->write ||
        !io->now || !io->delay || !io->frame_matches) {
        report->source_error = BDW_EDP_SOURCE_IMAGE;
        return report->panel.result;
    }

    struct source_attempt a = {.io = *io, .image = *image, .report = report};
    if (!valid_image(&a.image)) {
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
    };
    intel_bdw_edp_panel_restore(&panel, a.image.cdclk_khz, &report->panel);
    if (report->source_error == BDW_EDP_SOURCE_OWNER)
        report->panel.result = BDW_EDP_OWNERSHIP;
    return report->panel.result;
}
