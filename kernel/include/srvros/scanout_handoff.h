#ifndef SRVROS_SCANOUT_HANDOFF_H
#define SRVROS_SCANOUT_HANDOFF_H
#include <srvros/display_access.h>
#include <srvros/scanout_space.h>
#define HANDOFF_FIRMWARE 1u
#define HANDOFF_ENGINES 2u
#define HANDOFF_DMA 4u
#define HANDOFF_READERS 8u
#define HANDOFF_CACHE 16u
#define HANDOFF_ALL 31u
#define HANDOFF_INVALID 32u
#define HANDOFF_EXCLUSION 64u
#define HANDOFF_SNAPSHOT 128u
#define HANDOFF_SPACE 256u
#define HANDOFF_PAGES 16u
/* An audited provider must establish these assurances in the SAME boot and
 * maintain them through retirement. Stable/populated/absent entries alone grant
 * none. Native Air7,2 currently supplies no hardware assurances, so cannot open.
 * This software transaction is a trusted-adapter contract, not an IOMMU or a
 * boundary against malicious kernel callers using scanout_space directly. */
struct scanout_handoff_snapshot {
    uint64_t mmio, aperture, aperture_bytes, fence_begin, fence_end;
    uint32_t first, pages, assurances;
    uint64_t original[HANDOFF_PAGES];
};
struct scanout_handoff_io {
    void *context;
    /* Bounded, read-only validation of fresh endpoint/cache/inherited backing
     * and exact original PTEs, including every asserted hardware assurance.
     * The provider/context are bound until close; no nested open/close. */
    bool (*revalidate)(void *,const struct scanout_handoff_snapshot *);
};
struct scanout_handoff {
    /* Zero-init, CPU0 serialized and address-sealed. Copies cannot retire the
     * original exclusion pin. Providers must not mutate this object directly. */
    const struct scanout_handoff *object;
    struct scanout_space space;
    struct display_access_token exclusion;
    struct scanout_handoff_snapshot snapshot;
    struct scanout_handoff_io provider;
    bool active, busy, pinned;
};
uint32_t scanout_handoff_open(struct scanout_handoff *,struct scanout_space_pool *,
    const struct display_access_token *,const struct scanout_handoff_snapshot *,struct scanout_handoff_io);
/* Close stops reservation admission before validation, and requires the same
 * provider. Exposed/nonretired leases block retirement. Failed revalidation keeps
 * both the range and exclusion pinned; never restore ordinary clients then. */
bool scanout_handoff_close(struct scanout_handoff *,struct scanout_handoff_io);
#endif
