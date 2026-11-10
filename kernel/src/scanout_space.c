#include <srvros/scanout_space.h>

static bool range_valid(struct scanout_space_range r, uint32_t pages) {
    return r.pages && r.first < pages && r.pages <= pages - r.first;
}
static bool ranges_valid(const struct scanout_space_range *r, uint32_t count, uint32_t pages) {
    if (count > SCANOUT_SPACE_RANGES || (count && !r)) return false;
    for (uint32_t i = 0; i < count; i++) if (!range_valid(r[i], pages)) return false;
    return true;
}
static bool live(const struct scanout_space *s) {
    return s && s->object==s && s->epoch && s->owner && s->pages && s->pages <= SCANOUT_SPACE_PAGES &&
           (s->state == SPACE_ACTIVE || s->state == SPACE_CLOSING || s->state == SPACE_FAULT);
}
static void mark(struct scanout_space *s, uint32_t first, uint32_t pages, bool free) {
    for (uint32_t p = first; p < first + pages; p++) {
        uint64_t mask = 1ull << (p % 64);
        if (free) s->available[p / 64] |= mask;
        else s->available[p / 64] &= ~mask;
    }
}
static void finish_close(struct scanout_space *s) {
    if (s->state != SPACE_CLOSING) return;
    for (uint32_t i = 0; i < SCANOUT_SPACE_LEASES; i++) if (s->leases[i].use) return;
    s->state = SPACE_CLOSED;
}
bool scanout_space_open(struct scanout_space *s, struct scanout_space_pool *pool, uint64_t owner,
                        uint32_t pages, const struct scanout_space_range *managed, uint32_t managed_count,
                        const struct scanout_space_range *protected, uint32_t protected_count) {
    if (!s || (s->object && s->object!=s) || !pool || !owner || !pages || pages > SCANOUT_SPACE_PAGES ||
        (s->state != SPACE_EMPTY && s->state != SPACE_CLOSED) ||
        pool->epoch < s->epoch || pool->epoch == UINT64_MAX ||
        !ranges_valid(managed, managed_count, pages) || !ranges_valid(protected, protected_count, pages)) return false;
    /* Save the small range lists before changing the object, including when
     * a caller stores its import inside the previous object. */
    struct scanout_space_range grants[SCANOUT_SPACE_RANGES], guards[SCANOUT_SPACE_RANGES];
    for (uint32_t i = 0; i < managed_count; i++) grants[i] = managed[i];
    for (uint32_t i = 0; i < protected_count; i++) guards[i] = protected[i];
    *s = (struct scanout_space){.object=s, .epoch = ++pool->epoch, .owner = owner, .state = SPACE_ACTIVE, .pages = pages};
    for (uint32_t i = 0; i < managed_count; i++) mark(s, grants[i].first, grants[i].pages, true);
    for (uint32_t i = 0; i < protected_count; i++) mark(s, guards[i].first, guards[i].pages, false);
    return true;
}
bool scanout_space_reserve(struct scanout_space *s, uint64_t owner, uint64_t epoch,
                           uint32_t pages, uint32_t align, struct scanout_space_token *out) {
    if (!live(s) || s->state != SPACE_ACTIVE || owner != s->owner || epoch != s->epoch || !out ||
        !pages || pages > s->pages || !align || align > s->pages || (align & (align - 1)) || s->serial == UINT64_MAX) return false;
    uint32_t slot = 0;
    while (slot < SCANOUT_SPACE_LEASES && s->leases[slot].use) slot++;
    if (slot == SCANOUT_SPACE_LEASES) return false;
    for (uint32_t first = 0; first <= s->pages - pages;) {
        uint32_t n = 0;
        while (n < pages && (s->available[(first + n) / 64] & (1ull << ((first + n) % 64)))) n++;
        if (n == pages) {
            mark(s, first, pages, false);
            s->leases[slot] = (struct scanout_space_entry){++s->serial, first, pages, SPACE_RESERVED};
            *out = (struct scanout_space_token){s, s->epoch, s->serial, slot, first, pages};
            return true;
        }
        /* Everything up to the first occupied page cannot fit this request. */
        first = (first + n + align) & ~(align - 1);
    }
    return false;
}
static struct scanout_space_entry *entry(struct scanout_space *s, const struct scanout_space_token *t) {
    if (!live(s) || !t || t->object != s || t->epoch != s->epoch || !t->serial || t->slot >= SCANOUT_SPACE_LEASES) return 0;
    struct scanout_space_entry *e = &s->leases[t->slot];
    if (!e->use || e->serial != t->serial || e->first != t->first || e->pages != t->pages) return 0;
    return e;
}
bool scanout_space_expose(struct scanout_space *s, const struct scanout_space_token *t) {
    struct scanout_space_entry *e = entry(s, t);
    if (!e || s->state != SPACE_ACTIVE || e->use != SPACE_RESERVED) return false;
    e->use = SPACE_EXPOSED;
    return true;
}
bool scanout_space_retire(struct scanout_space *s, const struct scanout_space_token *t, uint32_t proof) {
    struct scanout_space_entry *e = entry(s, t);
    if (!e || e->use != SPACE_EXPOSED || proof != SCANOUT_SPACE_RETIRE_ALL) return false;
    e->use = SPACE_RETIRED;
    return true;
}
bool scanout_space_quarantine(struct scanout_space *s, const struct scanout_space_token *t) {
    struct scanout_space_entry *e = entry(s, t);
    if (!e || e->use != SPACE_EXPOSED) return false;
    e->use = SPACE_QUARANTINED;
    s->state = SPACE_FAULT;
    return true;
}
bool scanout_space_release(struct scanout_space *s, const struct scanout_space_token *t) {
    struct scanout_space_entry *e = entry(s, t);
    if (!e || (e->use != SPACE_RESERVED && e->use != SPACE_RETIRED)) return false;
    mark(s, e->first, e->pages, true);
    e->use = SPACE_UNUSED;
    finish_close(s);
    return true;
}
bool scanout_space_close(struct scanout_space *s, uint64_t owner, uint64_t epoch) {
    if (!live(s) || s->state == SPACE_FAULT || owner != s->owner || epoch != s->epoch) return false;
    s->state = SPACE_CLOSING;
    finish_close(s);
    return true;
}
