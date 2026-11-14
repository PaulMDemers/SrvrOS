#ifndef SRVROS_INTEL_GT_PREFLIGHT_H
#define SRVROS_INTEL_GT_PREFLIGHT_H
#include <stdbool.h>
#include <stdint.h>
#include <srvros/intel_gt_access.h>
enum intel_gt_preflight_status {GTP_REFUSED,GTP_OBSERVED,GTP_ACCESS,GTP_READ,GTP_CLOCK,GTP_CHANGED,GTP_RETAINED};
struct intel_gt_endpoint {uint64_t mmio;uint32_t asls,swsci;};
struct intel_gt_sample {uint64_t begin_us,end_us;uint32_t requests,ack,reads;};
struct intel_gt_preflight_result {uint32_t status,maps,retained;struct intel_gt_endpoint endpoint;struct intel_gt_sample sample[2];};
/* Read-only request-register exception and always-on ACK only. No engine access,
 * OpRegion pointer traversal, wake/reset/SCI request or ownership inference. */
struct intel_gt_preflight_io {
    void *context;
    bool (*guard)(void *);
    bool (*endpoint)(void *,struct intel_gt_endpoint *);
    bool (*map)(void *,uint32_t);
    bool (*read)(void *,uint32_t,uint32_t *);
    bool (*unmap)(void *);
    uint64_t (*now)(void *);
};
bool intel_gt_preflight(struct intel_gt_preflight_result *,const struct intel_gt_preflight_io *);
bool intel_gt_preflight_probe_report(void);
#endif
