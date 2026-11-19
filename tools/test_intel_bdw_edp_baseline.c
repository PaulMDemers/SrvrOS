#include <srvros/intel_bdw_edp_baseline.h>
#include <assert.h>
#include <stdio.h>

enum fault {
    NO_FAULT, IRQ_LIVE, STALE_DONE, BUSY, WRONG_PHY, LOST_HOLD, NO_CRC,
    OLD_FRAME, BAD_CRC, CRC_ERROR, UNCERTAIN_MASK, BAD_RESTORE, LOST_BACKING,
    LOST_PHY, CLOCK_STOP, PIPE_UNDERRUN
};

struct fixture {
    uint64_t time, capture_start;
    uint32_t imr, ier, iir, ctl, frame, crc;
    unsigned samples, reads, writes, ack, edges;
    enum fault fault;
    bool after_close;
};

static bool held(void *context)
{
    struct fixture *f = context;
    return f->fault != LOST_HOLD || f->writes == 0;
}

static bool capture(void *context, struct bdw_edp_baseline_sample *sample)
{
    struct fixture *f = context;
    ++f->samples;
    *sample = (struct bdw_edp_baseline_sample){
        .port = 0x20000000, .buffer = 0x80000010, .transport = 0x80040300,
        .lcpll = 0x40000000, .cdclk = 449, .backing_hash = 0x74617274,
    };
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i) {
        sample->phy[i][0] = 0x3FFFFF + i;
        sample->phy[i][1] = 0x7000e;
    }
    if (f->fault == WRONG_PHY)
        sample->phy[3][0] |= 1u << 24;
    if (f->samples > 1 && f->fault == LOST_BACKING)
        ++sample->backing_hash;
    if (f->samples > 1 && f->fault == LOST_PHY)
        ++sample->phy[7][1];
    return true;
}

static bool read(void *context, uint32_t reg, uint32_t *word)
{
    struct fixture *f = context;
    ++f->reads;
    switch (reg) {
    case BDW_EDP_PIPE_IMR: *word = f->imr; break;
    case BDW_EDP_PIPE_IER: *word = f->ier; break;
    case BDW_EDP_PIPE_IIR: *word = f->iir; break;
    case BDW_EDP_CRC_CTL: *word = f->ctl; break;
    case BDW_EDP_FRAME: *word = f->frame; break;
    case BDW_EDP_CRC_RES:
        assert(f->iir & BDW_EDP_CRC_DONE);
        *word = f->crc + (f->fault == BAD_CRC ? f->edges : 0);
        break;
    default: assert(0); return false;
    }
    return true;
}

static enum intel_gt_write write(void *context, uint32_t reg, uint32_t word)
{
    struct fixture *f = context;
    ++f->writes;
    switch (reg) {
    case BDW_EDP_CRC_CTL:
        assert((f->imr & BDW_EDP_CRC_IRQS) == 0 && !(f->ier & BDW_EDP_CRC_IRQS));
        assert(word == 0 || word == BDW_EDP_CRC_ENABLE);
        f->ctl = word;
        if (!word) {
            f->after_close = true;
            if (f->fault == BAD_RESTORE)
                f->ctl = BDW_EDP_CRC_ENABLE;
        }
        break;
    case BDW_EDP_PIPE_IIR:
        assert(word && !(word & ~BDW_EDP_CRC_IRQS));
        assert((f->iir & word) == word);
        f->iir &= ~word;
        ++f->ack;
        break;
    case BDW_EDP_PIPE_IMR:
        assert(!f->ctl && ((f->imr ^ word) & ~BDW_EDP_CRC_IRQS) == 0);
        f->imr = word;
        if (f->fault == UNCERTAIN_MASK && f->writes == 1)
            return GT_WRITE_UNCERTAIN;
        break;
    default: assert(0);
    }
    return GT_WRITE_DONE;
}

static uint64_t now(void *context)
{
    return ((struct fixture *)context)->time;
}

static void wait(void *context, uint32_t us)
{
    struct fixture *f = context;
    if (f->fault == CLOCK_STOP)
        return;
    f->time += us;
    if (f->time / 16667 == (f->time - us) / 16667)
        return;
    if (f->fault != OLD_FRAME || f->after_close)
        ++f->frame;
    if (f->ctl == BDW_EDP_CRC_ENABLE && f->fault != NO_CRC) {
        assert((f->imr & BDW_EDP_CRC_IRQS) == 0 && !(f->ier & BDW_EDP_CRC_IRQS));
        ++f->edges;
        f->iir |= BDW_EDP_CRC_DONE;
        if (f->fault == CRC_ERROR)
            f->iir |= BDW_EDP_CRC_ERROR;
        if (f->fault == PIPE_UNDERRUN)
            f->iir |= 0x80000000u;
    }
}

static bool exercise(struct fixture *f, struct bdw_edp_baseline_report *report)
{
    f->time = 701;
    f->frame = UINT32_MAX - 2;
    f->imr = 0xfffffffdu;
    f->iir = 2u;
    if (f->fault == IRQ_LIVE)
        f->ier = BDW_EDP_CRC_DONE;
    if (f->fault == STALE_DONE)
        f->iir |= BDW_EDP_CRC_DONE;
    if (f->fault == BUSY)
        f->ctl = BDW_EDP_CRC_ENABLE;
    if (!f->crc)
        f->crc = 0x441234ab;
    const struct bdw_edp_baseline_io io = {f, held, capture, read, write, now, wait};
    return intel_bdw_edp_baseline(&io, report);
}

int main(void)
{
    struct fixture f = {0};
    struct bdw_edp_baseline_report a, b;
    assert(exercise(&f, &a));
    assert(a.restored && !a.retained && a.status == BDW_EDP_BASELINE_OK);
    assert(f.writes == 8 && f.ack == 4 && f.samples == 2 && f.iir == 2 && f.imr == 0xfffffffdu);
    assert(a.events == 4 && a.frame[2] == 0 && a.crc[2] == f.crc);
    f = (struct fixture){.crc = 0x33ce971d};
    assert(exercise(&f, &b) && intel_bdw_edp_baseline_same(&a, &b));
    assert(a.crc[2] != b.crc[2]);
    ++b.sample.phy[1][1];
    assert(!intel_bdw_edp_baseline_same(&a, &b));

    for (enum fault i = IRQ_LIVE; i <= PIPE_UNDERRUN; ++i) {
        f = (struct fixture){.fault = i};
        assert(!exercise(&f, &a));
        assert(a.status != BDW_EDP_BASELINE_OK);
        if (i <= WRONG_PHY)
            assert(!a.writes && !a.retained);
        if (i == LOST_HOLD || i == BAD_RESTORE || i == LOST_BACKING ||
            i == LOST_PHY || i == CLOCK_STOP)
            assert(a.retained);
        if (i == NO_CRC || i == BAD_CRC || i == CRC_ERROR || i == OLD_FRAME ||
            i == UNCERTAIN_MASK || i == PIPE_UNDERRUN)
            assert(!a.retained && a.restored && !f.ctl && f.imr == 0xfffffffdu);
        assert(f.iir & 2u);
    }
    puts("Broadwell eDP baseline: CRC frame freshness, PHY and lease revalidation, and exact rollback passed");
    return 0;
}
