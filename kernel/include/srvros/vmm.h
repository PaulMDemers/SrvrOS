#ifndef SRVROS_VMM_H
#define SRVROS_VMM_H

#include <stdbool.h>
#include <stdint.h>

#define VMM_PAGE_PRESENT 0x001
#define VMM_PAGE_WRITABLE 0x002
#define VMM_PAGE_USER 0x004
#define VMM_PAGE_WRITE_THROUGH 0x008
#define VMM_PAGE_CACHE_DISABLE 0x010
#define VMM_PAGE_NO_EXECUTE (1ull << 63)

/* CPU0, IRQs masked: observational guard only, no lease or SMP proof.
 * Requires current untagged kernel CR3 and no registered nonkernel root. */
bool vmm_diagnostic_sole_kernel_root(uint64_t expected);
/* Fresh read-only translation comparison, at most4096 aligned pages. Requires
 * the same sole-root/IRQ exclusion as above. Reads every 4K leaf (or the large
 * leaf covering it), sharing ancestors only inside this uninterrupted call.
 * No permission/cache-type, alias-completeness, pin, DMA or SMP assurance. */
bool vmm_diagnostic_contiguous_kernel_range(uint64_t root, uint64_t virtual_base,
    uint64_t physical_base, uint64_t pages);
enum vmm_range_reason {
    VMM_RANGE_IRQ=1,VMM_RANGE_ROOT,VMM_RANGE_KERNEL,VMM_RANGE_CURRENT,
    VMM_RANGE_PML4,VMM_RANGE_POINTER,VMM_RANGE_COUNT,VMM_RANGE_OCCUPANCY,
    VMM_RANGE_INPUT,VMM_RANGE_PML4_PRESENT,VMM_RANGE_PML4_LARGE,
    VMM_RANGE_TABLE_POINTER,VMM_RANGE_PDPT_PRESENT,VMM_RANGE_PD_PRESENT,
    VMM_RANGE_PTE_PRESENT,VMM_RANGE_PTE_ADDRESS,VMM_RANGE_LARGE_ADDRESS,
    VMM_RANGE_ATTRIBUTES,VMM_RANGE_RESERVED
};
/* Optional value-only witness from the original checks; no second probe.
 * phase0=input,1=entry root,2=walk,3=exit root,4=complete. level1..4 is
 * PML4..PT; checked is the completed page prefix. detail is input subreason,
 * IF bit, registered-root count or occupancy, never a retained table pointer. */
struct vmm_range_observation {
    uint64_t checked,detail;
    uint32_t phase,reason,level;
};
bool vmm_diagnostic_contiguous_kernel_range_observe(uint64_t root,
    uint64_t virtual_base,uint64_t physical_base,uint64_t pages,
    struct vmm_range_observation *);
/* Read-only observation of this requested alias only, not all physical aliases.
 * Same sole-root/IRQ and 1..4096-page constraints. Read every leaf; require exact
 * translation and uniform leaf PAT:PCD:PWT and combined RW/US/NX entry bits.
 * physical_bits is the already captured CPU width (36..52). Check address and
 * large-page reserved bits before following a table. NX reports entry bits,
 * not EFER.NXE or executable authority. A/D and nonleaf PWT/PCD are not data
 * cache selectors. No allocation, PTE/TLB/cache write or retained table pointer.
 * Output is untouched on failure. leaf_sizes: 1=4KiB,2=2MiB,4=1GiB.
 * Does not establish coherence, TLB state, ownership, SMP or alias completeness. */
struct vmm_alias_observation {
    uint32_t pages,leaves,pat_index,writable,user,no_execute,leaf_sizes;
};
bool vmm_diagnostic_kernel_alias(uint64_t root,uint64_t virtual_base,
    uint64_t physical_base,uint32_t pages,unsigned physical_bits,
    struct vmm_alias_observation *,struct vmm_range_observation *);
void vmm_init(void);
/* Page operations require canonical virtual addresses; mapping/mutation also
 * require page alignment. Invalid addresses are refused before table access. */
bool vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
bool vmm_unmap_page(uint64_t virtual_address, uint64_t *physical_address);
bool vmm_virt_to_phys(uint64_t virtual_address, uint64_t *physical_address);
bool vmm_user_range_mapped(uint64_t virtual_address, uint64_t length, bool writable);
/* CPU0, IRQs already masked: current registered user root and executable
 * entry permission only, valid in this uninterrupted excluded continuation.
 * Does not establish process/image identity or pin the mapping. */
bool vmm_user_exec_current(uint64_t address, uint64_t expected_cr3);
uint64_t vmm_kernel_address_space(void);
uint64_t vmm_current_address_space(void);
void vmm_switch_address_space(uint64_t address_space);
/* CPU0: allocate/zero a fresh PMM frame, then register, prepare and publish
 * under saved IRQ exclusion. Failure returns output zero and frees only that
 * fresh root after unregistering it. Preserves the caller's IRQ state. */
bool vmm_create_address_space(uint64_t *address_space_out);
void vmm_destroy_address_space(uint64_t address_space);
void vmm_refresh_kernel_mappings(uint64_t address_space);
/* CPU0, IRQs already masked. Validate registry against the kernel root, then
 * copy only its upper PML4 half to a registered nonkernel target. An active
 * target is accepted only when already identical, without a store or TLB claim.
 * Allocates nothing; provides no lifetime guarantee. The target may be staged. */
bool vmm_prepare_user_kernel_mappings(uint64_t target_root);
bool vmm_map_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags);
bool vmm_ensure_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags);
bool vmm_unmap_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t *physical_address);
bool vmm_protect_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t flags);
bool vmm_virt_to_phys_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t *physical_address);

#endif
