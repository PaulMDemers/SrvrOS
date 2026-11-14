#ifndef SRVROS_INTEL_BDW_GGTT_H
#define SRVROS_INTEL_BDW_GGTT_H
#include <srvros/scanout_transfer.h>

struct intel_bdw_ggtt_page {
 uint64_t physical,gpu_address,original,replacement;
 uint32_t index,mmio_offset;
};

/* Pure one-page encoding for private, aligned addresses below 4 GiB.
 * It retains the exact original word and rejects the protected fence.
 * The caller still needs live ownership, RAM/cache admission and completion. */
bool intel_bdw_ggtt_page_plan(const struct scanout_transfer_manifest *,
                            uint64_t physical,struct intel_bdw_ggtt_page *);
#endif
