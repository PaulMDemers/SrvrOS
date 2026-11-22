#ifndef SRVROS_INTEL_ENGINE_CENSUS_H
#define SRVROS_INTEL_ENGINE_CENSUS_H
#include <srvros/intel_gt_access.h>
#define INTEL_ENGINE_COUNT 5u
#define INTEL_ENGINE_WORDS 6u
extern const uint32_t intel_engine_pages[INTEL_ENGINE_COUNT];
extern const uint32_t intel_engine_offsets[INTEL_ENGINE_WORDS];
struct intel_engine_census {
 uint32_t words[2][INTEL_ENGINE_COUNT][INTEL_ENGINE_WORDS];
 uint64_t begin_us,end_us;
 unsigned reads,samples,enabled,pending;
 bool complete,stable;
};
/* Read-only endpoint census, not engine retirement. The trusted adapter binds
 * the exact pages/profile/cache and keeps the cooperative wake lease live. */
struct intel_engine_io {
 void *context;
 bool (*guard)(void *);
 bool (*read_group)(void *,unsigned,uint32_t [INTEL_ENGINE_WORDS]);
 uint64_t (*now)(void *);
};
bool intel_engine_observe(struct intel_engine_census *,struct intel_gt_access *,struct intel_engine_io);
#endif
