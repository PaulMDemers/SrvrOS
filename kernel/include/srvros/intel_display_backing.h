#ifndef SRVROS_INTEL_DISPLAY_BACKING_H
#define SRVROS_INTEL_DISPLAY_BACKING_H
#include <srvros/intel_display_capture.h>
#define INTEL_BACKING_PTES 1536u
#define INTEL_BACKING_FENCES 32u
/* Only this bounded inherited-route study. No backing data or write callback. */
struct intel_backing_endpoint {
    struct intel_capture_identity identity;
    /* Host ID, host GGC/BDSM/BGSM/TOLUD, GPU GGC/BDSM, full MMIO safety. */
    uint32_t config[8];
    uint64_t fb_pages,fb_entry_base,fb_entry_length,fb_entry_type;
    uint64_t stolen_entry_base,stolen_entry_length,stolen_entry_type;
};
struct intel_backing_sample {
    struct intel_backing_endpoint endpoint;
    uint64_t begin_us,end_us,hash,first,last;
    uint64_t fence[INTEL_BACKING_FENCES];
    uint32_t reads,ptes,present,contiguous,stolen;
};
struct intel_backing_result {
    uint32_t status,maps,retained,count,tiled,fence_matches;
    uint64_t offset,bytes,ggtt_bytes,stolen_base,stolen_end;
    struct intel_backing_sample sample[2];
    uint64_t pte[INTEL_BACKING_PTES];
};
/* status: 0 absent, 1 observed, 2 route/config refused, 3 endpoint changed,
 * 4 mapping/read failure, 5 deadline, 6 mapping retained, 7 PTE changed. */
struct intel_backing_io {
    void *ctx;
    bool (*endpoint)(void *,struct intel_backing_endpoint *);
    bool (*map)(void *,uint32_t page_offset);
    bool (*unmap)(void *);
    bool (*read)(void *,uint32_t offset,uint32_t *);
    uint64_t (*now)(void *);
};
bool intel_display_backing(struct intel_backing_result *,const struct intel_capture_result *,const struct intel_backing_io *);
struct limine_framebuffer;
void intel_display_backing_probe_boot(struct limine_framebuffer *);
bool intel_display_backing_probe_report(void);
#endif
