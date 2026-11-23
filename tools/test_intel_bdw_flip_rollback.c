/* Exercise the actual surface and CRC protocols on one simulated Pipe A. */
#include <srvros/intel_bdw_flip_trial.h>
#include <srvros/intel_bdw_primary_flip.h>
#include <srvros/scanout_memory.h>
#include <assert.h>
#include <stdio.h>

enum { SECOND = 0x2200000, ORIGINAL_CRC = 0x12344321, SECOND_CRC = 0x76545678 };

struct panel {
    uint64_t time;
    uint32_t queued, live, frame, mask, interrupts, crc_ctl, crc;
    unsigned arms;
    bool owned, uncertain_return, wrong_image;
    struct bdw_flip_source source[2];
    struct intel_bdw_primary_flip flip[2];
    struct intel_bdw_primary_flip *registered[2];
    bool bad_restore, freeze_return, keep_crc_enabled;
    int lease[2], owner;
    const void *mapping[2];
    uint32_t alternate;
    uint32_t (*scan_crc)(uint32_t);
    bool (*mapping_held)(void);
};

static bool held(void *ctx)
{
    struct panel *p = ctx;
    return p->owned && (!p->mapping_held || p->mapping_held());
}

static uint64_t now(void *ctx)
{
    return ++((struct panel *)ctx)->time;
}

static void edge(struct panel *p)
{
    ++p->frame;
    if (!p->freeze_return || p->arms != 2)
        p->live = p->queued;
    if (p->crc_ctl && !(p->mask & BDW_EDP_CRC_DONE)) {
        if (p->scan_crc)
            p->crc = p->scan_crc(p->live);
        else
            p->crc = p->live == p->alternate && !p->wrong_image ? SECOND_CRC : ORIGINAL_CRC;
        if (p->bad_restore && p->arms == 2)
            ++p->crc;
        p->interrupts |= BDW_EDP_CRC_DONE;
    }
}

static void delay(void *ctx, uint32_t us)
{
    struct panel *p = ctx;
    p->time += us;
    /* The capture loop also reads the clock; latch within one poll of vblank. */
    if (p->time / 16667u > p->frame)
        edge(p);
}

static bool snapshot(void *ctx, struct bdw_edp_baseline_sample *sample)
{
    struct panel *p = ctx;
    *sample = (struct bdw_edp_baseline_sample){.backing_hash = p->live == p->alternate ? 2u : 1u};
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i) {
        sample->phy[i][0] = 1;
        sample->phy[i][1] = 1;
    }
    return held(p);
}

static bool proof(void *ctx, const struct bdw_flip_source *s, struct bdw_flip_proof *out)
{
    struct panel *p = ctx;
    unsigned i = s == &p->source[1];
    if (!held(p) || s != &p->source[i] || s->consumer != p->registered[i])
        return false;
    *out = (struct bdw_flip_proof){s, &p->owner, p, &p->lease, p->mapping[i],
        p->mapping[1-i], 0x1000, 9, i + 1, p->time, BDW_FLIP_REQUIRED, s->consumer->p.state};
    return true;
}

static bool read_reg(void *ctx, uint32_t reg, uint32_t *word)
{
    struct panel *p = ctx;
    if (!held(p))
        return false;
    switch (reg) {
    case 0x7019c: *word = p->queued; break;
    case 0x701ac: *word = p->live; break;
    case 0x70180: *word = 0xd8000400; break;
    case 0x70188: *word = 6144; break;
    case 0x6001c: *word = (1439u << 16) | 899; break;
    case 0x701a4: case BDW_EDP_PIPE_IER: *word = 0; break;
    case BDW_EDP_FRAME: *word = p->frame; break;
    case BDW_EDP_PIPE_IMR: *word = p->mask; break;
    case BDW_EDP_PIPE_IIR: *word = p->interrupts; break;
    case BDW_EDP_CRC_CTL: *word = p->crc_ctl; break;
    case BDW_EDP_CRC_RES: *word = p->crc; break;
    default: assert(false);
    }
    return true;
}

static enum intel_gt_write write_reg(void *ctx, uint32_t reg, uint32_t word)
{
    struct panel *p = ctx;
    assert(held(p));
    switch (reg) {
    case 0x7019c:
        assert(p->arms < 2 && word == (p->arms == 0 ? p->alternate : 0));
        p->queued = word;
        if (++p->arms == 2 && p->uncertain_return)
            return GT_WRITE_UNCERTAIN;
        break;
    case BDW_EDP_PIPE_IMR: p->mask = word; break;
    case BDW_EDP_PIPE_IIR: assert(word == BDW_EDP_CRC_DONE); p->interrupts &= ~word; break;
    case BDW_EDP_CRC_CTL:
        if (!(p->keep_crc_enabled && p->arms == 1 && !word))
            p->crc_ctl = word;
        break;
    default: assert(false);
    }
    return GT_WRITE_DONE;
}

static void init(struct panel *p)
{
    *p = (struct panel){.time = 1, .mask = UINT32_MAX, .owned = true, .alternate = SECOND};
    p->mapping[0] = &p->lease[0];
    p->mapping[1] = &p->lease[1];
    for (unsigned i = 0; i < 2; ++i) {
        p->registered[i] = &p->flip[i];
        p->source[i].object = &p->source[i];
        p->source[i].binding = (struct bdw_flip_binding){
            &p->owner, p, &p->lease, &p->lease[i], &p->lease[1-i],
            0x1000, 9, i + 1, 1, 0x10000000, i ? SECOND : 0, i ? 0 : SECOND,
            0x600000, 0x8086, 0x1626, (1439u << 16) | 899, 0xd8000400, 6144
        };
    }
}

static void capture_crc(struct panel *p, struct bdw_edp_baseline_report *r)
{
    const struct bdw_edp_baseline_io io = {p, held, snapshot, read_reg, write_reg, now, delay};
    uint32_t start = p->frame;
    assert(intel_bdw_edp_baseline(&io, r));
    assert(r->events == 4 && r->frame[0] > start && r->restored && !r->retained);
    assert(!p->crc_ctl && !p->interrupts && p->mask == UINT32_MAX);
}

static void vblank(struct panel *p)
{
    p->time = ((uint64_t)p->frame + 1) * 16667u;
    edge(p);
}

enum scenario { NORMAL, WRONG_IMAGE, TIMED_OUT, UNCERTAIN, LOST_OWNER, FOREIGN_LIVE };

static void trial(enum scenario which)
{
    struct panel p;
    init(&p);
    p.wrong_image = which == WRONG_IMAGE;
    struct bdw_edp_baseline_report original, candidate, restored;
    capture_crc(&p, &original);
    const struct bdw_flip_io ops = {&p, proof, read_reg, write_reg, now};
    p.source[0].binding.admitted_at = now(&p);
    assert(intel_bdw_primary_flip_begin(&p.flip[0], &p.source[0], ops));
    assert(!intel_bdw_primary_flip_rollback(&p.flip[0], &p.flip[1], &p.source[1]));
    assert(intel_bdw_primary_flip_poll(&p.flip[0]) == BDW_FLIP_WAIT);
    vblank(&p);
    assert(intel_bdw_primary_flip_poll(&p.flip[0]) == BDW_FLIP_DONE);
    capture_crc(&p, &candidate);
    assert(original.crc[3] == ORIGINAL_CRC);
    assert((candidate.crc[3] == SECOND_CRC) == !p.wrong_image);

    /* Even an unexpected image must be returned to the inherited surface
     * when the forward latch is certain and the original lease still holds. */
    struct bdw_flip_binding *reverse = &p.source[1].binding;
    assert(!intel_bdw_primary_flip_rollback(&p.flip[0], &p.flip[1], &p.source[1])); /* stale time */
    reverse->admitted_at = now(&p);
    const struct bdw_flip_binding good = *reverse;
    for (unsigned i = 0; i < 9; ++i) {
        switch (i) {
        case 0: reverse->root += 4096; break;
        case 1: ++reverse->epoch; break;
        case 2: --reverse->sequence; break;
        case 3: reverse->lifetime = &original; break;
        case 4: reverse->to_mapping = &candidate; break;
        case 5: reverse->to_gpu = 0x40000; break;
        case 6: reverse->from_gpu += 0x40000; break;
        case 7: reverse->mapped_bytes += 4096; break;
        case 8: reverse->control ^= 1u << 30; break;
        }
        assert(!intel_bdw_primary_flip_rollback(&p.flip[0], &p.flip[1], &p.source[1]));
        assert(!p.flip[0].rollback && !p.source[1].consumer && p.arms == 1);
        *reverse = good;
    }
    p.owned = which != LOST_OWNER;
    p.uncertain_return = which == UNCERTAIN;
    if (which == FOREIGN_LIVE)
        p.live = 0x40000;
    bool pending = intel_bdw_primary_flip_rollback(&p.flip[0], &p.flip[1], &p.source[1]);
    assert(p.flip[0].rollback == &p.flip[1] && p.source[1].consumer == &p.flip[1]);
    assert(pending == (which == NORMAL || which == WRONG_IMAGE || which == TIMED_OUT));
    assert(!intel_bdw_primary_flip_rollback(&p.flip[0], &p.flip[1], &p.source[1]));
    struct intel_bdw_primary_flip duplicate = {0};
    struct bdw_flip_source fresh = {.object = &fresh, .binding = good};
    ++fresh.binding.sequence;
    assert(!intel_bdw_primary_flip_rollback(&p.flip[0], &duplicate, &fresh));
    assert(!duplicate.object && !fresh.consumer);
    if (!pending) {
        assert(p.flip[1].retained && p.flip[1].p.uncertain == p.uncertain_return);
        assert(p.arms == (p.uncertain_return ? 2u : 1u));
        return;
    }
    assert(p.arms == 2 && p.live == SECOND);
    assert(intel_bdw_primary_flip_poll(&p.flip[1]) == BDW_FLIP_WAIT);
    if (which == TIMED_OUT) {
        p.time += BDW_FLIP_US;
        assert(intel_bdw_primary_flip_poll(&p.flip[1]) == BDW_FLIP_FAILED);
        assert(p.flip[1].retained && p.flip[1].reason == BDW_FLIP_CLOCK);
        return;
    }
    vblank(&p);
    assert(intel_bdw_primary_flip_poll(&p.flip[1]) == BDW_FLIP_DONE);
    capture_crc(&p, &restored);
    assert(intel_bdw_edp_baseline_same(&original, &restored));
    assert(restored.crc[3] == original.crc[3] && !p.live && !p.queued);
}

static struct bdw_flip_source *admit(void *ctx, bool returning)
{
    struct panel *p = ctx;
    p->source[returning].binding.admitted_at = now(p);
    return &p->source[returning];
}

static void diagnostic(void)
{
    enum { PASS, BAD_IMAGE, BAD_RESTORE, UNCERTAIN_STORE, STUCK_SURFACE,
           DIRTY_CRC, SAME_IMAGE };
    for (unsigned kind = PASS; kind <= SAME_IMAGE; ++kind) {
        struct panel p;
        init(&p);
        struct bdw_flip_trial record = {0};
        p.registered[0] = &record.forward;
        p.registered[1] = &record.back;
        p.wrong_image = kind == BAD_IMAGE;
        p.bad_restore = kind == BAD_RESTORE;
        p.uncertain_return = kind == UNCERTAIN_STORE;
        p.freeze_return = kind == STUCK_SURFACE;
        p.keep_crc_enabled = kind == DIRTY_CRC;
        const struct bdw_flip_trial_io io = {
            {&p, proof, read_reg, write_reg, now},
            {&p, held, snapshot, read_reg, write_reg, now, delay}, admit
        };
        uint32_t expected = kind == SAME_IMAGE ? ORIGINAL_CRC : SECOND_CRC;
        assert(intel_bdw_flip_trial(&record, &io, expected) == (kind == PASS));
        assert(!intel_bdw_flip_trial(&record, &io, expected));
        assert(record.original_restored == (kind == PASS || kind == BAD_IMAGE));
        assert(p.arms == (kind == SAME_IMAGE ? 0u : kind == DIRTY_CRC ? 1u : 2u));
        if (kind == DIRTY_CRC)
            assert(record.candidate.retained && record.stage == BDW_TRIAL_CANDIDATE);
        if (kind == UNCERTAIN_STORE || kind == STUCK_SURFACE)
            assert(record.back.retained && record.stage == BDW_TRIAL_RETURN);
    }
}

void test_flip_rollback(void)
{
    for (enum scenario s = NORMAL; s <= FOREIGN_LIVE; ++s)
        trial(s);
    diagnostic();
    puts("reset-free round trip: 6 rollback scenarios and 7 joined CRC diagnostics");
}

/* Called from the transfer fixture after real memory publication. Model the
 * tap from the emulated GGTT and RAM; the restoration uses the same mapping. */
bool test_flip_on_published_surface(struct scanout_memory *memory, const struct scanout_memory_token *token,
                                    bool uncertain_return, bool (*mapping_held)(void),
                                    uint32_t (*scan_crc)(uint32_t), uint32_t expected)
{
    struct panel p;
    init(&p);
    static struct bdw_flip_trial record; /* Survives a retained reader. */
    p.uncertain_return = uncertain_return;
    p.registered[0] = &record.forward;
    p.registered[1] = &record.back;
    p.alternate = memory->range.first * 4096u;
    p.mapping[1] = &memory->publication.range;
    p.mapping_held = mapping_held;
    p.scan_crc = scan_crc;
    for (unsigned i = 0; i < 2; ++i) {
        struct bdw_flip_binding *b = &p.source[i].binding;
        b->from_mapping = p.mapping[i];
        b->to_mapping = p.mapping[1-i];
        b->from_gpu = i ? p.alternate : 0;
        b->to_gpu = i ? 0 : p.alternate;
        b->mapped_bytes = (uint64_t)memory->range.pages * 4096;
    }
    const struct bdw_flip_trial_io io = {
        {&p, proof, read_reg, write_reg, now},
        {&p, held, snapshot, read_reg, write_reg, now, delay}, admit
    };
    struct scanout_memory_token stale = *token;
    ++stale.epoch;
    assert(!intel_bdw_mapped_flip_trial(&record, &io, expected, memory, &stale));
    struct scanout_reader *prior = memory->publication.readers;
    const struct bdw_flip_binding saved = p.source[0].binding;
    for (unsigned mismatch = 0; mismatch < 3; ++mismatch) {
        struct bdw_flip_trial invalid = {0};
        if (mismatch == 0) p.source[0].binding.to_mapping = &memory->range;
        if (mismatch == 1) p.source[0].binding.to_gpu += 256u * 1024;
        if (mismatch == 2) p.source[0].binding.mapped_bytes += 4096;
        assert(!intel_bdw_mapped_flip_trial(&invalid, &io, expected, memory, token));
        assert(invalid.stage == BDW_TRIAL_FORWARD && !invalid.forward.object && !p.arms);
        assert(prior && memory->publication.readers == prior);
        p.source[0].binding = saved;
    }
    bool passed = intel_bdw_mapped_flip_trial(&record, &io, expected, memory, token);
    if (uncertain_return) {
        assert(!passed && record.back.retained && memory->publication.readers == &record.reader);
        return false;
    }
    assert(record.original_restored && p.arms == 2 && !p.live && !p.queued);
    return passed;
}
