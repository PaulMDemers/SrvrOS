#include <srvros/console.h>
#include <srvros/halt.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>

#define ENTRIES_PER_TABLE 512
#define PAGE_MASK 0x000ffffffffff000ull
#define PAGE_2M_MASK 0x000fffffffe00000ull
#define PAGE_1G_MASK 0x000fffffc0000000ull
#define PAGE_LARGE (1ull << 7)
#define PAGE_SIZE 4096
#define VMM_MAX_ADDRESS_SPACES 64
#define VMM_MAX_KERNEL_MAPPINGS 128
#define VMM_MAX_TRACKED_TABLES 256
#define VMM_PAGE_TABLE_MIN_PHYSICAL 0x2000000ull

struct kernel_mapping {
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t flags;
};

struct address_space_record {
    uint64_t root;
    uint64_t tables[VMM_MAX_TRACKED_TABLES];
    uint64_t table_count;
};

static uint64_t *kernel_pml4;
static uint64_t kernel_cr3;
static uint64_t address_spaces[VMM_MAX_ADDRESS_SPACES];
static uint64_t address_space_count;
static struct address_space_record address_space_records[VMM_MAX_ADDRESS_SPACES];
/* Authoritative occupancy flags, not a cached count/summary. All record users
 * read/write this bitmap. A sole-root check reads every slot in one live load. */
_Static_assert(VMM_MAX_ADDRESS_SPACES == 64, "occupancy bitmap capacity");
static uint64_t address_space_occupancy;
static bool address_space_slot_used(unsigned slot) {
    return (__atomic_load_n(&address_space_occupancy,__ATOMIC_RELAXED) >> slot) & 1;
}
static void address_space_set_occupied(unsigned slot,bool used) {
    uint64_t mask=1ull << slot;
    if(used)__atomic_fetch_or(&address_space_occupancy,mask,__ATOMIC_RELAXED);
    else __atomic_fetch_and(&address_space_occupancy,~mask,__ATOMIC_RELAXED);
}
static struct kernel_mapping kernel_mappings[VMM_MAX_KERNEL_MAPPINGS];
static uint64_t kernel_mapping_count;

static uint64_t read_cr3(void) {
#ifdef SRVROS_VMM_HOST_TEST
    return vmm_test_cr3();
#else
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
#endif
}

static void write_cr3(uint64_t value) {
#ifdef SRVROS_VMM_HOST_TEST
    vmm_test_write_cr3(value);
#else
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
#endif
}

static void invlpg(uint64_t virtual_address) {
#ifdef SRVROS_VMM_HOST_TEST
    (void)virtual_address;
#else
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
#endif
}

static uint16_t pml4_index(uint64_t virtual_address) {
    return (virtual_address >> 39) & 0x1ff;
}

static uint16_t pdpt_index(uint64_t virtual_address) {
    return (virtual_address >> 30) & 0x1ff;
}

static uint16_t pd_index(uint64_t virtual_address) {
    return (virtual_address >> 21) & 0x1ff;
}

static uint16_t pt_index(uint64_t virtual_address) {
    return (virtual_address >> 12) & 0x1ff;
}

static uint64_t *entry_table(uint64_t entry) {
    return pmm_phys_to_virt(entry & PAGE_MASK);
}

static bool is_canonical(uint64_t virtual_address) {
    uint64_t top = virtual_address >> 47;
    return top == 0 || top == 0x1ffff;
}

static void zero_page(uint64_t *page) {
    for (uint64_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        page[i] = 0;
    }
}

static struct address_space_record *find_address_space_record(uint64_t address_space) {
    for (uint64_t i = 0; i < VMM_MAX_ADDRESS_SPACES; i++) {
        if (address_space_slot_used(i) && address_space_records[i].root == address_space) {
            return &address_space_records[i];
        }
    }
    return NULL;
}

static bool track_page_table(uint64_t address_space, uint64_t physical) {
    if (address_space == 0 || address_space == kernel_cr3) {
        return true;
    }

    struct address_space_record *record = find_address_space_record(address_space);
    if (record == NULL || record->table_count >= VMM_MAX_TRACKED_TABLES) {
        return false;
    }

    record->tables[record->table_count++] = physical;
    return true;
}

static uint64_t *ensure_next_table(uint64_t address_space,
    uint64_t *table,
    uint16_t index,
    uint64_t flags) {
    if (!is_canonical((uint64_t)table)) {
        return NULL;
    }
    if ((table[index] & VMM_PAGE_PRESENT) != 0) {
        if ((table[index] & PAGE_LARGE) != 0) {
            return NULL;
        }
        uint64_t *next = entry_table(table[index]);
        if (is_canonical((uint64_t)next)) {
            return next;
        }
    }

    uint64_t physical = pmm_alloc_frames_above_tagged(1,
        VMM_PAGE_TABLE_MIN_PHYSICAL,
        PMM_FRAME_PAGE_TABLE);
    if (physical == 0) {
        return NULL;
    }

    uint64_t *next = pmm_phys_to_virt(physical);
    zero_page(next);
    if (!track_page_table(address_space, physical)) {
        pmm_free_frame(physical);
        return NULL;
    }

    table[index] = physical | flags | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
    return next;
}

static uint64_t active_address_space(void) {
    uint64_t current = read_cr3() & PAGE_MASK;
    return current != 0 ? current : kernel_cr3;
}

static bool is_kernel_virtual(uint64_t virtual_address) {
    return (virtual_address >> 47) == 0x1ffff;
}

static bool register_address_space(uint64_t address_space) {
    if (address_space == 0 || address_space == kernel_cr3) {
        return true;
    }

    for (uint64_t i = 0; i < address_space_count; i++) {
        if (address_spaces[i] == address_space) {
            return true;
        }
    }
    if (address_space_count >= VMM_MAX_ADDRESS_SPACES) {
        return false;
    }
    struct address_space_record *record = NULL;
    for (uint64_t i = 0; i < VMM_MAX_ADDRESS_SPACES; i++) {
        if (!address_space_slot_used(i)) {
            record = &address_space_records[i];
            break;
        }
    }
    if (record == NULL) {
        return false;
    }

    address_spaces[address_space_count++] = address_space;
    address_space_set_occupied((unsigned)(record-address_space_records),true);
    record->root = address_space;
    record->table_count = 0;
    return true;
}

static void unregister_address_space(uint64_t address_space) {
    for (uint64_t i = 0; i < address_space_count; i++) {
        if (address_spaces[i] == address_space) {
            address_spaces[i] = address_spaces[--address_space_count];
            break;
        }
    }

    struct address_space_record *record = find_address_space_record(address_space);
    if (record != NULL) {
        address_space_set_occupied((unsigned)(record-address_space_records),false);
        record->root = 0;
        record->table_count = 0;
    }
}

static void register_kernel_mapping(uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags) {
    for (uint64_t i = 0; i < kernel_mapping_count; i++) {
        if (kernel_mappings[i].virtual_address == virtual_address) {
            kernel_mappings[i].physical_address = physical_address;
            kernel_mappings[i].flags = flags;
            return;
        }
    }

    if (kernel_mapping_count >= VMM_MAX_KERNEL_MAPPINGS) {
        return;
    }

    kernel_mappings[kernel_mapping_count++] = (struct kernel_mapping) {
        .virtual_address = virtual_address,
        .physical_address = physical_address,
        .flags = flags,
    };
}

void vmm_init(void) {
    kernel_cr3 = read_cr3() & PAGE_MASK;
    kernel_pml4 = pmm_phys_to_virt(kernel_cr3);

    console_printf("vmm: active pml4 phys=%x virt=%x\n",
        kernel_cr3,
        (uint64_t)kernel_pml4);
}

static bool range_refuse(struct vmm_range_observation *d,unsigned phase,unsigned reason,
    unsigned level,uint64_t checked,uint64_t detail) {
    if(d)*d=(struct vmm_range_observation){.phase=phase,.reason=reason,.level=level,.checked=checked,.detail=detail};
    return false;
}
static bool diagnostic_sole_root(uint64_t expected,struct vmm_range_observation *d,unsigned phase,uint64_t checked) {
    uint64_t flags;
#ifdef SRVROS_VMM_HOST_TEST
    flags=vmm_test_rflags();
#else
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags));
#endif
    /* Same short-circuit predicates/order as the boolean guard. Record only
     * values already read by this check; do not resample a failed context. */
    if(flags&(1ull<<9))return range_refuse(d,phase,VMM_RANGE_IRQ,0,checked,flags&(1ull<<9));
    if(!expected || (expected&~PAGE_MASK))return range_refuse(d,phase,VMM_RANGE_ROOT,0,checked,0);
    if(expected!=kernel_cr3)return range_refuse(d,phase,VMM_RANGE_KERNEL,0,checked,0);
    if(read_cr3()!=expected)return range_refuse(d,phase,VMM_RANGE_CURRENT,0,checked,0);
    if(!kernel_pml4)return range_refuse(d,phase,VMM_RANGE_PML4,0,checked,0);
    if(kernel_pml4!=pmm_phys_to_virt(expected))return range_refuse(d,phase,VMM_RANGE_POINTER,0,checked,0);
    uint64_t count=address_space_count;
    if(count)return range_refuse(d,phase,VMM_RANGE_COUNT,0,checked,count);
    uint64_t occupancy=__atomic_load_n(&address_space_occupancy,__ATOMIC_RELAXED);
    if(occupancy)return range_refuse(d,phase,VMM_RANGE_OCCUPANCY,0,checked,occupancy);
    return true;
}
bool vmm_diagnostic_sole_kernel_root(uint64_t expected) {
    return diagnostic_sole_root(expected,NULL,1,0);
}

bool vmm_diagnostic_contiguous_kernel_range_observe(uint64_t root,uint64_t va,uint64_t pa,uint64_t pages,
    struct vmm_range_observation *d) {
    if(d)*d=(struct vmm_range_observation){0};
    if(!pages || pages>4096)return range_refuse(d,0,VMM_RANGE_INPUT,0,0,1);
    if(va&(PAGE_SIZE-1))return range_refuse(d,0,VMM_RANGE_INPUT,0,0,2);
    if(pa&~PAGE_MASK)return range_refuse(d,0,VMM_RANGE_INPUT,0,0,3);
    if(va>UINT64_MAX-(pages-1)*PAGE_SIZE)return range_refuse(d,0,VMM_RANGE_INPUT,0,0,4);
    if(pa>PAGE_MASK-(pages-1)*PAGE_SIZE)return range_refuse(d,0,VMM_RANGE_INPUT,0,0,5);
    if(!is_canonical(va) || !is_canonical(va+(pages-1)*PAGE_SIZE))return range_refuse(d,0,VMM_RANGE_INPUT,0,0,6);
    if((va>>47)!=((va+(pages-1)*PAGE_SIZE)>>47))return range_refuse(d,0,VMM_RANGE_INPUT,0,0,7);
    if(!diagnostic_sole_root(root,d,1,0))return false;
    uint64_t done=0;
    /* Boot CPU and IRQ exclusion are observational preconditions, not a lease.
     * No callbacks, writes, allocations or persistent table pointers. Rewalk
     * from the root at every leaf-table boundary; read each PTE exactly once. */
    while (pages) {
        const volatile uint64_t *table=kernel_pml4;
        uint64_t entry=table[pml4_index(va)];
        if(!(entry&VMM_PAGE_PRESENT))return range_refuse(d,2,VMM_RANGE_PML4_PRESENT,1,done,0);
        if(entry&PAGE_LARGE)return range_refuse(d,2,VMM_RANGE_PML4_LARGE,1,done,0);
        table=entry_table(entry);
        if(!table || !is_canonical((uint64_t)table))return range_refuse(d,2,VMM_RANGE_TABLE_POINTER,2,done,0);
        entry=table[pdpt_index(va)];
        if(!(entry&VMM_PAGE_PRESENT))return range_refuse(d,2,VMM_RANGE_PDPT_PRESENT,2,done,0);
        uint64_t chunk,physical;
        if(entry&PAGE_LARGE) {
            physical=(entry&PAGE_1G_MASK)+(va&0x3fffffff);
            chunk=(0x40000000-(va&0x3fffffff))/PAGE_SIZE;
        } else {
            table=entry_table(entry);
            if(!table || !is_canonical((uint64_t)table))return range_refuse(d,2,VMM_RANGE_TABLE_POINTER,3,done,0);
            entry=table[pd_index(va)];
            if(!(entry&VMM_PAGE_PRESENT))return range_refuse(d,2,VMM_RANGE_PD_PRESENT,3,done,0);
            if(entry&PAGE_LARGE) {
                physical=(entry&PAGE_2M_MASK)+(va&0x1fffff);
                chunk=(0x200000-(va&0x1fffff))/PAGE_SIZE;
            } else {
                table=entry_table(entry);
                if(!table || !is_canonical((uint64_t)table))return range_refuse(d,2,VMM_RANGE_TABLE_POINTER,4,done,0);
                chunk=ENTRIES_PER_TABLE-pt_index(va);
                if(chunk>pages)chunk=pages;
                for(uint64_t i=0;i<chunk;i++) {
                    entry=table[pt_index(va)+i];
                    if(!(entry&VMM_PAGE_PRESENT))return range_refuse(d,2,VMM_RANGE_PTE_PRESENT,4,done+i,0);
                    if((entry&PAGE_MASK)!=pa+i*PAGE_SIZE)return range_refuse(d,2,VMM_RANGE_PTE_ADDRESS,4,done+i,0);
                }
                physical=pa;
            }
        }
        if(physical!=pa)return range_refuse(d,2,VMM_RANGE_LARGE_ADDRESS,0,done,0);
        if(chunk>pages)chunk=pages;
        pages-=chunk;done+=chunk;
        if(pages){va+=chunk*PAGE_SIZE;pa+=chunk*PAGE_SIZE;}
    }
    if(!diagnostic_sole_root(root,d,3,done))return false;
    if(d)*d=(struct vmm_range_observation){.phase=4,.checked=done};
    return true;
}
bool vmm_diagnostic_contiguous_kernel_range(uint64_t root,uint64_t va,uint64_t pa,uint64_t pages) {
    return vmm_diagnostic_contiguous_kernel_range_observe(root,va,pa,pages,NULL);
}

static bool map_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags,
    bool allow_existing) {
    if (!is_canonical(virtual_address) ||
        (virtual_address % PAGE_SIZE) != 0 || (physical_address % PAGE_SIZE) != 0) {
        return false;
    }
    if (address_space == 0) {
        address_space = kernel_cr3;
    }

    uint64_t table_flags = flags & VMM_PAGE_USER;
    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    uint64_t *pdpt = ensure_next_table(address_space, pml4, pml4_index(virtual_address), table_flags);
    if (pdpt == NULL) {
        return false;
    }

    uint64_t *pd = ensure_next_table(address_space, pdpt, pdpt_index(virtual_address), table_flags);
    if (pd == NULL) {
        return false;
    }

    uint64_t *pt = ensure_next_table(address_space, pd, pd_index(virtual_address), table_flags);
    if (pt == NULL) {
        return false;
    }

    uint16_t index = pt_index(virtual_address);
    if ((pt[index] & VMM_PAGE_PRESENT) != 0) {
        return allow_existing && (pt[index] & PAGE_MASK) == physical_address;
    }

    pt[index] = physical_address | flags | VMM_PAGE_PRESENT;
    if ((read_cr3() & PAGE_MASK) == address_space) {
        invlpg(virtual_address);
    }
    return true;
}

bool vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if (!is_kernel_virtual(virtual_address)) {
        return map_page_in_space(active_address_space(), virtual_address, physical_address, flags, false);
    }

    if (!map_page_in_space(kernel_cr3, virtual_address, physical_address, flags, true)) {
        return false;
    }
    register_kernel_mapping(virtual_address, physical_address, flags);

    for (uint64_t i = 0; i < address_space_count; i++) {
        if (!map_page_in_space(address_spaces[i], virtual_address, physical_address, flags, true)) {
            return false;
        }
    }

    return true;
}

bool vmm_map_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags) {
    return map_page_in_space(address_space, virtual_address, physical_address, flags, false);
}

bool vmm_ensure_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags) {
    return map_page_in_space(address_space, virtual_address, physical_address, flags, true);
}

bool vmm_unmap_page(uint64_t virtual_address, uint64_t *physical_address) {
    return vmm_unmap_page_in_space(active_address_space(), virtual_address, physical_address);
}

bool vmm_unmap_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t *physical_address) {
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;

    if (!is_canonical(virtual_address) || (virtual_address % PAGE_SIZE) != 0) {
        return false;
    }
    if (address_space == 0) {
        address_space = kernel_cr3;
    }

    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    entry = pml4[pml4_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    pdpt = entry_table(entry);
    if (!is_canonical((uint64_t)pdpt)) {
        return false;
    }

    entry = pdpt[pdpt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    pd = entry_table(entry);
    if (!is_canonical((uint64_t)pd)) {
        return false;
    }

    entry = pd[pd_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0 || (entry & PAGE_LARGE) != 0) {
        return false;
    }
    pt = entry_table(entry);
    if (!is_canonical((uint64_t)pt)) {
        return false;
    }

    uint16_t index = pt_index(virtual_address);
    entry = pt[index];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }

    if (physical_address != NULL) {
        *physical_address = entry & PAGE_MASK;
    }
    pt[index] = 0;
    if ((read_cr3() & PAGE_MASK) == address_space) {
        invlpg(virtual_address);
    }
    return true;
}

bool vmm_protect_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t flags) {
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;

    if (!is_canonical(virtual_address) || (virtual_address % PAGE_SIZE) != 0) {
        return false;
    }
    if (address_space == 0) {
        address_space = kernel_cr3;
    }

    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    entry = pml4[pml4_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    pdpt = entry_table(entry);
    if (!is_canonical((uint64_t)pdpt)) {
        return false;
    }

    entry = pdpt[pdpt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    pd = entry_table(entry);
    if (!is_canonical((uint64_t)pd)) {
        return false;
    }

    entry = pd[pd_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0 || (entry & PAGE_LARGE) != 0) {
        return false;
    }
    pt = entry_table(entry);
    if (!is_canonical((uint64_t)pt)) {
        return false;
    }

    uint16_t index = pt_index(virtual_address);
    entry = pt[index];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }

    pt[index] = (entry & PAGE_MASK) | flags | VMM_PAGE_PRESENT;
    if ((read_cr3() & PAGE_MASK) == address_space) {
        invlpg(virtual_address);
    }
    return true;
}

bool vmm_virt_to_phys(uint64_t virtual_address, uint64_t *physical_address) {
    return vmm_virt_to_phys_in_space(active_address_space(), virtual_address, physical_address);
}

bool vmm_virt_to_phys_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t *physical_address) {
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;
    if (physical_address == NULL || !is_canonical(virtual_address)) {
        return false;
    }
    if (address_space == 0) {
        address_space = kernel_cr3;
    }

    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    entry = pml4[pml4_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    pdpt = entry_table(entry);
    if (!is_canonical((uint64_t)pdpt)) {
        return false;
    }

    entry = pdpt[pdpt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    if ((entry & PAGE_LARGE) != 0) {
        *physical_address = (entry & PAGE_1G_MASK) + (virtual_address & 0x3fffffffull);
        return true;
    }
    pd = entry_table(entry);
    if (!is_canonical((uint64_t)pd)) {
        return false;
    }

    entry = pd[pd_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    if ((entry & PAGE_LARGE) != 0) {
        *physical_address = (entry & PAGE_2M_MASK) + (virtual_address & 0x1fffff);
        return true;
    }
    pt = entry_table(entry);
    if (!is_canonical((uint64_t)pt)) {
        return false;
    }

    entry = pt[pt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }

    *physical_address = (entry & PAGE_MASK) + (virtual_address & 0xfff);
    return true;
}

static bool vmm_entry_for_address(uint64_t address_space, uint64_t virtual_address, uint64_t *entry_out,
    uint64_t *leaf_size_out) {
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;
    uint64_t effective = VMM_PAGE_USER | VMM_PAGE_WRITABLE;
    uint64_t no_execute = 0;

    if (entry_out == NULL) {
        return false;
    }
    if (address_space == 0) {
        address_space = kernel_cr3;
    }

    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    entry = pml4[pml4_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0 || (entry & PAGE_LARGE) != 0) {
        return false;
    }
    effective &= entry;
    no_execute |= entry & VMM_PAGE_NO_EXECUTE;
    pdpt = entry_table(entry);
    if (!is_canonical((uint64_t)pdpt)) {
        return false;
    }

    entry = pdpt[pdpt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    effective &= entry;
    no_execute |= entry & VMM_PAGE_NO_EXECUTE;
    if ((entry & PAGE_LARGE) != 0) {
        *entry_out = (entry & ~(VMM_PAGE_USER | VMM_PAGE_WRITABLE)) | effective | no_execute;
        if (leaf_size_out) *leaf_size_out = 1ull << 30;
        return true;
    }
    pd = entry_table(entry);
    if (!is_canonical((uint64_t)pd)) {
        return false;
    }

    entry = pd[pd_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }
    effective &= entry;
    no_execute |= entry & VMM_PAGE_NO_EXECUTE;
    if ((entry & PAGE_LARGE) != 0) {
        *entry_out = (entry & ~(VMM_PAGE_USER | VMM_PAGE_WRITABLE)) | effective | no_execute;
        if (leaf_size_out) *leaf_size_out = 1ull << 21;
        return true;
    }

    pt = entry_table(entry);
    if (!is_canonical((uint64_t)pt)) {
        return false;
    }
    entry = pt[pt_index(virtual_address)];
    if ((entry & VMM_PAGE_PRESENT) == 0) {
        return false;
    }

    effective &= entry;
    no_execute |= entry & VMM_PAGE_NO_EXECUTE;
    *entry_out = (entry & ~(VMM_PAGE_USER | VMM_PAGE_WRITABLE)) | effective | no_execute;
    if (leaf_size_out) *leaf_size_out = PAGE_SIZE;
    return true;
}


/* Caller owns CPU0 IRQ exclusion through validation and publication. */
bool vmm_prepare_user_kernel_mappings(uint64_t target_root) {
    uint64_t flags;
#ifdef SRVROS_VMM_HOST_TEST
    flags=vmm_test_rflags();
#else
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
#endif
    if ((flags & (1ull << 9)) || !target_root || (target_root & ~PAGE_MASK) ||
        !kernel_cr3 || (kernel_cr3 & ~PAGE_MASK) || target_root == kernel_cr3 ||
        !kernel_pml4 || kernel_pml4 != pmm_phys_to_virt(kernel_cr3) ||
        !is_canonical((uint64_t)kernel_pml4) ||
        !find_address_space_record(target_root) || kernel_mapping_count > VMM_MAX_KERNEL_MAPPINGS)
        return false;
    uint64_t *target=pmm_phys_to_virt(target_root);
    if (!target || !is_canonical((uint64_t)target) || target == kernel_pml4) return false;
    const uint64_t supported=VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE |
        VMM_PAGE_WRITE_THROUGH | VMM_PAGE_CACHE_DISABLE | VMM_PAGE_NO_EXECUTE;
    bool needs_nx=false, needs_1g=false;
    for (uint64_t i=0; i<kernel_mapping_count; ++i) {
        const struct kernel_mapping *mapping=&kernel_mappings[i];
        uint64_t va=mapping->virtual_address, entry, leaf;
        if (!is_canonical(va) || (va >> 47) != 0x1ffff || (va & (PAGE_SIZE-1)) ||
            (mapping->physical_address & ~PAGE_MASK) || (mapping->flags & ~supported) ||
            !vmm_entry_for_address(kernel_cr3,va,&entry,&leaf)) return false;
        /* Bit 7 is 4K PAT but large-page PS; large-page PAT is bit 12. */
        uint64_t pat=leaf == PAGE_SIZE ? (1ull << 7) : (1ull << 12);
        if ((entry & pat) || (entry & VMM_PAGE_USER) ||
            ((entry ^ mapping->flags) & (VMM_PAGE_WRITABLE | VMM_PAGE_NO_EXECUTE |
                VMM_PAGE_WRITE_THROUGH | VMM_PAGE_CACHE_DISABLE))) return false;
        if (entry & VMM_PAGE_NO_EXECUTE) needs_nx=true;
        if (leaf == (1ull << 30)) needs_1g=true;
        uint64_t physical=leaf == (1ull << 30) ? (entry & PAGE_1G_MASK) :
            leaf == (1ull << 21) ? (entry & PAGE_2M_MASK) : (entry & PAGE_MASK);
        physical += va & (leaf - 1);
        if (physical != mapping->physical_address) return false;
    }
    if (needs_nx || needs_1g) {
        uint32_t eax, ebx, ecx, edx;
#ifdef SRVROS_VMM_HOST_TEST
        vmm_test_cpuid(0x80000000u,&eax,&ebx,&ecx,&edx);
#else
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                          : "a"(0x80000000u), "c"(0));
#endif
        if (eax < 0x80000001u) return false;
#ifdef SRVROS_VMM_HOST_TEST
        vmm_test_cpuid(0x80000001u,&eax,&ebx,&ecx,&edx);
#else
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                          : "a"(0x80000001u), "c"(0));
#endif
        if ((needs_nx && !(edx & (1u << 20))) ||
            (needs_1g && !(edx & (1u << 26)))) return false;
        if (needs_nx) {
            uint64_t efer;
#ifdef SRVROS_VMM_HOST_TEST
            efer=vmm_test_efer();
#else
            uint32_t lo, hi;
            __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xc0000080u));
            efer=((uint64_t)hi << 32) | lo;
#endif
            if (!(efer & (1ull << 11))) return false;
        }
    }
    /* An active root may retain cached translations. It is safe here only as
     * an already-identical no-op; no TLB-coherence claim is made. */
    if ((read_cr3() & PAGE_MASK) == target_root) {
        for (uint64_t i=ENTRIES_PER_TABLE/2; i<ENTRIES_PER_TABLE; ++i)
            if (target[i] != kernel_pml4[i]) return false;
        return true;
    }
    for (uint64_t i=ENTRIES_PER_TABLE/2; i<ENTRIES_PER_TABLE; ++i)
        target[i]=kernel_pml4[i];
    return true;
}

/* Entry permission only. CPU0, IRQs already masked; no scheduler or image
 * lifetime guarantee. The registry proves a current user root, not identity. */
bool vmm_user_exec_current(uint64_t address, uint64_t expected_cr3) {
    uint64_t flags;
#ifdef SRVROS_VMM_HOST_TEST
    flags=vmm_test_rflags();
#else
    __asm__ volatile ("pushfq; popq %0" : "=r"(flags) : : "memory");
#endif
    if ((flags & (1ull << 9)) || !address || (address >> 47) ||
        !expected_cr3 || (expected_cr3 & ~PAGE_MASK) || expected_cr3 == kernel_cr3)
        return false;
    uint64_t current=read_cr3() & PAGE_MASK;
    if (!current || current != expected_cr3 || !find_address_space_record(current))
        return false;
    uint32_t eax, ebx, ecx, edx;
#ifdef SRVROS_VMM_HOST_TEST
    vmm_test_cpuid(0x80000000u,&eax,&ebx,&ecx,&edx);
#else
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(0x80000000u), "c"(0));
#endif
    if (eax < 0x80000001u) return false;
#ifdef SRVROS_VMM_HOST_TEST
    vmm_test_cpuid(0x80000001u,&eax,&ebx,&ecx,&edx);
#else
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(0x80000001u), "c"(0));
#endif
    if (!(edx & (1u << 20))) return false;
    uint64_t efer;
#ifdef SRVROS_VMM_HOST_TEST
    efer=vmm_test_efer();
#else
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xc0000080u));
    efer=((uint64_t)hi << 32) | lo;
#endif
    if (!(efer & (1ull << 11))) return false;
    uint64_t entry, leaf_size;
    return vmm_entry_for_address(current,address,&entry,&leaf_size) &&
        (leaf_size != (1ull << 30) || (edx & (1u << 26))) &&
        (entry & VMM_PAGE_USER) && !(entry & VMM_PAGE_NO_EXECUTE);
}

bool vmm_user_range_mapped(uint64_t virtual_address, uint64_t length, bool writable) {
    if (length == 0) {
        return true;
    }
    if (virtual_address + length < virtual_address) {
        return false;
    }

    uint64_t end = virtual_address + length - 1;
    /* Page-table indices alone alias noncanonical addresses. User buffers
     * must remain entirely in the canonical lower half before dereference. */
    if ((virtual_address >> 47) != 0 || (end >> 47) != 0) {
        return false;
    }
    uint64_t page = virtual_address & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t address_space = active_address_space();

    while (page <= end) {
        uint64_t entry = 0;
        if (!vmm_entry_for_address(address_space, page, &entry, NULL) ||
            (entry & VMM_PAGE_USER) == 0 ||
            (writable && (entry & VMM_PAGE_WRITABLE) == 0)) {
            return false;
        }

        if (page > UINT64_MAX - PAGE_SIZE) {
            break;
        }
        page += PAGE_SIZE;
    }

    return true;
}

uint64_t vmm_kernel_address_space(void) {
    return kernel_cr3;
}

uint64_t vmm_current_address_space(void) {
    return active_address_space();
}

void vmm_switch_address_space(uint64_t address_space) {
    if (address_space == 0) {
        address_space = kernel_cr3;
    }
    if (address_space != 0 && (read_cr3() & PAGE_MASK) != address_space) {
        write_cr3(address_space);
    }
}

bool vmm_create_address_space(uint64_t *address_space_out) {
    if (address_space_out == NULL) return false;
    *address_space_out = 0;
    if (kernel_pml4 == NULL) return false;

    /* PMM returns a fresh, uniquely owned frame or zero on failure. */
    uint64_t physical = pmm_alloc_frames_above_tagged(1,
        VMM_PAGE_TABLE_MIN_PHYSICAL,
        PMM_FRAME_PAGE_TABLE);
    if (physical == 0) return false;
    uint64_t *pml4 = pmm_phys_to_virt(physical);
    zero_page(pml4);

    uint64_t flags;
#ifdef SRVROS_VMM_HOST_TEST
    flags=vmm_test_irq_save();
#else
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
#endif
    bool registered=register_address_space(physical);
    bool prepared=registered && vmm_prepare_user_kernel_mappings(physical);
    if (prepared) *address_space_out=physical;
    else if (registered) unregister_address_space(physical);
#ifdef SRVROS_VMM_HOST_TEST
    vmm_test_irq_restore(flags);
#else
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
    if (!prepared) pmm_free_frame(physical);
    return prepared;
}

void vmm_refresh_kernel_mappings(uint64_t address_space) {
    if (address_space == 0 || kernel_pml4 == NULL) {
        return;
    }

    uint64_t *pml4 = pmm_phys_to_virt(address_space);
    for (uint64_t i = ENTRIES_PER_TABLE / 2; i < ENTRIES_PER_TABLE; i++) {
        pml4[i] = kernel_pml4[i];
    }
    for (uint64_t i = 0; i < kernel_mapping_count; i++) {
        (void)map_page_in_space(address_space,
            kernel_mappings[i].virtual_address,
            kernel_mappings[i].physical_address,
            kernel_mappings[i].flags,
            true);
    }
}

void vmm_destroy_address_space(uint64_t address_space) {
    if (address_space == 0 || address_space == kernel_cr3) {
        return;
    }

    uint64_t current = read_cr3() & PAGE_MASK;
    if (current == address_space) {
        vmm_switch_address_space(kernel_cr3);
    }

    struct address_space_record *record = find_address_space_record(address_space);
    if (record != NULL) {
        for (uint64_t i = record->table_count; i > 0; i--) {
            pmm_free_frame(record->tables[i - 1]);
        }
    }
    unregister_address_space(address_space);
    pmm_free_frame(address_space);
}

/* Private CPU0 signal-frame copy policy. No page tables are read here. */
#include "../arch/x86_64/user_frame_copy.h"
struct user_frame_copy_cpu {
    uint64_t physical_limit;
    bool nx, nx_enabled, page1g;
};

static bool user_frame_copy_cpu_policy(uint64_t root, struct user_frame_copy_cpu *out) {
    uint64_t flags,cr0,cr4,efer,cr3,pat;
    uint32_t a,b,c,d;
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    flags=user_frame_test_rflags();
    cr0=user_frame_test_cr0();cr4=user_frame_test_cr4();
    efer=user_frame_test_efer();cr3=user_frame_test_cr3();
#else
    uint32_t lo,hi;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags) : : "memory");
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    __asm__ volatile("mov %%cr3,%0" : "=r"(cr3));
    __asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(0xc0000080u));
    efer=((uint64_t)hi<<32)|lo;
#endif
    const uint64_t required_cr0=(1ull<<0)|(1ull<<16)|(1ull<<31);
    const uint64_t refused_cr4=(1ull<<12)|(1ull<<17)|(1ull<<21)|(1ull<<22)|(1ull<<23)|(1ull<<24);
    if ((flags&(1ull<<9)) || (cr0&required_cr0)!=required_cr0 || !(cr4&(1ull<<5)) ||
        (cr4&refused_cr4) || !(efer&(1ull<<10)) || !root || (root&~PAGE_MASK) ||
        root==kernel_cr3 || cr3!=root || !find_address_space_record(root)) return false;
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    user_frame_test_cpuid(1,&a,&b,&c,&d);
#else
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(1u),"c"(0u));
#endif
    if (!(d&(1u<<16))) return false;
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    pat=user_frame_test_pat();
    user_frame_test_cpuid(0x80000000u,&a,&b,&c,&d);
#else
    __asm__ volatile("rdmsr" : "=a"(lo),"=d"(hi) : "c"(0x277u));
    pat=((uint64_t)hi<<32)|lo;
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0x80000000u),"c"(0u));
#endif
    if ((pat&0xff)!=6 || a<0x80000008u) return false;
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    user_frame_test_cpuid(0x80000001u,&a,&b,&c,&d);
#else
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0x80000001u),"c"(0u));
#endif
    bool nx=(d&(1u<<20))!=0, page1g=(d&(1u<<26))!=0;
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    user_frame_test_cpuid(0x80000008u,&a,&b,&c,&d);
#else
    __asm__ volatile("cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "a"(0x80000008u),"c"(0u));
#endif
    unsigned width=a&0xff;
    if (width<36 || width>52 || root >= (1ull<<width)) return false;
    if (out) *out=(struct user_frame_copy_cpu){1ull<<width,nx,(efer&(1ull<<11))!=0,page1g};
    return true;
}

bool vmm_user_frame_copy_policy_current(uint64_t root) {
    return user_frame_copy_cpu_policy(root,NULL);
}

/* The caller owns CPU0 exclusion, including the subsequent copy. Page table
 * frames are trusted kernel allocations; reject encodings before following. */
static bool user_frame_walk(uint64_t root,uint64_t va,const struct user_frame_copy_cpu *cpu,
    bool destination,uint64_t *physical,uint64_t *effective_nx) {
    const uint64_t p=1, rw=2, user=4, accessed=1ull<<5, dirty=1ull<<6;
    const uint64_t ps=1ull<<7, global=1ull<<8, nx=1ull<<63;
    const uint64_t physical_bits=(cpu->physical_limit-1)&PAGE_MASK;
    uint64_t table=root;
    bool effective_rw=true,effective_user=true;
    uint64_t seen_nx=0;
    for (unsigned level=4;level;--level) {
        if (!table || table>=cpu->physical_limit || table>cpu->physical_limit-PAGE_SIZE ||
            (table & (PAGE_SIZE-1))) return false;
        uint64_t *entries=pmm_phys_to_virt(table);
        if (!entries || !is_canonical((uint64_t)entries)) return false;
        unsigned shift=12+9*(level-1);
        uint64_t entry=entries[(va>>shift)&511];
        if (!(entry&p) || ((entry&nx) && (!cpu->nx || !cpu->nx_enabled))) return false;
        seen_nx|=entry&nx;
        bool large=(level==3 || level==2) && (entry&ps);
        bool leaf=level==1 || large;
        if (level==4 && (entry&ps)) return false;
        if (destination && large) return false;
        if (large && level==3 && !cpu->page1g) return false;
        unsigned leaf_shift=large ? shift : 12;
        uint64_t address_mask=physical_bits & ~((1ull<<leaf_shift)-1);
        uint64_t allowed=(leaf ? address_mask : physical_bits)|p|rw|user|accessed|nx;
        if (leaf) allowed|=dirty|(destination ? 0 : global)|(large ? ps : 0);
        if (entry & ~allowed) return false;
        effective_rw=effective_rw && (entry&rw);
        effective_user=effective_user && (entry&user);
        if (leaf) {
            if (!effective_rw || (destination ? !effective_user : effective_user)) return false;
            uint64_t base=entry&address_mask, offset=va&((1ull<<leaf_shift)-1);
            if (base>=cpu->physical_limit || offset>=cpu->physical_limit-base) return false;
            *physical=base+offset;
            if (effective_nx) *effective_nx=seen_nx;
            return true;
        }
        table=entry&physical_bits;
    }
    return false;
}

static bool user_frame_upper_extent(uint64_t base,uint64_t length) {
    return length && base<=UINT64_MAX-(length-1) && (base>>47)==0x1ffff &&
        ((base+length-1)>>47)==0x1ffff;
}
static bool user_frame_overlap(uint64_t a,uint64_t alen,uint64_t b,uint64_t blen) {
    return a<=b ? b-a<alen : a-b<blen;
}
static uint64_t user_frame_kernel_address(const void *pointer) {
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    return user_frame_test_kernel_address(pointer);
#else
    return (uint64_t)(uintptr_t)pointer;
#endif
}
static uint64_t user_frame_current_rsp(void) {
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
    return user_frame_test_rsp();
#else
    uint64_t value;
    __asm__ volatile("mov %%rsp,%0" : "=r"(value));
    return value;
#endif
}

/* Validate a complete resident kernel object and retain its physical page
 * spans for destination-alias rejection. At most two pages per object. */
struct user_frame_span { uint64_t base,length; };
static bool user_frame_kernel_spans(uint64_t root,uint64_t start,uint64_t length,
    const struct user_frame_copy_cpu *cpu,struct user_frame_span spans[2],unsigned *count) {
    if (!user_frame_upper_extent(start,length)) return false;
    unsigned n=0;
    while (length) {
        uint64_t physical;
        uint64_t chunk=PAGE_SIZE-(start&(PAGE_SIZE-1));
        if (chunk>length) chunk=length;
        if (n==2 || !user_frame_walk(root,start,cpu,false,&physical,NULL) ||
            physical>=cpu->physical_limit || chunk>cpu->physical_limit-physical) return false;
        spans[n++]=(struct user_frame_span){physical,chunk};
        start+=chunk;length-=chunk;
    }
    *count=n;
    return true;
}

static bool user_frame_copy_exclusions(uint64_t root,uint64_t destination,
    const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t page_count,
    const struct isr_frame *frame,const struct fpu_state *user_fpu,const struct fpu_state *kernel_fpu,
    const struct user_frame_exclusion *exclusions,size_t exclusion_count) {
    if (exclusion_count>USER_FRAME_EXTENDED_EXCLUSION_MAX || (exclusion_count && !exclusions)) return false;
    for(size_t i=0;i<exclusion_count;i++)
        if(exclusions[i].length>PAGE_SIZE ||
           !user_frame_upper_extent(exclusions[i].address,exclusions[i].length)) return false;
    struct user_frame_copy_cpu cpu;
    if (!user_frame_copy_cpu_policy(root,&cpu) || !source || !stack || !pages || !frame ||
        !user_fpu || !kernel_fpu || !length || length>USER_FRAME_COPY_LIMIT ||
        !destination || destination>UINT64_MAX-(length-1) || (destination>>47) ||
        ((destination+length-1)>>47) || stack->frames!=USER_FRAME_STACK_FRAMES ||
        !stack->physical || (stack->physical&(PAGE_SIZE-1)) ||
        stack->physical>=cpu.physical_limit ||
        USER_FRAME_STACK_FRAMES*PAGE_SIZE>cpu.physical_limit-stack->physical ||
        (stack->base&(PAGE_SIZE-1)) ||
        !user_frame_upper_extent(stack->base,USER_FRAME_STACK_FRAMES*PAGE_SIZE) ||
        stack->base>UINT64_MAX-USER_FRAME_STACK_FRAMES*PAGE_SIZE ||
        user_frame_kernel_address(pmm_phys_to_virt(stack->physical))!=stack->base) return false;
    uint64_t stack_end=stack->base+USER_FRAME_STACK_FRAMES*PAGE_SIZE;
    uint64_t src=user_frame_kernel_address(source), f=user_frame_kernel_address(frame);
    uint64_t rsp=user_frame_current_rsp();
    if ((src&15) || src<stack->base || src>stack_end-USER_FRAME_COPY_LIMIT ||
        f<stack->base || f>stack_end-sizeof(*frame) || rsp<stack->base || rsp>=stack_end ||
        (f&~15ull)<stack->base+FPU_COMMON_RESERVATION ||
        src+USER_FRAME_COPY_LIMIT>(f&~15ull)-FPU_COMMON_RESERVATION) return false;
    uint64_t start=destination&~(PAGE_SIZE-1), last=(destination+length-1)&~(PAGE_SIZE-1);
    size_t required=1+(start!=last);
    if (page_count!=required || (required==2 && pages[0].physical_address==pages[1].physical_address)) return false;
    struct user_frame_span src_spans[2],ufpu_spans[2],kfpu_spans[2];
    unsigned ns,nu,nk;
    if (!user_frame_kernel_spans(root,src,USER_FRAME_COPY_LIMIT,&cpu,src_spans,&ns) ||
        !user_frame_kernel_spans(root,user_frame_kernel_address(user_fpu),sizeof(*user_fpu),&cpu,ufpu_spans,&nu) ||
        !user_frame_kernel_spans(root,user_frame_kernel_address(kernel_fpu),sizeof(*kernel_fpu),&cpu,kfpu_spans,&nk)) return false;
    struct user_frame_span extra_spans[USER_FRAME_EXTENDED_EXCLUSION_MAX][2];
    unsigned extra_count[USER_FRAME_EXTENDED_EXCLUSION_MAX];
    for(size_t i=0;i<exclusion_count;i++)
        if(!user_frame_kernel_spans(root,exclusions[i].address,exclusions[i].length,&cpu,extra_spans[i],&extra_count[i])) return false;
    if (user_frame_overlap(src,USER_FRAME_COPY_LIMIT,user_frame_kernel_address(user_fpu),sizeof(*user_fpu)) ||
        user_frame_overlap(src,USER_FRAME_COPY_LIMIT,user_frame_kernel_address(kernel_fpu),sizeof(*kernel_fpu))) return false;
    for (unsigned i=0;i<ns;i++) {
        for (unsigned j=0;j<nu;j++) if(user_frame_overlap(src_spans[i].base,src_spans[i].length,ufpu_spans[j].base,ufpu_spans[j].length)) return false;
        for (unsigned j=0;j<nk;j++) if(user_frame_overlap(src_spans[i].base,src_spans[i].length,kfpu_spans[j].base,kfpu_spans[j].length)) return false;
    }
    uint64_t pos=src;
    for (unsigned i=0;i<ns;i++) {
        if (src_spans[i].base!=stack->physical+(pos-stack->base)) return false;
        pos+=src_spans[i].length;
    }
    const uint64_t meta_allowed=VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE;
    for (size_t i=0;i<required;i++) {
        uint64_t page=start+i*PAGE_SIZE,phys,effective_nx;
        if (pages[i].virtual_address!=page || !pages[i].physical_address ||
            (pages[i].physical_address&(PAGE_SIZE-1)) || pages[i].physical_address>=cpu.physical_limit ||
            PAGE_SIZE>cpu.physical_limit-pages[i].physical_address ||
            (pages[i].flags&~meta_allowed) ||
            (pages[i].flags&(VMM_PAGE_USER|VMM_PAGE_WRITABLE))!=(VMM_PAGE_USER|VMM_PAGE_WRITABLE) ||
            !user_frame_walk(root,page,&cpu,true,&phys,&effective_nx) || phys!=pages[i].physical_address) return false;
        /* Compare the leaf's effective NX against the trusted page metadata. */
        if ((effective_nx^pages[i].flags)&VMM_PAGE_NO_EXECUTE) return false;
        if (user_frame_overlap(phys,PAGE_SIZE,stack->physical,USER_FRAME_STACK_FRAMES*PAGE_SIZE)) return false;
        for (unsigned j=0;j<nu;j++) if(user_frame_overlap(phys,PAGE_SIZE,ufpu_spans[j].base,ufpu_spans[j].length)) return false;
        for (unsigned j=0;j<nk;j++) if(user_frame_overlap(phys,PAGE_SIZE,kfpu_spans[j].base,kfpu_spans[j].length)) return false;
        for (size_t j=0;j<exclusion_count;j++)
            for (unsigned k=0;k<extra_count[j];k++)
                if(user_frame_overlap(phys,PAGE_SIZE,extra_spans[j][k].base,extra_spans[j][k].length)) return false;
    }
    /* No failure, callback, service window or IRQ transition after this point. */
    for (size_t i=0;i<length;i++) {
#ifdef SRVROS_USER_FRAME_COPY_HOST_TEST
        *user_frame_test_user_pointer(destination+i)=source->bytes[i];
#else
        *(volatile uint8_t *)(uintptr_t)(destination+i)=source->bytes[i];
#endif
    }
    return true;
}

bool vmm_copy_user_frame_excluding_current(uint64_t root,uint64_t destination,
    const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t page_count,
    const struct isr_frame *frame,const struct fpu_state *user_fpu,const struct fpu_state *kernel_fpu,
    const struct user_frame_exclusion *exclusions,size_t exclusion_count) {
    if (exclusion_count>USER_FRAME_EXCLUSION_MAX) return false;
    return user_frame_copy_exclusions(root,destination,source,length,stack,pages,page_count,
        frame,user_fpu,kernel_fpu,exclusions,exclusion_count);
}

bool vmm_copy_user_frame_extended_current(uint64_t root,uint64_t destination,
    const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t page_count,
    const struct isr_frame *frame,const struct fpu_state *user_fpu,const struct fpu_state *kernel_fpu,
    const struct user_frame_exclusion *exclusions,size_t exclusion_count) {
    return user_frame_copy_exclusions(root,destination,source,length,stack,pages,page_count,
        frame,user_fpu,kernel_fpu,exclusions,exclusion_count);
}

bool vmm_copy_user_frame_current(uint64_t root,uint64_t destination,
    const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t page_count,
    const struct isr_frame *frame,const struct fpu_state *user_fpu,const struct fpu_state *kernel_fpu) {
    return vmm_copy_user_frame_excluding_current(root,destination,source,length,stack,pages,page_count,
        frame,user_fpu,kernel_fpu,NULL,0);
}

#ifdef SRVROS_ECAM_BOOT
#include "ecam_boot.inc"
#endif

#ifdef SRVROS_AIR61_READ_WINDOW
#include <srvros/air61_read_window.h>
#endif
#ifdef SRVROS_AIR61_WINDOW
#include "air61_window.inc"
#endif
#ifdef SRVROS_AIR61_READ_WINDOW
#include "air61_read_window.inc"
#endif
