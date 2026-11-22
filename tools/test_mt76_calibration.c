#include <srvros/mt76_calibration.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct event { char kind; uint32_t address, value; };
struct fixture {
    struct mt76_calibration state;
    unsigned calls, auths, fail, cancel, mismatch, all_ones, restore_fail;
    bool failed, cancelled, reenter;
    uint32_t alc, ibi;
    struct event events[32];
};
static struct mt76_calibration_io make_io(struct fixture *);
static bool authorize(void *p) {
    struct fixture *f = p;
    ++f->auths;
    if (f->auths == f->cancel) f->cancelled = true;
    if (f->reenter) {
        struct mt76_calibration copy = f->state;
        struct mt76_calibration_io io = make_io(f);
        assert(mt76_calibration_run(&f->state, &io, 1, 20) == MT76_CAL_REFUSED);
        assert(!memcmp(&copy, &f->state, sizeof(copy)));
    }
    return !f->cancelled;
}
static bool event(struct fixture *f, char kind, uint32_t a, uint32_t v) {
    assert(f->calls < 32);
    if (kind != 's') assert(!f->failed && !f->cancelled);
    f->events[f->calls++] = (struct event){kind, a, v};
    if (f->calls == f->fail) { f->failed = true; return false; }
    return true;
}
static bool read_word(void *p, uint32_t a, uint32_t *v) {
    struct fixture *f = p;
    bool ok = event(f, 'r', a, 0);
    switch (a) {
    case 0x1004: *v = 12; break;
    case 0x2000: *v = 0xf000f200; break;
    case 0x208: *v = 0x40000050; break;
    case 0x13b0: *v = f->alc; break;
    case 0x2124: *v = f->ibi; break;
    default: assert(0);
    }
    if (f->calls == f->mismatch) { *v ^= 1; f->failed = true; }
    if (f->calls == f->all_ones) { *v = UINT32_MAX; f->failed = true; }
    return ok;
}
static bool write_word(void *p, uint32_t a, uint32_t v) {
    struct fixture *f = p;
    bool ok = event(f, 'w', a, v);
    /* Even a reported failed write changes the device in this fixture. */
    if (a == 0x13b0) f->alc = v;
    else { assert(a == 0x2124); f->ibi = v; }
    return ok;
}
static bool command(void *p, enum mt76_calibration_command c, uint32_t v) {
    return event(p, 'c', c, v);
}
static bool delay(void *p, unsigned us) { return event(p, 'd', 0, us); }
static bool restore(void *p, uint32_t a, uint32_t saved, uint32_t *observed) {
    struct fixture *f = p;
    bool ok = event(f, 's', a, saved);
    assert((a == 0x13b0 && saved == 0x2f2f) ||
           (a == 0x2124 && saved == 0x12345678));
    if (a == 0x13b0) f->alc = saved;
    else f->ibi = saved;
    *observed = saved;
    if (f->restore_fail == a) { *observed ^= 1; return true; }
    return ok;
}
static struct mt76_calibration_io make_io(struct fixture *f) {
    return (struct mt76_calibration_io){f, authorize, read_word, write_word,
                                      command, delay, restore};
}
static struct fixture fresh(void) {
    return (struct fixture){.alc = 0x2f2f, .ibi = 0x12345678};
}
static enum mt76_calibration_status run(struct fixture *f) {
    struct mt76_calibration_io io = make_io(f);
    return mt76_calibration_run(&f->state, &io, 1, 20);
}
static void retired(struct fixture *f) {
    assert(f->state.attempted);
    assert((f->state.restore_attempted & f->state.modified_mask) == f->state.modified_mask);
    if (f->state.cleanup_complete) {
        assert(f->state.restored_mask == f->state.modified_mask);
        assert(f->alc == 0x2f2f && f->ibi == 0x12345678);
    }
    struct mt76_calibration copy = f->state;
    unsigned count = f->calls, auths = f->auths;
    assert(run(f) == MT76_CAL_REFUSED);
    assert(count == f->calls && auths == f->auths);
    assert(!memcmp(&copy, &f->state, sizeof(copy)));
}
int main(void) {
    struct fixture f = fresh(); f.reenter = true;
    assert(run(&f) == MT76_CAL_OK);
    assert(f.state.complete && f.state.cleanup_complete);
    assert(f.state.saved_mask == 3 && f.state.modified_mask == 3 && f.state.restored_mask == 3);
    assert(f.state.reads == 13 && f.state.writes == 2 &&
           f.state.commands == 3 && f.state.waits == 2);
    static const struct event expected[] = {
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0},
        {'r',0x13b0,0},{'r',0x2124,0},
        {'w',0x13b0,0},{'r',0x13b0,0},{'d',0,500},
        {'w',0x2124,0xffffff7e},{'r',0x2124,0},
        {'c',MT76_CAL_FULL,0x600},{'c',MT76_CAL_LC,0},{'d',0,15000},
        {'s',0x2124,0x12345678},{'s',0x13b0,0x2f2f},
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0},
        {'c',MT76_CAL_RXDCOC,1},
        {'r',0x1004,0},{'r',0x2000,0},{'r',0x208,0}
    };
    assert(f.calls == sizeof(expected)/sizeof(*expected) && f.auths == 21);
    for (unsigned i = 0; i < f.calls; ++i) {
        assert(f.events[i].kind == expected[i].kind);
        assert(f.events[i].address == expected[i].address && f.events[i].value == expected[i].value);
    }
    retired(&f);
    for (unsigned n = 1; n <= 22; ++n) {
        f = fresh(); f.fail = n;
        assert(run(&f) != MT76_CAL_OK && !f.state.complete);
        retired(&f);
    }
    for (unsigned n = 1; n <= 21; ++n) {
        f = fresh(); f.cancel = n;
        assert(run(&f) == MT76_CAL_REFUSED && !f.state.complete);
        retired(&f);
    }
    for (unsigned i = 0; i < sizeof(expected)/sizeof(*expected); ++i) {
        if (expected[i].kind != 'r') continue;
        f = fresh(); f.all_ones = i + 1;
        assert(run(&f) == MT76_CAL_IO && !f.state.complete); retired(&f);
        /* Saved words are observations, so arbitrary non-all-ones changes are
         * valid there. Guard and temporary readback mismatches must refuse. */
        if (i == 3 || i == 4) continue;
        f = fresh(); f.mismatch = i + 1;
        assert(run(&f) != MT76_CAL_OK && !f.state.complete); retired(&f);
    }
    for (unsigned i = 0; i < 2; ++i) {
        f = fresh(); f.restore_fail = i ? 0x2124 : 0x13b0;
        assert(run(&f) == MT76_CAL_RESTORE && !f.state.complete && !f.state.cleanup_complete);
        assert(f.state.restore_attempted == 3 && f.state.commands == 2);
        retired(&f);
    }
    f = fresh(); struct mt76_calibration_io io = make_io(&f);
    assert(mt76_calibration_run(NULL,&io,1,20) == MT76_CAL_REFUSED);
    assert(mt76_calibration_run(&f.state,NULL,1,20) == MT76_CAL_REFUSED);
    for (unsigned ch = 0; ch < 256; ++ch) if (ch != 1)
        assert(mt76_calibration_run(&f.state,&io,ch,20) == MT76_CAL_REFUSED);
    assert(mt76_calibration_run(&f.state,&io,1,40) == MT76_CAL_REFUSED);
    struct mt76_calibration_io bad = io;
    bad.authorize = NULL; assert(mt76_calibration_run(&f.state,&bad,1,20) == MT76_CAL_REFUSED);
    bad=io;bad.read=NULL;assert(mt76_calibration_run(&f.state,&bad,1,20)==MT76_CAL_REFUSED);
    bad=io;bad.write=NULL;assert(mt76_calibration_run(&f.state,&bad,1,20)==MT76_CAL_REFUSED);
    bad=io;bad.command=NULL;assert(mt76_calibration_run(&f.state,&bad,1,20)==MT76_CAL_REFUSED);
    bad=io;bad.delay=NULL;assert(mt76_calibration_run(&f.state,&bad,1,20)==MT76_CAL_REFUSED);
    bad=io;bad.restore=NULL;assert(mt76_calibration_run(&f.state,&bad,1,20)==MT76_CAL_REFUSED);
    assert(!f.calls && !f.auths && !f.state.attempted);
    puts("mt76-calibration: PASS ordered commands/waits, fresh snapshots, verified reverse restoration, every I/O failure/cancellation, all-ones, reentry and no replay; synthetic only");
}
