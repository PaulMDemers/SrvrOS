#ifndef SRVROS_INTEL_GGTT_WINDOW_H
#define SRVROS_INTEL_GGTT_WINDOW_H
#include <stdbool.h>
#include <stdint.h>
#define INTEL_WINDOW_ENTRIES 16u
#define INTEL_WINDOW_FIRST 8704u
#define INTEL_WINDOW_PAGE 0x811000u
enum intel_window_status {WINDOW_REFUSED,WINDOW_OBSERVED,WINDOW_ACCESS,WINDOW_READ,WINDOW_CLOCK,WINDOW_CHANGED,WINDOW_RETAINED};
struct intel_window_endpoint {uint64_t mmio,gmadr,ggtt_bytes,fence_begin,fence_end;uint32_t msac;};
struct intel_window_sample {uint64_t begin_us,end_us,hash,entry[INTEL_WINDOW_ENTRIES];uint32_t reads,entries,zero,bit0;};
struct intel_window_result {uint32_t status,maps,retained;uint64_t aperture_base,aperture_bytes;struct intel_window_endpoint endpoint;struct intel_window_sample sample[2];};
struct intel_window_io {
    void *ctx;
    bool (*guard)(void *); /* Exact native profile, UC, intact inherited backing. Unmapped only. */
    bool (*endpoint)(void *,struct intel_window_endpoint *);
    bool (*map)(void *,uint32_t page_offset); /* Read-only/NX single temporary MMIO page. */
    bool (*read)(void *,uint32_t offset,uint32_t *);
    bool (*unmap)(void *);
    uint64_t (*now)(void *);
};
/* Mobile5th-gen datasheet330835-003 section3.2.24. No destructive BAR sizing.
 * Pure geometry decoder, not a resource/managed-range or ownership grant. */
bool intel_ggtt_aperture(uint8_t msac,uint64_t raw_gmadr,uint64_t *base,uint64_t *bytes);
bool intel_ggtt_window(struct intel_window_result *,const struct intel_window_io *);
bool intel_ggtt_window_probe_report(void);
#endif
