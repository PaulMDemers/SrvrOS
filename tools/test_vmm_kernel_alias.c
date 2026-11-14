/* Exercise the actual VMM walker, not a second model of its decoding. */
static void range_lookup_hook(void);
#define SRVROS_VMM_RANGE_TEST
#define main previous_vmm_test_main
#include "test_vmm_user_range.c"
#undef main
static unsigned flip_at;
static void range_lookup_hook(void){if(flip_at && table_reads==flip_at)test_rflags|=1ull<<9;}
static uint64_t *t[7],root;
static const uint64_t va=0xffff800000000000ull,pa=0xb0000000;
static struct vmm_alias_observation out;
static struct vmm_range_observation detail;
static void layout(void){
 for(unsigned i=0;i<7;i++)memset(t[i],0,4096);
 root=kernel_cr3=test_cr3=(uint64_t)t[0];kernel_pml4=t[0];
 address_space_count=address_space_occupancy=0;test_rflags=2;
 t[0][256]=(uint64_t)t[1]|7;t[1][0]=(uint64_t)t[2]|7;
 for(unsigned n=0;n<3;n++){
  t[2][n]=(uint64_t)t[n+3]|7;
  for(unsigned i=0;i<512;i++)t[n+3][i]=(pa+(n*512+i)*4096)|3|VMM_PAGE_NO_EXECUTE;
 }
}
static bool check(uint64_t v,uint64_t p,unsigned pages,unsigned bits){
 uint64_t saved[7][512];for(unsigned i=0;i<7;i++)memcpy(saved[i],t[i],4096);
 memset(&out,0x5a,sizeof(out));struct vmm_alias_observation before=out;
 unsigned sw=switches,ret=retire_count;uint64_t flags=test_rflags,cr3=test_cr3;
 bool ok=vmm_diagnostic_kernel_alias(root,v,p,pages,bits,&out,&detail);
 if(ok)assert(out.pages==pages && detail.phase==4 && detail.checked==pages && !detail.reason);
 else assert(!memcmp(&before,&out,sizeof(out)) && detail.reason);
 for(unsigned i=0;i<7;i++)assert(!memcmp(saved[i],t[i],4096));
 assert(switches==sw && retire_count==ret && test_cr3==cr3);
 assert(test_rflags==flags || (flip_at && test_rflags==(flags|512)));
 return ok;
}
int main(void){
 for(unsigned i=0;i<7;i++){t[i]=aligned_alloc(4096,4096);assert(t[i]);}
 layout();assert(check(va,pa,1356,52));
 assert(out.leaves==1356 && out.pat_index==0 && out.writable && !out.user && out.no_execute && out.leaf_sizes==1);
 /* Every interior/final page must contribute, including PAT bit7 (not PS).
  * A/D bits have no effect; permission/PAT drift refuses the whole range. */
 for(unsigned i=0;i<1356;i++){
  uint64_t *p=&t[3+i/512][i%512],old=*p;
  for(unsigned bit=1;bit<=7;bit++)if(bit!=5 && bit!=6){*p=old^(1ull<<bit);assert(!check(va,pa,1356,52));*p=old;}
  *p=old^VMM_PAGE_NO_EXECUTE;assert(!check(va,pa,1356,52));
  *p=old^4096;assert(!check(va,pa,1356,52));
  *p=old|0x60;assert(check(va,pa,1356,52));*p=old;
 }
 for(unsigned index=0;index<8;index++){
  layout();for(unsigned n=0;n<3;n++)for(unsigned i=0;i<512;i++)t[n+3][i]|=((index&3)<<3)|((index>>2)<<7);
  assert(check(va,pa,1356,52) && out.pat_index==index);
 }
 /* Nonleaf cache flags do not select data PAT. Ancestor restrictions do. */
 layout();t[0][256]|=0x18;t[1][0]|=0x18;t[2][0]|=0x18;
 assert(check(va,pa,1356,52) && !out.pat_index);
 t[0][256]&=~2ull;assert(check(va,pa,1356,52) && !out.writable);
 layout();for(unsigned n=0;n<3;n++)for(unsigned i=0;i<512;i++)t[n+3][i]|=4;
 t[0][256]&=~4ull;assert(check(va,pa,1356,52) && !out.user);
 layout();t[2][1]&=~2ull;assert(!check(va,pa,1356,52) && detail.reason==VMM_RANGE_ATTRIBUTES && detail.checked==512);
 layout();for(unsigned n=0;n<3;n++)for(unsigned i=0;i<512;i++)t[n+3][i]&=~VMM_PAGE_NO_EXECUTE;
 t[0][256]|=VMM_PAGE_NO_EXECUTE;assert(check(va,pa,1356,52) && out.no_execute);
 /* Large PAT is bit12; reserved alignment bits are never silently masked. */
 layout();for(unsigned n=0;n<3;n++)t[2][n]=(pa+n*0x200000)|0x1083|VMM_PAGE_NO_EXECUTE;
 assert(check(va,pa,1356,52) && out.leaves==3 && out.pat_index==4 && out.leaf_sizes==2);
 t[2][1]|=0x2000;assert(!check(va,pa,1356,52) && detail.reason==VMM_RANGE_RESERVED);
 layout();t[1][0]=0x40000000|0x1083;
 assert(check(va,0x40000000,4096,52) && out.leaves==1 && out.pat_index==4 && out.leaf_sizes==4);
 t[1][0]|=1ull<<29;assert(!check(va,0x40000000,1,52));
 /* Mixed valid leaf sizes, a final 4K leaf, and a 1GiB boundary. */
 layout();t[2][0]=pa|0x83|VMM_PAGE_NO_EXECUTE;
 assert(check(va,pa,1356,52) && out.leaves==845 && out.leaf_sizes==3);
 layout();t[1][0]=0x40000000|0x83;t[1][1]=0x80000000|0x83;
 assert(check(va+0x3ffff000,0x7ffff000,2,52) && out.leaves==2);
 t[1][1]|=0x1000;assert(!check(va+0x3ffff000,0x7ffff000,2,52));
 /* Invalid input/context and exit loss must never publish a partial result. */
 layout();assert(!check(va,pa,0,52));assert(!check(va,pa,4097,52));
 assert(!check(va+1,pa,1,52));assert(!check(va,pa+1,1,52));
 assert(!check(0x7ffffffff000ull,pa,2,52));assert(!check(va,pa,1,35));assert(!check(va,pa,1,53));
 assert(!check(va,1ull<<39,1,39));assert(!check(va,(1ull<<39)-4096,2,39));
 assert(!check(va,pa,1,39) && detail.reason==VMM_RANGE_RESERVED); /* host table above39 */
 for(unsigned n=0;n<4;n++){
  layout();uint64_t *p=n==0?&t[0][256]:n==1?&t[1][0]:n==2?&t[2][0]:&t[3][0];
  *p=0;assert(!check(va,pa,1,52));
 }
 layout();test_rflags|=512;assert(!check(va,pa,1356,52) && detail.reason==VMM_RANGE_IRQ);
 layout();address_space_occupancy=8;assert(!check(va,pa,1356,52));
 layout();test_cr3|=1;assert(!check(va,pa,1356,52));
 layout();flip_at=table_reads+10;assert(!check(va,pa,1356,52));
 assert(detail.phase==3 && detail.checked==1356 && detail.reason==VMM_RANGE_IRQ);flip_at=0;
 for(unsigned i=0;i<7;i++)free(t[i]);
 puts("vmm-kernel-alias: PASS complete1356-page faults, PAT/permissions/large/mixed leaves, exclusion and no mutation/publication on failure");
}
