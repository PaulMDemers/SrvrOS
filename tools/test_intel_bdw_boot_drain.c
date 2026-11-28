#include <srvros/intel_bdw_boot_drain.h>
#include <srvros/intel_bdw_traffic.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint64_t mmio = 0xc0000000, aperture = 0xb0000000, bytes = 0x10000000;
static const uint64_t fence = 0x02200000, original = 0x8f005001;
static const uint64_t first_framebuffer = 0x8c001000;
enum { SLOT = 12292, DRIVER_SLOT = 16385, TICKET = 127 };
static struct bdw_boot_drain trial;
static struct bdw_boot_bind bind;
enum { SCRATCH = 0x2100000, PRIVATE = 0x2200000 };
static const uint64_t private_marker[] = {9,10};

struct fixture {
    const char *mode;
    uint64_t now, active;
    uint64_t pte, translation;
    unsigned index, requests, releases, waits, return_waits;
    unsigned stores, invalidates;
    struct scanout_interval range;
    struct bdw_boot_drain *drain;
    bool binding, occupied, inherited, no_store;
    bool parent_lost;
};

static bool is(const struct fixture *f, const char *mode)
{
    return !strcmp(f->mode, mode);
}

static uint64_t clock_now(void *opaque)
{
    struct fixture *f = opaque;
    if (f->active && is(f, "driver-frozen")) return f->drain->requested_at;
    if (f->active && is(f,"driver-range-drift")) f->range.pages.first = DRIVER_SLOT + 1;
    if (f->active && is(f, "expired")) f->now += BDW_TRAFFIC_BLOCK_US;
    if (f->active && f->drain->polls == 2 && is(f, "late-parent-loss")) f->parent_lost = true;
    return ++f->now;
}

static bool parent(void *opaque, const struct bdw_boot_page_binding *b)
{
    struct fixture *f = opaque;
    assert(b->driver == f && b->epoch == 3 && b->root == 0x4000 && b->index == f->index);
    struct scanout_interval conflict = {0};
    const struct scanout_space_range page = {f->index, 1};
    assert(!scanout_interval_claim(&conflict, mmio, aperture, bytes, fence,
                                    page));
    if (f->index == DRIVER_SLOT) {
        assert(f->drain->interval.lender == &f->range);
        scanout_interval_release(&f->range);
        assert(scanout_interval_held(&f->range,mmio,aperture,bytes,fence,f->range.pages));
        assert(!scanout_interval_borrow_driver(&conflict,&f->range,f,page));
    }
    if (f->occupied && !f->inherited) {
        assert(f->drain->interval.lender == &f->range);
        scanout_interval_release(&f->range);
        assert(scanout_interval_held(&f->range,mmio,aperture,bytes,fence,page));
        assert(!scanout_interval_borrow(&conflict,&f->range,mmio,aperture,bytes,fence,page));
    }
    assert(!display_access_release(&b->display, true));
    return !f->parent_lost && !is(f, "no-parent");
}

static uint64_t held(void *opaque)
{
    struct fixture *f = opaque;
    return f->active && (is(f,"lost-ticket") || (is(f,"driver-bind-lost") && f->stores == 2) ||
                         ((is(f,"occupied-lost") || is(f,"inherited-lost")) && f->stores == 1)) ?
           f->active + 1 : f->active;
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
    case 0x40d0:
        *word = is(f,"air-capture") || is(f,"occupied-air") || is(f,"inherited-air") ||
            is(f,"inherited-drain-air") ? 0x7fff : (closed ? 0 : BDW_GAM_CLIENTS);
        if (f->active && (is(f,"inherited-drain-reset") ||
            (f->drain->l3_ack && is(f,"inherited-drain-late-reset")))) *word |= 1u << 13;
        break;
    case 0x8028: *word = closed ? 0 : 0x7f; break;
    case 0x8014:
        *word = closed && !is(f,"widi-open") && !is(f,"occupied-widi") &&
            !is(f,"inherited-drain-widi") ? 0 : 0x1f;
        break;
    case 0x8038:
        *word = closed && f->active && (is(f,"stale-l3") ||
                (f->waits == 1 && !is(f,"no-l3") && !is(f,"driver-bind-no-l3") && !is(f,"widi-open") &&
                 !is(f,"occupied-no-l3") && !is(f,"occupied-widi") && !is(f,"inherited-no-l3") &&
                 !is(f,"inherited-drain-no-l3")));
        break;
    case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010:
        *word = closed ? (reg == 0x8010 && is(f,"inherited-no-engine") ? 0 : 0x20) : 0x10;
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
    assert(index == f->index);
    if (f->binding || f->inherited)
        *word = f->pte;
    else if (f->index == DRIVER_SLOT)
        *word = is(f, "driver-occupied") || (f->active && is(f, "driver-target-drift")) ? original : 0;
    else
        *word = f->active && is(f, "target-drift") ? original + 4096 : original;
    return true;
}

static enum intel_gt_write request(void *opaque, uint64_t *ticket)
{
    struct fixture *f = opaque;
    assert(!f->active && !f->requests++ && !*ticket);
    if (is(f,"no-effect") || is(f,"occupied-no-provider") ||
        is(f,"inherited-no-provider") || is(f,"inherited-drain-no-provider")) return GT_WRITE_NONE;
    *ticket = f->active = TICKET;
    return is(f, "ambiguous-request") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static enum intel_gt_write release(void *opaque, uint64_t ticket)
{
    struct fixture *f = opaque;
    assert(ticket == TICKET && ticket == f->active && !f->releases++);
    if (f->no_store) assert(f->stores == 0 && f->invalidates == 0);
    else if (f->occupied) assert(f->stores == 2 && f->invalidates == 2 && f->translation == f->drain->binding.original);
    else if (f->binding) assert(f->stores == 3 && f->invalidates == 3 && f->translation == (SCRATCH | 1));
    f->active = 0;
    return is(f, "ambiguous-return") ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static bool proof(void *opaque, const struct bdw_boot_page_source *source, struct bdw_boot_page_proof *out)
{
    struct fixture *f = opaque;
    assert(f->active == TICKET && f->drain->engine_acks == 31u && f->drain->l3_ack);
    *out = (struct bdw_boot_page_proof){.source=source,.driver=f,
        .driver_range=f->occupied && !f->inherited ? NULL : &f->range,.scratch=f->occupied ? 0 : SCRATCH,
        .firmware_range=f->occupied && !f->inherited ? &f->range : NULL,
        .inherited_backing=f->inherited ? first_framebuffer : 0,
        .epoch=3,.root=0x4000,.at=f->now,.index=f->index,.rights=BDW_PAGE_REQUIRED,.plane=0xd8000400};
    if (is(f,"occupied-no-firmware") || is(f,"inherited-no-firmware")) out->rights &= ~BDW_PAGE_FIRMWARE_EXCLUDED;
    if (is(f,"inherited-wrong-proof")) out->inherited_backing += 4096;
    return true;
}

static bool page_pte(void *opaque, uint32_t offset, uint64_t *word)
{
    struct fixture *f = opaque;
    assert(offset == 0x800000 + f->index * 8);
    return read_pte(opaque,f->index,word);
}

static enum intel_gt_write exchange(void *opaque, uint32_t offset, uint64_t before, uint64_t after)
{
    struct fixture *f = opaque;
    const uint64_t paths[] = {0,SCRATCH | 1,PRIVATE | 1,SCRATCH | 1};
    const uint64_t occupied[] = {f->drain->binding.original,PRIVATE | 1,f->drain->binding.original};
    const uint64_t *expected = f->occupied ? occupied : paths;
    assert(offset == 0x800000 + f->index * 8 && f->active == TICKET && f->stores < (f->occupied ? 2u : 3u) &&
           before == expected[f->stores] && after == expected[f->stores+1] && f->pte == before);
    f->pte = after;
    ++f->stores;
    return GT_WRITE_DONE;
}

static bool invalidate(void *opaque)
{
    struct fixture *f = opaque;
    assert(f->active == TICKET && ++f->invalidates == f->stores);
    if (!(f->stores == 3 && is(f,"driver-bind-stale")) &&
        !(f->stores == 2 && (is(f,"occupied-stale") || is(f,"inherited-stale"))))
        f->translation = f->pte;
    return true;
}

static bool aperture_read(void *opaque, uint64_t address, uint64_t words[2])
{
    struct fixture *f = opaque;
    assert(f->active == TICKET && address == aperture + 4096ull * f->index);
    bool baseline = f->translation == (f->occupied ? f->drain->binding.original : SCRATCH | 1);
    words[0] = baseline ? 7 : private_marker[0];
    words[1] = baseline ? 8 : private_marker[1];
    return true;
}

int test_bdw_boot_drain(const char *mode)
{
    struct fixture f = {.mode=mode + strlen("boot-drain-"), .now=100};
    f.inherited = !strncmp(f.mode,"inherited-",10);
    bool driver = f.inherited || !strncmp(f.mode,"driver-",7);
    f.occupied = f.inherited || !strncmp(f.mode,"occupied-",9);
    f.no_store = is(&f,"occupied-drain") || is(&f,"occupied-air") || !strncmp(f.mode,"inherited-drain",15);
    f.binding = (f.occupied && !f.no_store) || !strncmp(f.mode,"driver-bind-",12);
    f.drain = f.binding ? &bind.hold : &trial;
    f.index = driver ? DRIVER_SLOT : SLOT;
    struct scanout_interval inherited = {0};
    struct display_access_token token;
    assert(display_access_acquire(123, &token));
    assert(scanout_interval_protect_boot_framebuffer(mmio, aperture, bytes, fence));
    if (f.occupied && !f.inherited) {
        const struct scanout_space_range page = {is(&f,"occupied-foreign") ? SLOT + 1 : SLOT,1};
        assert(scanout_interval_import_firmware(&f.range,mmio,aperture,bytes,fence,page));
        f.pte = f.translation = original;
    }
    if (driver) {
        assert(scanout_interval_import_firmware(&inherited,mmio,aperture,bytes,fence,
                                                (struct scanout_space_range){SLOT,1}));
        const struct scanout_space_range page = {DRIVER_SLOT,1};
        if (is(&f,"driver-firmware"))
            assert(scanout_interval_import_firmware(&f.range,mmio,aperture,bytes,fence,page));
        else if (!is(&f,"driver-unowned"))
            assert(scanout_interval_claim_driver(&f.range,&f,mmio,aperture,bytes,fence,page));
    }
    if (f.inherited) f.pte = f.translation = first_framebuffer | 1;
    const struct bdw_boot_page_binding binding = {.driver=&f, .epoch=3, .root=0x4000, .mmio=mmio,
        .aperture=aperture, .aperture_bytes=bytes, .fence_end=fence, .display=token, .index=f.index,
        .plane=0xd8000400, .original=f.inherited ? f.pte : driver ? 0 : original,.backing=SCRATCH,.marker={7,8}};
    const struct bdw_boot_drain_io io = {&f, parent, clock_now, pause_poll, read_register, read_pte,
                                        request, held, release};
    bool ok;
    if (f.no_store && f.inherited) {
        bool passed = is(&f,"inherited-drain");
        bool wrong_backing = is(&f,"inherited-drain-wrong-backing");
        bool air_capture = is(&f,"inherited-drain-air");
        bool no_provider = is(&f,"inherited-drain-no-provider");
        bool retained = !passed && !wrong_backing && !air_capture && !no_provider;
        uint64_t first = first_framebuffer + (wrong_backing ? 4096 : 0);
        assert(intel_bdw_boot_drain_run_inherited(&trial,&binding,&f.range,first,&io) == passed);
        assert(trial.state == (passed ? BDW_DRAIN_RETURNED : wrong_backing ? BDW_DRAIN_NEW :
                               retained ? BDW_DRAIN_RETAINED : BDW_DRAIN_REFUSED));
        assert(f.requests == (unsigned)(!wrong_backing && !air_capture) && f.releases == (unsigned)passed);
        assert(!f.stores && !f.invalidates && f.pte == binding.original && f.translation == binding.original);
        assert(scanout_interval_held(&trial.interval,mmio,aperture,bytes,fence,f.range.pages) == retained);
        assert(scanout_interval_held(&f.range,mmio,aperture,bytes,fence,f.range.pages));
        assert(scanout_interval_held(&inherited,mmio,aperture,bytes,fence,inherited.pages));
        assert(display_access_release(&token,true) == !retained);
        if (passed || is(&f,"inherited-drain-late-reset")) assert(trial.engine_acks == 31 && trial.l3_ack);
        printf("PASS %s\n",mode);
        return 0;
    }
    if (f.no_store) {
        bool passed = is(&f,"occupied-drain");
        assert(intel_bdw_boot_drain_run_occupied(&trial,&binding,&f.range,&io) == passed);
        assert(trial.state == (passed ? BDW_DRAIN_RETURNED : BDW_DRAIN_REFUSED));
        assert(f.requests == (unsigned)passed && f.releases == (unsigned)passed && !f.stores && !f.invalidates);
        assert(!scanout_interval_held(&trial.interval,mmio,aperture,bytes,fence,f.range.pages));
        assert(scanout_interval_held(&f.range,mmio,aperture,bytes,fence,f.range.pages));
        assert(display_access_release(&token,true));
        printf("PASS %s\n",mode);
        return 0;
    }
    if (f.binding) {
        const struct bdw_boot_page_io page = {&f,proof,clock_now,page_pte,exchange,invalidate,aperture_read};
        if (f.inherited) {
            struct bdw_boot_page_binding occupied = binding;
            occupied.backing = PRIVATE;
            occupied.marker[0] = private_marker[0];
            occupied.marker[1] = private_marker[1];
            uint64_t first = first_framebuffer + (is(&f,"inherited-wrong-backing") ? 4096 : 0);
            ok = intel_bdw_boot_bind_inherited(&bind,&occupied,&f.range,first,&io,&page);
            assert(!intel_bdw_boot_bind_inherited(&bind,&occupied,&f.range,first,&io,&page));
            bool passed = is(&f,"inherited-roundtrip");
            bool wrong_backing = is(&f,"inherited-wrong-backing");
            bool air = is(&f,"inherited-air");
            bool retained = !passed && !wrong_backing && !air && !is(&f,"inherited-no-provider");
            bool stale = is(&f,"inherited-stale"), lost = is(&f,"inherited-lost");
            assert(ok == passed && !bind.seed.object && !f.range.sealed && f.releases == (unsigned)passed);
            assert(f.requests == (unsigned)(!wrong_backing && !air));
            assert(bind.hold.state == (passed ? BDW_DRAIN_RETURNED : retained ? BDW_DRAIN_RETAINED :
                                       wrong_backing ? BDW_DRAIN_NEW : BDW_DRAIN_REFUSED));
            assert(display_access_release(&token,true) == !retained);
            assert(scanout_interval_held(&f.range,mmio,aperture,bytes,fence,f.range.pages));
            assert(scanout_interval_held(&inherited,mmio,aperture,bytes,fence,inherited.pages));
            assert(scanout_interval_held(&bind.hold.interval,mmio,aperture,bytes,fence,f.range.pages) == retained);
            assert(f.stores == (passed || stale ? 2u : (unsigned)lost));
            assert(f.pte == (lost ? PRIVATE | 1 : binding.original));
            if (passed) assert(f.translation == binding.original && bind.page.state == BDW_PAGE_RETURNED);
            printf("PASS %s\n",mode);
            return 0;
        }
        if (f.occupied) {
            struct bdw_boot_page_binding occupied = binding;
            occupied.backing = PRIVATE;
            occupied.marker[0] = private_marker[0];
            occupied.marker[1] = private_marker[1];
            ok = intel_bdw_boot_bind_occupied(&bind,&occupied,&f.range,&io,&page);
            assert(!intel_bdw_boot_bind_occupied(&bind,&occupied,&f.range,&io,&page));
            bool passed = is(&f,"occupied-roundtrip");
            bool refused = is(&f,"occupied-foreign") || is(&f,"occupied-no-provider");
            assert(ok == passed && !bind.seed.object && f.requests == (unsigned)!is(&f,"occupied-foreign"));
            assert(f.releases == (unsigned)passed && display_access_release(&token,true) == (passed || refused));
            assert(bind.hold.state == (passed ? BDW_DRAIN_RETURNED : refused ? BDW_DRAIN_REFUSED : BDW_DRAIN_RETAINED));
            assert(scanout_interval_held(&f.range,mmio,aperture,bytes,fence,f.range.pages));
            assert(scanout_interval_held(&bind.hold.interval,mmio,aperture,bytes,fence,
                                         (struct scanout_space_range){SLOT,1}) == (!passed && !refused));
            assert(f.stores == (passed || is(&f,"occupied-stale") ? 2u : (unsigned)is(&f,"occupied-lost")));
            if (passed) assert(f.pte == original && f.translation == original && bind.page.state == BDW_PAGE_RETURNED);
            printf("PASS %s\n",mode);
            return 0;
        }
        ok = intel_bdw_boot_bind(&bind,&binding,&f.range,PRIVATE,private_marker,&io,&page);
        assert(!intel_bdw_boot_bind(&bind,&binding,&f.range,PRIVATE,private_marker,&io,&page));
        bool missing_l3 = is(&f,"driver-bind-no-l3");
        assert(f.range.sealed == !missing_l3 && f.requests == 1);
        bool passed = is(&f,"driver-bind-roundtrip");
        assert(ok == passed && display_access_release(&token,true) == passed);
        assert(f.releases == (unsigned)passed && bind.hold.state == (passed ? BDW_DRAIN_RETURNED : BDW_DRAIN_RETAINED));
        assert(scanout_interval_held(&bind.hold.interval,mmio,aperture,bytes,fence,
                                     (struct scanout_space_range){DRIVER_SLOT,1}) == !passed);
        if (missing_l3)
            assert(!bind.page.object && !bind.seed.object && !bind.hold.l3_ack && !f.stores);
        else {
            assert(bind.seed.state == BDW_PAGE_SEEDED && bind.page.state == (passed ? BDW_PAGE_RETURNED : BDW_PAGE_RETAINED));
            assert(f.stores == (is(&f,"driver-bind-lost") ? 2u : 3u));
        }
        assert(scanout_interval_held(&inherited,mmio,aperture,bytes,fence,inherited.pages));
        printf("PASS %s\n",mode);
        return 0;
    }
    ok = driver ? intel_bdw_boot_drain_run_driver(&trial,&binding,&f.range,&io) : intel_bdw_boot_drain_run(&trial,&binding,&io);
    assert(!intel_bdw_boot_drain_run_driver(&trial,&binding,&f.range,&io) && !intel_bdw_boot_drain_run(&trial,&binding,&io));
    bool pinned = scanout_interval_held(&trial.interval, mmio, aperture, bytes, fence,
                                       (struct scanout_space_range){f.index, 1});
    bool driver_refused = is(&f,"driver-unowned") || is(&f,"driver-firmware") || is(&f,"driver-occupied");
    if (is(&f, "roundtrip") || is(&f, "driver-roundtrip") || is(&f, "delayed-return") || is(&f, "separate-windows")) {
        assert(ok && trial.state == BDW_DRAIN_RETURNED && trial.polls == 2 && f.requests == 1 && f.releases == 1);
        assert(trial.engine_acks == 31u && trial.l3_ack && trial.requested_at < trial.release_begin);
        assert(trial.return_polls == (is(&f, "roundtrip") || driver ? 1u : 2u));
        if (is(&f, "separate-windows")) assert(trial.last - trial.begin > BDW_TRAFFIC_BLOCK_US);
        assert(!f.active && !pinned && display_access_release(&token, true));
    } else if (driver_refused || is(&f, "air-capture") || is(&f, "no-parent") || is(&f, "no-effect")) {
        assert(!ok && trial.state == BDW_DRAIN_REFUSED && !trial.polls && !f.releases);
        assert(f.requests == (unsigned)is(&f, "no-effect"));
        assert(!f.active && !pinned && display_access_release(&token, true));
    } else {
        assert(!ok && trial.state == BDW_DRAIN_RETAINED && f.requests == 1 && pinned);
        assert(f.releases == (unsigned)(is(&f, "ambiguous-return") || is(&f, "stuck-return")));
        assert(!display_access_release(&token, true));
    }
    if (driver)
        assert(!f.range.sealed && scanout_interval_held(&inherited,mmio,aperture,bytes,fence,inherited.pages));
    printf("PASS %s\n", mode);
    return 0;
}
