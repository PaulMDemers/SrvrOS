#ifndef SRVROS_INTEL_SOURCE_BUFFERS_H
#define SRVROS_INTEL_SOURCE_BUFFERS_H
#include <srvros/intel_nonzero_source.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_bdw_ggtt.h>
enum intel_source_buffers_reason {ISB_INPUT=1,ISB_SOURCE=2,ISB_CLOCK=4,ISB_BACKING=8,ISB_GUARD=16};
struct intel_source_buffers_io {void *ctx;bool (*guard)(void *);uint64_t (*now)(void *);};
/* Static/heap original one-use binding. Pins both private RAM holds and the
 * original source during its callback. This is not a GPU range transfer. */
struct intel_source_buffers {
 const struct intel_source_buffers *object;
 struct intel_nonzero_source *source;
 struct scanout_memory_hold *hold[2];
 struct intel_source_buffers_io io;
 const void *lifetime;
 struct scanout_memory_token memory[2];
 uint64_t physical[2],serial[2],expected,origin,parent_begin,begin,end,last,clock_at,clock_prev;
 struct intel_bdw_ggtt_page page[2];
 uint32_t epoch,kind,post_mode,reason,planned;
 bool registered,blocked,complete,returned;
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
 const void *dependent,*registered_child;
 unsigned child_borrows,child_returns;
 bool observer_entered,observer_complete;
#endif
};
/* Private full-buffer storage is prepared before source entry. This callback
 * performs only pure identity/codec checks and same-origin clocks; enclosing
 * source guards bracket it. The original preparation100ms/GT300ms stay fixed. */
bool intel_source_buffers_run(struct intel_source_buffers *,struct intel_nonzero_source *,
 struct scanout_memory_hold *,struct scanout_memory_hold *,struct intel_source_buffers_io);
bool intel_source_buffers_live(const struct intel_source_buffers *);
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
bool intel_source_buffers_transfer_run(struct intel_source_buffers *,struct intel_nonzero_source *,
 struct scanout_memory_hold *,struct scanout_memory_hold *,struct intel_source_buffers_io,
 bool (*)(void *,struct intel_source_buffers *),void *);
#endif
#endif
