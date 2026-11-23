#ifndef SRVROS_INTEL_BDW_BCS_SUBMIT_H
#define SRVROS_INTEL_BDW_BCS_SUBMIT_H
#include <srvros/intel_gt_access.h>
#include <srvros/intel_diagnostic_timing.h>
#define BCS_SUBMIT_PORT 0x22230u
#define BCS_SUBMIT_US 10000ull
#define BCS_SUBMIT_REQUIRED 31u
enum bcs_submit_requirement {
 BCS_SUBMIT_MODE=1,BCS_SUBMIT_CONTEXT=2,BCS_SUBMIT_QUEUE=4,
 BCS_SUBMIT_MAPPINGS=8,BCS_SUBMIT_VISIBILITY=16
};
struct intel_bdw_bcs_submit;
#ifdef SRVROS_BCS_SUBMIT_HOST_TEST
void intel_bdw_bcs_fixture_new_device(void);
#endif
/* Data bound to an independently owned, initialized private context. This
 * component does not construct/validate all descriptor/context mode fields.
 * The provider must do so before admitting CONTEXT, including page directory,
 * fresh context ID, stopped ring head0/tail24 and no inherited submissions.
 * Private spans and the CPU root page are conservatively below4GiB. */
struct bcs_submit_binding {
 const void *owner,*provider,*lifetime,*context;
 uint64_t root,epoch,sequence,descriptor;
 uint64_t ring_gpu,ring_physical,context_gpu,context_physical,completion_gpu,completion_physical;
 uint64_t aperture_bytes,fence_begin,fence_end;
 uint64_t prepare,gt,hda,outer,admitted_at;
 uint32_t vendor,device,tail;
};
struct bcs_submit_source {
 const struct bcs_submit_source *object;
 const struct intel_bdw_bcs_submit *consumer;
 struct bcs_submit_binding binding;
};
struct bcs_submit_proof {
 const struct bcs_submit_source *source;
 const void *owner,*provider,*lifetime,*context;
 uint64_t root,epoch,sequence,descriptor,checked_at;
 uint32_t requirements,prefix;
 /* Fresh BCS EXECLIST_STATUS (0x22234). When an element is executing,
  * bracket the context ID read at 0x22238 with matching status reads. */
 uint32_t queue_status, running_context;
};
struct bcs_submit_io {
 void *ctx;
 /* Fresh independent original owner/mode, initialized context, queue capacity,
  * mappings and visibility admission. prefix0..3: same reserved empty queue,
  * no competing poster, exact partial port state. prefix4: known issuance,
  * owned queue/context may already execute; not acknowledgement/completion.
  * Validate hardware again immediately before actual ordered UC DWORD access.
  * Echoing this binding or sampled idle alone is not native ownership proof. */
 bool (*ownership)(void *,const struct bcs_submit_source *,unsigned prefix,struct bcs_submit_proof *);
 enum intel_gt_write (*write)(void *,uint32_t port,uint32_t value);
 uint64_t (*now)(void *);
};
enum bcs_submit_state {BCS_PORT_EMPTY,BCS_PORT_WRITING,BCS_PORT_VERIFYING,BCS_PORT_ISSUED,BCS_PORT_FAULT};
enum bcs_submit_reason {BCS_PORT_SOURCE=1,BCS_PORT_CLOCK=2,BCS_PORT_OWNERSHIP=4,BCS_PORT_WRITE=8,BCS_PORT_RESULT=16};
struct bcs_submit_progress {
 uint64_t begin,last;
 uint32_t state,proofs,attempts,writes,triggered;
 bool uncertain;
};
struct intel_bdw_bcs_submit {
 const struct intel_bdw_bcs_submit *object;
 struct bcs_submit_source *source;
 struct bcs_submit_binding binding;
 struct bcs_submit_io io,original_io;
 struct bcs_submit_progress p;
 uint32_t words[4];
 uint64_t end,clock_at,clock_previous;
 unsigned reason,clock_reason;
 bool busy,issued,retained;
};
/* CPU0-serialized, one-use, original address/registration sealed. Exactly four
 * ordered DWORD writes: unused element1 high/low0, then element0 high/low.
 * Fourth known store triggers submission, never command completion or queue
 * acceptance. Original parent clocks and independent10ms whole-port bound.
 * All failures retain registration/backing, even a definitely unissued prefix.
 * No retry/rollback/cancel/free API. Success also keeps source registered until
 * the independent original context/completion owner establishes retirement.
 * Pure trusted-provider protocol only; no native adapter/image/caller exists. */
bool intel_bdw_bcs_submit_run(struct intel_bdw_bcs_submit *,struct bcs_submit_source *,struct bcs_submit_io);
#endif
