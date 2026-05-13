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
#define VMM_MAX_ADDRESS_SPACES 16
#define VMM_MAX_KERNEL_MAPPINGS 128
#define VMM_MAX_TRACKED_TABLES 256
#define VMM_PAGE_TABLE_MIN_PHYSICAL 0x2000000ull

struct kernel_mapping {
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t flags;
};

struct address_space_record {
    bool used;
    uint64_t root;
    uint64_t tables[VMM_MAX_TRACKED_TABLES];
    uint64_t table_count;
};

static uint64_t *kernel_pml4;
static uint64_t kernel_cr3;
static uint64_t address_spaces[VMM_MAX_ADDRESS_SPACES];
static uint64_t address_space_count;
static struct address_space_record address_space_records[VMM_MAX_ADDRESS_SPACES];
static struct kernel_mapping kernel_mappings[VMM_MAX_KERNEL_MAPPINGS];
static uint64_t kernel_mapping_count;

static uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void write_cr3(uint64_t value) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}

static void invlpg(uint64_t virtual_address) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
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
        if (address_space_records[i].used && address_space_records[i].root == address_space) {
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
        if (!address_space_records[i].used) {
            record = &address_space_records[i];
            break;
        }
    }
    if (record == NULL) {
        return false;
    }

    address_spaces[address_space_count++] = address_space;
    record->used = true;
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
        record->used = false;
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

static bool map_page_in_space(uint64_t address_space,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags,
    bool allow_existing) {
    if ((virtual_address % PAGE_SIZE) != 0 || (physical_address % PAGE_SIZE) != 0) {
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

    if ((virtual_address % PAGE_SIZE) != 0) {
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
    if (physical_address == NULL) {
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

static bool vmm_entry_for_address(uint64_t address_space, uint64_t virtual_address, uint64_t *entry_out) {
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint64_t entry;

    if (entry_out == NULL) {
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
        *entry_out = entry;
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
        *entry_out = entry;
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

    *entry_out = entry;
    return true;
}

bool vmm_user_range_mapped(uint64_t virtual_address, uint64_t length, bool writable) {
    if (length == 0) {
        return true;
    }
    if (virtual_address + length < virtual_address) {
        return false;
    }

    uint64_t end = virtual_address + length - 1;
    uint64_t page = virtual_address & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t address_space = active_address_space();

    while (page <= end) {
        uint64_t entry = 0;
        if (!vmm_entry_for_address(address_space, page, &entry) ||
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
    if (address_space_out == NULL || kernel_pml4 == NULL) {
        return false;
    }

    uint64_t physical = pmm_alloc_frames_above_tagged(1,
        VMM_PAGE_TABLE_MIN_PHYSICAL,
        PMM_FRAME_PAGE_TABLE);
    if (physical == 0) {
        return false;
    }

    uint64_t *pml4 = pmm_phys_to_virt(physical);
    zero_page(pml4);

    *address_space_out = physical;
    if (!register_address_space(physical)) {
        pmm_free_frame(physical);
        *address_space_out = 0;
        return false;
    }
    vmm_refresh_kernel_mappings(physical);

    return true;
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
