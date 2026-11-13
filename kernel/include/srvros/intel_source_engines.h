#ifndef SRVROS_INTEL_SOURCE_ENGINES_H
#define SRVROS_INTEL_SOURCE_ENGINES_H
#include <srvros/intel_source_requester.h>
enum intel_source_engines_state {ISE_EMPTY,ISE_OPENING,ISE_PREPARING,ISE_HELD,ISE_CANCELLING,ISE_CLOSED,ISE_FAULT};
enum intel_source_engines_reason {ISE_INPUT=1,ISE_SOURCE=2,ISE_CLOCK=4,ISE_GUARD=8,ISE_READ=16,ISE_STATE=32,ISE_WRITE=64,ISE_PENDING=128,ISE_CHILD=256};
struct intel_source_engines_io {
 void *ctx;
 bool (*guard)(void *);
 bool (*sample)(void *,uint32_t [6],unsigned *);
 bool (*control)(void *,unsigned,uint32_t *);
 enum intel_gt_write (*transition)(void *,unsigned,uint32_t,uint32_t *);
 uint64_t (*now)(void *);
 void (*pause)(void *);
};
/* One-use original registration beneath the inhibited requester. Readiness
 * control only: not engine/DMA/prefetch/cache/translation/reader retirement. */
struct intel_source_engines {
 const struct intel_source_engines *object;
 struct intel_source_requester *requester;
 struct intel_source_transfer *transfer;
 struct intel_nonzero_source *source;
 const void *provider,*lifetime;
 struct intel_source_engines_io io,original_io;
 uint64_t origin,parent_begin,begin,last,end,clock_at,clock_prev,child_begin,child_end;
 uint64_t sample_begin[4],sample_end[4],phase_begin[10],phase_end[10];
 uint32_t epoch,kind,post_mode,state,reason,words[4][6],ack[10],final[10];
 unsigned counts[4],phase_polls[10],samples,reads,attempts,writes,acks,polls,phase,step,index,borrows,returns;
 bool busy,registered,parent_blocked,holds_blocked,child_entered,child_complete,restored,complete;
};
bool intel_source_engines_run(struct intel_source_engines *,struct intel_source_requester *,
 struct intel_source_engines_io,bool (*)(void *,struct intel_source_engines *),void *);
bool intel_source_engines_live(const struct intel_source_engines *);
bool intel_source_engines_deadline(struct intel_source_engines *,uint64_t);
bool intel_source_engines_write_ready(const struct intel_source_engines *,unsigned,uint32_t);
#endif
