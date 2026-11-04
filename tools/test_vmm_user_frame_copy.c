/* Actual-source CPU/page-walk model with real guarded copy backing. */
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uint64_t flags,cr0=(1ull<<0)|(1ull<<16)|(1ull<<31),cr4=1ull<<5,efer=(1ull<<10)|(1ull<<11),cr3,pat=6,stack_rsp;
static uint32_t width=48,features=(1u<<20)|(1u<<26),maxext=0x80000008,has_pat=1;
static uint8_t *destination_backing,*real_stack;
static unsigned translate_calls;
static const uint64_t stack_phys=0x40000000ull,stack_logical=0xffff900000000000ull;
static uint64_t user_frame_test_rflags(void){return flags;}
static uint64_t user_frame_test_cr0(void){return cr0;}
static uint64_t user_frame_test_cr4(void){return cr4;}
static uint64_t user_frame_test_efer(void){return efer;}
static uint64_t user_frame_test_cr3(void){return cr3;}
static uint64_t user_frame_test_pat(void){return pat;}
static uint64_t user_frame_test_rsp(void){return stack_rsp;}
static uint64_t user_frame_test_kernel_address(const void *p){
    uintptr_t v=(uintptr_t)p,base=(uintptr_t)real_stack;
    if(real_stack && v>=base && v-base<32ull*4096) return stack_logical+(v-base);
    return 0xffff800000000000ull|(v&0x7fffffffffffull);
}
static uint8_t *user_frame_test_user_pointer(uint64_t va){assert(va>=0x400000 && va<0x402000);return destination_backing+(va-0x400000);}
static void user_frame_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){
    *a=*b=*c=*d=0;if(leaf==1)*d=has_pat?(1u<<16):0;
    else if(leaf==0x80000000u)*a=maxext;else if(leaf==0x80000001u)*d=features;
    else if(leaf==0x80000008u)*a=width;
}
static uint64_t vmm_test_rflags(void){return flags;}
static uint64_t vmm_test_cr3(void){return cr3;}
static void vmm_test_write_cr3(uint64_t v){(void)v;abort();}
static uint64_t vmm_test_irq_save(void){abort();}
static void vmm_test_irq_restore(uint64_t v){(void)v;abort();}
static void vmm_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){user_frame_test_cpuid(leaf,a,b,c,d);}
static uint64_t vmm_test_efer(void){return efer;}
#define SRVROS_USER_FRAME_COPY_HOST_TEST
#define SRVROS_VMM_HOST_TEST
#include "../kernel/src/memory/vmm.c"
void *pmm_phys_to_virt(uint64_t physical){
    translate_calls++;
    if(physical>=stack_phys && physical<stack_phys+32ull*4096) return real_stack+(physical-stack_phys);
    return (void *)(uintptr_t)physical;
}
static uint64_t *allocated_tables[32];static unsigned table_count;
static uint64_t *new_table(void){uint64_t *p=aligned_alloc(4096,4096);assert(p && table_count<32);allocated_tables[table_count++]=p;memset(p,0,4096);return p;}
static uint64_t *map_leaf(uint64_t *root,uint64_t va,uint64_t physical,bool user){
    uint64_t *table=root;
    for(int shift=39;shift>=21;shift-=9){
        uint64_t *entry=&table[(va>>shift)&511];
        if(!*entry)*entry=(uint64_t)(uintptr_t)new_table()|3|(user?4:0);
        table=(void *)(uintptr_t)(*entry&PAGE_MASK);
    }
    uint64_t *entry=&table[(va>>12)&511];*entry=physical|3|(user?4:0);return entry;
}
static void check_unchanged(const uint8_t *before){assert(!memcmp(before,destination_backing,8192));}
/* Test the effective permission rule independently for source-stack and FPU
 * paths, including each possible sole supervisor level. Restore every entry. */
static void check_supervisor_path(uint64_t *root,uint64_t va,
    struct user_frame_source *source,const struct user_frame_stack *owned,
    const struct user_frame_page pages[2],const struct isr_frame *frame,
    const struct fpu_state *uf,const struct fpu_state *kf,const uint8_t before[8192]) {
    uint64_t *entries[4],saved[4],*table=root;unsigned count=0;
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t *entry=&table[(va>>shift)&511];
        entries[count]=entry;saved[count++]=*entry;
        if(shift==12 || ((shift==30 || shift==21) && (*entry&(1ull<<7)))) break;
        table=(void *)(uintptr_t)(*entry&PAGE_MASK);
    }
    for(unsigned i=0;i<count;i++) *entries[i]=saved[i]|4;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,owned,pages,2,frame,uf,kf));
    check_unchanged(before);
    for(unsigned supervisor=0;supervisor<count;supervisor++) {
        *entries[supervisor]&=~4ull;
        assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,owned,pages,2,frame,uf,kf));
        assert(!memcmp(destination_backing+0xff0,source->bytes,64));
        assert(!memcmp(destination_backing,before,0xff0));
        assert(!memcmp(destination_backing+0x1030,before+0x1030,8192-0x1030));
        memcpy(destination_backing,before,8192);
        *entries[supervisor]|=4;
    }
    for(unsigned i=0;i<count;i++) *entries[i]=saved[i];
}
int main(void){
    long size=sysconf(_SC_PAGESIZE);assert(size==4096);
    uint8_t *guard=mmap(NULL,16384,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(guard!=MAP_FAILED);
    destination_backing=guard+4096;assert(!mprotect(destination_backing,8192,PROT_READ|PROT_WRITE));
    uint8_t *stack=aligned_alloc(4096,USER_FRAME_STACK_FRAMES*PAGE_SIZE);assert(stack);real_stack=stack;memset(stack,0,USER_FRAME_STACK_FRAMES*PAGE_SIZE);
    uint8_t *fpu_pages=aligned_alloc(4096,3*PAGE_SIZE);assert(fpu_pages);memset(fpu_pages,0,3*PAGE_SIZE);
    uint8_t *extra_pages=aligned_alloc(4096,6*PAGE_SIZE);assert(extra_pages);memset(extra_pages,0x73,6*PAGE_SIZE);
    uint64_t *root=new_table();kernel_cr3=0x1000;cr3=(uint64_t)(uintptr_t)root;
    address_space_records[0]=(struct address_space_record){.used=true,.root=cr3};
    struct user_frame_source *source=(void *)(stack+4096);
    struct isr_frame *frame=(void *)(stack+8192);
    struct fpu_state *uf=(void *)(fpu_pages+PAGE_SIZE-16),*kf=(void *)(fpu_pages+2*PAGE_SIZE+64);
    uint64_t stack_va=user_frame_test_kernel_address(stack);stack_rsp=stack_va+256;
    struct user_frame_stack owned={stack_va,stack_phys,USER_FRAME_STACK_FRAMES};
    for(unsigned i=1;i<=4;i++)map_leaf(root,stack_va+i*PAGE_SIZE,stack_phys+i*PAGE_SIZE,false);
    for(unsigned i=0;i<3;i++)map_leaf(root,user_frame_test_kernel_address(fpu_pages+i*PAGE_SIZE),(uint64_t)(uintptr_t)(fpu_pages+i*PAGE_SIZE),false);
    uint64_t extra_va=user_frame_test_kernel_address(extra_pages);
    uint64_t *extra_leaf[6];
    for(unsigned i=0;i<6;i++)extra_leaf[i]=map_leaf(root,extra_va+i*PAGE_SIZE,(uint64_t)(uintptr_t)(extra_pages+i*PAGE_SIZE),false);
    uint64_t *d0=map_leaf(root,0x400000,(uint64_t)(uintptr_t)destination_backing,true);
    uint64_t *d1=map_leaf(root,0x401000,(uint64_t)(uintptr_t)(destination_backing+4096),true);
    struct user_frame_page pages[2]={{0x400000,(uint64_t)(uintptr_t)destination_backing,6},{0x401000,(uint64_t)(uintptr_t)(destination_backing+4096),6}};
    for(unsigned i=0;i<1024;i++)source->bytes[i]=(uint8_t)i;
    uint8_t original_source[1024];memcpy(original_source,source->bytes,1024);
    uint8_t table_snapshot[32][4096];unsigned tables_before=table_count;
    for(unsigned i=0;i<tables_before;i++)memcpy(table_snapshot[i],allocated_tables[i],4096);
    struct address_space_record registry_before=address_space_records[0];
    memset(destination_backing,0xa5,8192);uint8_t before[8192];memcpy(before,destination_backing,8192);
    uint8_t extra_before[6*PAGE_SIZE];memcpy(extra_before,extra_pages,sizeof(extra_before));
    struct user_frame_exclusion excluded[3]={{extra_va+PAGE_SIZE-16,96},
        {extra_va+3*PAGE_SIZE-8,16},{extra_va+5*PAGE_SIZE-16,3056}};
#define EXCLUDING(list,count) vmm_copy_user_frame_excluding_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf,(list),(count))
    assert(EXCLUDING(excluded,3));assert(!memcmp(destination_backing+0xff0,source->bytes,64));
    assert(!memcmp(destination_backing,before,0xff0) && !memcmp(destination_backing+0x1030,before+0x1030,8192-0x1030));
    memset(destination_backing,0xa5,8192);
    for(unsigned object=0;object<3;object++)for(unsigned span=0;span<2;span++)for(unsigned page=0;page<2;page++) {
        unsigned leaf=object*2+span;uint64_t saved_leaf=*extra_leaf[leaf];
        *extra_leaf[leaf]=pages[page].physical_address|3;
        assert(!EXCLUDING(excluded,3));check_unchanged(before);*extra_leaf[leaf]=saved_leaf;
    }
    unsigned before_translation=translate_calls;
    assert(!EXCLUDING(NULL,1));assert(!EXCLUDING(excluded,4));check_unchanged(before);
    uint64_t saved_address=excluded[2].address,saved_length=excluded[2].length;
    excluded[2].length=0;assert(!EXCLUDING(excluded,3));
    excluded[2].length=PAGE_SIZE+1;assert(!EXCLUDING(excluded,3));
    excluded[2].length=64;excluded[2].address=UINT64_MAX-31;assert(!EXCLUDING(excluded,3));
    excluded[2].address=0x800000000000ull;assert(!EXCLUDING(excluded,3));
    excluded[2].address=saved_address;excluded[2].length=saved_length;check_unchanged(before);
    assert(translate_calls==before_translation); /* All extents precede any walk. */
    uint64_t saved_extra=*extra_leaf[5];*extra_leaf[5]&=~1ull;
    assert(!EXCLUDING(excluded,3));check_unchanged(before);
    *extra_leaf[5]=saved_extra|VMM_PAGE_CACHE_DISABLE;
    assert(!EXCLUDING(excluded,3));check_unchanged(before);*extra_leaf[5]=saved_extra;
    assert(!memcmp(extra_before,extra_pages,sizeof(extra_before)));
#undef EXCLUDING
    assert(vmm_user_frame_copy_policy_current(cr3));
    assert(!vmm_user_frame_copy_policy_current(0));
    assert(!vmm_user_frame_copy_policy_current(kernel_cr3));
    address_space_records[0].used=false;assert(!vmm_user_frame_copy_policy_current(cr3));address_space_records[0].used=true;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,0,&owned,pages,2,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,1025,&owned,pages,2,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0,source,64,&owned,pages,1,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0x800000000000ull,source,64,&owned,pages,1,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,1,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,(void *)((uint8_t *)source+1),64,&owned,pages,2,frame,uf,kf));
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,(void *)((uint8_t *)frame-512),64,&owned,pages,2,frame,uf,kf));
    uint64_t saved_rsp=stack_rsp;stack_rsp=stack_va-1;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));stack_rsp=saved_rsp;
    check_unchanged(before);
    *d1&=~1ull;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    *d1|=1;
    pages[1].physical_address=pages[0].physical_address;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    pages[1].physical_address=(uint64_t)(uintptr_t)(destination_backing+4096);
    uint64_t saved=*d0;*d0=stack_phys|7;pages[0].physical_address=stack_phys;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    *d0=saved;pages[0].physical_address=(uint64_t)(uintptr_t)destination_backing;
    /* Protected FPU objects are outside the owned stack; the first straddles
     * a page, so its metadata tail lives in the second physical page. */
    uint64_t protected_pages[]={(uint64_t)(uintptr_t)(fpu_pages+PAGE_SIZE),
                                (uint64_t)(uintptr_t)(fpu_pages+2*PAGE_SIZE)};
    for(unsigned i=0;i<2;i++) {
        *d0=protected_pages[i]|7;pages[0].physical_address=protected_pages[i];
        assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    }
    *d0=saved;pages[0].physical_address=(uint64_t)(uintptr_t)destination_backing;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,(void *)source,kf));check_unchanged(before);
    uint64_t ufva=user_frame_test_kernel_address(fpu_pages);
    uint64_t *alias_leaf=map_leaf(root,ufva+PAGE_SIZE,stack_phys+PAGE_SIZE,false);
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    *alias_leaf=(uint64_t)(uintptr_t)(fpu_pages+PAGE_SIZE)|3;
    flags=1ull<<9;assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);flags=0;
    for(unsigned bit=0;bit<3;bit++) {
        uint64_t mask=1ull<<(bit==0?0:bit==1?16:31);cr0^=mask;
        assert(!vmm_user_frame_copy_policy_current(cr3));cr0^=mask;
    }
    unsigned cr4_bits[]={12,17,21,22,23,24};
    for(unsigned i=0;i<sizeof(cr4_bits)/sizeof(cr4_bits[0]);i++) {
        cr4|=1ull<<cr4_bits[i];assert(!vmm_user_frame_copy_policy_current(cr3));cr4&=~(1ull<<cr4_bits[i]);
    }
    cr4=0;assert(!vmm_user_frame_copy_policy_current(cr3));cr4=1ull<<5;
    efer=0;assert(!vmm_user_frame_copy_policy_current(cr3));efer=(1ull<<10)|(1ull<<11);
    has_pat=0;assert(!vmm_user_frame_copy_policy_current(cr3));has_pat=1;
    pat=0;assert(!vmm_user_frame_copy_policy_current(cr3));pat=6;
    maxext=0x80000007;assert(!vmm_user_frame_copy_policy_current(cr3));maxext=0x80000008;
    width=35;assert(!vmm_user_frame_copy_policy_current(cr3));width=53;assert(!vmm_user_frame_copy_policy_current(cr3));width=48;
    cr3|=1;assert(!vmm_user_frame_copy_policy_current(cr3));cr3&=~1ull;
    uint64_t *table=root;
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t *entry=&table[(0x401000ull>>shift)&511],old=*entry;
        for(unsigned bit=0;bit<3;bit++) {
            *entry=old&~(1ull<<bit);
            assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
        }
        unsigned forbidden[]={3,4,8,9,52,59};
        for(unsigned i=0;i<sizeof(forbidden)/sizeof(forbidden[0]);i++) {
            *entry=old|(1ull<<forbidden[i]);
            assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
        }
        *entry=old;
        if(shift>12)table=(void *)(uintptr_t)(old&PAGE_MASK);
    }
    pages[1].physical_address+=PAGE_SIZE;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    pages[1].physical_address-=PAGE_SIZE;
    pages[1].flags|=VMM_PAGE_CACHE_DISABLE;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    pages[1].flags&=~VMM_PAGE_CACHE_DISABLE;
    uint64_t orig_root=root[0];root[0]|=1ull<<7;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);root[0]=orig_root;
    struct user_frame_stack bad=owned;bad.physical=(1ull<<48)-PAGE_SIZE;
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&bad,pages,2,frame,uf,kf));check_unchanged(before);
    *d1|=1ull<<7;assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);*d1&=~(1ull<<7);
    *d1|=1ull<<63;assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    pages[1].flags|=VMM_PAGE_NO_EXECUTE;
    features&=~(1u<<20);assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);features|=1u<<20;
    efer&=~(1ull<<11);assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);efer|=1ull<<11;
    assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));
    memset(destination_backing,0xa5,8192);*d1&=~(1ull<<63);pages[1].flags&=~VMM_PAGE_NO_EXECUTE;
    table=root;
    for(int shift=39;shift>=12;shift-=9) {
        uint64_t *entry=&table[((stack_va+PAGE_SIZE)>>shift)&511],old=*entry;
        unsigned forbidden[]={3,4,9,52,59};
        for(unsigned i=0;i<sizeof(forbidden)/sizeof(forbidden[0]);i++) {
            *entry=old|(1ull<<forbidden[i]);
            assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
        }
        if(shift>12) {*entry=old|(1ull<<8);assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);}
        *entry=old;if(shift>12)table=(void *)(uintptr_t)(old&PAGE_MASK);
    }
    uint64_t *upper_pdpt=(void *)(uintptr_t)(root[(stack_va>>39)&511]&PAGE_MASK);
    uint64_t *upper_pd=(void *)(uintptr_t)(upper_pdpt[(stack_va>>30)&511]&PAGE_MASK);
    uint64_t *upper_entry=&upper_pd[(stack_va>>21)&511],old_upper=*upper_entry;
    check_supervisor_path(root,stack_va+PAGE_SIZE,source,&owned,pages,frame,uf,kf,before);
    check_supervisor_path(root,user_frame_test_kernel_address(uf),source,&owned,pages,frame,uf,kf,before);
    check_supervisor_path(root,user_frame_test_kernel_address(kf),source,&owned,pages,frame,uf,kf,before);
    *upper_entry=stack_phys|3|(1ull<<7)|(1ull<<12);
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    *upper_entry&=~(1ull<<12);
    check_supervisor_path(root,stack_va+PAGE_SIZE,source,&owned,pages,frame,uf,kf,before);
    assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));
    *upper_entry=old_upper;
    memset(destination_backing,0xa5,8192);
    uint64_t *pdpt_entry=&upper_pdpt[(stack_va>>30)&511],old_pdpt=*pdpt_entry;
    *pdpt_entry=stack_phys|3|(1ull<<7);
    check_supervisor_path(root,stack_va+PAGE_SIZE,source,&owned,pages,frame,uf,kf,before);
    assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));
    memset(destination_backing,0xa5,8192);
    features&=~(1u<<26);
    assert(!vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));check_unchanged(before);
    *pdpt_entry=old_pdpt;features|=1u<<26;
    assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));
    assert(!memcmp(destination_backing+0xff0,source->bytes,64));
    memset(destination_backing,0xa5,8192);
    assert(vmm_copy_user_frame_current(cr3,0x400000,source,1,&owned,pages,1,frame,uf,kf));
    assert(destination_backing[0]==source->bytes[0] && destination_backing[1]==0xa5);
    memset(destination_backing,0xa5,8192);
    assert(vmm_copy_user_frame_current(cr3,0x400e00,source,1024,&owned,pages,2,frame,uf,kf));
    assert(!memcmp(destination_backing+0xe00,source->bytes,1024));
    memset(destination_backing,0xa5,8192);
    assert(vmm_copy_user_frame_current(cr3,0x400ff0,source,64,&owned,pages,2,frame,uf,kf));
    assert(!memcmp(destination_backing,before,0xff0) && !memcmp(destination_backing+0x1030,before+0x1030,8192-0x1030));
    assert(table_count==tables_before && cr3==(uint64_t)(uintptr_t)root && flags==0);
    for(unsigned i=0;i<tables_before;i++)assert(!memcmp(table_snapshot[i],allocated_tables[i],4096));
    assert(!memcmp(&registry_before,&address_space_records[0],sizeof(registry_before)));
    assert(!memcmp(original_source,source->bytes,1024));
    assert(!munmap(guard,16384));free(extra_pages);free(fpu_pages);free(stack);
    for(unsigned i=0;i<table_count;i++)free(allocated_tables[i]);
    puts("user-frame-copy: modeled policy/walk, guarded backing, atomic preflight and aliases");
}
