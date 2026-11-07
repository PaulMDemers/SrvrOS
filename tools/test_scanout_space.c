#include <srvros/scanout_space.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct scanout_space s, saved, other;
static struct scanout_space_pool pool;
static uint32_t random_state = 0x675218ab;
static uint32_t random32(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static void reset(void) {memset(&s, 0, sizeof s); pool.epoch = 0;}
static void open_all(unsigned pages) {
    struct scanout_space_range grant = {0, pages};
    assert(scanout_space_open(&s, &pool, 42, pages, &grant, 1, 0, 0));
}
static void unchanged(bool result) {assert(!result); assert(!memcmp(&s, &saved, sizeof s));}
static void no_authority_and_protected(void) {
    reset();
    assert(scanout_space_open(&s, &pool, 42, SCANOUT_SPACE_PAGES, 0, 0, 0, 0));
    struct scanout_space_token t;
    saved = s;
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 1, &t));
    assert(scanout_space_close(&s, 42, s.epoch) && s.state == SPACE_CLOSED);
    /* Exact observed native fence spans 34MiB, not just 1356 backing pages.
     * All-space authority below is synthetic; the native adapter grants none. */
    struct scanout_space_range all = {0, SCANOUT_SPACE_PAGES}, fence = {0, 34 * 256};
    assert(scanout_space_open(&s, &pool, 42, SCANOUT_SPACE_PAGES, &all, 1, &fence, 1));
    assert(scanout_space_reserve(&s, 42, s.epoch, 1356, 16, &t));
    assert(t.first == 34 * 256);
    struct scanout_space_token b;
    assert(scanout_space_reserve(&s, 42, s.epoch, 1356, 16, &b));
    assert(b.first == (t.first + t.pages + 15u) / 16u * 16u);
    assert(scanout_space_release(&s, &t));
    assert(scanout_space_release(&s, &b));
    for (unsigned p = 0; p < fence.pages; p++) assert(!(s.available[p / 64] & (1ull << (p % 64))));
    assert(scanout_space_close(&s, 42, s.epoch));
    /* Managed holes stay unavailable; touching/overlapping protected imports
     * cannot create free pages even when grants are duplicated or reordered. */
    struct scanout_space_range grants[] = {{4, 8}, {8, 8}, {4, 8}}, guards[] = {{6, 4}, {12, 4}};
    assert(scanout_space_open(&s, &pool, 42, 32, grants, 3, guards, 2));
    for (unsigned p = 0; p < 32; p++) assert(!!(s.available[p / 64] & (1ull << (p % 64))) == (p == 4 || p == 5 || p == 10 || p == 11));
}
static void exposure_and_quarantine(void) {
    reset(); open_all(64);
    struct scanout_space_token t, b, c;
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &t));
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &b));
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &c));
    assert(scanout_space_expose(&s, &t));
    saved = s;
    unchanged(scanout_space_release(&s, &t));
    unchanged(scanout_space_expose(&s, &t));
    for (unsigned proof = 0; proof < 16; proof++) if (proof != SCANOUT_SPACE_RETIRE_ALL) unchanged(scanout_space_retire(&s, &t, proof));
    assert(scanout_space_expose(&s, &b));
    assert(scanout_space_quarantine(&s, &b) && s.state == SPACE_FAULT);
    saved = s;
    unchanged(scanout_space_retire(&s, &b, SCANOUT_SPACE_RETIRE_ALL));
    unchanged(scanout_space_release(&s, &b));
    unchanged(scanout_space_close(&s, 42, s.epoch));
    struct scanout_space_token out;
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 1, &out));
    assert(scanout_space_retire(&s, &t, SCANOUT_SPACE_RETIRE_ALL));
    assert(scanout_space_release(&s, &t)); /* Independently retired other lease. */
    assert(scanout_space_release(&s, &c)); /* Never exposed; safe cancellation. */
    assert(s.state == SPACE_FAULT && !(s.available[b.first / 64] & (1ull << (b.first % 64))));
    saved = s;
    struct scanout_space_range all = {0, 64};
    unchanged(scanout_space_open(&s, &pool, 42, 64, &all, 1, 0, 0));
}
static void close_and_reuse(void) {
    reset(); open_all(64);
    struct scanout_space_token t, b;
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &t));
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &b));
    assert(scanout_space_expose(&s, &t));
    uint64_t epoch = s.epoch;
    assert(scanout_space_close(&s, 42, epoch) && s.state == SPACE_CLOSING);
    saved = s;
    unchanged(scanout_space_expose(&s, &b));
    unchanged(scanout_space_release(&s, &t));
    assert(scanout_space_release(&s, &b) && s.state == SPACE_CLOSING);
    assert(scanout_space_retire(&s, &t, SCANOUT_SPACE_RETIRE_ALL));
    assert(scanout_space_release(&s, &t) && s.state == SPACE_CLOSED);
    open_all(64); assert(s.epoch > epoch);
    struct scanout_space_token fresh;
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &fresh));
    saved = s;
    unchanged(scanout_space_release(&s, &t));
    unchanged(scanout_space_expose(&s, &t));
    unchanged(scanout_space_retire(&s, &t, SCANOUT_SPACE_RETIRE_ALL));
    unchanged(scanout_space_quarantine(&s, &t));
    unchanged(scanout_space_close(&s, 42, epoch));
    assert(scanout_space_release(&s, &fresh));
    assert(scanout_space_reserve(&s, 42, s.epoch, 8, 1, &b));
    assert(b.slot == fresh.slot && b.first == fresh.first && b.serial > fresh.serial);
    saved = s;
    unchanged(scanout_space_release(&s, &fresh));
    struct scanout_space_token bad = b; bad.object = &other;
    unchanged(scanout_space_release(&s, &bad));
    bad = b; bad.first++; unchanged(scanout_space_release(&s, &bad));
    bad = b; bad.pages++; unchanged(scanout_space_expose(&s, &bad));
    bad = b; bad.slot = SCANOUT_SPACE_LEASES; unchanged(scanout_space_release(&s, &bad));
}
static void limits(void) {
    reset();
    struct scanout_space_range all = {0, 64}, bad = {UINT32_MAX, 8};
    saved = s;
    unchanged(scanout_space_open(&s, &pool, 0, 64, &all, 1, 0, 0));
    unchanged(scanout_space_open(&s, &pool, 42, 0, &all, 1, 0, 0));
    unchanged(scanout_space_open(&s, &pool, 42, SCANOUT_SPACE_PAGES + 1, &all, 1, 0, 0));
    unchanged(scanout_space_open(&s, &pool, 42, 64, &bad, 1, 0, 0));
    unchanged(scanout_space_open(&s, &pool, 42, 64, &all, 1, &bad, 1));
    unchanged(scanout_space_open(&s, &pool, 42, 64, 0, 1, 0, 0));
    unchanged(scanout_space_open(&s, &pool, 42, 64, &all, SCANOUT_SPACE_RANGES + 1, 0, 0));
    pool.epoch = UINT64_MAX;
    unchanged(scanout_space_open(&s, &pool, 42, 64, &all, 1, 0, 0));
    assert(pool.epoch == UINT64_MAX);
    pool.epoch = 0; open_all(64);
    struct scanout_space_token t;
    saved = s;
    unchanged(scanout_space_reserve(&s, 41, s.epoch, 1, 1, &t));
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 0, 1, &t));
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 65, 1, &t));
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 0, &t));
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 3, &t));
    unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 128, &t));
    for (unsigned i = 0; i < SCANOUT_SPACE_LEASES; i++) assert(scanout_space_reserve(&s, 42, s.epoch, 1, 1, &t));
    saved = s; unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 1, &t));
    reset(); open_all(64); s.serial = UINT64_MAX;
    saved = s; unchanged(scanout_space_reserve(&s, 42, s.epoch, 1, 1, &t));
}
static void oracle(void) {
    enum {PAGES = 257, OPERATIONS = 50000};
    reset();
    bool free[PAGES];
    struct scanout_space_range grants[SCANOUT_SPACE_RANGES], guards[SCANOUT_SPACE_RANGES];
    for (unsigned i = 0; i < SCANOUT_SPACE_RANGES; i++) {
        unsigned a = random32() % PAGES, b = random32() % PAGES;
        grants[i] = (struct scanout_space_range){a, 1 + random32() % (PAGES - a)};
        guards[i] = (struct scanout_space_range){b, 1 + random32() % ((PAGES - b < 4) ? PAGES - b : 4)};
    }
    assert(scanout_space_open(&s, &pool, 42, PAGES, grants, SCANOUT_SPACE_RANGES, guards, 8));
    for (unsigned p = 0; p < PAGES; p++) {
        free[p] = false;
        for (unsigned i = 0; i < SCANOUT_SPACE_RANGES; i++) if (p >= grants[i].first && p - grants[i].first < grants[i].pages) free[p] = true;
        for (unsigned i = 0; i < 8; i++) if (p >= guards[i].first && p - guards[i].first < guards[i].pages) free[p] = false;
    }
    struct scanout_space_token tokens[SCANOUT_SPACE_LEASES];
    bool used[SCANOUT_SPACE_LEASES] = {false};
    bool starts[PAGES] = {false};
    unsigned allocations = 0, releases = 0, refusals = 0, distinct = 0;
    for (unsigned n = 0; n < OPERATIONS; n++) {
        unsigned slot = random32() % SCANOUT_SPACE_LEASES;
        if (used[slot] && random32() % 2) {
            assert(scanout_space_release(&s, &tokens[slot]));
            for (unsigned p = tokens[slot].first; p < tokens[slot].first + tokens[slot].pages; p++) free[p] = true;
            used[slot] = false;
            releases++;
        } else if (!used[slot]) {
            unsigned pages = 1 + random32() % 40, align = 1u << (random32() % 6), expected = PAGES;
            for (unsigned p = 0; p + pages <= PAGES; p++) {
                if (p % align) continue;
                bool fit = true;
                for (unsigned q = p; q < p + pages; q++) if (!free[q]) fit = false;
                if (fit) {expected = p; break;}
            }
            assert(scanout_space_reserve(&s, 42, s.epoch, pages, align, &tokens[slot]) == (expected < PAGES));
            if (expected < PAGES) {
                assert(tokens[slot].first == expected); used[slot] = true;
                allocations++;
                if (!starts[expected]) {starts[expected] = true; distinct++;}
                for (unsigned p = expected; p < expected + pages; p++) free[p] = false;
            } else refusals++;
        }
        for (unsigned p = 0; p < PAGES; p++) assert(!!(s.available[p / 64] & (1ull << (p % 64))) == free[p]);
    }
    assert(allocations > 1000 && releases > 1000 && refusals > 1000 && distinct > 32);
    printf("scanout-space oracle: allocations=%u releases=%u refusals=%u distinct_starts=%u\n", allocations, releases, refusals, distinct);
}
int main(void) {
    no_authority_and_protected(); exposure_and_quarantine(); close_and_reuse(); limits(); oracle();
    puts("scanout-space: PASS protected full fence, unavailable gaps, epoch/serial reuse, exposure retirement, sticky quarantine and 50000-operation allocation oracle");
}
