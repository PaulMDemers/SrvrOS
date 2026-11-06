/* Actual-source tests of preparation and checked user-root creation. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uint64_t test_cr3,test_flags,test_efer=(1ull<<11);
static uint32_t test_max=0x80000001u,test_features=(1u<<20)|(1u<<26);
static unsigned switches,allocs,frees,msrs;
static bool create_mode,create_alloc_failure;
static uint64_t create_frame,*create_output,create_expected_output,create_initial_flags;
static unsigned irq_saves,irq_restores;
static uint64_t vmm_test_irq_save(void) {
    assert(create_mode && create_output && !*create_output);
    const uint64_t *page=(const uint64_t *)(uintptr_t)create_frame;
    for(unsigned i=0;i<512;i++)assert(!page[i]); /* Zeroed before exclusion. */
    uint64_t saved=test_flags;test_flags&=~(1ull<<9);irq_saves++;return saved;
}
static void vmm_test_irq_restore(uint64_t flags) {
    assert(create_mode && !(test_flags&(1ull<<9)) && flags==create_initial_flags);
    assert(create_output && *create_output==create_expected_output);
    test_flags=flags;irq_restores++;
}
static uint64_t vmm_test_cr3(void) { return test_cr3; }
static void vmm_test_write_cr3(uint64_t v) { test_cr3=v;switches++; }
static uint64_t vmm_test_rflags(void) { return test_flags; }
static void vmm_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
    *a=*b=*c=*d=0;if(leaf==0x80000000u)*a=test_max;if(leaf==0x80000001u)*d=test_features;
}
static uint64_t vmm_test_efer(void) { msrs++;return test_efer; }
#define SRVROS_VMM_HOST_TEST
#include "../kernel/src/memory/vmm.c"
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t m,enum pmm_frame_tag tag) {
    assert(create_mode && n==1 && m==VMM_PAGE_TABLE_MIN_PHYSICAL && tag==PMM_FRAME_PAGE_TABLE);
    assert(test_flags==create_initial_flags && create_output && !*create_output);
    allocs++;return create_alloc_failure ? 0 : create_frame;
}
void pmm_free_frame(uint64_t p) {
    assert(create_mode && p==create_frame && !find_address_space_record(p));frees++;
}
void *pmm_phys_to_virt(uint64_t p) { return (void *)(uintptr_t)p; }
void console_printf(const char *fmt,...) { (void)fmt;assert(!"unexpected console"); }
static uint64_t *tables[5]; /* kernel root, PDPT, PD, PT, staged root */
static size_t host_page;
static const uint64_t va=UINT64_C(0xffff800000001000);
static uint64_t root(void) { return (uint64_t)tables[4]; }
static void check(uint64_t target,bool expected)
{
    uint64_t saved[5][512], saved_cr3=test_cr3, saved_kernel=kernel_cr3;
    uint64_t *saved_kernel_pml4=kernel_pml4;
    struct kernel_mapping maps[VMM_MAX_KERNEL_MAPPINGS];
    struct address_space_record records[VMM_MAX_ADDRESS_SPACES];
    uint64_t spaces[VMM_MAX_ADDRESS_SPACES];
    for(unsigned i=0;i<5;i++)memcpy(saved[i],tables[i],4096);
    memcpy(maps,kernel_mappings,sizeof(maps));memcpy(records,address_space_records,sizeof(records));
    memcpy(spaces,address_spaces,sizeof(spaces));
    uint64_t count=kernel_mapping_count,space_count=address_space_count,flags=test_flags,efer=test_efer;
    uint32_t max=test_max,features=test_features;unsigned sw=switches,al=allocs,fr=frees;
    assert(vmm_prepare_user_kernel_mappings(target)==expected);
    for(unsigned i=0;i<4;i++)assert(!memcmp(saved[i],tables[i],4096));
    for(unsigned i=0;i<5;i++)for(size_t j=4096;j<host_page;j++)
        assert(((const unsigned char *)tables[i])[j]==0xa5);
    assert(!memcmp(saved[4],tables[4],2048));
    if(expected)assert(!memcmp(tables[4]+256,tables[0]+256,2048));
    else assert(!memcmp(saved[4]+256,tables[4]+256,2048));
    assert(!memcmp(maps,kernel_mappings,sizeof(maps)) && !memcmp(records,address_space_records,sizeof(records)));
    assert(!memcmp(spaces,address_spaces,sizeof(spaces)) && kernel_mapping_count==count && address_space_count==space_count);
    assert(test_cr3==saved_cr3 && kernel_cr3==saved_kernel && test_flags==flags && test_efer==efer);
    assert(kernel_pml4==saved_kernel_pml4);
    assert(test_max==max && test_features==features && switches==sw && allocs==al && frees==fr);
}
static void reset_mapping(void)
{
    for(unsigned i=0;i<5;i++)memset(tables[i],0,4096);
    const uint64_t rw=VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE;
    tables[0][256]=(uint64_t)tables[1]|rw;tables[1][0]=(uint64_t)tables[2]|rw;
    tables[2][0]=(uint64_t)tables[3]|rw;tables[3][1]=0x2000|rw;
    /* Unrecorded upper entries still copy as part of the inherited kernel half. */
    tables[0][400]=0xabc000|rw;tables[4][400]=0xdef000|rw;
    tables[4][0]=0x551000|rw;
    kernel_mapping_count=1;kernel_mappings[0]=(struct kernel_mapping){va,0x2000,VMM_PAGE_WRITABLE};
    kernel_cr3=(uint64_t)tables[0];kernel_pml4=tables[0];test_cr3=kernel_cr3;test_flags=0x46;
    test_max=0x80000001u;test_features=(1u<<20)|(1u<<26);test_efer=1ull<<11;
    memset(address_space_records,0,sizeof(address_space_records));memset(address_spaces,0,sizeof(address_spaces));
    address_space_records[0]=(struct address_space_record){.used=true,.root=root(),.tables={0x123000},.table_count=1};
    address_spaces[0]=root();address_space_count=1;
}
static void create_tests(void)
{
    uint64_t kernel_snapshot[4][512];
    for(unsigned initial_if=0;initial_if<2;initial_if++) {
        reset_mapping();memset(address_spaces,0,sizeof(address_spaces));
        memset(address_space_records,0,sizeof(address_space_records));address_space_count=0;
        uint64_t out=UINT64_MAX;
        create_mode=true;create_frame=root();create_output=&out;create_alloc_failure=false;
        create_initial_flags=initial_if?0x246:0x46;test_flags=create_initial_flags;
        create_expected_output=create_frame;
        for(unsigned i=0;i<4;i++)memcpy(kernel_snapshot[i],tables[i],4096);
        unsigned a=allocs,f=frees,s=irq_saves,r=irq_restores,sw=switches;
        assert(vmm_create_address_space(&out) && out==create_frame);
        assert(allocs==a+1 && frees==f && irq_saves==s+1 && irq_restores==r+1);
        assert(test_flags==create_initial_flags && switches==sw && test_cr3==kernel_cr3);
        assert(address_space_count==1 && address_spaces[0]==create_frame && find_address_space_record(create_frame));
        for(unsigned i=0;i<256;i++)assert(!tables[4][i]);
        assert(!memcmp(tables[4]+256,tables[0]+256,2048));
        for(unsigned i=0;i<4;i++)assert(!memcmp(kernel_snapshot[i],tables[i],4096));
        create_mode=false;
    }
    reset_mapping();memset(address_spaces,0,sizeof(address_spaces));
    memset(address_space_records,0,sizeof(address_space_records));address_space_count=0;
    uint64_t out=UINT64_MAX;create_mode=true;create_frame=root();create_output=&out;
    create_initial_flags=test_flags=0x246;create_expected_output=0;create_alloc_failure=true;
    unsigned a=allocs,f=frees,s=irq_saves,r=irq_restores;
    assert(!vmm_create_address_space(&out) && !out);
    assert(allocs==a+1 && frees==f && irq_saves==s && irq_restores==r && test_flags==0x246);
    create_alloc_failure=false;
    /* The first registry record is valid; a later stale record must roll
     * back registration and free exactly the newly allocated root. */
    kernel_mapping_count=2;kernel_mappings[1]=(struct kernel_mapping){va+4096,0x3000,VMM_PAGE_WRITABLE};
    for(unsigned i=0;i<4;i++)memcpy(kernel_snapshot[i],tables[i],4096);
    out=UINT64_MAX;a=allocs;f=frees;s=irq_saves;r=irq_restores;
    assert(!vmm_create_address_space(&out) && !out);
    assert(allocs==a+1 && frees==f+1 && irq_saves==s+1 && irq_restores==r+1);
    assert(!address_space_count && !find_address_space_record(create_frame) && test_flags==0x246);
    for(unsigned i=0;i<512;i++)assert(!tables[4][i]);
    for(unsigned i=0;i<4;i++)assert(!memcmp(kernel_snapshot[i],tables[i],4096));
    reset_mapping();kernel_mapping_count=1;address_space_count=VMM_MAX_ADDRESS_SPACES;
    for(unsigned i=0;i<VMM_MAX_ADDRESS_SPACES;i++) {
        address_spaces[i]=0x100000+(uint64_t)i*4096;
        address_space_records[i]=(struct address_space_record){.used=true,.root=address_spaces[i]};
    }
    uint64_t spaces[VMM_MAX_ADDRESS_SPACES];struct address_space_record records[VMM_MAX_ADDRESS_SPACES];
    memcpy(spaces,address_spaces,sizeof(spaces));memcpy(records,address_space_records,sizeof(records));
    out=UINT64_MAX;create_output=&out;create_initial_flags=test_flags=0x46;
    a=allocs;f=frees;s=irq_saves;r=irq_restores;
    assert(!vmm_create_address_space(&out) && !out);
    assert(allocs==a+1 && frees==f+1 && irq_saves==s+1 && irq_restores==r+1);
    assert(test_flags==0x46 && !memcmp(spaces,address_spaces,sizeof(spaces)) && !memcmp(records,address_space_records,sizeof(records)));
    kernel_pml4=NULL;out=UINT64_MAX;a=allocs;
    assert(!vmm_create_address_space(&out) && !out && allocs==a);
    assert(!vmm_create_address_space(NULL) && allocs==a);
    create_mode=false;create_output=NULL;
}
int main(void)
{
    long page=sysconf(_SC_PAGESIZE);assert(page>=4096 && page%4096==0);host_page=(size_t)page;
    for(unsigned i=0;i<5;i++) {
        tables[i]=mmap(NULL,host_page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
        assert(tables[i]!=MAP_FAILED);memset(tables[i],0xa5,host_page);
    }
    reset_mapping();
    address_space_count=0;memset(address_space_records,0,sizeof(address_space_records));
    test_cr3=kernel_cr3;test_flags=2;
    assert(vmm_diagnostic_sole_kernel_root(kernel_cr3));
    assert(!vmm_diagnostic_sole_kernel_root(0));
    assert(!vmm_diagnostic_sole_kernel_root(kernel_cr3|1));
    test_flags|=1ull<<9;assert(!vmm_diagnostic_sole_kernel_root(kernel_cr3));test_flags=2;
    test_cr3|=1;assert(!vmm_diagnostic_sole_kernel_root(kernel_cr3));test_cr3=kernel_cr3;
    address_space_count=1;assert(!vmm_diagnostic_sole_kernel_root(kernel_cr3));address_space_count=0;
    address_space_records[63].used=true;assert(!vmm_diagnostic_sole_kernel_root(kernel_cr3));address_space_records[63].used=false;
    reset_mapping();check(root(),true); /* Inactive differing target copies. */
    create_tests();reset_mapping();check(root(),true);
    test_cr3=root()|5;
    /* The active identical root is mapped read-only: even idempotent stores
     * must fault, establishing that success performs no target writes. */
    assert(mprotect(tables[4],host_page,PROT_READ)==0);
    check(root(),true);
    assert(mprotect(tables[4],host_page,PROT_READ|PROT_WRITE)==0);
    tables[4][400]^=0x1000;check(root(),false);tables[4][400]^=0x1000;
    kernel_mapping_count=2;kernel_mappings[1]=(struct kernel_mapping){va+4096,0x3000,VMM_PAGE_WRITABLE};
    assert(mprotect(tables[4],host_page,PROT_READ)==0);
    check(root(),false); /* Matching active root still validates every record. */
    assert(mprotect(tables[4],host_page,PROT_READ|PROT_WRITE)==0);
    test_cr3=kernel_cr3;kernel_mapping_count=1;
    /* Empty and plain mappings do not require NX or 1 GiB capability. */
    test_max=0;test_features=0;test_efer=0;unsigned old_msrs=msrs;
    kernel_mapping_count=0;check(root(),true);kernel_mapping_count=1;check(root(),true);assert(msrs==old_msrs);
    reset_mapping();kernel_mappings[0].flags|=VMM_PAGE_PRESENT;check(root(),true);
    uint64_t *anc[]={&tables[0][256],&tables[1][0],&tables[2][0]};
    for(unsigned i=0;i<3;i++) {
        *anc[i]&=~VMM_PAGE_PRESENT;check(root(),false);*anc[i]|=VMM_PAGE_PRESENT;
        *anc[i]&=~VMM_PAGE_WRITABLE;check(root(),false);*anc[i]|=VMM_PAGE_WRITABLE;
        *anc[i]|=VMM_PAGE_NO_EXECUTE;check(root(),false);*anc[i]&=~VMM_PAGE_NO_EXECUTE;
        /* Ancestor cache attributes do not alter leaf cache attributes. */
        *anc[i]|=VMM_PAGE_WRITE_THROUGH|VMM_PAGE_CACHE_DISABLE;check(root(),true);
        *anc[i]&=~(VMM_PAGE_WRITE_THROUGH|VMM_PAGE_CACHE_DISABLE);
    }
    tables[3][1]|=VMM_PAGE_USER;
    check(root(),true); /* Ancestor supervisor permission masks a user leaf. */
    for(unsigned i=0;i<3;i++)*anc[i]|=VMM_PAGE_USER;
    check(root(),false);
    for(unsigned i=0;i<3;i++)*anc[i]&=~VMM_PAGE_USER;
    tables[3][1]&=~VMM_PAGE_USER;check(root(),true);
    tables[2][0]&=~VMM_PAGE_WRITABLE;kernel_mappings[0].flags&=~VMM_PAGE_WRITABLE;
    check(root(),true); /* Read-only ancestor masks writable leaf. */
    tables[2][0]|=VMM_PAGE_WRITABLE;check(root(),false);
    kernel_mappings[0].flags|=VMM_PAGE_WRITABLE;
    tables[0][256]|=VMM_PAGE_NO_EXECUTE;kernel_mappings[0].flags|=VMM_PAGE_NO_EXECUTE;
    check(root(),true); /* Effective ancestor NX with executable leaf. */
    tables[0][256]&=~VMM_PAGE_NO_EXECUTE;kernel_mappings[0].flags&=~VMM_PAGE_NO_EXECUTE;
    tables[3][1]|=VMM_PAGE_NO_EXECUTE;check(root(),false);
    kernel_mappings[0].flags|=VMM_PAGE_NO_EXECUTE;check(root(),true);
    test_features=1u<<26;check(root(),false);test_features=(1u<<20)|(1u<<26);
    test_efer=(1u<<8)|(1u<<10);check(root(),false);test_efer=1u<<11;
    tables[3][1]&=~VMM_PAGE_NO_EXECUTE;kernel_mappings[0].flags&=~VMM_PAGE_NO_EXECUTE;
    tables[3][1]|=VMM_PAGE_WRITE_THROUGH|VMM_PAGE_CACHE_DISABLE;check(root(),false);
    kernel_mappings[0].flags|=VMM_PAGE_WRITE_THROUGH;check(root(),false);
    kernel_mappings[0].flags&=~VMM_PAGE_WRITE_THROUGH;
    kernel_mappings[0].flags|=VMM_PAGE_CACHE_DISABLE;check(root(),false);
    kernel_mappings[0].flags&=~VMM_PAGE_CACHE_DISABLE;
    kernel_mappings[0].flags|=VMM_PAGE_WRITE_THROUGH|VMM_PAGE_CACHE_DISABLE;check(root(),true);
    tables[3][1]|=1ull<<7;check(root(),false);tables[3][1]&=~(1ull<<7);
    tables[3][1]|=(1ull<<5)|(1ull<<6);check(root(),true);
    reset_mapping();tables[2][0]=0x200000|VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|PAGE_LARGE;
    kernel_mappings[0].physical_address=0x201000;check(root(),true);
    tables[2][0]|=1ull<<12;check(root(),false);tables[2][0]&=~(1ull<<12);
    kernel_mappings[0].physical_address=0x200000;check(root(),false);
    reset_mapping();tables[1][0]=0x40000000|VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|PAGE_LARGE;
    kernel_mappings[0].physical_address=0x40001000;check(root(),true);
    test_features=1u<<20;check(root(),false);test_features=1u<<26;test_efer=0;check(root(),true);
    tables[1][0]|=1ull<<12;check(root(),false);
    reset_mapping();kernel_mapping_count=2;kernel_mappings[1]=(struct kernel_mapping){va+4096,0x3000,VMM_PAGE_WRITABLE};
    check(root(),false); /* First valid record must not publish any target bytes. */
    kernel_mappings[1]=(struct kernel_mapping){va,0x3000,VMM_PAGE_WRITABLE};check(root(),false);
    reset_mapping();kernel_mappings[0].virtual_address=0x1000;check(root(),false);
    kernel_mappings[0].virtual_address=va+1;check(root(),false);
    kernel_mappings[0].virtual_address=va;kernel_mappings[0].physical_address=0x2001;check(root(),false);
    kernel_mappings[0].physical_address=0x2000;kernel_mappings[0].flags|=VMM_PAGE_USER;check(root(),false);
    kernel_mappings[0].flags=VMM_PAGE_WRITABLE|(1ull<<9);check(root(),false);
    reset_mapping();kernel_mapping_count=VMM_MAX_KERNEL_MAPPINGS+1;check(root(),false);
    reset_mapping();test_flags=0x246;check(root(),false);test_flags=0x46;
    check(0,false);check(root()|1,false);check(root()|(1ull<<63),false);check(kernel_cr3,false);
    address_space_records[0].used=false;check(root(),false);address_space_records[0].used=true;
    kernel_pml4=NULL;check(root(),false);kernel_pml4=tables[0];
    kernel_pml4=tables[1];check(root(),false);kernel_pml4=tables[0];
    kernel_cr3|=1;check(root(),false);kernel_cr3&=~1ull;
    kernel_cr3=0;check(root(),false);
    for(unsigned i=0;i<5;i++)assert(munmap(tables[i],host_page)==0);
    puts("vmm-prepare-host: PASS (mapping validation and checked root creation)");
}
