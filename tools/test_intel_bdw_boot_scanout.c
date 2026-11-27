#include <srvros/intel_bdw_boot_scanout.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct bdw_boot_page trial, other;
static struct bdw_boot_page_source source;
static const uint64_t original = 0x8f005001, replacement = 0x02000001;
static const uint64_t baseline[2] = {0x0102030405060708ULL,0x1112131415161718ULL};
static const uint32_t index = 12292, register_offset = 0x800000 + 8 * 12292;
static const uint64_t mmio = 0xc0000000, aperture_base = 0xb0000000, aperture_bytes = 0x10000000;
static const uint64_t fence_end = 0x02200000, root = 0x9000;

struct fixture {
    const char *mode;
    uint64_t clock, cached;
    volatile uint64_t pte;
    unsigned stores, invalidations, apertures;
};

static bool is(const struct fixture *f, const char *mode)
{
    return !strcmp(f->mode,mode);
}

static uint64_t clock_now(void *context)
{
    struct fixture *f = context;
    if (f->stores == 1 && is(f,"boot-page-clock-expired"))
        f->clock += SCANOUT_PUBLISH_DEADLINE_US;
    return ++f->clock;
}

static bool proof(void *context, const struct bdw_boot_page_source *s, struct bdw_boot_page_proof *p)
{
    struct fixture *f = context;
    assert(s == &source && s->consumer == &trial && !display_access_allowed());
    uint32_t rights = BDW_PAGE_REQUIRED;
    if (is(f,"boot-page-no-port")) rights &= ~BDW_PAGE_ENGINE_PORTS_SEALED;
    if (is(f,"boot-page-no-proxy")) rights &= ~BDW_PAGE_PROXY_EXCLUDED;
    if (is(f,"boot-page-no-firmware")) rights &= ~BDW_PAGE_FIRMWARE_EXCLUDED;
    if (is(f,"boot-page-no-prior")) rights &= ~BDW_PAGE_PRIOR_ACCESS_RETIRED;
    if (is(f,"boot-page-no-alias")) rights &= ~BDW_PAGE_ALIASES_PINNED;
    if (f->apertures == 3 && is(f,"boot-page-rollback-alias-lost")) rights &= ~BDW_PAGE_ALIASES_PINNED;
    if (is(f,"boot-page-cache-lost") && f->stores) rights &= ~BDW_PAGE_BACKING_VISIBLE;
    if (trial.apertures) {
        struct scanout_interval competing = {0};
        assert(!scanout_interval_claim(&competing,mmio,aperture_base,aperture_bytes,fence_end,
                                       (struct scanout_space_range){index,1}));
        assert(!display_access_release(&s->binding.display,true));
    }
    *p = (struct bdw_boot_page_proof){.source=s,.driver=f,.epoch=1,.root=root,.at=f->clock,
        .index=index,.rights=rights,.plane=0xd8000400,.programmed=0,.live=0};
    if (f->stores && is(f,"boot-page-front-drift")) p->live = 4096;
    if (f->stores && is(f,"boot-page-epoch-lost")) ++p->epoch;
    if (is(f,"boot-page-stale-proof")) --p->at;
    return true;
}

static bool read_pte(void *context, uint32_t offset, uint64_t *word)
{
    struct fixture *f = context;
    assert(offset == register_offset);
    *word = f->pte;
    return true;
}

static enum intel_gt_write exchange(void *context, uint32_t offset, uint64_t expected, uint64_t word)
{
    struct fixture *f = context;
    const struct bdw_boot_page_io io = trial.io;
    assert(offset == register_offset && f->stores < 2 && f->stores == f->invalidations);
    assert(!intel_bdw_boot_page_run(&other,&source,&io));
    assert(expected == (!f->stores ? original : replacement) && word == (!f->stores ? replacement : original));
    enum intel_gt_write result = intel_bdw_ggtt_exchange(&f->pte,expected,word);
    ++f->stores;
    return is(f,"boot-page-uncertain-store") ? GT_WRITE_UNCERTAIN : result;
}

static bool invalidate(void *context)
{
    struct fixture *f = context;
    assert(f->stores == f->invalidations + 1);
    ++f->invalidations;
    if ((f->stores == 1 && is(f,"boot-page-stale-forward")) ||
        (f->stores == 2 && is(f,"boot-page-stale-rollback"))) return true;
    f->cached = f->pte;
    return true;
}

static bool read_aperture(void *context, uint64_t physical, uint64_t words[2])
{
    struct fixture *f = context;
    assert(physical == aperture_base + 4096ull * index && f->stores == f->invalidations);
    assert(f->apertures++ < 3);
    const uint64_t *pixels = f->cached == replacement ? source.binding.marker : baseline;
    words[0] = pixels[0];
    words[1] = pixels[1];
    if (!f->stores && is(f,"boot-page-foreign-pte")) f->pte ^= 4096;
    return true;
}

int test_bdw_boot_page(const char *mode)
{
    struct fixture f = {.mode=mode,.clock=100,.pte=original,.cached=original};
    struct display_access_token token;
    assert(display_access_acquire(123,&token));
    assert(scanout_interval_protect_boot_framebuffer(mmio,aperture_base,aperture_bytes,fence_end));
    source = (struct bdw_boot_page_source){.object=&source,.binding={.driver=&f,.epoch=1,.root=root,
        .mmio=mmio,.aperture=aperture_base,.aperture_bytes=aperture_bytes,.fence_end=fence_end,.display=token,
        .index=index,.plane=0xd8000400,.original=original,.backing=replacement-1,
        .marker={0x9a8b7c6d5e4f3210ULL,0xfedcba9876543210ULL}}};
    const struct bdw_boot_page_io io = {&f,proof,clock_now,read_pte,exchange,invalidate,read_aperture};

    struct scanout_interval conflict = {0};
    if (is(&f,"boot-page-overlap"))
        assert(scanout_interval_import_firmware(&conflict,mmio,aperture_base,aperture_bytes,fence_end,
                                                 (struct scanout_space_range){index-1,3}));
    bool ok = intel_bdw_boot_page_run(&trial,&source,&io);
    assert(!intel_bdw_boot_page_run(&other,&source,&io));
    bool claimed = scanout_interval_held(&trial.interval,mmio,aperture_base,aperture_bytes,fence_end,
                                        (struct scanout_space_range){index,1});
    if (is(&f,"boot-page-roundtrip")) {
        assert(ok && trial.state == BDW_PAGE_RETURNED && !claimed && !source.consumer &&
               f.pte == original && f.cached == original && f.stores == 2 && f.invalidations == 2 && f.apertures == 3 &&
               trial.stores == 2 && trial.invalidations == 2 && trial.space.state == SPACE_CLOSED);
        assert(display_access_release(&token,true));
    } else if (is(&f,"boot-page-overlap") || is(&f,"boot-page-stale-proof") || !strncmp(mode,"boot-page-no-",13)) {
        assert(!ok && trial.state == BDW_PAGE_REFUSED && !claimed && !source.consumer &&
               f.pte == original && f.cached == original && !f.stores && !f.invalidations && !f.apertures);
        assert(display_access_release(&token,true));
    } else {
        assert(!ok && trial.state == BDW_PAGE_RETAINED && claimed && source.consumer == &trial);
        assert(!display_access_release(&token,true));
        if (is(&f,"boot-page-foreign-pte")) assert(!f.stores && f.pte != original);
        else if (is(&f,"boot-page-stale-rollback") || is(&f,"boot-page-rollback-alias-lost")) {
            uint64_t cached = is(&f,"boot-page-stale-rollback") ? replacement : original;
            assert(f.stores == 2 && f.invalidations == 2 && f.pte == original && f.cached == cached);
            assert(trial.publication.state == PUB_FAULT && trial.space.leases[trial.range.slot].use == SPACE_QUARANTINED);
            assert(!scanout_space_release(&trial.space,&trial.range));
        }
        else {
            assert(f.stores == 1 && f.pte == replacement && f.cached == original);
            assert(f.invalidations == (unsigned)is(&f,"boot-page-stale-forward"));
            if (is(&f,"boot-page-uncertain-store")) assert(!trial.stores && trial.publication.writes == 1);
        }
    }
    printf("PASS %s\n",mode);
    return 0;
}
