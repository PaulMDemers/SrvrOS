#ifndef SRVROS_INTEL_COMMAND_MAPPING_H
#define SRVROS_INTEL_COMMAND_MAPPING_H
#include <srvros/intel_command_memory.h>
#include <srvros/intel_gt_access.h>
#include <srvros/intel_bdw_ggtt.h>

/* A trusted-provider transaction, not a source of native GPU ownership.
 * The exact interval MUST ALREADY be transferred with all hardware witnesses.
 * No current native provider can supply that transfer. Static/heap storage. */
#define COMMAND_MAP_REQUIRED 31u
#define COMMAND_MAP_POLLS 16u
enum command_map_phase {COMMAND_MAP_CAPTURE,COMMAND_MAP_REPLACEMENT,COMMAND_MAP_ORIGINAL};
enum command_map_requirement {COMMAND_MAP_INTERVAL=1,COMMAND_MAP_RAM=2,COMMAND_MAP_CPU_ALIAS=4,
 COMMAND_MAP_DMA_CACHE=8,COMMAND_MAP_READERS=16};
enum command_map_state {COMMAND_MAP_EMPTY,COMMAND_MAP_OPENING,COMMAND_MAP_USING,
 COMMAND_MAP_RESTORING,COMMAND_MAP_CLOSED,COMMAND_MAP_FAULT};
enum command_map_reason {COMMAND_MAP_SOURCE=1,COMMAND_MAP_CLOCK=2,COMMAND_MAP_OWNERSHIP=4,
 COMMAND_MAP_READ=8,COMMAND_MAP_WRITE=16,COMMAND_MAP_COMPLETE=32,COMMAND_MAP_CHILD=64,COMMAND_MAP_RETIRE=128};
struct intel_command_mapping;
struct command_map_proof {
 const struct intel_command_mapping *object;
 const struct intel_command_memory *cpu;
 const struct scanout_transfer *transfer;
 uint64_t root,epoch,physical,checked_at;
 uint32_t first,pages,requirements;
};
struct command_map_io {
 void *ctx;
 bool (*ownership)(void *,const struct intel_command_mapping *,enum command_map_phase,struct command_map_proof *);
 /* Exact aligned QWord entry access at GTTMMADR+offset. The native provider
  * must revalidate actual hardware/mapping/cache ownership after its last clock
  * immediately before access. Partial/split/unknown writes are UNCERTAIN. */
 bool (*read)(void *,uint32_t offset,uint64_t *);
 enum intel_gt_write (*write)(void *,uint32_t offset,uint64_t);
 /* Documented posted-write AND translation/cache completion for these exact
  * four entries, not just a CPU fence/readback or a standalone SA operation. */
 bool (*complete)(void *,const struct intel_command_mapping *,enum command_map_phase);
 /* false = known pending. Unknown source/ownership must fail ownership(). */
 bool (*readers_retired)(void *,const struct intel_command_mapping *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
struct intel_command_mapping {
 const struct intel_command_mapping *object;
 struct intel_command_memory *cpu;
 struct scanout_transfer *transfer;
 struct scanout_transfer_token token;
 struct scanout_transfer_manifest manifest;
 struct scanout_transfer_io transfer_io;
 struct intel_command_lease lease;
 struct intel_command_io cpu_io;
 struct command_map_io io,original_io;
 struct scanout_space_token range;
 struct scanout_publish_pool pool;
 struct scanout_publish_token publication_token;
 uint64_t physical,serial,cpu_end,original[4],replacement[4];
 uint64_t begin,last,end,clock_at,clock_previous;
 uint32_t state,reason,clock_reason,phase,proofs,reads,attempts,writes,completions,reader_checks,polls;
 bool busy,registered,retained,uncertain,complete,checking_transfer;
 /* Large object last: callback snapshots copy only the bounded header and
  * four actual entries. Never copy the complete publication on kernel stack. */
 struct scanout_publication publication;
};
/* Original-address/consumed-once, CPU0 serialized. Reserve all four transferred
 * pages, register on the CPU hold before callbacks, publish, run one bounded
 * child, drain, restore exact originals, complete and retire both leases.
 * The child grants no permission to initialize/submit an executable context.
 * Any failed started lifetime retains RAM/range/exclusion; no forced cleanup,
 * retry, allocator release or exposure to the ordinary desktop. */
bool intel_command_mapping_run(struct intel_command_mapping *,struct intel_command_memory *,
 struct scanout_transfer *,const struct scanout_transfer_token *,struct command_map_io,
 bool (*child)(void *,struct intel_command_mapping *),void *);
/* Pure same-object address lookup while the registered child is executing;
 * not fresh hardware admission or permission to submit commands. */
bool intel_command_mapping_page(const struct intel_command_mapping *,unsigned page,uint64_t *gpu,uint64_t *physical);
#endif
