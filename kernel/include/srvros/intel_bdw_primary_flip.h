#ifndef SRVROS_INTEL_BDW_PRIMARY_FLIP_H
#define SRVROS_INTEL_BDW_PRIMARY_FLIP_H
#include <srvros/intel_gt_access.h>

/* Primary A only. A trusted native display owner MUST already own both exact
 * mappings, backing, CPU/device visibility and exclusive synchronous plane
 * programming. This protocol creates none of those prerequisites. */
#define BDW_FLIP_REQUIRED 63u
#define BDW_FLIP_US 100000u
#define BDW_FLIP_POLLS 256u
enum bdw_flip_requirement {
 BDW_FLIP_OWNER=1,BDW_FLIP_MAPPINGS=2,BDW_FLIP_VISIBLE=4,
 BDW_FLIP_ROUTE=8,BDW_FLIP_EXCLUSIVE=16,BDW_FLIP_READERS=32
};
enum bdw_flip_state {BDW_FLIP_EMPTY,BDW_FLIP_ARMING,BDW_FLIP_PENDING,BDW_FLIP_LATCHED,BDW_FLIP_FAULT};
enum bdw_flip_status {BDW_FLIP_INVALID,BDW_FLIP_WAIT,BDW_FLIP_DONE,BDW_FLIP_FAILED};
enum bdw_flip_reason {BDW_FLIP_SOURCE=1,BDW_FLIP_CLOCK=2,BDW_FLIP_OWNERSHIP=4,
 BDW_FLIP_READ=8,BDW_FLIP_LAYOUT=16,BDW_FLIP_WRITE=32,BDW_FLIP_LIMIT=64};
struct intel_bdw_primary_flip;
struct bdw_flip_binding {
 const void *owner,*provider,*lifetime,*from_mapping,*to_mapping;
 uint64_t root,epoch,sequence,admitted_at,aperture_bytes;
 uint64_t from_gpu,to_gpu,mapped_bytes;
 uint32_t vendor,device,source_size,control,stride;
};
struct bdw_flip_source {
 const struct bdw_flip_source *object;
 const struct intel_bdw_primary_flip *consumer;
 struct bdw_flip_binding binding;
};
struct bdw_flip_proof {
 const struct bdw_flip_source *source;
 const void *owner,*provider,*lifetime,*from_mapping,*to_mapping;
 uint64_t root,epoch,sequence,checked_at;
 uint32_t requirements,state;
};
struct bdw_flip_io {
 void *ctx;
 /* Independently maintain source/epoch/resources, both full mapping/backing
  * leases and visibility, fixed enabled route, no stereo/async/competing
  * poster and all auxiliary readers (including firmware/FBC/PSR). Before
  * each actual access, revalidate after the last clock. State is ARMING or
  * PENDING; pending ownership must allow the requested plane to latch. */
 bool (*ownership)(void *,const struct bdw_flip_source *,struct bdw_flip_proof *);
 bool (*read)(void *,uint32_t offset,uint32_t *);
 enum intel_gt_write (*write)(void *,uint32_t offset,uint32_t);
 uint64_t (*now)(void *);
};
struct bdw_flip_progress {
 uint64_t begin,last;
 uint32_t state,proofs,reads,attempts,writes,polls,frame;
 uint32_t before_frame,after_frame,programmed,live[2];
 bool uncertain;
};
struct intel_bdw_primary_flip {
 const struct intel_bdw_primary_flip *object, *rollback;
 struct bdw_flip_source *source;
 struct bdw_flip_binding binding;
 struct bdw_flip_io io,original_io;
 struct bdw_flip_progress p;
 uint64_t end,clock_at,clock_previous;
 uint32_t reason;
 bool busy,retained;
};
/* Zero-init, original-address, consumed-once, CPU0 serialized. Begin checks
 * geometry and programmed/live agreement, then arms exactly one PRI_SURF
 * store. It never waits for a frame. Each poll performs one finite observation
 * batch; the caller schedules further polls without blocking its UI loop.
 * One 100ms origin covers admission, store and every poll/final check.
 * Any started failure retains source/backing, with no retry/restore/free.
 * DONE proves a synchronous live-surface latch only. It does NOT release the
 * old buffer or prove all-reader/DMA/cache/prefetch retirement. The original
 * owner must establish those independently before reuse or mapping teardown.
 * No native caller or image is enabled by this trusted-provider core. */
bool intel_bdw_primary_flip_begin(struct intel_bdw_primary_flip *,struct bdw_flip_source *,struct bdw_flip_io);
enum bdw_flip_status intel_bdw_primary_flip_poll(struct intel_bdw_primary_flip *);
/* Arm a single return to the original scanout after a confirmed forward latch.
 * The provider registers a fresh source for the reverse mapping pair under the
 * uninterrupted original lease. Poll the return just like the forward flip.
 * Both latches leave all-reader retirement to the owner. */
bool intel_bdw_primary_flip_rollback(struct intel_bdw_primary_flip *forward,
                                    struct intel_bdw_primary_flip *back,
                                    struct bdw_flip_source *reverse);
#endif
