#ifndef SRVROS_X86_CACHE_H
#define SRVROS_X86_CACHE_H
#include <stdbool.h>
#include <stdint.h>
#define X86_CACHE_VARIABLES 16u
#define X86_CACHE_FIXED 11u
#define X86_CACHE_UNKNOWN 255u
enum x86_cache_status {CACHE_REFUSED,CACHE_OBSERVED,CACHE_READ,CACHE_CLOCK,CACHE_CHANGED,CACHE_INVALID};
struct x86_cache_cpu {uint32_t vendor[3],signature,features,physical_bits;uint64_t cr0;};
struct x86_cache_snapshot {
    struct x86_cache_cpu cpu;
    uint64_t pat,cap,def_type,base[X86_CACHE_VARIABLES],mask[X86_CACHE_VARIABLES],fixed[X86_CACHE_FIXED];
    uint64_t begin_us,end_us;
    uint32_t status,reads,count,fixed_count;
};
/* CPU0-only, read-only, architectural feature-gated MSRs. No mapping or grant.
 * Callbacks must be bounded. Snapshots are observations, not atomic ownership. */
struct x86_cache_io {
    void *ctx;
    bool (*cpu)(void *,struct x86_cache_cpu *);
    bool (*msr)(void *,uint32_t,uint64_t *);
    uint64_t (*now)(void *);
};
bool x86_cache_snapshot(struct x86_cache_snapshot *,const struct x86_cache_io *);
bool x86_cache_equal(const struct x86_cache_snapshot *,const struct x86_cache_snapshot *);
/* Hypothetical 4KiB leaf, not a walk of an existing alias. Index is PAT:PCD:PWT.
 * Only pages >=1MiB: fixed MTRRs are captured but not interpreted here. Output
 * remains untouched on refusal, including undefined variable-MTRR overlaps. */
bool x86_cache_page(const struct x86_cache_snapshot *,uint64_t physical,unsigned pat_index,
                    unsigned *mtrr_type,unsigned *pat_type,unsigned *effective_type);
/* Pure hypothetical uniform policy over 1..4096 complete pages. Checks every
 * enabled variable-MTRR boundary, including interior holes; never assumes the
 * endpoints describe the interior. Outputs are untouched on refusal. This is
 * neither an actual-alias walk nor a coherence, mapping or GPU ownership grant. */
bool x86_cache_range(const struct x86_cache_snapshot *,uint64_t physical,uint32_t pages,
                    unsigned pat_index,unsigned *mtrr_type,unsigned *pat_type,
                    unsigned *effective_type,unsigned *checks);
void intel_cache_probe_boot(void);
bool intel_cache_probe_report(void);
/* Fresh recheck against completed native observation; not mapping authority. */
bool intel_cache_probe_uc_page(uint64_t physical);
/* Fresh complete CPU0 context equality only; no DMA/cache/mapping grant. */
bool intel_cache_probe_context(struct x86_cache_snapshot *);
/* Only the separate BDW request/ACK observation pages; fresh CPU context. */
bool intel_cache_probe_gt_uc_page(uint64_t physical,uint32_t offset);
bool intel_cache_probe_firmware_wb_page(uint64_t physical);
/* Exact display-HDA first page only; no widening of GPU observer gates. */
bool intel_cache_probe_hda_uc_page(uint64_t physical);
bool intel_cache_probe_reset_uc_page(uint64_t,uint32_t);
bool intel_cache_probe_completion_uc_page(uint64_t,uint32_t);
bool intel_cache_probe_reader_uc_page(uint64_t,uint32_t);
bool intel_cache_probe_engine_uc_page(uint64_t,uint32_t);
bool intel_cache_probe_wake_uc_page(uint64_t physical,uint32_t offset);
#endif
