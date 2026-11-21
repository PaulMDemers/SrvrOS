#ifndef SRVROS_INTEL_BDW_STOLEN_BORROW_H
#define SRVROS_INTEL_BDW_STOLEN_BORROW_H
#include <srvros/intel_bdw_stolen_scanout.h>
#include <srvros/scanout_memory.h>
#include <srvros/display_access.h>

/* Trusted CPU0-serialized adapter, not a hardware ownership mechanism.
 * No native producer currently supplies these rights. In particular a census,
 * EFI memory-map reservation or stable PTE does not supply any of them. */
#define BDW_BORROW_ALL 127u
#define BDW_BORROW_OPERATION_US 100000u
enum bdw_borrow_right {
 BDW_BORROW_RESERVATION=1,BDW_BORROW_FIRMWARE=2,BDW_BORROW_ENGINES=4,
 BDW_BORROW_DMA=8,BDW_BORROW_CACHE=16,BDW_BORROW_READERS=32,BDW_BORROW_MAPPINGS=64
};
enum bdw_borrow_phase {BDW_BORROW_ADMIT,BDW_BORROW_LIVE,BDW_BORROW_RETURN};
enum bdw_borrow_state {BDW_BORROW_EMPTY,BDW_BORROW_ACTIVE,BDW_BORROW_CLOSING,BDW_BORROW_CLOSED,BDW_BORROW_FAULT};
enum bdw_borrow_reason {BDW_BORROW_IDENTITY=1,BDW_BORROW_CLOCK=2,BDW_BORROW_PROOF=4,BDW_BORROW_EXCLUSION=8,BDW_BORROW_CHILD=16};
struct bdw_stolen_binding {
 const void *provider,*lifetime;
 uint64_t owner,root,epoch,admitted_at,expires_at;
 struct bdw_stolen_facts facts;
 struct bdw_stolen_layout layout;
};
struct intel_bdw_stolen_borrow;
struct bdw_stolen_source {
 const struct bdw_stolen_source *object;
 struct intel_bdw_stolen_borrow *consumer;
 struct bdw_stolen_binding binding;
 uint32_t rights;
 bool consumed;
};
struct bdw_stolen_proof {
 const struct bdw_stolen_source *source;
 const void *provider,*lifetime;
 uint64_t owner,root,epoch,checked_at;
 uint32_t rights,phase;
};
struct bdw_stolen_borrow_io {
 void *ctx;
 /* Fresh original-lifetime proof of the exact two spans, their firmware
  * reservation and engine/DMA/cache/reader exclusion. LIVE keeps ownership
  * while registered children exist; RETURN additionally proves all readers,
  * CPU aliases and visibility obligations retired and original state intact.
  * Read-only validation: never acquire new ownership or mutate these objects.
  * The original source clock/deadline must remain enforced by the provider. */
 bool (*validate)(void *,const struct bdw_stolen_source *,enum bdw_borrow_phase,struct bdw_stolen_proof *);
 uint64_t (*now)(void *);
};
struct bdw_stolen_slot {
 const struct bdw_stolen_slot *object;
 struct intel_bdw_stolen_borrow *parent;
 struct scanout_memory *child;
 uint32_t index,pages;
 bool claimed,returned;
};
struct intel_bdw_stolen_borrow {
 const struct intel_bdw_stolen_borrow *object;
 struct bdw_stolen_source *source;
 struct bdw_stolen_binding binding;
 struct bdw_stolen_borrow_io io,original_io;
 struct display_access_token exclusion;
 struct bdw_stolen_slot slot[2];
 uint64_t last;
 uint32_t state,reason,checks;
 bool pinned,busy,creating;
};
struct bdw_stolen_borrow_token {const struct intel_bdw_stolen_borrow *object;uint64_t owner,epoch;};
/* Original-address, zero-init, one use. Source storage/context and all child
 * storage must survive faults. Register before callbacks and pin the actual
 * display exclusion. Missing rights refuses without callbacks or registration;
 * any failure after pinning retains the source/exclusion, including admission.
 * Fixed parent expiry is never restarted; each validation also has a100ms cap. */
bool intel_bdw_stolen_borrow_begin(struct intel_bdw_stolen_borrow *,struct bdw_stolen_source *,
 const struct display_access_token *,struct bdw_stolen_borrow_io,struct bdw_stolen_borrow_token *);
bool intel_bdw_stolen_borrow_check(struct intel_bdw_stolen_borrow *,const struct bdw_stolen_borrow_token *);
/* One existing scanout_memory child per candidate. The complete candidate,
 * including padding/translation tail, remains reserved even if the child uses
 * fewer pages. No PMM allocate/free, CPU alias, PTE store or publication here.
 * A CPU borrow returns a physical range, never an admitted virtual mapping. */
bool intel_bdw_stolen_borrow_memory(struct intel_bdw_stolen_borrow *,const struct bdw_stolen_borrow_token *,
 unsigned,struct scanout_memory *,struct scanout_memory_pool *,const struct scanout_memory_layout *,struct scanout_memory_token *);
/* Permanently stop new children/CPU borrows first. Existing CPU borrows/holds
 * may finish through the ordinary memory API. Wait without unpinning while a
 * child remains. Only fresh RETURN proof allows original registration/unpin;
 * returning a child never frees firmware RAM or returns the outer reservation.
 * No retries after uncertainty; no native memory grant follows from this core. */
bool intel_bdw_stolen_borrow_close(struct intel_bdw_stolen_borrow *,const struct bdw_stolen_borrow_token *);
#endif
