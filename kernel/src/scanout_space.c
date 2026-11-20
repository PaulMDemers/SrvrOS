#include <srvros/scanout_space.h>

static bool range_valid(struct scanout_space_range r, uint32_t pages) {
    return r.pages && r.first < pages && r.pages <= pages - r.first;
}
static struct scanout_interval *intervals;

bool scanout_interval_claim(struct scanout_interval *node, uint64_t mmio, uint64_t aperture,
                            uint64_t bytes, uint64_t fence_end, struct scanout_space_range pages) {
    if (!node || !mmio || !aperture || !bytes || bytes > SCANOUT_SPACE_PAGES * 4096ull ||
        ((mmio | aperture | bytes | fence_end) & 4095) || aperture > UINT64_MAX - bytes ||
        !fence_end || fence_end > bytes || pages.first < fence_end / 4096 ||
        !range_valid(pages, (uint32_t)(bytes / 4096))) return false;
    for (struct scanout_interval *held = intervals; held; held = held->next) {
        if (held == node) return false;
        bool distinct_apertures = aperture + bytes <= held->aperture ||
                                  held->aperture + held->aperture_bytes <= aperture;
        if (held->mmio != mmio && distinct_apertures) continue;
        if (held->mmio != mmio || held->aperture != aperture || held->aperture_bytes != bytes ||
            held->fence_end != fence_end || (pages.first < held->pages.first + held->pages.pages &&
            held->pages.first < pages.first + pages.pages)) return false;
    }
    *node = (struct scanout_interval){mmio, aperture, bytes, fence_end, pages, intervals};
    intervals = node;
    return true;
}

void scanout_interval_release(struct scanout_interval *node) {
    struct scanout_interval **link = &intervals;
    while (*link && *link != node) link = &(*link)->next;
    if (*link) {
        *link = node->next;
        node->next = 0;
    }
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
bool scanout_space_open_transferred(struct scanout_space *s, struct scanout_space_pool *pool, uint64_t owner,
                                    uint32_t pages, struct scanout_space_range managed,
                                    struct scanout_space_range protected, const uint64_t *originals) {
    if (!s || !pool || !originals || !range_valid(managed, pages) || !range_valid(protected, pages) ||
        (managed.first < protected.first + protected.pages && protected.first < managed.first + managed.pages))
        return false;
    uintptr_t base = (uintptr_t)originals, space = (uintptr_t)s, epoch = (uintptr_t)pool;
    uint64_t bytes = (uint64_t)managed.pages * sizeof(*originals);
    if (base % _Alignof(uint64_t) || bytes > UINTPTR_MAX - base ||
        space > UINTPTR_MAX - sizeof(*s) || epoch > UINTPTR_MAX - sizeof(*pool) ||
        (base < space + sizeof(*s) && space < base + bytes) ||
        (base < epoch + sizeof(*pool) && epoch < base + bytes))
        return false;
    if (!scanout_space_open(s, pool, owner, pages, &managed, 1, &protected, 1)) return false;
    s->originals = originals;
    s->original_range = managed;
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
bool scanout_space_verify_original(struct scanout_space *s, const struct scanout_space_token *t,
                                   uint32_t offset, uint64_t value) {
    const struct scanout_space_entry *e = entry(s, t);
    if (!e || s->state == SPACE_FAULT || offset >= e->pages ||
        (e->use != SPACE_RESERVED && e->use != SPACE_EXPOSED))
        return false;
    if (!s->originals && !s->original_range.pages) return true;
    uint32_t page = e->first + offset;
    if (s->originals && page >= s->original_range.first && page - s->original_range.first < s->original_range.pages &&
        s->originals[page - s->original_range.first] == value)
        return true;
    s->state = SPACE_FAULT;
    return false;
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
