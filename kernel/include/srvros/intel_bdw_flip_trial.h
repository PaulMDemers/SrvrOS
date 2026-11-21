#ifndef SRVROS_INTEL_BDW_FLIP_TRIAL_H
#define SRVROS_INTEL_BDW_FLIP_TRIAL_H
#include <srvros/intel_bdw_edp_baseline.h>
#include <srvros/intel_bdw_primary_flip.h>

enum bdw_flip_trial_stage {
    BDW_TRIAL_EMPTY, BDW_TRIAL_ORIGINAL, BDW_TRIAL_FORWARD,
    BDW_TRIAL_CANDIDATE, BDW_TRIAL_RETURN, BDW_TRIAL_VERIFY_RETURN
};

struct bdw_flip_trial {
    enum bdw_flip_trial_stage stage;
    struct intel_bdw_primary_flip forward, back;
    struct bdw_edp_baseline_report original, candidate, restored;
    bool candidate_matched, original_restored;
};

struct bdw_flip_trial_io {
    struct bdw_flip_io plane;
    struct bdw_edp_baseline_io crc;
    /* Register a fresh leg while both mappings stay continuously pinned. */
    struct bdw_flip_source *(*admit)(void *ctx, bool returning);
};

/* Blocking, isolated diagnostic only. Provide an original zeroed report and
 * hold both immutable images through the entire call. Even confirmed return
 * leaves lease retirement to the owner. All callbacks must be bounded. */
bool intel_bdw_flip_trial(struct bdw_flip_trial *, const struct bdw_flip_trial_io *,
                          uint32_t expected_candidate_crc);
#endif
