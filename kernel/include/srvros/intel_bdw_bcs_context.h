#ifndef SRVROS_INTEL_BDW_BCS_CONTEXT_H
#define SRVROS_INTEL_BDW_BCS_CONTEXT_H
#include <stdbool.h>
#include <stdint.h>
#define BDW_BCS_IMAGE_BYTES 16384u
#define BDW_BCS_IMAGE_WORDS 4096u
#define BDW_BCS_STATE_WORDS 80u
/* CPU image constructor only. Prospective GGTT addresses are not ownership.
 * One private ring, two context pages (HWSP then register state), completion.
 * Narrow legacy32/GGTT-only candidate: PPGTT disabled, all PDP registers zero.
 * Does not certify actual mode, workaround/cache policy, context-ID uniqueness,
 * translations, visibility or execution. In particular reserved descriptor bit5
 * stays clear; a real provider must resolve coherency before any submission. */
struct intel_bdw_bcs_context_config {
 uint64_t ring_gpu,context_gpu,completion_gpu,aperture_bytes,fence_begin,fence_end,nonce;
 uint32_t context_id,vendor,device;
};
/* Entire private 16KiB arena is initialized, including all unused bytes.
 * Original config/output/descriptor must be disjoint; no changes on bad input.
 * Does not map, publish, flush CPU caches, access hardware, submit or own RAM.
 * Caller supplies independently owned writable CPU RAM with stable config. */
bool intel_bdw_bcs_context_build(const struct intel_bdw_bcs_context_config *,volatile uint32_t *,uint64_t *descriptor);
bool intel_bdw_bcs_context_verify(const struct intel_bdw_bcs_context_config *,volatile const uint32_t *,uint64_t descriptor);
/* Legacy32 BCS context using four consecutive private directory frames.
 * Its ring dispatches the BCS batch in the private command page. */
bool intel_bdw_bcs_private_context_build(const struct intel_bdw_bcs_context_config *,uint64_t pdp0,
                                         volatile uint32_t *,uint64_t *descriptor);
bool intel_bdw_bcs_private_context_verify(const struct intel_bdw_bcs_context_config *,uint64_t pdp0,
                                          volatile const uint32_t *,uint64_t descriptor);
/* Candidate RCS indirect context for the borrowed 32-page command interval.
 * Pages 0..3 belong to BCS. Page 4 holds this batch, and page 5 holds its
 * private register-save word. Preparing it on the CPU grants no GPU access. */
struct intel_bdw_rcs_indirect {
 uint32_t pointer, words[32];
};
bool intel_bdw_rcs_indirect_build(uint32_t first, uint32_t pages, struct intel_bdw_rcs_indirect *);
bool intel_bdw_rcs_indirect_verify(uint32_t first, uint32_t pages, const struct intel_bdw_rcs_indirect *);
#endif
