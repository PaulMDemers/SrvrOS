#ifndef SRVROS_SCANOUT_PUBLISH_H
#define SRVROS_SCANOUT_PUBLISH_H
#include <srvros/scanout_space.h>
/* CPU0-serialized mechanism. No native caller, MMIO implementation, DMA grant,
 * cache policy or PTE encoding is supplied here. Callbacks must be bounded.
 * The owner pins RAM/context and excludes all other writers until retirement
 * or external reset. Entry vector/output must not alias the transaction object. */
#define SCANOUT_PUBLISH_PAGES 1536u
#define SCANOUT_PUBLISH_DEADLINE_US 100000u
enum scanout_publish_state {PUB_EMPTY,PUB_PREPARING,PUB_PREPARED,PUB_WRITING,PUB_PUBLISHED,PUB_RESTORING,PUB_RETIRED,PUB_CANCELLED,PUB_FAULT};
enum scanout_publish_error {PUB_OK,PUB_IO,PUB_CHANGED,PUB_CLOCK,PUB_GUARD,PUB_LEASE};
struct scanout_publish_io {
    void *ctx;
    bool (*guard)(void *);
    bool (*read)(void *,uint32_t index,uint64_t *);
    /* false may mean a partial write. Every attempt is already exposed. */
    bool (*write)(void *,uint32_t index,uint64_t);
    /* Adapter must complete posted writes AND retire relevant translations.
     * A CPU barrier or PTE readback alone is NOT this proof. */
    bool (*flush)(void *);
    /* True only after exact route completion and all device readers retire.
     * False means still busy: restore writes nothing and can be retried. */
    bool (*readers_retired)(void *);
    uint64_t (*now)(void *);
};
struct scanout_publish_pool {uint64_t epoch;};
struct scanout_publication {
    /* Zero-init, original-address sealed. Copies cannot use or restart the
     * admitted transaction, even with a token rebound to the copy. */
    const struct scanout_publication *object;
    uint64_t epoch,owner,last_us,phase_us;
    struct scanout_space *space;
    struct scanout_space_token range;
    struct scanout_publish_io io;
    uint32_t state,error,pages,reads,writes,flushes;
    bool busy,exposed;
    uint64_t before[SCANOUT_PUBLISH_PAGES],after[SCANOUT_PUBLISH_PAGES];
};
struct scanout_publish_token {const struct scanout_publication *object;uint64_t epoch,owner;};
/* Linear CPU storage calculation only; not display-format or cache admission. */
bool scanout_linear_storage(uint32_t width,uint32_t height,uint32_t pitch,uint64_t *bytes,uint32_t *pages);
bool scanout_publish_prepare(struct scanout_publication *,struct scanout_publish_pool *,uint64_t owner,
                            struct scanout_space *,const struct scanout_space_token *,
                            const uint64_t *entries,uint32_t pages,const struct scanout_publish_io *,
                            struct scanout_publish_token *);
bool scanout_publish_apply(struct scanout_publication *,const struct scanout_publish_token *);
bool scanout_publish_restore(struct scanout_publication *,const struct scanout_publish_token *);
bool scanout_publish_cancel(struct scanout_publication *,const struct scanout_publish_token *);
#endif
