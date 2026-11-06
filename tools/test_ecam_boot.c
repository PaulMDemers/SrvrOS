/* Actual VMM backend, fault-injected physical primitives; RAM only. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t pages[8][512],tags[8],clock_ticks,cr3=0x2000000,irq_flags=2;
static unsigned allocs,frees,loads,stores,invalidates,event_fault,event_index,load_fault,store_fault;
static bool store_after,alloc_fail,alloc_borrowed,ram_fail,admit_fail,clock_fail,free_fail;
static bool fail_load_after_parent,fail_invalidate_after,stop_clock;
static unsigned frozen_time;
static unsigned stage_mutation;
static unsigned checks[3];
static uint64_t vmm_test_cr3(void){return cr3;}
static void vmm_test_write_cr3(uint64_t n){(void)n;assert(false);}
static uint64_t vmm_test_rflags(void){return irq_flags;}
static void vmm_test_cpuid(uint32_t l,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){(void)l;(void)a;(void)b;(void)c;(void)d;assert(false);}
static uint64_t vmm_test_efer(void){assert(false);return 0;}
static uint64_t vmm_test_irq_save(void){assert(false);return 0;}
static void vmm_test_irq_restore(uint64_t n){(void)n;assert(false);}
/* Unrelated user-frame machinery must never execute in this fixture. Its
 * existing host hooks also keep x86 instructions out of ARM host builds. */
static _Noreturn void ecam_unexpected_user_frame(void){assert(!"unexpected user-frame route");abort();}
static uint64_t user_frame_test_rflags(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_cr0(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_cr4(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_cr3(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_efer(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_pat(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_rsp(void){ecam_unexpected_user_frame();}
static uint64_t user_frame_test_kernel_address(const void *p){(void)p;ecam_unexpected_user_frame();}
static uint8_t *user_frame_test_user_pointer(uint64_t p){(void)p;ecam_unexpected_user_frame();}
static void user_frame_test_cpuid(uint32_t n,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){(void)n;(void)a;(void)b;(void)c;(void)d;ecam_unexpected_user_frame();}
static unsigned frame_index(uint64_t pa){assert(pa>=0x2000000 && pa<0x2008000);return (pa-0x2000000)/4096;}
static uint64_t ecam_test_virtual(uint64_t pa){return 0xffff800000000000ull+pa;}
static bool ecam_test_load(uint64_t pa,unsigned i,uint64_t *out){
 assert(!(pa&4095) && i<512);loads++;if(load_fault==loads)return false;
 *out=pages[frame_index(pa)][i];return true;
}
static bool ecam_test_store(uint64_t pa,unsigned i,uint64_t before,uint64_t after){
 stores++;assert(i<512);uint64_t *p=&pages[frame_index(pa)][i];
 if(stores==store_fault && !store_after)return false;
 if(*p!=before)return false;
 *p=after;
 if(fail_load_after_parent && pa==0x2001000 && i==12)load_fault=loads+1;
 return stores!=store_fault;
}
static bool ecam_test_invalidate(uint64_t va){
 assert(va==0xffffc00300000000ull+(invalidates%39936)*4096ull);invalidates++;return !fail_invalidate_after;
}
static bool ecam_test_event(unsigned event,unsigned n){
 if(event==1 && n==511 && stage_mutation) {
  if(stage_mutation==1)pages[0][384]^=32;
  if(stage_mutation==2)pages[1][12]=0x2005003;
  if(stage_mutation==3)pages[0][0]=0x2005003;
  return false;
 }
 if(event==4 && free_fail)return false;
 return !(event_fault==event && event_index==n);
}
#define SRVROS_VMM_HOST_TEST
#define SRVROS_USER_FRAME_COPY_HOST_TEST
#define SRVROS_ECAM_BOOT
#define SRVROS_ECAM_HOST_TEST
#include "../kernel/src/memory/vmm.c"
void *pmm_phys_to_virt(uint64_t pa){return (char *)pages+(pa-0x2000000);}
uint64_t pmm_hhdm_offset(void){return 0xffff800000000000ull;}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t m,enum pmm_frame_tag tag){
 assert(n==1 && m==0x2000000 && tag==PMM_FRAME_PAGE_TABLE);allocs++;
 if(alloc_fail)return 0;
 if(alloc_borrowed)return 0x2001000;
 tags[5]=tag;return 0x2005000;
}
bool pmm_frame_has_tag(uint64_t pa,enum pmm_frame_tag tag){return tags[frame_index(pa)]==(unsigned)tag;}
void pmm_free_frame(uint64_t pa){assert(pa==0x2005000 && tags[5]==PMM_FRAME_PAGE_TABLE);frees++;tags[5]=PMM_FRAME_FREE;memset(pages[5],0xcc,4096);}
#ifndef ECAM_PROFILE_FIXTURE
void console_printf(const char *s,...){(void)s;assert(false);}
#endif
static uint64_t now(void *ctx){(void)ctx;return stop_clock?frozen_time:1000+(++clock_ticks)/(clock_fail?1:16)+(clock_fail?100001:0);}
static bool ram(void *ctx,uint64_t pa){(void)ctx;return !ram_fail && !(pa&4095) && pa>=0x2000000 && pa<0x2008000;}
static bool check(void *ctx,unsigned phase){(void)ctx;assert(phase<3);checks[phase]++;return !admit_fail;}
static const struct ecam_boot_env io={.check=check,.ram=ram,.now=now};
static void reset(void){
 memset(&eb,0,sizeof(eb));memset(&eb_diagnostic,0,sizeof(eb_diagnostic));eb_stage="none";memset(&eb_evidence,0,sizeof(eb_evidence));memset(pages,0,sizeof(pages));memset(tags,PMM_FRAME_RESERVED,sizeof(tags));
 for(unsigned i=0;i<8;i++)tags[i]=PMM_FRAME_PAGE_TABLE;
 clock_ticks=allocs=frees=loads=stores=invalidates=event_fault=event_index=load_fault=store_fault=0;
 store_after=alloc_fail=alloc_borrowed=ram_fail=admit_fail=clock_fail=free_fail=false;
 fail_load_after_parent=fail_invalidate_after=stop_clock=false;frozen_time=1000;
 stage_mutation=0;
 memset(checks,0,sizeof(checks));cr3=0x2000000;irq_flags=2;
 kernel_cr3=cr3;kernel_pml4=pages[0];kernel_mapping_count=0;address_space_count=0;
 memset(kernel_mappings,0,sizeof(kernel_mappings));memset(address_space_records,0,sizeof(address_space_records));memset(address_spaces,0,sizeof(address_spaces));
 pages[0][384]=0x2001003;pages[0][256]=0x2002003;
 pages[2][0]=0x2003003;pages[3][16]=0x2004003;
 for(unsigned i=0;i<8;i++)pages[4][i]=(0x2000000+4096*i)|3;
 memset(pages[5],0xa5,4096);tags[5]=PMM_FRAME_FREE;
}
static void retained(void){
 assert(eb.r.state==ECAM_POISONED && !frees);
 uint64_t parent=pages[1][12];unsigned writes=stores;
 assert(!vmm_ecam_boot_prepare(&io) && !vmm_ecam_boot_handoff() && !vmm_ecam_boot_retire());
 assert(!frees && parent==pages[1][12] && writes==stores);
}
int main(void){
 reset();uint64_t output;assert(!vmm_ecam_boot_copy_ram(0x2000000,&output,8));
 reset();assert(vmm_ecam_boot_prepare(&io));assert(eb.r.state==ECAM_READY && allocs==1 && !frees && invalidates==39936 && pages[1][12]==0x2005003);
 assert(!vmm_ecam_boot_copy_ram(0x2000000,&output,8));
 assert(eb.r.words<ECAM_BOOT_WORD_CAP && checks[0]==1 && checks[1]==1);
 for(unsigned i=0;i<512;i++)assert(pages[5][i]==eb_leaf(i));
 assert(vmm_ecam_boot_retire() && eb.r.state==ECAM_RETIRED && frees==1 && invalidates==79872 && !pages[1][12]);
 assert(!vmm_ecam_boot_retire());
 for(unsigned mode=1;mode<=2;mode++) {
  reset();
  if(mode==1)pages[3][16]=0x2000083; /* HHDM2MiB leaf. */
  else pages[2][0]=0x83; /* HHDM1GiB leaf, supported by the native gate. */
  assert(vmm_ecam_boot_prepare(&io));unsigned prepare_words=eb.r.words;
  assert(vmm_ecam_boot_retire() && frees==1 && invalidates==79872);
  printf("checked RAM walker shift=%u prepare_words=%u lifetime_words=%u cap=%u\n",mode==1?21:30,prepare_words,eb.r.words,ECAM_BOOT_WORD_CAP);
 }
 reset();kernel_mapping_count=1;kernel_mappings[0]=(struct kernel_mapping){0xffffc000fee00000ull,0xfee00000,0x12};
 struct kernel_mapping saved=kernel_mappings[0];assert(vmm_ecam_boot_prepare(&io));assert(vmm_ecam_boot_handoff());
 assert(eb.r.state==ECAM_ACTIVE && eb.r.metadata_attempts==39936 && kernel_mapping_count==128 && !memcmp(&saved,kernel_mappings,sizeof(saved)));
 for(unsigned i=1;i<128;i++)assert(kernel_mappings[i].virtual_address==ECAM_BOOT_VA+(i-1)*4096ull);
 assert(!vmm_ecam_boot_retire() && !frees);
 /* Every possible initial registry count has the exact old append/truncate
  * result, even for unordered/duplicate old VAs outside the ECAM span. */
 for(unsigned count=0;count<=128;count++) {
  reset();kernel_mapping_count=count;
  for(unsigned i=0;i<count;i++)kernel_mappings[i]=(struct kernel_mapping){0xffffc00000000000ull+4096*((i*37)%17),0x100000ull+i*4096,0x12+i};
  struct kernel_mapping before[128];memcpy(before,kernel_mappings,sizeof(before));
  assert(vmm_ecam_boot_prepare(&io) && vmm_ecam_boot_handoff());
  assert(kernel_mapping_count==128 && eb.r.metadata_attempts==39936 && !frees);
  for(unsigned i=0;i<128;i++) {
   struct kernel_mapping want=i<count?before[i]:(struct kernel_mapping){ECAM_BOOT_VA+(i-count)*4096ull,ECAM_BOOT_PA+(i-count)*4096ull,0x8000000000000012ull};
   assert(!memcmp(&want,&kernel_mappings[i],sizeof(want)));
  }
 }
 reset();kernel_mapping_count=1;kernel_mappings[0]=saved;
 assert(vmm_ecam_boot_prepare(&io) && vmm_ecam_boot_handoff());
 assert(vmm_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
 assert(eb.r.adoptions==1 && eb.r.adopted_us==eb.r.end && eb.r.words<65536);
 unsigned adopted_loads=loads;assert(!vmm_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES) && loads==adopted_loads);
 for(unsigned mode=0;mode<14;mode++) {
  reset();assert(vmm_ecam_boot_prepare(&io) && vmm_ecam_boot_handoff());
  uint64_t va=ECAM_BOOT_VA,pa=ECAM_BOOT_PA,bytes=ECAM_BOOT_BYTES;
  if(mode==0)va+=4096;
  if(mode==1)pa+=4096;
  if(mode==2)bytes-=4096;
  if(mode==3)pages[0][384]^=32;
  if(mode==4)pages[1][12]^=32;
  if(mode==5)pages[5][0]^=64;
  if(mode==6)tags[5]=PMM_FRAME_FREE;
  if(mode==7)kernel_mappings[127].flags^=1;
  if(mode==8)kernel_mapping_count--;
  if(mode==9)irq_flags|=512;
  if(mode==10)admit_fail=true;
  if(mode==11)eb.r.words=65536;
  if(mode==12){stop_clock=true;frozen_time=eb.r.begin-1;}
  if(mode==13){stop_clock=true;frozen_time=eb.r.begin+100000;}
  assert(!vmm_ecam_boot_adopt(va,pa,bytes) && !eb.r.adoptions);
  const char *reason[]={"geometry","geometry","geometry","root-word","pdpt-word","PD-word","frame-ownership","metadata-word","metadata-count","sole-root","fresh-gate","word-cap","clock-invalid","lifetime-deadline"};
  assert(eb_diagnostic.reason && !strcmp(eb_diagnostic.reason,reason[mode]));
  assert(eb_diagnostic.prepared_us && eb_diagnostic.handoff_us && eb_diagnostic.adopt_entry_last_us && eb_diagnostic.words==eb.r.words);
  if(mode>=3 && mode<=5){assert(eb_diagnostic.details==3);assert((eb_diagnostic.expected^eb_diagnostic.observed)==(mode==5?64:32));}
  if(mode>=12)assert(eb_diagnostic.rejected_clock && eb_diagnostic.sample_us==frozen_time && eb_diagnostic.sample_clock_valid==(mode==13));
  struct ecam_boot_diagnostic first=eb_diagnostic;
  retained();assert(!memcmp(&first,&eb_diagnostic,sizeof(first)));
 }
 for(unsigned mode=0;mode<10;mode++){
  reset();if(mode==0)irq_flags|=512;if(mode==1)admit_fail=true;if(mode==2)ram_fail=true;if(mode==3)cr3|=1;
  if(mode==4)address_space_count=1;
  if(mode==5)pages[1][12]=2;
  if(mode==6)alloc_fail=true;
  if(mode==7)kernel_mappings[127].virtual_address=ECAM_BOOT_VA;
  if(mode==8)pages[2][0]|=1ull<<52;
  if(mode==9)pages[2][0]=0xfed90003; /* Device page cannot be a table source. */
  assert(!vmm_ecam_boot_prepare(&io) && eb.r.state==ECAM_REFUSED && !frees && !stores);
 }
 reset();alloc_borrowed=true;assert(!vmm_ecam_boot_prepare(&io));retained();
 /* Every detached private store failure is clean, including ambiguous ack. */
 for(unsigned i=1;i<=512;i++)for(unsigned after=0;after<2;after++){
  reset();store_fault=i;store_after=after;assert(!vmm_ecam_boot_prepare(&io));
  assert(eb.r.state==ECAM_REFUSED && frees==1 && !pages[1][12]);
 }
 for(unsigned after=0;after<2;after++){
  reset();store_fault=513;store_after=after;assert(!vmm_ecam_boot_prepare(&io));retained();
 }
 const unsigned positions[]={0,20000,39935};
 for(unsigned i=0;i<3;i++){
  reset();event_fault=2;event_index=positions[i];assert(!vmm_ecam_boot_prepare(&io));retained();
  reset();assert(vmm_ecam_boot_prepare(&io));event_fault=2;event_index=positions[i];assert(!vmm_ecam_boot_retire());retained();
  reset();assert(vmm_ecam_boot_prepare(&io));event_fault=3;event_index=positions[i];assert(!vmm_ecam_boot_handoff());retained();
 }
 reset();assert(vmm_ecam_boot_prepare(&io));pages[1][12]^=32;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));pages[5][511]=1;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));free_fail=true;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));clock_fail=true;assert(!vmm_ecam_boot_retire());retained();
 reset();load_fault=5;assert(!vmm_ecam_boot_prepare(&io) && !stores && !allocs);
 for(unsigned after=0;after<2;after++){
  reset();assert(vmm_ecam_boot_prepare(&io));store_fault=stores+1;store_after=after;
  assert(!vmm_ecam_boot_retire());retained();
 }
 reset();fail_load_after_parent=true;assert(!vmm_ecam_boot_prepare(&io));retained();
 reset();assert(vmm_ecam_boot_prepare(&io));fail_load_after_parent=true;assert(!vmm_ecam_boot_retire());retained();
 reset();fail_invalidate_after=true;assert(!vmm_ecam_boot_prepare(&io));retained();
 reset();assert(vmm_ecam_boot_prepare(&io));fail_invalidate_after=true;assert(!vmm_ecam_boot_retire());retained();
 reset();stop_clock=true;assert(!vmm_ecam_boot_prepare(&io) && eb.r.state==ECAM_REFUSED && frees==1 && !pages[1][12]);
 reset();assert(vmm_ecam_boot_prepare(&io));frozen_time=eb.last;stop_clock=true;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));eb.r.words=ECAM_BOOT_WORD_CAP;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));tags[5]=PMM_FRAME_DMA;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));frozen_time=eb.last-1;stop_clock=true;assert(!vmm_ecam_boot_retire());retained();
 reset();assert(vmm_ecam_boot_prepare(&io));frozen_time=eb.last;stop_clock=true;assert(!vmm_ecam_boot_handoff());retained();
 for(unsigned i=1;i<=3;i++){reset();stage_mutation=i;assert(!vmm_ecam_boot_prepare(&io));retained();}
 puts("actual ECAM backend PASS: 4-level RAM walks, 512 detached-store faults before/after, parent ambiguity, full invalidation, guards, registry handoff, exact cleanup and retained poison");
 return 0;
}
