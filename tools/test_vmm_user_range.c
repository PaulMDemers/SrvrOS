#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static uint64_t test_cr3;
static uint64_t vmm_test_cr3(void) { return test_cr3; }
#define SRVROS_VMM_HOST_TEST
#include "../kernel/src/memory/vmm.c"
void *pmm_phys_to_virt(uint64_t address) { return (void *)(uintptr_t)address; }
int main(void) {
    uint64_t *pml4 = aligned_alloc(4096, 4096), *pdpt = aligned_alloc(4096, 4096);
    uint64_t *pd = aligned_alloc(4096, 4096), *pt = aligned_alloc(4096, 4096);
    assert(pml4 && pdpt && pd && pt);
    memset(pml4, 0, 4096); memset(pdpt, 0, 4096);
    memset(pd, 0, 4096); memset(pt, 0, 4096);
    const uint64_t rw_user = VMM_PAGE_PRESENT | VMM_PAGE_USER | VMM_PAGE_WRITABLE;
    pml4[0] = (uint64_t)pdpt | rw_user; pdpt[0] = (uint64_t)pd | rw_user;
    pd[0] = (uint64_t)pt | rw_user; pt[1] = 0x1000 | rw_user;
    test_cr3 = (uint64_t)pml4;
    assert(vmm_user_range_mapped(0x1000, 1, true));
    assert(!vmm_user_range_mapped(0x1000000001000ull, 1, false));
    assert(!vmm_user_range_mapped(0x7fffffffffffull, 2, false));
    for (unsigned level = 0; level < 3; level++) {
        uint64_t *entry = level == 0 ? &pml4[0] : level == 1 ? &pdpt[0] : &pd[0];
        *entry &= ~VMM_PAGE_USER;
        assert(!vmm_user_range_mapped(0x1000, 1, false));
        *entry |= VMM_PAGE_USER; *entry &= ~VMM_PAGE_WRITABLE;
        assert(vmm_user_range_mapped(0x1000, 1, false));
        assert(!vmm_user_range_mapped(0x1000, 1, true));
        *entry |= VMM_PAGE_WRITABLE;
    }
    /* Large mappings still inherit permissions from every ancestor. */
    pd[0] = 0x200000 | rw_user | PAGE_LARGE;
    assert(vmm_user_range_mapped(0x1000, 1, true));
    pml4[0] &= ~VMM_PAGE_USER;
    assert(!vmm_user_range_mapped(0x1000, 1, false));
    pml4[0] |= VMM_PAGE_USER; pdpt[0] &= ~VMM_PAGE_WRITABLE;
    assert(!vmm_user_range_mapped(0x1000, 1, true));
    pdpt[0] = 0x40000000 | rw_user | PAGE_LARGE;
    assert(vmm_user_range_mapped(0x1000, 1, true));
    pml4[0] &= ~VMM_PAGE_WRITABLE;
    assert(!vmm_user_range_mapped(0x1000, 1, true));
    free(pt); free(pd); free(pdpt); free(pml4);
}
