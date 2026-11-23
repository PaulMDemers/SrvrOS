#include <srvros/intel_bdw_flip_trial.h>

static bool await_latch(struct intel_bdw_primary_flip *leg, const struct bdw_flip_trial_io *io)
{
    enum bdw_flip_status status;
    while ((status = intel_bdw_primary_flip_poll(leg)) == BDW_FLIP_WAIT)
        io->crc.delay(io->crc.context, 1000);
    return status == BDW_FLIP_DONE;
}

static bool fresh_capture(const struct intel_bdw_primary_flip *leg,
                          const struct bdw_edp_baseline_report *report)
{
    /* The capture starts after the live latch. Use modular differences so a
     * frame counter roll-over cannot turn an older capture into a fresh one. */
    uint32_t frames = report->frame[0] - leg->p.after_frame;
    return frames && frames < 0x80000000u;
}

static bool run_trial(struct bdw_flip_trial *r, const struct bdw_flip_trial_io *io,
                      uint32_t expected_candidate_crc, const struct scanout_publication *candidate)
{
    if (!r || r->stage != BDW_TRIAL_EMPTY || !io || !io->admit || !io->plane.ctx ||
        io->plane.ctx != io->crc.context || io->plane.now != io->crc.now ||
        !io->plane.ownership || !io->plane.read || !io->plane.write ||
        !expected_candidate_crc || expected_candidate_crc == UINT32_MAX)
        return false;

    r->stage = BDW_TRIAL_ORIGINAL;
    if (!intel_bdw_edp_baseline(&io->crc, &r->original) ||
        r->original.crc[3] == expected_candidate_crc)
        return false;

    r->stage = BDW_TRIAL_FORWARD;
    struct bdw_flip_source *source = io->admit(io->plane.ctx, false);
    if (candidate && (!source || source->binding.to_mapping != &candidate->range ||
        source->binding.to_gpu != (uint64_t)candidate->range.first * 4096 ||
        source->binding.mapped_bytes != (uint64_t)candidate->pages * 4096))
        return false;
    if (!intel_bdw_primary_flip_begin(&r->forward, source, io->plane) ||
        !await_latch(&r->forward, io))
        return false;

    r->stage = BDW_TRIAL_CANDIDATE;
    bool captured = intel_bdw_edp_baseline(&io->crc, &r->candidate);
    if (r->candidate.retained || !r->candidate.restored)
        return false;
    r->candidate_matched = captured && fresh_capture(&r->forward, &r->candidate) &&
                           r->candidate.crc[3] == expected_candidate_crc;

    /* A bad pattern is a test failure, but a known forward latch with clean
     * CRC state permits one freshly admitted return to the original image. */
    r->stage = BDW_TRIAL_RETURN;
    source = io->admit(io->plane.ctx, true);
    if (!intel_bdw_primary_flip_rollback(&r->forward, &r->back, source) ||
        !await_latch(&r->back, io))
        return false;

    r->stage = BDW_TRIAL_VERIFY_RETURN;
    r->original_restored = intel_bdw_edp_baseline(&io->crc, &r->restored) &&
                           fresh_capture(&r->back, &r->restored) &&
                           intel_bdw_edp_baseline_same(&r->original, &r->restored) &&
                           r->original.crc[3] == r->restored.crc[3];
    return r->candidate_matched && r->original_restored;
}

bool intel_bdw_flip_trial(struct bdw_flip_trial *r, const struct bdw_flip_trial_io *io,
                          uint32_t expected_candidate_crc)
{
    return run_trial(r, io, expected_candidate_crc, 0);
}

bool intel_bdw_mapped_flip_trial(struct bdw_flip_trial *r, const struct bdw_flip_trial_io *io,
                                 uint32_t expected_candidate_crc, struct scanout_memory *memory,
                                 const struct scanout_memory_token *token)
{
    if (!r || r->stage != BDW_TRIAL_EMPTY || !memory || memory->object != memory || !token ||
        token->object != memory || token->epoch != memory->epoch || token->owner != memory->owner ||
        memory->state != MEM_GPU || memory->closing || memory->busy ||
        memory->space != memory->publication.space || memory->owner != memory->publication.owner ||
        memory->backing_pages != memory->publication.pages ||
        !scanout_publish_pin_reader(&memory->publication, &memory->publication_token, &r->reader))
        return false;

    bool passed = run_trial(r, io, expected_candidate_crc, &memory->publication);
    /* A bad candidate image can still return cleanly. An attempted plane
     * write followed by uncertain scanout leaves the reader registered. */
    if (!r->forward.p.attempts || r->original_restored) {
        if (!scanout_publish_unpin_reader(&memory->publication, &r->reader)) return false;
    }
    return passed;
}
