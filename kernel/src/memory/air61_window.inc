/* Included only in the dedicated opt-in diagnostic VMM. */
#include <srvros/air61_window.h>
static struct {
 struct air61_window_result r;
 struct air61_window_env io;
 struct hac_result census;
 uint64_t root[512],pdpt[512],last,hhdm;
 bool busy,in_census,snapshot,checking,owned[2];
 struct air61_window_evidence evidence;
} aw;
#define AW_ADDR (((1ull<<39)-1)&~4095ull)
static bool aw_fault(const char *why){if(!aw.r.fault)aw.r.fault=why;return false;}
static bool aw_poison(void){aw.r.state=AIR61_WINDOW_POISONED;return false;}
static bool aw_guard(void){return (aw.r.state!=AIR61_WINDOW_POISONED && vmm_diagnostic_sole_kernel_root(aw.r.token.root)) || aw_fault("root/exclusion");}
static bool aw_tick(void){
 if(!aw_guard())return false;
 uint64_t t=aw.io.now(aw.io.ctx);
 if(aw.r.state==AIR61_WINDOW_POISONED)return false;
 if(!t || t<aw.last || t<aw.r.begin || t-aw.r.begin>=AIR61_WINDOW_LIMIT_US)return aw_fault("clock/deadline");
 aw.r.end=aw.last=t;return true;
}
static bool aw_check(unsigned phase){
 aw.checking=true;bool ok=aw.io.check(aw.io.ctx,phase);aw.checking=false;
 return (ok && aw.r.state!=AIR61_WINDOW_POISONED && aw_guard()) || aw_fault("fresh-gate");
}
/* Bootstrap HHDM reads are RAM-only, like the checked ECAM accessor. The full
 * translation path is then verified before every source word/store. Census
 * entry counts keep their original meaning; mapper path words have a separate
 * fixed budget and all phases share this operation's100ms lifetime. */
static bool aw_ram_address(uint64_t pa){return pa && pa<=AW_ADDR && !(pa&4095) && pa!=AIR61_WINDOW_PA && pa!=AIR61_WINDOW_PA+4096;}
static bool aw_raw(uint64_t pa,unsigned i,uint64_t *out){
 if(!aw_ram_address(pa) || i>=512 || !aw_tick())return aw_fault("RAM-address");
 if(!aw.io.ram(aw.io.ctx,pa) || !aw_guard())return aw_fault("RAM-source");
 if(!aw.in_census){if(aw.r.words>=AIR61_WINDOW_WORD_CAP)return aw_fault("word-cap");aw.r.words++;}
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 return aw_test_load(pa,i,out) || aw_fault("RAM-load");
#else
 volatile uint64_t *p=pmm_phys_to_virt(pa);
 if(!p || !is_canonical((uint64_t)(uintptr_t)p))return aw_fault("RAM-pointer");
 *out=p[i];return true;
#endif
}
static bool aw_address(uint64_t pa,unsigned i,volatile uint64_t **out){
 if(!aw_ram_address(pa) || i>=512 || !aw.io.ram(aw.io.ctx,pa) || !aw_guard())return aw_fault("RAM-address");
 uint64_t address=pa+i*8;
 if(!aw.hhdm || aw.hhdm>UINT64_MAX-address)return aw_fault("HHDM-overflow");
 uint64_t va=(uint64_t)(uintptr_t)pmm_phys_to_virt(address);
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 va=aw_test_virtual(address);
#endif
 if(va!=aw.hhdm+address || !is_canonical(va))return aw_fault("HHDM-pointer");
 uint64_t table=aw.r.token.root;bool writable=true,user=true;
 for(unsigned level=0;level<4;level++){
  unsigned shift=39-9*level;uint64_t word;
  if(!aw_raw(table,(va>>shift)&511,&word))return false;
  if(!(word&1) || (word&~(AW_ADDR|0x8000000000000fffull)) || (!level && (word&128)))return aw_fault("HHDM-entry");
  /* x86 combines R/W and U/S through every level. Firmware can leave U/S
   * set in ancestors of supervisor-only RAM leaves. Do not approve a pointer
   * until the full translation is checked; stores additionally need all R/W
   * bits. Keep a supervisor leaf even when an ancestor already excludes user. */
  writable=writable && (word&2);user=user && (word&4);
  if(level==3 || ((level==1 || level==2) && (word&128))){
   if(!writable || user || (word&4))return aw_fault("HHDM-permissions");
   unsigned bits=level==1?30:level==2?21:12;uint64_t low=(1ull<<bits)-1;
   if(level!=3 && (word&AW_ADDR&low&~4096ull))return aw_fault("HHDM-alignment");
   if(((word&AW_ADDR&~low)|(va&low))!=address)return aw_fault("HHDM-translation");
   *out=(volatile uint64_t *)(uintptr_t)va;return true;
  }
  table=word&AW_ADDR;
 }
 return aw_fault("HHDM-level");
}
static bool aw_word(uint64_t pa,unsigned i,uint64_t *v){volatile uint64_t *p;return aw_address(pa,i,&p) && aw_raw(pa,i,v);}
bool vmm_air61_window_copy_ram(uint64_t pa,void *output,unsigned length){
 if(!output || !length || length>136 || pa>=(1ull<<39) || length>(1ull<<39)-pa || !aw.checking || !aw_guard())return false;
 uint8_t *out=output;
 for(unsigned at=0;at<length;){uint64_t address=pa+at,word;if(!aw_word(address&~4095ull,(address&4095)/8,&word))return false;
  for(unsigned off=address&7;off<8 && at<length;off++)out[at++]=(uint8_t)(word>>(off*8));
 }return true;
}
static bool aw_store(uint64_t pa,unsigned i,uint64_t before,uint64_t after,bool private){
 volatile uint64_t *p;uint64_t v;
 if(!aw_address(pa,i,&p) || (!private && (!aw_raw(pa,i,&v) || v!=before)))return aw_fault("store-expected");
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 bool ok=aw_test_store(pa,i,before,after,private);
#else
 bool ok=true;
 if(private)*p=after;
 else ok=__atomic_compare_exchange_n(p,&before,after,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
#endif
 if(private || pa==aw.r.frames[0] || pa==aw.r.frames[1])aw.r.private_writes++;
 return ok || aw_fault("store-failed");
}
static bool aw_invalidate(void){
 if(!aw_tick())return false;
 aw.r.invalidations++;
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 bool ok=aw_test_invalidate(AIR61_WINDOW_VA);
#else
 invlpg(AIR61_WINDOW_VA);bool ok=true;
#endif
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 return ok || aw_fault("invalidation");
}
static bool aw_parent_value(uint64_t expected){uint64_t v;return (aw_word(aw.r.pdpt,7,&v) && v==expected) || aw_fault("parent-readback");}
static bool aw_census_read(void *ctx,uint64_t pa,unsigned i,uint64_t *v){(void)ctx;return aw_word(pa,i,v);}
static uint64_t aw_census_now(void *ctx){(void)ctx;return aw_tick()?aw.last:0;}
static bool aw_known_table(uint64_t pa){for(unsigned i=0;i<aw.census.pages;i++)if(aw.census.table[i].pa==pa)return true;return false;}
static bool aw_owned(unsigned i){return aw.owned[i] && aw.r.frames[i] && aw.r.frames[i]!=aw.r.token.root && aw.r.frames[i]!=aw.r.pdpt && !aw_known_table(aw.r.frames[i]) && pmm_frame_has_tag(aw.r.frames[i],PMM_FRAME_PAGE_TABLE);}
static uint64_t aw_private_word(unsigned f,unsigned i){return f==0?(i==409?aw.r.frames[1]|3:0):(i==80?AIR61_WINDOW_LEAF:0);}
static bool aw_verify(bool linked,bool private){
 if(!aw_check(2) || !aw.snapshot)return false;
 uint64_t verify_begin=aw.last;
 for(unsigned i=0;i<512;i++){
  uint64_t a,b;
  if(!aw_word(aw.r.token.root,i,&a) || !aw_word(aw.r.pdpt,i,&b) || a!=aw.root[i] || b!=(i==7?(linked?aw.r.frames[0]|3:0):aw.pdpt[i]))return aw_fault("borrowed-change");
  if(linked){aw.evidence.root_linked[i]=a;aw.evidence.pdpt_linked[i]=b;}
  else if(aw.r.unlink_attempts){aw.evidence.root_retired[i]=a;aw.evidence.pdpt_retired[i]=b;}
 }
 if(private)for(unsigned f=0;f<2;f++){
  if(!aw_owned(f))return aw_fault("frame-owner");
  for(unsigned i=0;i<512;i++){uint64_t v;if(!aw_word(aw.r.frames[f],i,&v) || v!=aw_private_word(f,i))return aw_fault("private-change");if(f)aw.evidence.pt[i]=v;else aw.evidence.pd[i]=v;}
 }
 if(!aw_tick())return false;
 if(aw.last<=verify_begin)return aw_fault("clock-stalled");
 if(linked)aw.evidence.linked=true;else if(aw.r.unlink_attempts)aw.evidence.retired=true;
 return true;
}
static bool aw_free(unsigned i){
 if(!aw.owned[i])return true;
 if(!aw_tick() || !aw_owned(i))return aw_fault("free-owner");
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 if(!aw_test_free(aw.r.frames[i]))return aw_fault("free-failed");
#else
 pmm_free_frame(aw.r.frames[i]);
#endif
 aw.r.frees++;
 if(!pmm_frame_has_tag(aw.r.frames[i],PMM_FRAME_FREE))return aw_fault("free-tag");
 aw.owned[i]=false;return true;
}
static bool aw_refuse(void){
 if(aw.r.state==AIR61_WINDOW_POISONED || aw.r.parent_attempts)return aw_poison();
 if((aw.owned[0] || aw.owned[1]) && (!aw_verify(false,false) || !aw_free(1) || !aw_free(0)))return aw_poison();
 aw.r.claimed=false;aw.r.state=AIR61_WINDOW_REFUSED;return false;
}
static bool aw_reentry(void){aw_fault("reentry");aw.r.claimed=true;return aw_poison();}
const struct air61_window_result *vmm_air61_window_result(void){return &aw.r;}
const struct hac_result *vmm_air61_window_census(void){return &aw.census;}
const struct air61_window_evidence *vmm_air61_window_evidence(void){return &aw.evidence;}
bool vmm_air61_window_prepare(const struct air61_window_env *io,uint64_t owner,uint64_t epoch,struct air61_window_token *out){
 if(aw.busy)return aw_reentry();
 if(aw.r.state!=AIR61_WINDOW_NEW || !io || !io->check || !io->ram || !io->now || !owner || !epoch || !out)return false;
 aw.busy=true;aw.r.state=AIR61_WINDOW_CLAIMED;aw.r.claimed=true;aw.io=*io;
 aw.r.token=(struct air61_window_token){owner,epoch,kernel_cr3};aw.hhdm=pmm_hhdm_offset();aw.r.hhdm=aw.hhdm;
 aw.r.begin=aw.last=io->now(io->ctx);
 if(!aw.r.begin){aw_fault("clock/deadline");goto refused;}
 if(!aw_tick() || !aw_check(0))goto refused;
 const struct hac_io census_io={.read=aw_census_read,.now=aw_census_now};
 aw.in_census=true;bool captured=hac_capture(&aw.census,&census_io,aw.r.token.root);aw.in_census=false;
 if(!captured || aw.census.target_leaves || !aw_tick()) {aw_fault("alias-census");goto refused;}
 uint64_t root_entry;
 if(!aw_word(aw.r.token.root,384,&root_entry) || (root_entry&3)!=3 || (root_entry&~(AW_ADDR|0x800000000000003bull))){aw_fault("borrowed-path");goto refused;}
 aw.r.pdpt=root_entry&AW_ADDR;
 for(unsigned i=0;i<512;i++)if(!aw_word(aw.r.token.root,i,&aw.root[i]) || !aw_word(aw.r.pdpt,i,&aw.pdpt[i]))goto refused;
 for(unsigned i=0;i<512;i++){aw.evidence.root_before[i]=aw.root[i];aw.evidence.pdpt_before[i]=aw.pdpt[i];}
 aw.snapshot=true;
 if(aw.pdpt[7]){aw_fault("existing-slot");goto refused;}
 for(unsigned f=0;f<2;f++){
  if(!aw_tick())goto refused;
  uint64_t pa=pmm_alloc_frames_above_tagged(1,VMM_PAGE_TABLE_MIN_PHYSICAL,PMM_FRAME_PAGE_TABLE);aw.r.frames[f]=pa;
  if(!pa){aw_fault("allocation");goto refused;}
  if(!aw_ram_address(pa) || pa<VMM_PAGE_TABLE_MIN_PHYSICAL || aw_known_table(pa) || (f && pa==aw.r.frames[0]) || !pmm_frame_has_tag(pa,PMM_FRAME_PAGE_TABLE) || !aw.io.ram(aw.io.ctx,pa)){aw_fault("allocation-owner");aw_poison();goto refused;}
  aw.owned[f]=true;
  for(unsigned i=0;i<512;i++)if(!aw_store(pa,i,0,0,true))goto refused;
 }
 if(!aw_store(aw.r.frames[1],80,0,AIR61_WINDOW_LEAF,false) || !aw_store(aw.r.frames[0],409,0,aw.r.frames[1]|3,false))goto refused;
 if(!aw_verify(false,true) || !aw_check(1) || !aw_verify(false,true))goto refused;
 uint64_t publish_begin=aw.last;
 aw.r.parent_attempts++;
 if(!aw_store(aw.r.pdpt,7,0,aw.r.frames[0]|3,false) || !aw_parent_value(aw.r.frames[0]|3) || !aw_invalidate() || !aw_verify(true,true))goto refused;
 if(aw.last<=publish_begin){aw_fault("clock-stalled");goto refused;}
 aw.r.state=AIR61_WINDOW_ACTIVE;*out=aw.r.token;aw.busy=false;return true;
 refused:aw_refuse();aw.busy=false;return false;
}
bool vmm_air61_window_retire(const struct air61_window_token *token){
 if(aw.busy)return aw_reentry();
 if(aw.r.state!=AIR61_WINDOW_ACTIVE || !token || token->owner!=aw.r.token.owner || token->epoch!=aw.r.token.epoch || token->root!=aw.r.token.root)return false;
 aw.busy=true;uint64_t retire_begin=aw.last;
 if(!aw_verify(true,true))goto poisoned;
 if(aw.last<=retire_begin){aw_fault("clock-stalled");goto poisoned;}
 uint64_t clear_begin=aw.last;
 aw.r.unlink_attempts++;
 if(!aw_store(aw.r.pdpt,7,aw.r.frames[0]|3,0,false) || !aw_parent_value(0) || !aw_invalidate() || !aw_verify(false,true))goto poisoned;
 if(aw.last<=clear_begin){aw_fault("clock-stalled");goto poisoned;}
 if(!aw_free(1) || !aw_free(0) || !aw_tick())goto poisoned;
 aw.r.claimed=false;aw.r.state=AIR61_WINDOW_RETIRED;aw.busy=false;return true;
 poisoned:aw_poison();aw.busy=false;return false;
}
#undef AW_ADDR
