#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_hsw_engine_hold.h>

enum fixture {
    NORMAL, IMMEDIATE, NO_TICKET, STALE, LATE_STALE, BUSY, BYPASS, INVALID_REGISTER,
    STOP_UNCERTAIN, NO_COMPLETION, REQUEST_NOT_CLEARED, LOST_STOP, LOST_TICKET, FLUSH_UNCERTAIN,
    REOPEN_UNCERTAIN, REOPEN_STUCK, STALLED_CLOCK,
};

enum { STOP = 0x100, IDLE = 0x200, REQUEST = 0x20, COMPLETE = 1u << 24 };
struct model {
    enum fixture fixture;
    unsigned phase, reads, calls;
    uint64_t now;
};

static bool valid(void *ctx, uint64_t ticket) {
    struct model *m = ctx;
    return ticket == 42 && m->fixture != NO_TICKET &&
           !(m->fixture == LOST_TICKET && m->phase == 2);
}

static uint64_t now(void *ctx) {
    struct model *m = ctx;
    return m->fixture == STALLED_CLOCK ? 1 : ++m->now;
}

static bool read_status(void *ctx, uint64_t ticket, struct hsw_hold_sample *s) {
    struct model *m = ctx;
    assert(valid(ctx, ticket));
    m->reads++;
    *s = (struct hsw_hold_sample){.mi_mode = IDLE};
    if (m->phase == 0) {
        if (m->fixture == INVALID_REGISTER) s->instpm = UINT32_MAX;
        if (m->fixture == STALE) s->sync_status = COMPLETE;
        if (m->fixture == BUSY) s->mi_mode = 0;
        if (m->fixture == BYPASS) s->mi_mode |= 2;
    } else if (m->phase == 1) {
        s->mi_mode = STOP | (m->reads > 1 ? IDLE : 0);
        if (m->fixture == LATE_STALE) s->sync_status = COMPLETE;
    } else if (m->phase == 2) {
        s->mi_mode |= STOP;
        if (m->fixture == NORMAL && m->reads == 1) s->instpm = REQUEST;
        if (m->fixture == REQUEST_NOT_CLEARED) s->instpm = REQUEST;
        if (m->fixture != NO_COMPLETION && m->fixture != STALLED_CLOCK && !s->instpm)
            s->sync_status = COMPLETE;
        if (m->fixture == REQUEST_NOT_CLEARED) s->sync_status = COMPLETE;
        if (m->fixture == LOST_STOP) s->mi_mode &= ~STOP;
    } else if (m->fixture == REOPEN_STUCK) {
        s->mi_mode |= STOP;
    }
    return true;
}

static enum hsw_hold_change change(void *ctx, uint64_t ticket, enum hsw_hold_op op) {
    struct model *m = ctx;
    assert(valid(ctx, ticket) && (unsigned)op == m->phase);
    m->calls++;
    m->phase++;
    m->reads = 0;
    if ((op == HSW_ENGINE_STOP && m->fixture == STOP_UNCERTAIN) ||
        (op == HSW_ENGINE_FLUSH && m->fixture == FLUSH_UNCERTAIN) ||
        (op == HSW_ENGINE_REOPEN && m->fixture == REOPEN_UNCERTAIN)) return HSW_ENGINE_UNCERTAIN;
    return HSW_ENGINE_CHANGED;
}

int main(void) {
    struct hsw_engine_hold absent = {0};
    assert(!intel_hsw_engine_hold(&absent, NULL, 0));
    assert(absent.state == HSW_ENGINE_HOLD_REFUSED && !strcmp(absent.reason, "no-provider"));
    assert(!absent.attempted && !absent.polls && !absent.completed_at);
    const struct {
        enum fixture fixture;
        enum hsw_hold_state state;
        unsigned calls;
        bool completion;
    } cases[] = {
        {NORMAL, HSW_ENGINE_HOLD_RETURNED, 3, true},
        {IMMEDIATE, HSW_ENGINE_HOLD_RETURNED, 3, true},
        {NO_TICKET, HSW_ENGINE_HOLD_REFUSED, 0, false},
        {STALE, HSW_ENGINE_HOLD_REFUSED, 0, false},
        {LATE_STALE, HSW_ENGINE_HOLD_RETAINED, 1, false},
        {BUSY, HSW_ENGINE_HOLD_REFUSED, 0, false},
        {BYPASS, HSW_ENGINE_HOLD_REFUSED, 0, false},
        {INVALID_REGISTER, HSW_ENGINE_HOLD_REFUSED, 0, false},
        {STOP_UNCERTAIN, HSW_ENGINE_HOLD_RETAINED, 1, false},
        {NO_COMPLETION, HSW_ENGINE_HOLD_RETAINED, 2, false},
        {REQUEST_NOT_CLEARED, HSW_ENGINE_HOLD_RETAINED, 2, false},
        {LOST_STOP, HSW_ENGINE_HOLD_RETAINED, 2, false},
        {LOST_TICKET, HSW_ENGINE_HOLD_RETAINED, 2, false},
        {FLUSH_UNCERTAIN, HSW_ENGINE_HOLD_RETAINED, 2, false},
        {REOPEN_UNCERTAIN, HSW_ENGINE_HOLD_RETAINED, 3, true},
        {REOPEN_STUCK, HSW_ENGINE_HOLD_RETAINED, 3, true},
        {STALLED_CLOCK, HSW_ENGINE_HOLD_RETAINED, 2, false},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        struct model model = {.fixture = cases[i].fixture};
        struct hsw_engine_hold hold = {0};
        const struct hsw_hold_io io = {&model, valid, now, read_status, change};
        assert(intel_hsw_engine_hold(&hold, &io, 42) == (cases[i].state == HSW_ENGINE_HOLD_RETURNED));
        assert(hold.state == cases[i].state && !!hold.completed_at == cases[i].completion);
        assert(hold.attempted == (1u << cases[i].calls) - 1 && model.calls == cases[i].calls);
        if (model.fixture == STALLED_CLOCK) assert(hold.polls == 65536);
        assert(!intel_hsw_engine_hold(&hold, &io, 42) && model.calls == cases[i].calls);
    }
    puts("haswell-engine-hold: PASS refusal, fresh completion, bounded failure, retained ownership and verified reopen");
}
