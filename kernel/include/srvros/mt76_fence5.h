#ifndef SRVROS_MT76_FENCE5_H
#define SRVROS_MT76_FENCE5_H
#include <srvros/mt76_dma5.h>
#define MT76_FENCE5_WAIT_US 750000u
#define MT76_FENCE5_EVENTS 32u
enum mt76_fence5_state { MT76_FENCE5_EMPTY,MT76_FENCE5_IDLE,MT76_FENCE5_STOP,MT76_FENCE5_STOP_WAIT,MT76_FENCE5_STOP_CONTEXT,MT76_FENCE5_DRAIN,MT76_FENCE5_REBASE,MT76_FENCE5_REBASE_WAIT,MT76_FENCE5_VERIFY,MT76_FENCE5_FINAL_DRAIN,MT76_FENCE5_COMMIT,MT76_FENCE5_FAULT };
enum mt76_fence5_result { MT76_FENCE5_PENDING,MT76_FENCE5_READY,MT76_FENCE5_FAILED };
/* Tokens identify retained command ownership records;they must NOT be stamped
 * onto a raw command event simply because its pointer matches a current command.
 * submit is nonblocking and returns exact published identity,or uncertainty.
 * completion consumes only independently attributed raw events. context reads
 * real endpoint output. frontier consumes bounded events/observes empty while
 * endpoint is stopped,never manufactures proof from an empty running poll.
 * All callbacks serialize on CPU0 and must not recurse or mutate the DMA ledger.
 * Both directions' ledger mutations pause throughout this fence. Begin requires
 * all eight pages free. At most31 fences/62 unique command pointers per object.
 * Every false step/invalid object requires caller publication closure/retention;
 * a corrupted pointer is not dereferenced merely to mark a fault. */
struct mt76_fence5_command {uint64_t epoch,token,pointer,dequeue;unsigned slot,dci,type;};
struct mt76_fence5_completion {struct mt76_fence5_command command;unsigned cc;};
struct mt76_fence5_context {uint64_t epoch,dequeue;unsigned slot,dci,state;};
struct mt76_fence5_frontier {uint64_t epoch,sequence;unsigned scanned,target_events;bool empty,command_pending;};
struct mt76_fence5_io {
 void *context;
 bool (*owned)(void *,uint64_t,unsigned);
 enum mt76_fence5_result (*submit)(void *,const struct mt76_fence5_command *,struct mt76_fence5_command *);
 enum mt76_fence5_result (*completion)(void *,struct mt76_fence5_completion *);
 bool (*endpoint)(void *,unsigned,unsigned,struct mt76_fence5_context *);
 bool (*frontier)(void *,struct mt76_fence5_frontier *);
};
/* Zero object initialized once,never copied/reinitialized;no allocation,keys,
 * DMA writes,doorbells or hardware authority live in this engine. */
struct mt76_fence5 {
 uintptr_t identity;uint64_t seal;uint64_t command_ring,command_pointers[63];unsigned pointer_count;struct mt76_dma5 *dma;struct mt76_fence5_io io,io_guard;
 struct mt76_dma5 guard;struct mt76_fence5_command command;
 enum mt76_fence5_state state;uint64_t epoch,last_us,phase_us,next_token,last_frontier,sequence,completed;
 unsigned slot,dci[2],direction,scanned;bool busy;
};
bool mt76_fence5_init(struct mt76_fence5 *,struct mt76_dma5 *,uint64_t,unsigned,const unsigned dci[2],const struct mt76_fence5_io *,uint64_t);
bool mt76_fence5_begin(struct mt76_fence5 *,unsigned,uint64_t);
/* One bounded nonblocking worker step; owned and action are separate calls.
 * Valid owned failures fault the retained ledger. EVERY false requires caller
 * publication closure/retention,including invalid handles. Completion is IDLE. */
bool mt76_fence5_step(struct mt76_fence5 *,uint64_t);
/* Cancellation after begin is uncertainty:pin/fault,never reopen publication. */
bool mt76_fence5_cancel(struct mt76_fence5 *,uint64_t);
#endif
