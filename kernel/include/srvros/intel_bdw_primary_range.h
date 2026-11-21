#ifndef SRVROS_INTEL_BDW_PRIMARY_RANGE_H
#define SRVROS_INTEL_BDW_PRIMARY_RANGE_H
#include <stdbool.h>
#include <stdint.h>

/* Pure interpretation of two Broadwell raw snapshots, never a live owner.
 * pipe[] follows intel_capture_pipe_offsets; reader[] follows the existing
 * thirteen programmed/live/stereo primary/cursor/sprite words. Captures must
 * be power/resource-qualified by the caller. No callback, I/O or token exists. */
struct intel_bdw_primary_sample {
 uint32_t source[3],pipe[3][9],reader[3][13];
};
struct intel_bdw_primary_input {
 uint64_t aperture_bytes,ggtt_bytes,fence_begin,fence_end;
 uint32_t first,pages,samples;
 struct intel_bdw_primary_sample sample[2];
};
enum intel_bdw_primary_reason {
 BDW_PRIMARY_INPUT=1,BDW_PRIMARY_UNSTABLE=2,BDW_PRIMARY_LAYOUT=3,
 BDW_PRIMARY_OTHER_READER=4,BDW_PRIMARY_EXTENT=5,BDW_PRIMARY_OVERLAP=6,
 BDW_PRIMARY_BACKING=7
};
struct intel_bdw_primary_range {
 uint64_t begin,end,pte_end,candidate_begin,candidate_end;
 uint32_t width,height,stride,tiled,reason;
 bool classified;
};
/* Conservative primary-A-only, BGRX8888, linear/X-tiled, zero panning,
 * no rotation/async, coincident programmed/live right/left-eye addresses,
 * other primary/cursor/sprite controls zero. Zero left base is NOT evidence
 * that stereo mode is off; all four potential eye addresses must coincide.
 * Requires the documented256KiB surface alignment and128-PTE tail, and
 * includes full padded rows.
 * Requires that entire envelope stay inside the caller's already validated
 * protected fence and candidate outside that FULL fence, not just pixels.
 * Success proves arithmetic/observed layout only: no DMA/prefetch retirement,
 * pending-flip/firmware/FBC/PSR exclusion, HANDOFF assurance or GPU transfer.
 * Rejected inputs produce no usable span; overlapping input/output refuses
 * without modifying either. Host-only until a separately qualified caller. */
bool intel_bdw_primary_range_check(const struct intel_bdw_primary_input *,struct intel_bdw_primary_range *);

struct intel_backing_result;
/* This Air's original image and tail are one contiguous run of present,
 * below-4GiB stolen pages. Check that the candidate's original page does
 * not alias any of them. The original target stays firmware-owned. */
bool intel_bdw_primary_backing_check(const struct intel_bdw_primary_range *,
    const struct intel_backing_result *,const uint64_t *tail,unsigned tail_pages,
    uint64_t candidate_pte);
#endif
