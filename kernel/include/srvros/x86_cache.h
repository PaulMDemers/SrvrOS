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
void intel_cache_probe_boot(void);
bool intel_cache_probe_report(void);
#endif
