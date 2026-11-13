#ifndef SRVROS_SCANOUT_MEMORY_H
#define SRVROS_SCANOUT_MEMORY_H
#include <srvros/scanout_publish.h>
/* CPU0 serialized trusted-adapter mechanism. This does not grant a GGTT range,
 * encode a hardware PTE, infer cache/DMA authority or implement an allocator.
 * Allocator/provider contexts must outlive the original object, including faults.
 * Allocate objects in owned storage, not on a small kernel stack. */
enum scanout_memory_state {MEM_EMPTY,MEM_ALLOCATING,MEM_RAM,MEM_PREPARED,MEM_GPU,MEM_RELEASED,MEM_FAULT};
enum scanout_memory_error {MEM_OK,MEM_INPUT,MEM_ALLOC,MEM_BACKING,MEM_MAPPING,MEM_RELEASE};
struct scanout_memory_layout {uint64_t visible_bytes,bytes;uint32_t width,height,pitch,rows,pages,tiled;};
/* Storage arithmetic only. X-tile rows are padded to8 with512-byte pitch.
 * A native adapter must separately admit actual tiling/swizzle/cache policy. */
bool scanout_memory_layout(uint32_t width,uint32_t height,uint32_t pitch,bool tiled,struct scanout_memory_layout *);
struct scanout_memory_io {
    void *ctx;
    /* Zero guarantees no allocation. Nonzero is a uniquely owned pinned span. */
    uint64_t (*allocate)(void *,uint32_t pages);
    bool (*validate)(void *,uint64_t physical,uint32_t pages);
    /* Called at most once. False may have partially released: never retry. */
    bool (*release)(void *,uint64_t physical,uint32_t pages);
};
struct scanout_memory;
struct scanout_memory_pool {uint64_t epoch;};
struct scanout_memory_token {const struct scanout_memory *object;uint64_t epoch,owner;};
struct scanout_memory_cpu {struct scanout_memory_token memory;uint64_t serial,physical,bytes;};
struct scanout_memory_hold;
/* CPU borrow bytes covers every allocated page, including row/page padding.
 * Initialize that complete span before exposing it through a GPU mapping. */
struct scanout_memory_binding {
    void *ctx;
    /* Trusted format/DMA-policy adapter; called for each exact owned page. */
    bool (*encode)(void *,uint64_t physical,uint64_t *entry);
    struct scanout_publish_io publication;
};
struct scanout_memory {
    const struct scanout_memory *object;
    uint64_t epoch,owner,physical,serial,cpu_serial;
    struct scanout_memory_layout layout;
    struct scanout_memory_io allocator;
    struct scanout_space *space;
    struct scanout_space_token range;
    struct scanout_publication publication;
    struct scanout_publish_token publication_token;
    uint64_t entries[SCANOUT_PUBLISH_PAGES];
    uint32_t state,error,allocations,release_attempts,releases;
    struct scanout_memory_hold *dependent;
    const struct scanout_memory_hold *registered;
    uint64_t hold_serial;
    bool busy,closing;
};
/* Original-address, one-use private RAM hold. This never supplies GPU address,
 * cache/DMA permission or a hardware completion. Keep storage through faults.
 * A pending hold blocks CPU access, mapping and release, including close. */
struct scanout_memory_hold {
    const struct scanout_memory_hold *object;
    struct scanout_memory_token memory;
    struct scanout_memory_io allocator;
    struct scanout_memory_layout layout;
    uint64_t physical,bytes,serial;
    const void *dependent,*registered;
    /* Trusted adapter's enclosing root pin. Set before publication to that
     * root; clear only after its actual clean retirement, never on timeout. */
    const void *lifetime;
    bool active,busy,fault;
};
bool scanout_memory_hold_begin(struct scanout_memory *,const struct scanout_memory_token *,struct scanout_memory_hold *);
/* Pure original-token/backing identity check; no fresh allocator validation. */
bool scanout_memory_hold_live(const struct scanout_memory_hold *);
/* Validates allocator backing before returning the registered original hold.
 * Failed validation retains it; no forced release or repeated free. */
bool scanout_memory_hold_end(struct scanout_memory_hold *);
/* Zero-init, original-address sealed; reuse only after complete release, with a
 * fresh pool epoch. Outputs/pool must not alias the object. */
bool scanout_memory_create(struct scanout_memory *,struct scanout_memory_pool *,uint64_t owner,
    const struct scanout_memory_layout *,uint64_t physical_limit,struct scanout_memory_io,struct scanout_memory_token *);
bool scanout_memory_cpu_begin(struct scanout_memory *,const struct scanout_memory_token *,struct scanout_memory_cpu *);
bool scanout_memory_cpu_end(struct scanout_memory *,const struct scanout_memory_cpu *);
/* Requires an ALREADY admitted original address-space owner and its live guard.
 * No absent/populated PTE or successful reset can supply that authority. */
bool scanout_memory_prepare(struct scanout_memory *,const struct scanout_memory_token *,struct scanout_space *,
    struct scanout_publish_pool *,uint32_t align_pages,struct scanout_memory_binding);
bool scanout_memory_apply(struct scanout_memory *,const struct scanout_memory_token *);
/* Blocks new CPU borrows and mapping. Pending readers return false with all RAM
 * and address leases retained. Uncertain stores/releases latch a terminal fault.
 * After restore/translation/reader retirement, releases address then RAM once. */
bool scanout_memory_close(struct scanout_memory *,const struct scanout_memory_token *);
#endif
