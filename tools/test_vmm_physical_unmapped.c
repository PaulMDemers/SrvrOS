/* Execute the actual whole-root VMM walker. No target RAM is dereferenced. */
static void vmm_unmapped_efer_hook(void);
#define SRVROS_VMM_UNMAPPED_CONTEXT_TEST
#define SRVROS_VMM_ALIAS_FIXTURE_MAIN unique_alias_regression
#include "test_vmm_alias_inventory.c"
static struct vmm_physical_span query[2];
static struct vmm_unmapped_detail absent_detail;
static unsigned cases,terminal_drift;
static void vmm_unmapped_efer_hook(void) {
 if(efer_calls==2) {
  if(terminal_drift==1)test_rflags|=512;
  if(terminal_drift==2)test_cr3^=4096;
 }
}
static void unmapped_layout(void) {
 layout();terminal_drift=efer_calls=0;
 for(unsigned n=0;n<6;n++)for(unsigned i=0;i<512;i++)t[3+n][i]=0;
 query[0]=(struct vmm_physical_span){pa,1536};query[1]=(struct vmm_physical_span){pa+1536*4096ull,1536};
}
static bool absent(void) {
 uint64_t before[9][512];for(unsigned i=0;i<9;i++)memcpy(before[i],t[i],4096);
 memset(&result,0xa5,sizeof(result));struct vmm_alias_inventory old=result;
 unsigned sw=switches,ret=retire_count;
 /* Targets are deliberately NOT in the table RAM whitelist. */
 bool ok=vmm_diagnostic_kernel_physical_unmapped(root,query,2,52,&ram[1],1,&result,&absent_detail);
 assert(sw==switches && ret==retire_count);
 for(unsigned i=0;i<9;i++)assert(!memcmp(before[i],t[i],4096));
 if(!ok)assert(!memcmp(&old,&result,sizeof(result)));
 else {
  assert(!absent_detail.walk.reason && !absent_detail.entry && !absent_detail.bytes);
  for(unsigned i=0;i<2;i++)assert(!result.matches[i] && !result.pages[i]);
 }
 ++cases;return ok;
}
int main(void) {
 assert(!unique_alias_regression());
 for(unsigned i=0;i<260;i++){t[i]=aligned_alloc(4096,4096);assert(t[i]);}
 unmapped_layout();assert(absent());assert(result.tables==9 && result.entries==4608);
 /* Every candidate page can have a hidden mapping, including the last leaf. */
 for(unsigned i=0;i<3072;i++) {
  unmapped_layout();unsigned n=i/512,index=i%512;
  uint64_t entry=pa+4096ull*i;entry|=VMM_PAGE_PRESENT|((i&1)?VMM_PAGE_NO_EXECUTE:0)|((i&2)?128:0);
  t[n+3][index]=entry;assert(!absent());
  assert(absent_detail.walk.reason==VMM_ALIAS_PRESENT && absent_detail.walk.level==4);
  assert(absent_detail.entry==entry && absent_detail.virtual_base==va+4096ull*i && absent_detail.physical==pa+4096ull*i);
  assert(absent_detail.bytes==4096 && absent_detail.span==i/1536 && absent_detail.prefix.leaves==1);
 }
 unmapped_layout();t[2][0]=pa|0x1083;assert(!absent());
 assert(absent_detail.walk.reason==VMM_ALIAS_PRESENT && absent_detail.walk.level==3 && absent_detail.bytes==0x200000 && absent_detail.entry==(pa|0x1083));
 unmapped_layout();t[1][0]=0x80000000ull|0x1083;assert(!absent());
 assert(absent_detail.walk.reason==VMM_ALIAS_PRESENT && absent_detail.walk.level==2 && absent_detail.bytes==0x40000000 && absent_detail.physical==0x80000000);
 unmapped_layout();t[0][511]=(uint64_t)t[9]|3;t[9][511]=(uint64_t)t[10]|3;t[10][511]=(uint64_t)t[11]|3;t[11][511]=(pa+1536*4096ull)|3;
 assert(!absent() && absent_detail.span==1 && absent_detail.virtual_base==0xfffffffffffff000ull);
 for(unsigned mode=1;mode<=2;mode++) {
  unmapped_layout();terminal_drift=mode;assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_ROOT);
  unmapped_layout();t[3][0]=pa|3;terminal_drift=mode;
  assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_ROOT && !absent_detail.entry);
 }
 /* Adjacent mappings, even huge ones, are not overlapping mappings. */
 unmapped_layout();t[3][0]=(pa-4096)|3;t[3][511]=(pa+3072*4096ull)|3;assert(absent());
 unmapped_layout();t[1][1]=0x40000000ull|0x83;assert(absent());
 /* A target may never be followed as page-table backing. */
 unmapped_layout();query[0]=(struct vmm_physical_span){root,1};assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_TABLE_BACKING && !table_reads);
 unmapped_layout();query[0]=(struct vmm_physical_span){(uint64_t)t[1],1};assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_TABLE_BACKING && table_reads==1);
 unmapped_layout();t[1][7]=root|3;assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_CYCLE);
 unmapped_layout();t[1][0]=0x80000000ull|0x83|(1ull<<20);assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_RESERVED);
 unmapped_layout();t[2][0]=0x40000000ull|0x83|(1ull<<21);assert(absent());
 unmapped_layout();t[2][0]=0x40000000ull|0x83|(1ull<<13);assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_RESERVED);
 unmapped_layout();assert(absent());unsigned conversions=table_reads;
 for(unsigned kind=1;kind<=6;kind++)for(unsigned at=2;at<=conversions;at++) {
  unmapped_layout();change_at=at;change_kind=kind;assert(!absent());assert(absent_detail.walk.reason!=VMM_ALIAS_PRESENT);
 }
 /* Finding a leaf is not permission to bypass terminal root/mode guards. */
 for(unsigned kind=1;kind<=6;kind++) {
  unmapped_layout();t[3][0]=pa|3;change_at=4;change_kind=kind;assert(!absent());
  assert(absent_detail.walk.reason!=VMM_ALIAS_PRESENT && !absent_detail.entry);
 }
 unmapped_layout();for(unsigned i=0;i<247;i++)t[0][i]=(uint64_t)t[12]|3;t[12][0]=0x40000000ull|0x83;
 assert(absent() && result.tables==256 && result.entries==131072);
 t[0][247]=(uint64_t)t[12]|3;assert(!absent() && absent_detail.walk.reason==VMM_ALIAS_LIMIT);
 for(unsigned mode=0;mode<8;mode++) {
  unmapped_layout();
  if(mode==0)query[0].pages=0;
  if(mode==1)query[1]=query[0];
  if(mode==2)query[0].physical++;
  if(mode==3)query[0].physical=UINT64_MAX-4095;
  if(mode==4)query[0].pages=4097;
  if(mode==5)test_rflags|=512;
  if(mode==6)ram[1].length=1;
  if(mode==7)test_cr4|=1ull<<17;
  assert(!absent());
 }
 unmapped_layout();assert(!vmm_diagnostic_kernel_physical_unmapped(root,query,0,52,&ram[1],1,&result,&absent_detail));
 assert(!vmm_diagnostic_kernel_physical_unmapped(root,query,3,52,&ram[1],1,&result,&absent_detail));
 assert(!vmm_diagnostic_kernel_physical_unmapped(root,query,2,53,&ram[1],1,&result,&absent_detail));
 assert(!vmm_diagnostic_kernel_physical_unmapped(root,query,2,52,&ram[1],1,0,&absent_detail));
 assert(!vmm_diagnostic_kernel_physical_unmapped(root,0,2,52,&ram[1],1,&result,&absent_detail));
 for(unsigned i=0;i<260;i++)free(t[i]);
 printf("vmm-physical-unmapped: PASS cases=%u original-walker=1 no-target-load=1 writes=0 ownership=0\n",cases);return 0;
}
