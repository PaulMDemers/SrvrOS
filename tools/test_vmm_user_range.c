#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t test_cr3;
static unsigned switches;
static uint64_t test_rflags, test_efer=1ull<<11;
static uint32_t test_maxext=0x80000001u, test_nx=1u<<20;
static unsigned cpuid_calls, efer_calls, table_reads;
static uint64_t vmm_test_rflags(void) { return test_rflags; }
static uint64_t vmm_test_irq_save(void) { uint64_t old=test_rflags;test_rflags&=~(1ull<<9);return old; }
static void vmm_test_irq_restore(uint64_t flags) { test_rflags=flags; }
static void vmm_test_cpuid(uint32_t leaf,uint32_t *eax,uint32_t *ebx,uint32_t *ecx,uint32_t *edx) {
    cpuid_calls++; *eax=*ebx=*ecx=*edx=0;
    if(leaf==0x80000000u) *eax=test_maxext;
    if(leaf==0x80000001u) *edx=test_nx;
}
static uint64_t vmm_test_efer(void) { efer_calls++; return test_efer; }
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
void *pmm_phys_to_virt(uint64_t address) { table_reads++; return (void *)(uintptr_t)address; }

static uint64_t *exec_tables[4];
static void exec_check(uint64_t address,uint64_t root,bool expected)
{
    uint64_t saved[4][512];
    struct address_space_record records[VMM_MAX_ADDRESS_SPACES];
    uint64_t spaces[VMM_MAX_ADDRESS_SPACES];
    for(unsigned i=0;i<4;i++) memcpy(saved[i],exec_tables[i],4096);
    uint64_t registry_occupancy=address_space_occupancy;
    memcpy(records,address_space_records,sizeof(records));memcpy(spaces,address_spaces,sizeof(spaces));
    uint64_t old_cr3=test_cr3,old_kernel=kernel_cr3,old_count=address_space_count;
    uint64_t old_flags=test_rflags,old_efer=test_efer;
    uint32_t old_max=test_maxext,old_nx=test_nx;
    unsigned old_switches=switches,old_retire=retire_count;
    assert(vmm_user_exec_current(address,root)==expected);
    for(unsigned i=0;i<4;i++) assert(!memcmp(saved[i],exec_tables[i],4096));
    assert(address_space_occupancy==registry_occupancy);
    assert(!memcmp(records,address_space_records,sizeof(records)) && !memcmp(spaces,address_spaces,sizeof(spaces)));
    assert(test_cr3==old_cr3 && kernel_cr3==old_kernel && address_space_count==old_count);
    assert(test_rflags==old_flags && test_efer==old_efer && test_maxext==old_max && test_nx==old_nx);
    assert(switches==old_switches && retire_count==old_retire);
}

static void test_executable_query(void)
{
    for(unsigned i=0;i<4;i++) { exec_tables[i]=aligned_alloc(4096,4096); assert(exec_tables[i]); memset(exec_tables[i],0,4096); }
    uint64_t *pml4=exec_tables[0], *pdpt=exec_tables[1], *pd=exec_tables[2], *pt=exec_tables[3];
    const uint64_t u=VMM_PAGE_PRESENT|VMM_PAGE_USER|VMM_PAGE_WRITABLE;
    uint64_t root=(uint64_t)pml4;
    pml4[0]=(uint64_t)pdpt|u;pdpt[0]=(uint64_t)pd|u;pd[0]=(uint64_t)pt|u;pt[1]=0x1000|u;
    kernel_cr3=0x1000;test_cr3=root;test_rflags=0x46;test_maxext=0x80000001u;test_nx=(1u<<20)|(1u<<26);test_efer=1u<<11;
    address_spaces[0]=root;address_space_count=1;
    address_space_set_occupied(0,true);address_space_records[0]=(struct address_space_record){.root=root};
    exec_check(0x1000,root,true);
    test_nx=1u<<20;exec_check(0x1000,root,true);test_nx=(1u<<20)|(1u<<26);
    pt[1]&=~VMM_PAGE_WRITABLE;exec_check(0x1000,root,true);pt[1]|=VMM_PAGE_WRITABLE;
    uint64_t *levels[]={&pml4[0],&pdpt[0],&pd[0],&pt[1]};
    for(unsigned level=0;level<4;level++) {
        *levels[level]&=~VMM_PAGE_PRESENT;exec_check(0x1000,root,false);*levels[level]|=VMM_PAGE_PRESENT;
        *levels[level]&=~VMM_PAGE_USER;exec_check(0x1000,root,false);*levels[level]|=VMM_PAGE_USER;
        *levels[level]|=VMM_PAGE_NO_EXECUTE;exec_check(0x1000,root,false);*levels[level]&=~VMM_PAGE_NO_EXECUTE;
    }
    pml4[0]|=VMM_PAGE_NO_EXECUTE;
    assert(vmm_user_range_mapped(0x1000,1,true)); /* Existing range API ignores NX. */
    exec_check(0x1000,root,false);pml4[0]&=~VMM_PAGE_NO_EXECUTE;
    pml4[0]|=PAGE_LARGE;exec_check(0x1000,root,false);pml4[0]&=~PAGE_LARGE;
    uint64_t pd_saved=pd[0],pdpt_saved=pdpt[0];
    pd[0]=0x200000|u|PAGE_LARGE;exec_check(0x1000,root,true);
    test_nx=1u<<20;exec_check(0x1000,root,true);test_nx=(1u<<20)|(1u<<26);
    for(unsigned i=0;i<3;i++) {
        uint64_t *p=i==0?&pml4[0]:i==1?&pdpt[0]:&pd[0];
        *p|=VMM_PAGE_NO_EXECUTE;exec_check(0x1000,root,false);*p&=~VMM_PAGE_NO_EXECUTE;
        *p&=~VMM_PAGE_USER;exec_check(0x1000,root,false);*p|=VMM_PAGE_USER;
        *p&=~VMM_PAGE_PRESENT;exec_check(0x1000,root,false);*p|=VMM_PAGE_PRESENT;
    }
    pdpt[0]=0x40000000|u|PAGE_LARGE;exec_check(0x1000,root,true);
    test_nx=1u<<20;exec_check(0x1000,root,false);test_nx=(1u<<20)|(1u<<26);
    for(unsigned i=0;i<2;i++) {
        uint64_t *p=i?&pdpt[0]:&pml4[0];
        *p|=VMM_PAGE_NO_EXECUTE;exec_check(0x1000,root,false);*p&=~VMM_PAGE_NO_EXECUTE;
        *p&=~VMM_PAGE_USER;exec_check(0x1000,root,false);*p|=VMM_PAGE_USER;
        *p&=~VMM_PAGE_PRESENT;exec_check(0x1000,root,false);*p|=VMM_PAGE_PRESENT;
    }
    pdpt[0]=pdpt_saved;pd[0]=pd_saved;
    pt[0]=0x3000|u;exec_check(0,root,false);exec_check(1,root,true);pt[0]=0;
    /* Explicit alias at the top canonical lower-half byte. */
    pml4[255]=(uint64_t)pdpt|u;pdpt[511]=(uint64_t)pd|u;
    pd[511]=(uint64_t)pt|u;pt[511]=0x3000|u;
    exec_check(UINT64_C(0x7fffffffffff),root,true);
    exec_check(UINT64_C(0x800000000000),root,false);exec_check(UINT64_MAX,root,false);
    pml4[255]=pdpt[511]=pd[511]=pt[511]=0;
    exec_check(0x1000,0,false);exec_check(0x1000,root|1,false);
    exec_check(0x1000,root|UINT64_C(0x8000000000000000),false);
    test_cr3=0;exec_check(0x1000,root,false);test_cr3=root|5;exec_check(0x1000,root,true);
    test_cr3=root+4096;exec_check(0x1000,root,false);test_cr3=root;
    test_cr3=0x9000;unsigned unread_tables=table_reads;
    exec_check(0x1000,0x9000,false);assert(table_reads==unread_tables);test_cr3=root;
    address_space_set_occupied(0,false);exec_check(0x1000,root,false);address_space_set_occupied(0,true);
    kernel_cr3=root;exec_check(0x1000,root,false);kernel_cr3=0x1000;
    test_rflags=0x246;exec_check(0x1000,root,false);test_rflags=0x46;
    unsigned e=efer_calls,t=table_reads;
    test_maxext=0x80000000u;exec_check(0x1000,root,false);assert(efer_calls==e && table_reads==t);
    test_maxext=0x80000001u;test_nx=1u<<29;exec_check(0x1000,root,false);assert(efer_calls==e && table_reads==t);
    test_nx=1u<<26;exec_check(0x1000,root,false);assert(efer_calls==e && table_reads==t);
    test_nx=1u<<20;test_efer=(1u<<8)|(1u<<10);exec_check(0x1000,root,false);assert(efer_calls==e+1 && table_reads==t);
    memset(address_spaces,0,sizeof(address_spaces));address_space_occupancy=0;memset(address_space_records,0,sizeof(address_space_records));
    address_space_count=0;kernel_cr3=0;test_cr3=0;test_efer=1u<<11;
    for(unsigned i=0;i<4;i++) free(exec_tables[i]);
    puts("vmm-executable-host: PASS (unwired query)");
}
int main(void) {
    test_executable_query();
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
    address_space_set_occupied(0,true);address_space_records[0]=(struct address_space_record){.root=root,
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
