#include <srvros/intel_bdw_boot_drain.h>
#include <srvros/intel_bdw_traffic.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint64_t mmio = 0xc0000000, aperture = 0xb0000000, bytes = 0x10000000;
static const uint64_t fence = 0x02200000, original = 0x8f005001;
enum { SLOT = 12292, TICKET = 127 };
static struct bdw_boot_drain trial;

struct fixture {
    const char *mode;
    uint64_t now, active;
    unsigned requests, releases, waits, return_waits;
    bool parent_lost;
};

static bool is(const struct fixture *f, const char *mode)
{
    return !strcmp(f->mode, mode);
}

static uint64_t clock_now(void *opaque)
{
    struct fixture *f = opaque;
    if (f->active && is(f, "expired")) f->now += BDW_TRAFFIC_BLOCK_US;
    if (f->active && trial.polls == 2 && is(f, "late-parent-loss")) f->parent_lost = true;
    return ++f->now;
}

static bool parent(void *opaque, const struct bdw_boot_page_binding *b)
{
    struct fixture *f = opaque;
    assert(b->driver == f && b->epoch == 3 && b->root == 0x4000 && b->index == SLOT);
    struct scanout_interval conflict = {0};
    assert(!scanout_interval_claim(&conflict, mmio, aperture, bytes, fence,
                                    (struct scanout_space_range){SLOT, 1}));
    assert(!display_access_release(&b->display, true));
    return !f->parent_lost && !is(f, "no-parent");
}

static uint64_t held(void *opaque)
{
    struct fixture *f = opaque;
    return f->active && is(f, "lost-ticket") ? f->active + 1 : f->active;
}

static void pause_poll(void *opaque)
{
    struct fixture *f = opaque;
    f->now += is(f, "separate-windows") ? 6000 : 100;
    if (f->active) ++f->waits;
    else ++f->return_waits;
}

static bool read_register(void *opaque, uint32_t reg, uint32_t *word)
{
    struct fixture *f = opaque;
    bool closed = f->active || (f->releases && (is(f, "stuck-return") ||
                                               ((is(f, "delayed-return") || is(f, "separate-windows")) && !f->return_waits)));
    switch (reg) {
    case 0x40d0: *word = is(f, "air-capture") ? 0x7fff : (closed ? 0 : BDW_GAM_CLIENTS); break;
    case 0x8028: *word = closed ? 0 : 0x7f; break;
    case 0x8014: *word = closed && !is(f, "widi-open") ? 0 : 0x1f; break;
    case 0x8038: *word = closed && f->active && (is(f, "stale-l3") || (f->waits == 1 && !is(f, "no-l3") && !is(f, "widi-open"))); break;
    case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010:
        *word = closed ? 0x20 : 0x10;
        break;
    case 0x8040: case 0x8108: case 0x12870: case 0x12878: case 0x1c870: case 0x1c878:
        *word = reg == 0x8040;
        break;
    case 0x70180: *word = 0xd8000400; break;
    case 0x7019c: *word = 0; break;
    case 0x701ac: *word = f->active && is(f, "plane-drift") ? 4096 : 0; break;
    default: assert(false); return false;
    }
    return true;
}

static bool read_pte(void *opaque, uint32_t index, uint64_t *word)
{
    struct fixture *f = opaque;
    assert(index == SLOT);
    *word = f->active && is(f, "target-drift") ? original + 4096 : original;
    return true;
}

static enum intel_gt_write request(void *opaque, uint64_t *ticket)
{
    struct fixture *f = opaque;
    assert(!f->active && !f->requests++ && !*ticket);
    if (is(f, "no-effect")) return GT_WRITE_NONE;
    *ticket = f->active = TICKET;
    return is(f, "ambiguous-request") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static enum intel_gt_write release(void *opaque, uint64_t ticket)
{
    struct fixture *f = opaque;
    assert(ticket == TICKET && ticket == f->active && !f->releases++);
    f->active = 0;
    return is(f, "ambiguous-return") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

int test_bdw_boot_drain(const char *mode)
{
    struct fixture f = {.mode=mode + strlen("boot-drain-"), .now=100};
    struct display_access_token token;
    assert(display_access_acquire(123, &token));
    assert(scanout_interval_protect_boot_framebuffer(mmio, aperture, bytes, fence));
    const struct bdw_boot_page_binding binding = {.driver=&f, .epoch=3, .root=0x4000, .mmio=mmio,
        .aperture=aperture, .aperture_bytes=bytes, .fence_end=fence, .display=token, .index=SLOT,
        .plane=0xd8000400, .original=original};
    const struct bdw_boot_drain_io io = {&f, parent, clock_now, pause_poll, read_register, read_pte,
                                        request, held, release};
    bool ok = intel_bdw_boot_drain_run(&trial, &binding, &io);
    assert(!intel_bdw_boot_drain_run(&trial, &binding, &io));
    bool pinned = scanout_interval_held(&trial.interval, mmio, aperture, bytes, fence,
                                       (struct scanout_space_range){SLOT, 1});
    if (is(&f, "roundtrip") || is(&f, "delayed-return") || is(&f, "separate-windows")) {
        assert(ok && trial.state == BDW_DRAIN_RETURNED && trial.polls == 2 && f.requests == 1 && f.releases == 1);
        assert(trial.return_polls == (is(&f, "roundtrip") ? 1u : 2u));
        if (is(&f, "separate-windows")) assert(trial.last - trial.begin > BDW_TRAFFIC_BLOCK_US);
        assert(!f.active && !pinned && display_access_release(&token, true));
    } else if (is(&f, "air-capture") || is(&f, "no-parent") || is(&f, "no-effect")) {
        assert(!ok && trial.state == BDW_DRAIN_REFUSED && !trial.polls && !f.releases);
        assert(f.requests == (unsigned)is(&f, "no-effect"));
        assert(!f.active && !pinned && display_access_release(&token, true));
    } else {
        assert(!ok && trial.state == BDW_DRAIN_RETAINED && f.requests == 1 && pinned);
        assert(f.releases == (unsigned)(is(&f, "ambiguous-return") || is(&f, "stuck-return")));
        assert(!display_access_release(&token, true));
    }
    printf("PASS %s\n", mode);
    return 0;
}
