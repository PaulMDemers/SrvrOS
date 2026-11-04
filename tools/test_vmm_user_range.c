#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static uint64_t test_cr3;
static unsigned switches;
static void vmm_test_write_cr3(uint64_t value) { test_cr3=value;switches++; }
static uint64_t vmm_test_cr3(void) { return test_cr3; }
#define SRVROS_VMM_HOST_TEST
#include "../kernel/src/memory/vmm.c"
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t minimum,enum pmm_frame_tag tag) {
    (void)n;(void)minimum;(void)tag;assert(0 && "unexpected page-table allocation");return 0;
}
static uint64_t retired[3];
static unsigned retire_count;
void pmm_free_frame(uint64_t frame) {
    /* Active address space must be switched before any table frame retires. */
    assert(test_cr3==kernel_cr3 && retire_count<3);
    retired[retire_count++]=frame;
}
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
    pml4[511]=(uint64_t)pdpt | VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE;
    test_cr3 = (uint64_t)pml4;
    /* Noncanonical addresses share table indices with canonical mappings.
     * Every public walker must refuse before lookup or mutation. */
    const uint64_t invalid[] = {0x1000000001000ull, 0xffff000000001000ull};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        uint64_t physical=0xfeed;
        assert(!vmm_virt_to_phys_in_space(test_cr3,invalid[i],&physical));
        assert(physical==0xfeed);
        assert(!vmm_protect_page_in_space(test_cr3,invalid[i],VMM_PAGE_USER));
        assert(pt[1]==(0x1000 | rw_user));
        assert(!vmm_unmap_page_in_space(test_cr3,invalid[i],&physical));
        assert(physical==0xfeed && pt[1]==(0x1000 | rw_user));
        assert(!vmm_map_page_in_space(test_cr3,invalid[i]+4096,0x9000,rw_user));
        assert(!pt[2]);
        assert(!vmm_ensure_page_in_space(test_cr3,invalid[i],0x1000,rw_user));
    }
    uint64_t physical=0;
    assert(vmm_virt_to_phys_in_space(test_cr3,0x1123,&physical) && physical==0x1123);
    assert(vmm_virt_to_phys_in_space(test_cr3,0xffffff8000001123ull,&physical) && physical==0x1123);
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
    /* Teardown frees only the registered user tables, in reverse order,
     * after switching off the active root; kernel/zero roots are retained. */
    kernel_cr3=(uint64_t)pml4;
    const uint64_t root=0x8000;
    address_spaces[0]=root;address_space_count=1;
    address_space_records[0]=(struct address_space_record){.used=true,.root=root,
        .tables={0x9000,0xa000},.table_count=2};
    test_cr3=root;
    vmm_destroy_address_space(root);
    assert(switches==1 && retire_count==3);
    assert(retired[0]==0xa000 && retired[1]==0x9000 && retired[2]==root);
    assert(!address_space_count && !find_address_space_record(root));
    vmm_destroy_address_space(0);vmm_destroy_address_space(kernel_cr3);
    assert(switches==1 && retire_count==3);
    free(pt); free(pd); free(pdpt); free(pml4);
}
