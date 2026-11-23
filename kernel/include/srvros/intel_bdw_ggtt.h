#ifndef SRVROS_INTEL_BDW_GGTT_H
#define SRVROS_INTEL_BDW_GGTT_H
#include <srvros/scanout_transfer.h>
#include <srvros/intel_gt_access.h>

/* Engine submission, WiDi ring, wireless video and wireless audio pages. */
#define BDW_SUBMISSION_PORT_COUNT 8u
extern const uint32_t intel_bdw_submission_ports[BDW_SUBMISSION_PORT_COUNT];

/* Air7,2 inherits active Pipe A with wireless video fused off. */
static inline bool intel_bdw_air_wireless_video_disabled(uint32_t strap)
{
 return (strap&(1u<<20)) && !(strap&((1u<<31)|(1u<<30)|(1u<<26)));
}

struct intel_bdw_ggtt_page {
 uint64_t physical,gpu_address,original,replacement;
 uint32_t index,mmio_offset;
};

/* Plan one absolute GGTT index in the full captured interval. Check the
 * entire interval against the aperture and inherited fence before indexing. */
bool intel_bdw_ggtt_plan_entry(const struct scanout_transfer_manifest *,
                               uint32_t index, uint64_t physical, struct intel_bdw_ggtt_page *);

/* Pure one-page encoding for private, aligned addresses below 4 GiB.
 * It retains the exact original word and rejects the protected fence.
 * The caller still needs live ownership, RAM/cache admission and completion. */
bool intel_bdw_ggtt_page_plan(const struct scanout_transfer_manifest *,
                            uint64_t physical,struct intel_bdw_ggtt_page *);
/* The caller pins an exclusive UC mapping and holds the hardware readers.
 * Readback orders the QWord store. Translation invalidation remains separate. */
enum intel_gt_write intel_bdw_ggtt_exchange(volatile uint64_t *pte,
                                            uint64_t expected, uint64_t replacement);
#endif
