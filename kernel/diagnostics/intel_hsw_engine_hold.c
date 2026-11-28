#include <srvros/intel_hsw_engine_hold.h>

#define BCS_STOP_RING (1u << 8)
#define BCS_RING_IDLE (1u << 9)
#define BCS_BYPASS_FENCE_WRITE (1u << 1)
#define BCS_SYNC_FLUSH (1u << 5)
#define BCS_SYNC_STATUS (1u << 24)
#define HSW_ENGINE_HOLD_US 4000u
#define HSW_ENGINE_HOLD_POLLS 65536u

static bool held(struct hsw_engine_hold *hold, const struct hsw_hold_io *io) {
    if (!io->valid(io->ctx, hold->ticket)) return false;
    uint64_t now = io->now(io->ctx);
    if (now < hold->last || now - hold->begin > HSW_ENGINE_HOLD_US) return false;
    hold->last = now;
    return true;
}

static bool observe(struct hsw_engine_hold *hold, const struct hsw_hold_io *io,
                    struct hsw_hold_sample *sample) {
    if (hold->polls >= HSW_ENGINE_HOLD_POLLS || !held(hold, io)) return false;
    hold->polls++;
    return io->read(io->ctx, hold->ticket, sample) && held(hold, io) &&
           sample->mi_mode != UINT32_MAX && sample->instpm != UINT32_MAX &&
           sample->sync_status != UINT32_MAX && !(sample->mi_mode & BCS_BYPASS_FENCE_WRITE);
}

static enum hsw_hold_change change(struct hsw_engine_hold *hold, const struct hsw_hold_io *io,
                                   enum hsw_hold_op op) {
    if (!held(hold, io)) return HSW_ENGINE_UNCHANGED;
    hold->attempted |= 1u << op;
    return io->change(io->ctx, hold->ticket, op);
}

bool intel_hsw_engine_hold(struct hsw_engine_hold *hold, const struct hsw_hold_io *io, uint64_t ticket) {
    if (!hold || hold->state != HSW_ENGINE_HOLD_NEW) return false;
    hold->state = HSW_ENGINE_HOLD_REFUSED;
    hold->reason = "no-provider";
    if (!io || !ticket || !io->valid || !io->now || !io->read || !io->change) return false;

    hold->reason = "baseline";
    hold->ticket = ticket;
    hold->begin = hold->last = io->now(io->ctx);
    struct hsw_hold_sample sample;
    if (!hold->begin || !observe(hold, io, &sample) || (sample.mi_mode & BCS_STOP_RING) ||
        !(sample.mi_mode & BCS_RING_IDLE) || (sample.instpm & BCS_SYNC_FLUSH) ||
        (sample.sync_status & BCS_SYNC_STATUS)) return false;

    hold->reason = "stop";
    enum hsw_hold_change stopped = change(hold, io, HSW_ENGINE_STOP);
    if (stopped == HSW_ENGINE_UNCHANGED) return false;
    hold->state = HSW_ENGINE_HOLD_RETAINED;
    if (stopped != HSW_ENGINE_CHANGED) return false;

    hold->reason = "idle-or-stale";
    const uint32_t stop_idle = BCS_STOP_RING | BCS_RING_IDLE;
    do {
        if (!observe(hold, io, &sample) || (sample.instpm & BCS_SYNC_FLUSH) ||
            (sample.sync_status & BCS_SYNC_STATUS)) return false;
    } while ((sample.mi_mode & stop_idle) != stop_idle);

    hold->reason = "flush";
    if (change(hold, io, HSW_ENGINE_FLUSH) != HSW_ENGINE_CHANGED) return false;

    /* Request self-clear alone proves nothing. Require a fresh completion
     * over the clear baseline under the same stop and provider ticket. */
    hold->reason = "completion";
    do {
        if (!observe(hold, io, &sample) || (sample.mi_mode & stop_idle) != stop_idle) return false;
    } while ((sample.instpm & BCS_SYNC_FLUSH) || !(sample.sync_status & BCS_SYNC_STATUS));
    hold->completed_at = hold->last;

    hold->reason = "reopen";
    if (change(hold, io, HSW_ENGINE_REOPEN) != HSW_ENGINE_CHANGED) return false;
    do {
        if (!observe(hold, io, &sample) || (sample.instpm & BCS_SYNC_FLUSH)) return false;
    } while (sample.mi_mode & BCS_STOP_RING);
    hold->state = HSW_ENGINE_HOLD_RETURNED;
    hold->reason = "none";
    return true;
}
