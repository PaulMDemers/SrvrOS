#ifndef SRVROS_INTEL_GT_ACCESS_H
#define SRVROS_INTEL_GT_ACCESS_H
#include <srvros/display_access.h>
#define INTEL_GT_REQUEST 0xa188u
#define INTEL_GT_ACK 0x130044u
#define INTEL_GT_THREAD 0x13805cu
#define GT_ACCESS_INITIALIZE 1u
#define GT_ACCESS_REQUESTERS_RETIRED 2u
#define GT_ACCESS_KERNEL_BIT 4u
#define GT_ACCESS_REQUIRED 7u
#define GT_ACCESS_EXCLUSION 8u
#define GT_ACCESS_INPUT 16u
#define GT_ACCESS_STATE 32u
#define GT_ACCESS_GUARD 64u
#define GT_ACCESS_IO 128u
#define GT_ACCESS_CLOCK 256u
#define GT_ACCESS_FOREIGN 512u
#define GT_ACCESS_TIMEOUT 1024u
#define GT_ACCESS_POLL_CAP 4096u
#define GT_ACCESS_PHASE_US 100000ull
#define GT_ACCESS_TOTAL_US 300000ull
/* BDW PRM2c pp493-495: mandatory initial all-request clear, then the thread's
 * masked request/acknowledgement protocol. Pinned i915 additionally waits for
 * GT thread C0. Not a GPU/firmware, range, reset or engine-stop authority.
 * Zero-initialize owners. Boot CPU serialized; independently establish all grants.
 * Native writers are isolated, explicit cooperative boot diagnostics only.
 * Snapshot zeros do not supply grants. Request comparisons use low16 bits;
 * the raw upper16 mask is retained but is not requester state. */
enum intel_gt_state {GT_ACCESS_EMPTY,GT_ACCESS_OPENING,GT_ACCESS_AWAKE,GT_ACCESS_CLOSED,GT_ACCESS_FAULT};
enum intel_gt_write {GT_WRITE_NONE,GT_WRITE_DONE,GT_WRITE_UNCERTAIN};
/* Private provider context/grants must survive through retirement. Supply
 * ordered UC DWORD I/O: DONE means definitely issued, not hardware completion;
 * reads/ACK prove completion. NONE guarantees no issued write. */
struct intel_gt_io {
    void *context;
    bool (*guard)(void *); /* Exact resource/profile, all grants still held. */
    bool (*read)(void *,uint32_t,uint32_t *);
    enum intel_gt_write (*write)(void *,uint32_t,uint32_t);
    uint64_t (*now)(void *);
    void (*pause)(void *); /* Bounded service/delay, never changes this owner. */
};
struct intel_gt_access {
    const struct intel_gt_access *object;
    struct display_access_token exclusion;
    struct intel_gt_io io;
    uint64_t begin,last;
    uint32_t state,reason,requests,ack,thread,write_attempts,writes,polls;
    bool pinned;
    unsigned borrows;
    struct intel_gt_borrow *dependents;
};
struct intel_gt_borrow {
    const struct intel_gt_borrow *object;
    struct intel_gt_access *parent;
    struct intel_gt_borrow *next;
    bool active;
};
/* Address-sealed one-use dependent. A live/faulted dependent prevents parent
 * close and requester restoration. Only a clean dependent may return its pin. */
bool intel_gt_access_borrow(struct intel_gt_access *,struct intel_gt_borrow *);
bool intel_gt_access_return(struct intel_gt_borrow *);
bool intel_gt_access_open(struct intel_gt_access *,const struct display_access_token *,uint32_t grants,struct intel_gt_io);
bool intel_gt_access_check(struct intel_gt_access *);
/* Faults/uncertain completion retain the CPU exclusion pin until reset. No
 * blind retries, fallback-bit toggles, global resets or forced lease release. */
bool intel_gt_access_close(struct intel_gt_access *);
#endif
