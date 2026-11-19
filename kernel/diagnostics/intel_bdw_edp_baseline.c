#include <srvros/intel_bdw_edp_baseline.h>

#define SAMPLE_US 200000u
#define RETIRE_US 65000u
#define PIPE_UNDERRUN 0x80000000u

struct capture {
    struct bdw_edp_baseline_io io;
    struct bdw_edp_baseline_report *report;
    uint64_t begin, last;
    uint32_t budget;
};

static bool check(struct capture *c)
{
    if (!c->io.held(c->io.context))
        return false;
    uint64_t now = c->io.now(c->io.context);
    if (!now || now < c->last || (c->budget && now - c->begin >= c->budget))
        return false;
    c->last = now;
    return true;
}

static bool start(struct capture *c, uint32_t budget)
{
    c->budget = 0;
    if (!check(c))
        return false;
    c->begin = c->last;
    c->budget = budget;
    return true;
}

static bool read(struct capture *c, uint32_t reg, uint32_t *word)
{
    return check(c) && c->io.read(c->io.context, reg, word) && check(c);
}

static bool write(struct capture *c, uint32_t reg, uint32_t word)
{
    if (!check(c))
        return false;
    enum intel_gt_write result = c->io.write(c->io.context, reg, word);
    if (result != GT_WRITE_NONE) {
        ++c->report->writes;
        c->report->retained = true;
    }
    return result == GT_WRITE_DONE && check(c);
}

static bool posted(struct capture *c, uint32_t reg, uint32_t word)
{
    uint32_t readback;
    return write(c, reg, word) && read(c, reg, &readback) && word == readback;
}

static bool pause(struct capture *c)
{
    if (!check(c))
        return false;
    uint64_t before = c->last;
    c->io.delay(c->io.context, 100);
    return check(c) && c->last - before >= 100;
}

static bool stable(const struct bdw_edp_baseline_sample *a, const struct bdw_edp_baseline_sample *b)
{
    if (a->port != b->port || a->buffer != b->buffer || a->transport != b->transport ||
        a->lcpll != b->lcpll || a->cdclk != b->cdclk || a->backing_hash != b->backing_hash)
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        if (a->phy[i][0] != b->phy[i][0] || a->phy[i][1] != b->phy[i][1])
            return false;
    return true;
}

static bool snapshot(struct capture *c, struct bdw_edp_baseline_sample *sample)
{
    if (!check(c) || !c->io.snapshot(c->io.context, sample) || !check(c) || !sample->backing_hash)
        return false;
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i)
        if (!(sample->phy[i][0] & 0xffffffu) || !(sample->phy[i][1] & 31u) ||
            (sample->phy[i][0] & ~0x80ffffffu) || (sample->phy[i][1] & ~0x001f001fu))
            return false;
    return true;
}

static bool sample_crc(struct capture *c)
{
    struct bdw_edp_baseline_report *r = c->report;
    for (unsigned i = 0; i < BDW_EDP_CRC_FRAMES; ++i) {
        uint32_t iir, first, last;
        for (;;) {
            if (!read(c, BDW_EDP_PIPE_IIR, &iir) || (iir & (BDW_EDP_CRC_ERROR | PIPE_UNDERRUN)))
                return false;
            if (iir & BDW_EDP_CRC_DONE)
                break;
            if (!pause(c))
                return false;
        }
        if (!read(c, BDW_EDP_FRAME, &first) || !read(c, BDW_EDP_CRC_RES, &r->crc[i]) ||
            !read(c, BDW_EDP_FRAME, &last) || first != last ||
            (i && ((uint32_t)(last - r->frame[i - 1]) == 0 ||
                   (uint32_t)(last - r->frame[i - 1]) > 2)))
            return false;
        r->frame[i] = last;
        if (!write(c, BDW_EDP_PIPE_IIR, BDW_EDP_CRC_DONE) ||
            !read(c, BDW_EDP_PIPE_IIR, &iir) || (iir & (BDW_EDP_CRC_IRQS | PIPE_UNDERRUN)))
            return false;
        ++r->events;
    }
    /* i915 discards the first two results on this pipeline. */
    return r->crc[2] && r->crc[2] != UINT32_MAX && r->crc[2] == r->crc[3];
}

static bool retire(struct capture *c)
{
    uint32_t mask, ctl, begin, frame, pending, ier;
    uint32_t original = c->report->original_mask;
    uint32_t temporary = original & ~BDW_EDP_CRC_IRQS;
    if (!start(c, RETIRE_US) ||
        !read(c, BDW_EDP_PIPE_IER, &ier) || (ier & BDW_EDP_CRC_IRQS) ||
        !read(c, BDW_EDP_PIPE_IMR, &mask) || (mask != original && mask != temporary) ||
        !read(c, BDW_EDP_CRC_CTL, &ctl) || (ctl != 0 && ctl != BDW_EDP_CRC_ENABLE))
        return false;
    if (ctl && (mask != temporary || !posted(c, BDW_EDP_CRC_CTL, 0)))
        return false;
    if (mask != original) {
        if (!read(c, BDW_EDP_FRAME, &begin))
            return false;
        do {
            if (!pause(c) || !read(c, BDW_EDP_FRAME, &frame))
                return false;
        } while ((uint32_t)(frame - begin) < 2);
        if ((uint32_t)(frame - begin) > 4 || !read(c, BDW_EDP_PIPE_IIR, &pending))
            return false;
        pending &= BDW_EDP_CRC_IRQS;
        if ((pending && !write(c, BDW_EDP_PIPE_IIR, pending)) ||
            !read(c, BDW_EDP_PIPE_IIR, &pending) || (pending & BDW_EDP_CRC_IRQS) ||
            !posted(c, BDW_EDP_PIPE_IMR, original))
            return false;
    }
    return read(c, BDW_EDP_CRC_CTL, &ctl) && ctl == 0 &&
           read(c, BDW_EDP_PIPE_IIR, &pending) && !(pending & BDW_EDP_CRC_IRQS) &&
           read(c, BDW_EDP_PIPE_IMR, &mask) && mask == original;
}

bool intel_bdw_edp_baseline(const struct bdw_edp_baseline_io *io,
                            struct bdw_edp_baseline_report *r)
{
    if (!r)
        return false;
    *r = (struct bdw_edp_baseline_report){0};
    if (!io || !io->held || !io->snapshot || !io->read || !io->write || !io->now || !io->delay)
        return false;
    struct capture c = {.io = *io, .report = r};
    uint32_t ctl, ier, iir;
    if (!start(&c, SAMPLE_US) || !snapshot(&c, &r->sample) ||
        !read(&c, BDW_EDP_CRC_CTL, &ctl) || ctl ||
        !read(&c, BDW_EDP_PIPE_IMR, &r->original_mask) ||
        (r->original_mask & BDW_EDP_CRC_IRQS) != BDW_EDP_CRC_IRQS ||
        !read(&c, BDW_EDP_PIPE_IER, &ier) || (ier & BDW_EDP_CRC_IRQS) ||
        !read(&c, BDW_EDP_PIPE_IIR, &iir) || (iir & (BDW_EDP_CRC_IRQS | PIPE_UNDERRUN)))
        return false;

    r->status = BDW_EDP_BASELINE_INCOMPLETE;
    bool sampled = posted(&c, BDW_EDP_PIPE_IMR, r->original_mask & ~BDW_EDP_CRC_IRQS) &&
                   posted(&c, BDW_EDP_CRC_CTL, BDW_EDP_CRC_ENABLE) && sample_crc(&c);
    if (r->retained)
        r->restored = retire(&c);
    if (!r->restored)
        return false;

    struct bdw_edp_baseline_sample after;
    if (!start(&c, SAMPLE_US) || !snapshot(&c, &after) || !stable(&r->sample, &after))
        return false;
    r->retained = false;
    if (sampled)
        r->status = BDW_EDP_BASELINE_OK;
    return sampled;
}

bool intel_bdw_edp_baseline_same(const struct bdw_edp_baseline_report *a,
                                 const struct bdw_edp_baseline_report *b)
{
    return a && b && a->status == BDW_EDP_BASELINE_OK && b->status == BDW_EDP_BASELINE_OK &&
           a->restored && b->restored && !a->retained && !b->retained &&
           stable(&a->sample, &b->sample);
}
