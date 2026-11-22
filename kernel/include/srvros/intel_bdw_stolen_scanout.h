#ifndef SRVROS_INTEL_BDW_STOLEN_SCANOUT_H
#define SRVROS_INTEL_BDW_STOLEN_SCANOUT_H
#include <stdbool.h>
#include <stdint.h>

/* A prospective use of EXISTING mappings, not a GTT writer or allocator.
 * Geometry/complete-vector classification supplies no firmware transfer,
 * cache/reader retirement or permission to modify stolen RAM. */
struct bdw_stolen_facts {
 uint32_t host,gpu,subsystem,ggc,bdsm,bgsm,tolud,reserved;
 uint64_t aperture_bytes,ggtt_bytes,fence_begin,fence_end;
 uint32_t width,height,stride;
};
struct bdw_stolen_range {uint64_t gpu,bytes;uint32_t pages;};
struct bdw_stolen_plan {
 uint64_t data_begin,data_end,usable_end,gtt_begin,gtt_end;
 uint64_t pixel_bytes,translation_bytes;
 struct bdw_stolen_range range[3]; /* full old fence, then two linear buffers */
};
struct bdw_stolen_vectors {
 const uint64_t *pte[2][3];
 uint32_t count[2][3];
};
struct bdw_stolen_layout {
 struct bdw_stolen_plan plan;
 uint64_t physical[3],physical_end[3];
 uint32_t checked;
 bool classified;
};
/* Complete contiguous firmware import following the active framebuffer.
 * Return its exclusive end in GPU pages, or zero for an inconsistent layout. */
uint32_t intel_bdw_stolen_import_end(const struct bdw_stolen_layout *, uint64_t fence_end,
                                     uint64_t aperture_bytes);

/* Decode locked PCI host bounds and the BDW WOPCM reservation. Every proposed
 * new GPU range is outside the complete inherited fence, with256KiB alignment
 * and the128-PTE display tail. The first stolen RAM page is never reusable. */
bool intel_bdw_stolen_scanout_plan(const struct bdw_stolen_facts *,struct bdw_stolen_plan *);
/* Require two complete equal, present, contiguous, low32-bit/flag1 vectors for
 * all three ranges. Check all physical extents against DSM, GTT, WOPCM and
 * each other. Data only: never infer that an otherwise fitting range is free.
 * Refusal clears the output; overlapping input/output is rejected untouched. */
bool intel_bdw_stolen_scanout_check(const struct bdw_stolen_facts *,const struct bdw_stolen_vectors *,struct bdw_stolen_layout *);
/* Separate read-only native image. Ordinary image supplies only a refusal
 * stub. This does not write PTEs, framebuffer RAM, plane or engine registers. */
/* Pure exact terminal-capture recipe; only the isolated image admits it.
 * No process creation, hardware access or persistent diagnostic state. */
bool intel_stolen_probe_recipe(const char *cmdline);
bool intel_stolen_probe_report(const char *cmdline);
bool intel_stolen_probe_retained(void);
#endif
