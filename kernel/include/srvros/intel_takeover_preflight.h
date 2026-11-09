#ifndef SRVROS_INTEL_TAKEOVER_PREFLIGHT_H
#define SRVROS_INTEL_TAKEOVER_PREFLIGHT_H
#include <srvros/intel_completion_audit.h>
#define INTEL_TAKEOVER_WORDS 6u
#define INTEL_TAKEOVER_INPUT 1u
#define INTEL_TAKEOVER_CLOCK 2u
#define INTEL_TAKEOVER_PARENT 4u
#define INTEL_TAKEOVER_GUARD 8u
#define INTEL_TAKEOVER_READ 16u
#define INTEL_TAKEOVER_FLOAT 32u
#define INTEL_TAKEOVER_CHANGED 64u
#define INTEL_TAKEOVER_RETURN 128u
#define INTEL_TAKEOVER_COUNT 256u
struct intel_takeover_result {
    const struct intel_takeover_result *object;
    struct intel_gt_access *parent;
    const struct intel_completion_result *census;
    struct intel_gt_borrow borrow;
    uint32_t epoch,words[2][INTEL_TAKEOVER_WORDS];
    uint64_t begin_us,last_us,end_us;
    unsigned counts[2],attempts,samples,reads,reason,busy,reserved,geometry;
    bool blocked,returned,stable,quiescent,complete;
};
struct intel_takeover_io {
    void *ctx;
    bool (*guard)(void *);
    /* Exact RO order: five RESET_CTRL DWORDs, then GDRST. Progress includes
     * the last observed word on a failed/float read; never more than six. */
    bool (*sample)(void *,uint32_t [INTEL_TAKEOVER_WORDS],unsigned *);
    uint64_t (*now)(void *);
};
/* Zero-init, address-sealed, one-use observation. Quiescent is a sampled
 * prerequisite, not authority to reset, flush, transfer an interval or map RAM. */
bool intel_takeover_observe(struct intel_takeover_result *,struct intel_gt_access *,
    const struct intel_completion_result *,struct intel_takeover_io);
#endif
