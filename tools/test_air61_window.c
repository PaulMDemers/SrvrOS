/* Actual VMM + census backend. Only physical primitives are fault injected. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/pmm.h>
static uint64_t pages[8][512],tags[8],cr3,irq_flags,ticks;
static unsigned allocs,frees,loads,stores,invalidates,alloc_fail_at,free_fail_at,store_fail_at;
static unsigned load_fail_at,invalidate_fail_at,delay_load_at,root_change_load_at,ram_fail_load_at;
static bool delay_at_publish,delay_in_census;
static bool aw_census_active(void);
static int gate_fail=-1;
static bool borrowed,duplicate,store_after,ram_fail,bad_pointer,reenter,zero_clock;
static bool fail_publish_readback,census_change,force_cap,bad_alloc,corrupt_parent,corrupt_after_clear;
static bool stop_clock,stop_at_publish;static uint64_t stopped_time;
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

static uint64_t aw_clock_sample(void);
static unsigned frame_index(uint64_t pa){assert(pa>=0x2000000 && pa<0x2008000 && !(pa&4095));return (pa-0x2000000)/4096;}
static uint64_t aw_test_virtual(uint64_t pa){return 0xffff800000000000ull+pa+(bad_pointer?4096:0);}
static bool aw_test_load(uint64_t pa,unsigned i,uint64_t *out){assert(pa!=0xfed90000 && i<512);loads++;if(loads==load_fail_at)return false;*out=pages[frame_index(pa)][i];if(loads==delay_load_at)ticks+=1600000;if(delay_in_census && aw_census_active()){delay_in_census=false;ticks+=1600000;}if(loads==root_change_load_at)cr3+=4096;if(loads==ram_fail_load_at)ram_fail=true;return true;}
static bool aw_test_store(uint64_t pa,unsigned i,uint64_t before,uint64_t after,bool private){
 stores++;assert(i<512);uint64_t *p=&pages[frame_index(pa)][i];
 if(stores==store_fail_at && !store_after)return false;
 if(!private && *p!=before)return false;
 *p=after;if(delay_at_publish && pa==0x2001000 && i==7)ticks+=1600000;if(stop_at_publish && pa==0x2001000 && i==7){stop_clock=true;stopped_time=aw_clock_sample();}if(corrupt_parent && pa==0x2001000 && i==7)*p^=32;if(corrupt_after_clear && pa==0x2001000 && i==7 && !after)pages[1][8]^=32;if(fail_publish_readback && pa==0x2001000 && i==7)load_fail_at=loads+1;return stores!=store_fail_at;
}
static bool aw_test_invalidate(uint64_t va){assert(va==0xffffc001f3250000ull);invalidates++;return invalidates!=invalidate_fail_at;}
static bool aw_test_free(uint64_t pa){unsigned i=frame_index(pa);assert(i==6 || i==7);assert(tags[i]==PMM_FRAME_PAGE_TABLE);if(frees+1==free_fail_at)return false;frees++;tags[i]=PMM_FRAME_FREE;memset(pages[i],0xcc,4096);return true;}
#define SRVROS_VMM_HOST_TEST
#define SRVROS_USER_FRAME_COPY_HOST_TEST
#define SRVROS_AIR61_WINDOW
#define SRVROS_AIR61_WINDOW_HOST_TEST
#include "../kernel/src/memory/vmm.c"
static bool aw_census_active(void){return aw.in_census;}
void *pmm_phys_to_virt(uint64_t pa){return (char *)pages+(pa-0x2000000);}
uint64_t pmm_hhdm_offset(void){return 0xffff800000000000ull;}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t min,enum pmm_frame_tag tag){
 assert(n==1 && min==0x2000000 && tag==PMM_FRAME_PAGE_TABLE);allocs++;
 if(allocs==alloc_fail_at)return 0;
 if(bad_alloc)return AIR61_WINDOW_PA;
 if(borrowed)return 0x2001000;
 unsigned f=duplicate?6:5+allocs;assert(f<8);tags[f]=tag;return 0x2000000+4096*f;
}
bool pmm_frame_has_tag(uint64_t pa,enum pmm_frame_tag tag){return pa>=0x2000000 && pa<0x2008000 && !(pa&4095) && tags[frame_index(pa)]==(unsigned)tag;}
void pmm_free_frame(uint64_t pa){(void)pa;assert(false);}
#ifndef AIR61_WINDOW_PROFILE_FIXTURE
void console_printf(const char *fmt,...){(void)fmt;assert(false);}
#endif
static uint64_t aw_clock_sample(void){return aw.last;}
static uint64_t now(void *ctx){(void)ctx;if(stop_clock)return stopped_time;return zero_clock?0:1000+(++ticks)/16;}
static bool ram(void *ctx,uint64_t pa){(void)ctx;if(census_change && aw.in_census && aw.census.reads[0]==2560 && !aw.census.reads[1]){census_change=false;pages[4][2]^=32;}return !ram_fail && !(pa&4095) && pa>=0x2000000 && pa<0x2008000;}
static bool check(void *ctx,unsigned phase){
 (void)ctx;assert(phase<3);if(force_cap)aw.r.words=AIR61_WINDOW_WORD_CAP;
 if(reenter){reenter=false;struct air61_window_token token={0};assert(!vmm_air61_window_prepare(&aw.io,1,1,&token));}
 return (int)phase!=gate_fail;
}
static const struct air61_window_env io={.check=check,.ram=ram,.now=now};
static struct air61_window_token token;
static void reset(void){
 memset(&aw,0,sizeof(aw));memset(pages,0,sizeof(pages));memset(tags,0,sizeof(tags));memset(&token,0,sizeof(token));
 for(unsigned i=0;i<6;i++)tags[i]=PMM_FRAME_PAGE_TABLE;
 cr3=kernel_cr3=0x2000000;irq_flags=2;kernel_pml4=pages[0];address_space_count=0;kernel_mapping_count=0;
 address_space_occupancy=0;memset(address_space_records,0,sizeof(address_space_records));memset(address_spaces,0,sizeof(address_spaces));memset(kernel_mappings,0,sizeof(kernel_mappings));
 pages[0][384]=0x2001003;pages[0][256]=0x2002003;pages[2][0]=0x2003003;pages[3][16]=0x2004003;
 for(unsigned i=0;i<8;i++)pages[4][i]=(0x2000000+4096*i)|3;
 memset(pages[6],0xa5,4096);memset(pages[7],0xa5,4096);
 ticks=allocs=frees=loads=stores=invalidates=alloc_fail_at=free_fail_at=store_fail_at=load_fail_at=invalidate_fail_at=delay_load_at=root_change_load_at=ram_fail_load_at=0;delay_at_publish=delay_in_census=false;
 borrowed=duplicate=store_after=ram_fail=bad_pointer=reenter=zero_clock=false;fail_publish_readback=census_change=force_cap=bad_alloc=corrupt_parent=corrupt_after_clear=stop_clock=stop_at_publish=false;stopped_time=0;gate_fail=-1;
}
static bool open_window(void){return vmm_air61_window_prepare(&io,10,20,&token);}
static void poisoned(void){
 assert(aw.r.state==AIR61_WINDOW_POISONED && aw.r.claimed && aw.r.fault);
 unsigned oldstores=stores,oldfrees=frees,oldloads=loads;uint64_t parent=pages[1][7];
 assert(!open_window() && !vmm_air61_window_retire(&token));
 assert(stores==oldstores && frees==oldfrees && loads==oldloads && pages[1][7]==parent);
}
int main(void){
 reset();uint64_t denied=0;unsigned denied_loads=loads;assert(!vmm_air61_window_copy_ram(0x2000000,&denied,8) && loads==denied_loads);
 uint64_t before[2][512];memcpy(before[0],pages[0],4096);memcpy(before[1],pages[1],4096);
 assert(vmm_air61_window_result()==&aw.r && vmm_air61_window_census()==&aw.census);
 assert(open_window() && aw.r.state==AIR61_WINDOW_ACTIVE && aw.r.claimed && allocs==2 && !frees);
 assert(aw.census.status==1 && aw.census.pages==5 && aw.census.reads[0]==2560 && aw.census.reads[1]==2560 && !aw.census.target_leaves);
 assert(pages[1][7]==0x2006003 && pages[6][409]==0x2007003 && pages[7][80]==AIR61_WINDOW_LEAF);
 uint64_t physical=0;assert(vmm_virt_to_phys_in_space(cr3,AIR61_WINDOW_VA,&physical) && physical==AIR61_WINDOW_PA);
 struct air61_window_token wrong=token;wrong.owner++;assert(!vmm_air61_window_retire(&wrong) && aw.r.state==AIR61_WINDOW_ACTIVE && !frees);
 wrong=token;wrong.epoch++;assert(!vmm_air61_window_retire(&wrong));wrong=token;wrong.root+=4096;assert(!vmm_air61_window_retire(&wrong));
 assert(!open_window() && aw.r.state==AIR61_WINDOW_ACTIVE);
 assert(vmm_air61_window_retire(&token) && aw.r.state==AIR61_WINDOW_RETIRED && !aw.r.claimed && frees==2 && invalidates==2);
 assert(!memcmp(before[0],pages[0],4096) && !memcmp(before[1],pages[1],4096));
 assert(aw.r.words<AIR61_WINDOW_WORD_CAP && aw.r.end-aw.r.begin<AIR61_WINDOW_LIMIT_US && aw.r.parent_attempts==1 && aw.r.unlink_attempts==1);
 printf("air61-window successful lifetime words=%u elapsed=%llu\n",aw.r.words,(unsigned long long)(aw.r.end-aw.r.begin));
 assert(!open_window() && !vmm_air61_window_retire(&token));
 /* Retained native ancestor pattern: U/S may be set above a supervisor
  * leaf. Exercise every combination at each supported source leaf size. */
 for(unsigned size=0;size<3;size++)for(unsigned mask=0;mask<8;mask++){
  reset();
  if(mask&1)pages[0][256]|=4;
  if(mask&2)pages[2][0]|=4;
  if(mask&4)pages[3][16]|=4;
  if(size==1)pages[3][16]=0x2000083;
  if(size==2)pages[2][0]=0x83;
  memcpy(before[0],pages[0],4096);memcpy(before[1],pages[1],4096);
  assert(open_window() && vmm_air61_window_retire(&token));
  assert(!memcmp(before[0],pages[0],4096) && !memcmp(before[1],pages[1],4096));
  assert(frees==2 && !aw.r.claimed && aw.r.words<=AIR61_WINDOW_WORD_CAP);
 }
 for(unsigned size=0;size<3;size++){
  unsigned last=size==0?3:size==1?2:1;
  /* Refuse user leaves even with a supervisor ancestor, and fully user paths. */
  for(unsigned upper=0;upper<2;upper++){
   reset();if(upper){pages[0][256]|=4;pages[2][0]|=4;pages[3][16]|=4;}
   if(size==0)for(unsigned i=0;i<8;i++)pages[4][i]|=4;
   if(size==1)pages[3][16]=0x2000087;
   if(size==2)pages[2][0]=0x87;
   assert(!open_window() && !allocs && !stores && !aw.r.claimed);
   assert(!strcmp(aw.r.fault,"HHDM-permissions"));
  }
  /* A read-only ancestor or leaf cannot authorize a later source/store. */
  for(unsigned level=0;level<=last;level++){
   reset();pages[0][256]|=4;pages[2][0]|=4;pages[3][16]|=4;
   if(size==1)pages[3][16]=0x2000083;
   if(size==2)pages[2][0]=0x83;
   if(level==0)pages[0][256]&=~2ull;
   if(level==1)pages[2][0]&=~2ull;
   if(level==2)pages[3][16]&=~2ull;
   if(level==3)for(unsigned i=0;i<8;i++)pages[4][i]&=~2ull;
   assert(!open_window() && !allocs && !stores && !aw.r.claimed);
   assert(!strcmp(aw.r.fault,"HHDM-permissions"));
  }
 }
 /* Even a newly permissible ancestor U/S change after publication must fail
  * the exact retained raw comparison, with backing/claim held on uncertainty. */
 reset();assert(open_window());pages[0][256]^=4;
 assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 /* Delays inside a bounded walk cannot produce a completed census or grant. */
 reset();delay_load_at=2;assert(!open_window() && !allocs && !stores && loads<=5 && !aw.r.claimed);
 assert(!strcmp(aw.r.fault,"clock/deadline"));
 reset();root_change_load_at=1;assert(!open_window() && loads==1 && !allocs && !stores);
 reset();ram_fail_load_at=1;assert(!open_window() && loads==1 && !allocs && !stores);
 /* A census read consumes exactly one immediately preceding clock ticket. */
 reset();aw.io=io;aw.r.token.root=cr3;aw.r.begin=aw.last=1000;aw.hhdm=pmm_hhdm_offset();aw.in_census=true;
 uint64_t sample=0;assert(!aw_census_read(NULL,cr3,0,&sample) && !loads);
 reset();aw.io=io;aw.r.token.root=cr3;aw.r.begin=aw.last=1000;aw.hhdm=pmm_hhdm_offset();aw.in_census=true;
 assert(aw_census_now(NULL));assert(aw_census_read(NULL,cr3,0,&sample) && loads==5);
 assert(!aw_census_read(NULL,cr3,1,&sample) && loads==5);
 /* An elapsed walk must fail the fresh pre-store check before mutation. */
 reset();aw.io=io;aw.r.token.root=cr3;aw.r.begin=aw.last=1000;aw.hhdm=pmm_hhdm_offset();delay_load_at=4;
 assert(!aw_store(0x2001000,7,0,0x2006003,false) && !stores && !pages[1][7]);
 reset();assert(open_window());delay_load_at=loads+2;unsigned unchanged_stores=stores;
 assert(!vmm_air61_window_retire(&token) && stores==unchanged_stores && !frees);poisoned();
 reset();delay_at_publish=true;assert(!open_window() && !frees && aw.r.parent_attempts==1);poisoned();
 for(unsigned n=1;n<=2;n++){reset();alloc_fail_at=n;assert(!open_window() && aw.r.state==AIR61_WINDOW_REFUSED && !aw.r.claimed && frees==n-1 && !pages[1][7]);}
 reset();bad_alloc=true;assert(!open_window() && !frees);poisoned();
 reset();borrowed=true;assert(!open_window() && !frees);poisoned();
 reset();duplicate=true;assert(!open_window() && !frees);poisoned();
 for(int n=0;n<3;n++){reset();gate_fail=n;assert(!open_window());if(n==2)poisoned();else assert(aw.r.state==AIR61_WINDOW_REFUSED && !aw.r.claimed);}
 reset();pages[4][8]=AIR61_WINDOW_PA|0x19;assert(!open_window() && !allocs && aw.r.state==AIR61_WINDOW_REFUSED);
 reset();pages[1][7]=0x2005003;assert(!open_window() && !allocs && aw.r.state==AIR61_WINDOW_REFUSED);
 reset();pages[1][7]=0x2005000;assert(!open_window() && !allocs && aw.r.state==AIR61_WINDOW_REFUSED);
 reset();bad_pointer=true;assert(!open_window() && !allocs);
 reset();load_fail_at=3;assert(!open_window() && !allocs);
 reset();zero_clock=true;assert(!open_window() && !allocs && !aw.r.claimed);
 reset();irq_flags=0x202;assert(!open_window() && !allocs);
 reset();cr3+=4096;assert(!open_window() && !allocs);
 reset();force_cap=true;assert(!open_window() && !allocs && aw.r.state==AIR61_WINDOW_REFUSED);
 reset();census_change=true;assert(!open_window() && !allocs && aw.census.status==5 && aw.r.state==AIR61_WINDOW_REFUSED);
 reset();pages[0][384]|=4;assert(!open_window() && !allocs);
 reset();reenter=true;assert(!open_window() && !frees && !allocs);poisoned();
 for(unsigned after=0;after<2;after++){reset();store_fail_at=1;store_after=after;assert(!open_window() && aw.r.state==AIR61_WINDOW_REFUSED && frees==1 && !aw.r.claimed);}
 for(unsigned n=1025;n<=1026;n++)for(unsigned after=0;after<2;after++){reset();store_fail_at=n;store_after=after;assert(!open_window() && aw.r.state==AIR61_WINDOW_REFUSED && frees==2 && !aw.r.claimed);}
 /* Two zero pages + two private CAS stores precede the only parent CAS. */
 for(unsigned after=0;after<2;after++){reset();store_fail_at=1027;store_after=after;assert(!open_window() && !frees);poisoned();}
 reset();stop_at_publish=true;assert(!open_window() && !frees);poisoned();
 reset();corrupt_parent=true;assert(!open_window() && !frees);poisoned();
 reset();fail_publish_readback=true;assert(!open_window() && !frees);poisoned();
 reset();invalidate_fail_at=1;assert(!open_window() && !frees);poisoned();
 for(unsigned bit=0;bit<4;bit++){reset();assert(open_window());pages[7][80]^=1ull<<(bit==0?5:bit==1?6:bit==2?1:12);assert(!vmm_air61_window_retire(&token) && !frees);poisoned();}
 reset();assert(open_window());pages[1][7]^=32;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());pages[0][0]=1;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());irq_flags|=512;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());cr3+=4096;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());ram_fail=true;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());ticks+=1600000;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());zero_clock=true;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());store_fail_at=stores+1;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());invalidate_fail_at=2;assert(!vmm_air61_window_retire(&token) && !frees && !pages[1][7]);poisoned();
 reset();assert(open_window());aw.r.words=AIR61_WINDOW_WORD_CAP;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());stop_clock=true;stopped_time=aw.last;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());ticks=0;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());tags[6]=PMM_FRAME_RESERVED;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());pages[4][0]^=4096;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());fail_publish_readback=true;assert(!vmm_air61_window_retire(&token) && !frees && !pages[1][7]);poisoned();
 reset();assert(open_window());corrupt_after_clear=true;assert(!vmm_air61_window_retire(&token) && !frees && !pages[1][7]);poisoned();
 reset();assert(open_window());reenter=true;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 reset();assert(open_window());free_fail_at=2;assert(!vmm_air61_window_retire(&token) && frees==1 && aw.owned[0] && !aw.owned[1]);poisoned();
 reset();assert(open_window());free_fail_at=1;assert(!vmm_air61_window_retire(&token) && !frees);poisoned();
 puts("air61-window PASS actual VMM/census, exact RO NX PAT3 leaf, token/refusal, borrowed frames, failure before/after publication, strict A/D, root/source/time, reentry and retained poison; zero target loads");return 0;
}
