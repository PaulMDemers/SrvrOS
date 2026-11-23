#ifndef SRVROS_INTEL_BDW_BLITTER_H
#define SRVROS_INTEL_BDW_BLITTER_H

#include <srvros/intel_bdw_bcs_context.h>
#include <srvros/intel_bdw_bcs_submit.h>
#include <srvros/intel_bdw_private_vm.h>
#include <srvros/scanout_publish.h>
#include <srvros/scanout_transfer.h>

/* Boot-CPU serialized. The owner pins the original contiguous 19-frame DMA
 * allocation. Its first seven frames use GGTT, followed by the private VM. */
enum { BDW_BLITTER_GLOBAL_PAGES = 7 };
struct bdw_blitter_image {
    struct intel_bdw_bcs_context_config config;
    volatile uint32_t *global;
    volatile struct bdw_private_vm_image *vm;
    uint64_t physical, epoch, descriptor;
};
bool intel_bdw_blitter_prepare(struct bdw_blitter_image *,const struct intel_bdw_bcs_context_config *,
                              volatile uint32_t *global,volatile struct bdw_private_vm_image *,
                              uint64_t physical,uint64_t epoch);
bool intel_bdw_blitter_verify(const struct bdw_blitter_image *);

/* A single already initialized BCS engine. held() revalidates exclusive
 * submission, the pinned UC BCS register page at BAR0+0x22000, forcewake,
 * DMA domain, coherent CPU aliases, private PAT and original HWSP lifetime.
 * The original HWSP stays pinned across installation and restoration. A firmware
 * interval reservation alone cannot supply this owner or a published mapping. */
struct bdw_blitter_device {
    volatile uint32_t *registers;
    void *owner;
    bool (*held)(void *,const struct bdw_blitter_image *);
    uint64_t (*now)(void *);
    uint64_t mmio,root,outer,gt,hda,prepare;
    uint32_t original_hwsp;
    uint32_t csb_head; /* Last hardware event consumed by this queue owner. */
};

enum bdw_blitter_state { BDW_BLITTER_NEW, BDW_BLITTER_PINNED, BDW_BLITTER_ISSUED,
                         BDW_BLITTER_RETIRED, BDW_BLITTER_RETAINED };
struct bdw_blitter_request {
    struct bcs_submit_source source;
    struct intel_bdw_bcs_submit port;
    struct scanout_reader reader;
    struct scanout_publication *pages;
    const struct bdw_blitter_image *image;
    struct bdw_blitter_device device;
    enum bdw_blitter_state state;
    uint32_t csb_start,csb_end,csb_read;
    uint64_t last;
};

/* One request per zero-initialized object. Success requires a fresh context
 * completion, queue idle and both result cache lines. Uncertain hardware
 * keeps its registered GPU reader and backing until external device recovery. */
bool intel_bdw_blitter_run(struct bdw_blitter_request *,const struct bdw_blitter_image *,
                          const struct bdw_blitter_device *,struct scanout_publication *,
                          const struct scanout_publish_token *);

/* Static storage. Once exposed, the transfer and DMA allocation stay alive
 * until the engine retires and the exact original entries are restored. */
struct bdw_blitter_window {
    volatile uint64_t *pte; /* Start of the pinned, writable UC GGTT register page. */
    uint64_t physical;
    void *owner;
    /* Own the exact alias and exclude other CPU/device writers. During GGTT
     * publication/restoration this includes the device-side traffic block. */
    bool (*held)(void *,const struct bdw_blitter_window *,const struct scanout_transfer *);
    bool (*complete)(void *); /* Posted stores and translation invalidation completed. */
    bool (*readers_retired)(void *);
};
struct bdw_blitter_mapping {
    struct scanout_publication pages;
    struct scanout_publish_pool pool;
    struct scanout_publish_token publication;
    struct scanout_space_token range;
    struct bdw_blitter_request request;
    struct bdw_blitter_window window;
    struct bdw_blitter_device device;
    const struct bdw_blitter_image *image;
    struct scanout_transfer *transfer;
    struct scanout_transfer_token grant;
};
/* The transfer owner supplies the pinned register window, synchronous translation
 * completion and device reader drain. A reserved transfer is ineligible. */
bool intel_bdw_blitter_map_and_run(struct bdw_blitter_mapping *,const struct bdw_blitter_image *,
                                  const struct bdw_blitter_device *,struct scanout_transfer *,
                                  const struct scanout_transfer_token *,const struct bdw_blitter_window *);
#endif
