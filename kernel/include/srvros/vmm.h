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
 * IF bit, current CR3 non-address bits, registered-root count or occupancy,
 * never a retained table pointer. */
struct vmm_range_observation {
    uint64_t checked,detail;
    uint32_t phase,reason,level;
};
/* Same original guard and short-circuit order; no table walk or second probe.
 * Success: phase4 and zero remaining fields. Failure: phase1, first reason.
 * Does not change IRQ state, CR3, roots or admission requirements. */
bool vmm_diagnostic_sole_kernel_root_observe(uint64_t expected,
    struct vmm_range_observation *);
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
/* Read-only, bounded whole-root reverse lookup for at most two private spans.
 * Caller: boot CPU, IRQs already excluded, immutable captured RAM ranges;
 * four-level paging without PCID/PKU/PKS, no other live address space. Physical
 * width and NX/1GiB support bind to CPU0; CR4/EFER rechecked before return. Every
 * reachable present leaf is examined. Any overlap outside the expected alias
 * refuses, including aliases with identical attributes. No allocator, mapping,
 * pin, SMP/TLB/coherence/DMA/firmware authority or persistent table pointers.
 * RAM ranges are sorted/nonoverlapping; table pages and requested spans must
 * fit wholly within them. A partial scan leaves the output untouched. */
#define VMM_ALIAS_SPANS 2u
#define VMM_ALIAS_TABLE_LIMIT 256u
#define VMM_ALIAS_RAM_LIMIT 256u
struct vmm_alias_span {uint64_t virtual_base,physical;uint32_t pages;};
struct vmm_alias_ram {uint64_t base,length;};
struct vmm_alias_inventory {uint32_t tables,entries,leaves,matches[VMM_ALIAS_SPANS],pages[VMM_ALIAS_SPANS];};
enum vmm_alias_reason {VMM_ALIAS_INPUT=1,VMM_ALIAS_ROOT,VMM_ALIAS_MODE,
 VMM_ALIAS_RAM,VMM_ALIAS_TABLE,VMM_ALIAS_CYCLE,VMM_ALIAS_LIMIT,
 VMM_ALIAS_RESERVED,VMM_ALIAS_FOREIGN,VMM_ALIAS_COVERAGE,VMM_ALIAS_TABLE_BACKING};
struct vmm_alias_detail {uint32_t reason,level;struct vmm_range_observation root;};
/* Require exact, unique read-only supervisor NX UC-index-2 register leaves
 * in the sole boot root. Caller holds CPU0 and checks CR0.WP and effective UC. */
bool vmm_diagnostic_readonly_register_aliases(uint64_t root,const struct vmm_alias_span *,
 unsigned count,unsigned bits,const struct vmm_alias_ram *,unsigned ram_count);
bool vmm_diagnostic_unique_kernel_aliases(uint64_t root,
 const struct vmm_alias_span *,unsigned count,unsigned physical_bits,
 const struct vmm_alias_ram *,unsigned ram_count,
 struct vmm_alias_inventory *,struct vmm_alias_detail *);

/* Same bounded whole-root read-only observer, but require NO present leaf
 * intersecting either physical span. Targets may be firmware-reserved/MMIO;
 * only page-table backing must fit the immutable RAM whitelist. Never follow a
 * table in a target span. No target load, allocation, unmap or ownership grant.
 * A present-leaf refusal reports the original first leaf after final context
 * guards, not a second probe. Prefix is not an exhaustive alias inventory.
 * The ordinary inventory output remains untouched on every refusal. */
struct vmm_physical_span {uint64_t physical;uint32_t pages;};
#define VMM_ALIAS_PRESENT 12u
struct vmm_unmapped_detail {
 struct vmm_alias_detail walk;
 struct vmm_alias_inventory prefix;
 uint64_t virtual_base,physical,bytes,entry;
 uint32_t span;
};
bool vmm_diagnostic_kernel_physical_unmapped(uint64_t root,
 const struct vmm_physical_span *,unsigned count,unsigned physical_bits,
 const struct vmm_alias_ram *,unsigned ram_count,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);

/* Boot CPU0 only, IRQs already excluded, no APs or user roots. Original-address
 * zero-init/single-use handle for an exclusive CPU mapping deny interval. Begin
 * checks actual PMM RESERVED tags and whole-root absence; public VMM mutations
 * cannot introduce aliases or roots until close. In-flight mutations refuse
 * admission. The two spans and RAM whitelist are copied, never table pointers.
 * Check/close repeat tags, absence and original CR4/EFER/root validation. Any
 * uncertain live check retains a faulted guard until reset. Wrong/copied handles
 * cannot release it. No PMM tag/bitmap change, free, target load, mapping store,
 * hardware/firmware/DMA/cache/reader right or writable CPU alias is supplied.
 * Caller maintains original enclosing exclusion/deadline/storage throughout. */
struct vmm_reserved_guard {const struct vmm_reserved_guard *object;uint64_t epoch;};
#define VMM_ALIAS_PMM_RESERVED 13u
#define VMM_ALIAS_GUARD 14u
#define VMM_ALIAS_BUSY 15u
enum vmm_reserved_guard_state {VMM_GUARD_EMPTY,VMM_GUARD_PREPARING,VMM_GUARD_ACTIVE,
 VMM_GUARD_CHECKING,VMM_GUARD_FAULT};
bool vmm_reserved_guard_begin(struct vmm_reserved_guard *,uint64_t root,
 const struct vmm_physical_span *,unsigned count,unsigned physical_bits,
 const struct vmm_alias_ram *,unsigned ram_count,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);
/* Also exclude the CPU-visible device aperture page for this guard's full
 * lifetime. The device owner supplies its decoded BAR address. The MMIO page
 * is checked for aliases separately from the PMM-tagged backing spans. */
bool vmm_reserved_guard_begin_io_page(struct vmm_reserved_guard *,uint64_t root,
 const struct vmm_physical_span *,unsigned count,unsigned bits,
 const struct vmm_alias_ram *,unsigned ram_count,uint64_t io_page,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);
/* Keep the aperture and the RAM page storing its translation inaccessible.
 * The driver supplies the decoded register BAR page and locked table backing. */
bool vmm_reserved_guard_begin_register_window(struct vmm_reserved_guard *,uint64_t root,
 const struct vmm_physical_span *,unsigned count,unsigned bits,const struct vmm_alias_ram *,unsigned ram_count,
 uint64_t aperture,uint64_t backing,uint64_t reg,struct vmm_alias_inventory *,struct vmm_unmapped_detail *);
/* Additional MMIO pages must have no CPU alias at admission. The same
 * guard refuses later mappings until its final absence check and close. */
#define VMM_GUARD_IO_LIMIT 7u
bool vmm_reserved_guard_begin_submission_window(struct vmm_reserved_guard *,uint64_t root,
 const struct vmm_physical_span *,unsigned count,unsigned bits,const struct vmm_alias_ram *,unsigned ram_count,
 uint64_t aperture,uint64_t backing,uint64_t reg,const uint64_t *ports,unsigned port_count,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);
/* Adopt the only writable MMIO alias. The VMM freezes its VA and rejects
 * other mappings of its PA until this child retires. No device store occurs. */
bool vmm_reserved_guard_pin_register(const struct vmm_reserved_guard *,uint64_t root,uint64_t aperture,
 uint64_t reg,uint64_t alias,const void *child);
bool vmm_reserved_guard_register_pinned(const struct vmm_reserved_guard *,uint64_t root,uint64_t aperture,
 uint64_t reg,uint64_t alias,const void *child);
bool vmm_reserved_guard_io_page_held(const struct vmm_reserved_guard *,uint64_t root,uint64_t io_page);
/* CPU0 with interrupts disabled. A child pins this exact root and aperture
 * page until it returns the borrow. A parent close is retryable while pinned. */
bool vmm_reserved_guard_pin_io_page(const struct vmm_reserved_guard *,uint64_t root,uint64_t page,const void *child);
bool vmm_reserved_guard_io_page_pinned(const struct vmm_reserved_guard *,uint64_t root,uint64_t page,const void *child);
bool vmm_reserved_guard_unpin_io_page(const struct vmm_reserved_guard *,uint64_t root,uint64_t page,const void *child);
bool vmm_reserved_guard_check(struct vmm_reserved_guard *,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);
bool vmm_reserved_guard_close(struct vmm_reserved_guard *,
 struct vmm_alias_inventory *,struct vmm_unmapped_detail *);

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
