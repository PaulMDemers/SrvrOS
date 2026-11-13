#ifndef SRVROS_INTEL_SOURCE_TRANSFER_H
#define SRVROS_INTEL_SOURCE_TRANSFER_H
#include <srvros/intel_source_buffers.h>
enum intel_source_transfer_state {IST_EMPTY,IST_OPENING,IST_REFUSED,IST_ACTIVE,IST_CLOSING,IST_CLOSED,IST_FAULT};
enum intel_source_transfer_reason {IST_INPUT=1,IST_SOURCE=2,IST_CLOCK=4,IST_GUARD=8,IST_OWNER=16};
/* Actual provider evidence is supplied by a trusted adapter, never inferred
 * from the stable original entry. Native currently supplies {0,255}. */
struct intel_source_transfer_evidence {uint32_t assurances,obligations;};
struct intel_source_transfer_io {
 void *ctx;
 bool (*guard)(void *);
 uint64_t (*now)(void *);
 bool (*revalidate)(void *,const struct scanout_transfer_manifest *,enum scanout_transfer_phase);
};
/* Large, one-use original object, static/heap through faults. The complete
 * transfer owner stays nested inside the ORIGINAL source/buffer registration.
 * Its generic phase clock may never restart the preparation/GT deadlines. */
struct intel_source_transfer {
 const struct intel_source_transfer *object;
 struct intel_source_buffers *buffers;
 struct intel_nonzero_source *source;
 const struct scanout_handoff_snapshot *input;
 void *provider;
 const void *lifetime;
 struct scanout_memory_token memory[2];
 struct scanout_memory_hold *hold[2];
 uint64_t physical[2],serial[2],expected,origin,parent_begin,begin,end,last,clock_at,clock_prev;
 uint32_t epoch,kind,post_mode,state,reason,assurances,obligations,expected_assurances,expected_obligations;
 struct display_access_token exclusion;
 struct scanout_transfer owner;
 struct scanout_transfer_pool pool;
 struct scanout_space_pool space_pool;
 struct scanout_transfer_token token;
 struct intel_source_transfer_io io;
 struct intel_source_transfer_io original_io;
 uint32_t callbacks,clocks,borrows,returns;
 bool busy,registered,source_blocked,holds_blocked,refused,opened,checked,closed;
};
/* True may mean known refusal: inspect state. Refused admission still owns a
 * source child until close. Missing assurances/open obligations must cause no
 * transfer provider, transfer clock, arena creation or imported interval. */
bool intel_source_transfer_open(struct intel_source_transfer *,struct intel_source_buffers *,
 const struct display_access_token *,struct intel_source_transfer_evidence,struct intel_source_transfer_io);
bool intel_source_transfer_live(const struct intel_source_transfer *);
bool intel_source_transfer_check(struct intel_source_transfer *);
/* Stops admission and drains leases. A still-owned child may retire while the
 * owner closes; busy leases retain the original registration. Faults are
 * terminal and retain source/RAM/root; no forced close or optimistic retry. */
bool intel_source_transfer_close(struct intel_source_transfer *);
#endif
