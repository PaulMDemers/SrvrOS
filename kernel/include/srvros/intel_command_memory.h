#ifndef SRVROS_INTEL_COMMAND_MEMORY_H
#define SRVROS_INTEL_COMMAND_MEMORY_H
#include <stddef.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_nonzero_reset.h>
#define INTEL_COMMAND_PAGES 4u
#define INTEL_COMMAND_BYTES 16384u
#define INTEL_COMMAND_OWNER 0x4243534d454dull
/* Private CPU command arena only: ring page0, two context-storage pages1/2,
 * completion page3. Zero storage is NOT an initialized executable LRC context,
 * a GPU mapping, GPU address-space ownership or permission to submit a ring. */
struct intel_command_lease {
 const void *owner,*provider,*lifetime;
 uint64_t root,epoch,sequence,prepare,gt,hda,outer;
};
struct intel_command_alias {
 uint32_t pages,leaves,index,writable,user,nx,sizes,mtrr,pat,effective;
};
struct intel_command_io {
 struct scanout_memory_io ram;
 bool (*map)(void *,uint64_t root,uint64_t va,uint64_t physical);
 bool (*unmap)(void *,uint64_t root,uint64_t va,uint64_t *physical);
 bool (*observe)(void *,uint64_t root,uint64_t va,uint64_t physical,struct intel_command_alias *);
 bool (*initialize)(void *,uint64_t va,uint32_t bytes);
 bool (*preserved)(void *,uint64_t va,uint32_t bytes);
 /* Independent ORIGINAL held-source admission/retirement, not a descriptor
  * echo or a saved receipt. No native GPU ownership assurance is requested. */
 bool (*guard)(void *,const struct intel_command_lease *);
 bool (*retired)(void *,const struct intel_command_lease *);
 uint64_t (*now)(void *);
};
enum intel_command_state {COMMAND_EMPTY,COMMAND_PREPARING,COMMAND_PREPARED,COMMAND_HELD,
 COMMAND_RETURNED,COMMAND_CLOSED,COMMAND_FAULT};
enum intel_command_reason {COMMAND_INPUT=1,COMMAND_BACKING=2,COMMAND_MAPPING=4,COMMAND_CONTENT=8,
 COMMAND_SOURCE=16,COMMAND_CLOCK=32,COMMAND_PARENT=64,COMMAND_RETIRE=128};
/* Own this large scanout_memory-backed object in static/heap storage. Address-
 * sealed and one-use. All partial allocations/maps/failures retain the arena;
 * no retry, forced unmap or free after uncertainty. */
struct intel_command_memory {
 const struct intel_command_memory *object;
 struct scanout_memory memory;
 struct scanout_memory_pool pool;
 struct scanout_memory_token token;
 struct scanout_memory_hold hold;
 struct intel_command_io io,original_io;
 struct intel_command_lease lease;
 struct intel_command_alias alias;
 uint64_t root,virtual_base,physical,serial,prepare_begin,prepare_end,begin,last,end,close_begin,close_end;
 uint64_t clock_at,clock_previous;
 uint32_t state,reason,clock_reason,maps,retired,observations,guards,initializations,checks;
 bool busy,registered,complete,retained;
};
bool intel_command_memory_prepare(struct intel_command_memory *,uint64_t root,uint64_t virtual_base,struct intel_command_io);
/* Registers the original arena hold during a bounded original GPU diagnostic,
 * validates its real CPU mapping/cache twice, then returns only this child.
 * The hold and RAM remain pinned until the enclosing source actually retires. */
bool intel_command_memory_run(struct intel_command_memory *,const struct intel_command_lease *);
bool intel_command_memory_returned(const struct intel_command_memory *);
/* Only after independent original-parent retirement: verify full zero contents,
 * retire the exact four CPU aliases, then release the original RAM once. */
bool intel_command_memory_close(struct intel_command_memory *);
#endif
