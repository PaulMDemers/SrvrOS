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
bool vmm_create_address_space(uint64_t *address_space_out);
void vmm_destroy_address_space(uint64_t address_space);
void vmm_refresh_kernel_mappings(uint64_t address_space);
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
