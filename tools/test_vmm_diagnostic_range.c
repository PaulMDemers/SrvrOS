/* Actual VMM, inherited host stubs; no model of the new walker. */
#define main previous_vmm_test_main
#include "test_vmm_user_range.c"
#undef main

static uint64_t *tables[7];
static uint64_t root, base=0xffff800000000000ull;
static const uint64_t physical=0xb0000000;
static bool check(uint64_t va,uint64_t pa,uint64_t pages) {
 uint64_t saved[7][512];
 for(unsigned i=0;i<7;i++)memcpy(saved[i],tables[i],4096);
 uint64_t before_cr3=test_cr3,before_flags=test_rflags,before_occupancy=address_space_occupancy;
 unsigned before_switch=switches,before_retire=retire_count;
 bool ok=vmm_diagnostic_contiguous_kernel_range(root,va,pa,pages);
 for(unsigned i=0;i<7;i++)assert(!memcmp(saved[i],tables[i],4096));
 assert(test_cr3==before_cr3 && test_rflags==before_flags && address_space_occupancy==before_occupancy);
 assert(switches==before_switch && retire_count==before_retire);
 return ok;
}
static void layout(void) {
 for(unsigned i=0;i<7;i++)memset(tables[i],0,4096);
 kernel_pml4=tables[0];root=kernel_cr3=test_cr3=(uint64_t)tables[0];
 test_rflags=0x46;address_space_count=address_space_occupancy=0;
 tables[0][256]=(uint64_t)tables[1]|3;
 tables[1][0]=(uint64_t)tables[2]|3;
 for(unsigned i=0;i<3;i++) {
  tables[2][i]=(uint64_t)tables[3+i]|3;
  for(unsigned j=0;j<512;j++)tables[3+i][j]=(physical+(i*512+j)*4096)|VMM_PAGE_PRESENT|VMM_PAGE_NO_EXECUTE;
 }
}
int main(void) {
 for(unsigned i=0;i<7;i++){tables[i]=aligned_alloc(4096,4096);assert(tables[i]);}
 layout();
 /* Same1350 visible framebuffer pages; all leaf presence/address faults are
  * detected even at the final page and across both PT boundaries. */
 assert(check(base,physical,1350));
 for(unsigned i=0;i<1350;i++) {
  uint64_t *p=&tables[3+i/512][i%512],old=*p;
  *p=old&~VMM_PAGE_PRESENT;assert(!check(base,physical,1350));
  *p=old^4096;assert(!check(base,physical,1350));*p=old;
 }
 unsigned before=table_reads;
 for(unsigned i=0;i<1350;i++){uint64_t out;assert(vmm_virt_to_phys_in_space(root,base+i*4096,&out) && out==physical+i*4096);}
 unsigned scalar=table_reads-before;before=table_reads;
 assert(check(base,physical,1350));unsigned batched=table_reads-before;
 assert(scalar==5400 && batched==11);
 /* Every ancestor is fresh on the next invocation. */
 for(unsigned n=0;n<3;n++) {
  uint64_t *p=n==0?&tables[0][256]:n==1?&tables[1][0]:&tables[2][2],old=*p;
  *p=0;assert(!check(base,physical,1350));*p=old;
 }
 assert(check(base+4096,physical+4096,1349));
 assert(!check(base,physical,1537));
 assert(!check(base+1,physical,1));assert(!check(base,physical+1,1));
 assert(!check(base,physical,0));assert(!check(base,physical,4097));
 assert(!check(base,UINT64_MAX&~4095ull,2));
 assert(!check(UINT64_MAX&~4095ull,physical,2));
 assert(!check(0x800000000000ull,physical,1));
 assert(!check(0x7ffffffff000ull,physical,2));
 unsigned reads=table_reads;test_rflags|=1ull<<9;assert(!check(base,physical,1) && table_reads==reads);test_rflags&=~(1ull<<9);
 test_cr3=root+4096;assert(!check(base,physical,1) && table_reads==reads);test_cr3=root;
 address_space_count=1;assert(!check(base,physical,1));address_space_count=0;
 address_space_occupancy=1;assert(!check(base,physical,1));address_space_occupancy=0;
 kernel_pml4=tables[1];assert(!check(base,physical,1));kernel_pml4=tables[0];
 uint64_t old=root;root|=1;assert(!check(base,physical,1));root=old;
 /* Large leaves and boundary rewalks agree with scalar semantics. */
 tables[2][0]=physical|VMM_PAGE_PRESENT|PAGE_LARGE;
 tables[2][1]=(physical+0x200000)|VMM_PAGE_PRESENT|PAGE_LARGE;
 assert(check(base,physical,1024));tables[2][1]^=0x200000;assert(!check(base,physical,1024));
 layout();tables[1][0]=0x40000000|VMM_PAGE_PRESENT|PAGE_LARGE;
 assert(check(base,0x40000000,4096));
 tables[1][1]=0x80000000|VMM_PAGE_PRESENT|PAGE_LARGE;
 assert(check(base+0x3ffff000,0x7ffff000,2));tables[1][1]=0;assert(!check(base+0x3ffff000,0x7ffff000,2));
 /* Cross a PML4 boundary without reusing the old ancestor path. */
 layout();tables[0][257]=(uint64_t)tables[6]|3;tables[1][511]=0x40000000|VMM_PAGE_PRESENT|PAGE_LARGE;
 tables[6][0]=0x80000000|VMM_PAGE_PRESENT|PAGE_LARGE;
 assert(check(base+0x7ffffff000ull,0x7ffff000,2));tables[0][257]=0;assert(!check(base+0x7ffffff000ull,0x7ffff000,2));
 layout();tables[0][256]|=PAGE_LARGE;assert(!check(base,physical,1));
 for(unsigned i=0;i<7;i++)free(tables[i]);
 printf("vmm-diagnostic-range: PASS all1350 leaf faults, boundaries, exclusion, no mutations; table conversions scalar=%u batched=%u\n",scalar,batched);
}
