#ifndef SRVROS_INTEL_DISPLAY_HDA_H
#define SRVROS_INTEL_DISPLAY_HDA_H
#include <stdbool.h>
#include <stdint.h>
#define INTEL_HDA_BASE 0xc1610000ull
#define INTEL_HDA_GLOBALS 11u
#define INTEL_HDA_STREAMS 8u /* Diagnostic evidence budget; larger GCAP refuses. */
struct intel_hda_endpoint {
 uint32_t id,class_rev,subsystem,command,flags,pm,state,bar,bar_high;
 bool reserved;
};
struct intel_hda_sample {
 uint32_t global[INTEL_HDA_GLOBALS],stream[INTEL_HDA_STREAMS][5];
 unsigned streams;
};
struct intel_hda_result {
 const struct intel_hda_result *object;
 struct intel_hda_endpoint endpoint;
 struct intel_hda_sample sample[2];
 uint64_t begin_us,end_us,last_us;
 unsigned samples,reads,maps,retired,reason,run_mask,dma_mask;
 bool stable,complete,retained;
};
struct intel_hda_io {
 void *ctx;
 bool (*endpoint)(void *,struct intel_hda_endpoint *);
 bool (*guard)(void *); /* Fresh cache/resource context at pass boundaries. */
 bool (*map)(void *);
 bool (*read)(void *,unsigned,unsigned,uint32_t *);
 bool (*unmap)(void *);
 uint64_t (*now)(void *);
};
extern const unsigned intel_hda_offsets[INTEL_HDA_GLOBALS],intel_hda_widths[INTEL_HDA_GLOBALS];
bool intel_hda_endpoint_valid(const struct intel_hda_endpoint *);
bool intel_hda_read_allowed(unsigned offset,unsigned width);
/* Read callbacks only. No commands, reset, status acknowledgements or grants.
 * Zero-init and consume once; uncertain alias cleanup must retain exclusion. */
bool intel_hda_observe(struct intel_hda_result *,bool admitted,struct intel_hda_io);
bool intel_display_hda_probe_report(void);
#endif
