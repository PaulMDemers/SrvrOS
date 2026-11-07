/* Dedicated dormant two-pass zero-target-load qualifier. Primitives follow the
 * accepted window backend; the existing profile is not reset or reused. */
#include <srvros/air61_read_window.h>
static struct {
 struct air61_read_series_result result;
 struct air61_read_window_env io;
 unsigned current;
 struct {
  struct air61_read_pass_result view;
  uint64_t root[512],pdpt[512],last,hhdm;
  bool in_census,snapshot,checking,access_scope,census_ticket,owned[2],source_ticket_ready;
  struct air61_window_token source_ticket;
 } pass[AIR61_READ_PASSES];
} rw;
#define rc (rw.pass[rw.current])
#define RW_ADDR (((1ull<<39)-1)&~4095ull)
static bool rw_fault(const char *why){if(!rc.view.window.fault)rc.view.window.fault=why;if(!rw.result.fault)rw.result.fault=why;return false;}
static bool rw_poison(void){rc.view.window.state=AIR61_WINDOW_POISONED;rw.result.state=AIR61_READ_POISONED;return false;}
static bool rw_guard(void){rc.view.window.guard_checks++;return (rc.view.window.state!=AIR61_WINDOW_POISONED && rc.view.window.token.owner==rw.result.owner && rc.view.window.token.epoch==rw.result.epoch+rw.current && rc.view.window.token.root==rw.result.root && vmm_diagnostic_sole_kernel_root(rc.view.window.token.root)) || rw_fault("root/exclusion");}
static bool rw_tick(void){
 if(!rw_guard())return false;
 rc.view.window.clock_samples++;uint64_t t=rw.io.now(rw.io.ctx);
 if(rc.view.window.state==AIR61_WINDOW_POISONED)return false;
 if(!t || t<rc.last || t<rc.view.window.begin || t-rc.view.window.begin>=AIR61_WINDOW_LIMIT_US)return rw_fault("clock/deadline");
 rc.view.window.end=rc.last=t;return true;
}
static bool rw_check(unsigned phase){
 rc.checking=true;bool ok=rw.io.check(rw.io.ctx,rw.current,phase);rc.checking=false;
 return (ok && rc.view.window.state!=AIR61_WINDOW_POISONED && rw_guard()) || rw_fault("fresh-gate");
}
/* Bootstrap HHDM reads are RAM-only, like the checked ECAM accessor. The full
 * translation path is then verified before every source word/store. Only
 * redundant internal clock samples are coalesced by bounded access scopes. Census
 * entry counts keep their original meaning; mapper path words have a separate
 * fixed budget and all phases share this operation's100ms lifetime. */
static bool rw_ram_address(uint64_t pa){return pa && pa<=RW_ADDR && !(pa&4095) && pa!=AIR61_WINDOW_PA && pa!=AIR61_WINDOW_PA+4096;}
static bool rw_ram(uint64_t pa){rc.view.window.ram_checks++;return rw.io.ram(rw.io.ctx,rw.current,pa);}
static bool rw_raw(uint64_t pa,unsigned i,uint64_t *out){
 if(!rc.access_scope || !rw_ram_address(pa) || i>=512 || !rw_guard())return rw_fault("RAM-address");
 if(!rw_ram(pa) || !rw_guard())return rw_fault("RAM-source");
 if(!rc.in_census){if(rc.view.window.words>=AIR61_WINDOW_WORD_CAP)return rw_fault("word-cap");rc.view.window.words++;}
rc.view.window.raw_loads++;
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 return aw_test_load(pa,i,out) || rw_fault("RAM-load");
#else
 volatile uint64_t *p=pmm_phys_to_virt(pa);
 if(!p || !is_canonical((uint64_t)(uintptr_t)p))return rw_fault("RAM-pointer");
 *out=p[i];return true;
#endif
}
static bool rw_address(uint64_t pa,unsigned i,volatile uint64_t **out){
 if(!rw_ram_address(pa) || i>=512 || !rw_ram(pa) || !rw_guard())return rw_fault("RAM-address");
 uint64_t address=pa+i*8;
 if(!rc.hhdm || rc.hhdm>UINT64_MAX-address)return rw_fault("HHDM-overflow");
 uint64_t va=(uint64_t)(uintptr_t)pmm_phys_to_virt(address);
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 va=aw_test_virtual(address);
#endif
 if(va!=rc.hhdm+address || !is_canonical(va))return rw_fault("HHDM-pointer");
 uint64_t table=rc.view.window.token.root;bool writable=true,user=true;
 for(unsigned level=0;level<4;level++){
  unsigned shift=39-9*level;uint64_t word;
  if(!rw_raw(table,(va>>shift)&511,&word))return false;
  if(!(word&1) || (word&~(RW_ADDR|0x8000000000000fffull)) || (!level && (word&128)))return rw_fault("HHDM-entry");
  /* x86 combines R/W and U/S through every level. Firmware can leave U/S
   * set in ancestors of supervisor-only RAM leaves. Do not approve a pointer
   * until the full translation is checked; stores additionally need all R/W
   * bits. Keep a supervisor leaf even when an ancestor already excludes user. */
  writable=writable && (word&2);user=user && (word&4);
  if(level==3 || ((level==1 || level==2) && (word&128))){
   if(!writable || user || (word&4))return rw_fault("HHDM-permissions");
   unsigned bits=level==1?30:level==2?21:12;uint64_t low=(1ull<<bits)-1;
   if(level!=3 && (word&RW_ADDR&low&~4096ull))return rw_fault("HHDM-alignment");
   if(((word&RW_ADDR&~low)|(va&low))!=address)return rw_fault("HHDM-translation");
   *out=(volatile uint64_t *)(uintptr_t)va;return true;
  }
  table=word&RW_ADDR;
 }
 return rw_fault("HHDM-level");
}
/* A scope contains at most four checked HHDM loads plus one data load/store.
 * Census HAC room() supplies a fresh clock immediately before every read and
 * checks again before any following read or final acceptance. Consume that
 * ticket once. Ordinary reads check entry/completion; stores also check just
 * before mutation. No translation, provenance or raw value is cached. */
static bool rw_word(uint64_t pa,unsigned i,uint64_t *v){
 if(rc.access_scope){rw_fault("access-reentry");return rw_poison();}
 if(rc.in_census){if(!rc.census_ticket)return rw_fault("census-clock-ticket");rc.census_ticket=false;}
 else if(!rw_tick())return false;
 rc.view.window.logical_reads++;
 rc.access_scope=true;volatile uint64_t *p;
 bool ok=rw_address(pa,i,&p) && rw_raw(pa,i,v);rc.access_scope=false;
 if(!ok)return false;
 bool done=rc.in_census?rw_guard():rw_tick();
 if(done && rc.in_census)rc.view.window.census_reads++;
 return done;
}
bool vmm_air61_read_window_copy_ram(unsigned pass,uint64_t pa,void *output,unsigned length){
 if(!rw.result.busy || pass!=rw.current)return false;
 if(!output || !length || length>136 || pa>=(1ull<<39) || length>(1ull<<39)-pa || !rc.checking || !rw_guard())return false;
 uint8_t *out=output;
 for(unsigned at=0;at<length;){uint64_t address=pa+at,word;if(!rw_word(address&~4095ull,(address&4095)/8,&word))return false;
  for(unsigned off=address&7;off<8 && at<length;off++)out[at++]=(uint8_t)(word>>(off*8));
 }return true;
}
static bool rw_store(uint64_t pa,unsigned i,uint64_t before,uint64_t after,bool private){
 if(rc.access_scope || rc.in_census || !rw_tick())return rw_fault("store-scope");
 rc.view.window.logical_stores++;rc.access_scope=true;volatile uint64_t *p;uint64_t v;
 bool address_ok=rw_address(pa,i,&p) && (private || (rw_raw(pa,i,&v) && v==before));rc.access_scope=false;
 if(!address_ok || !rw_tick())return rw_fault("store-expected");
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 bool ok=aw_test_store(pa,i,before,after,private);
#else
 bool ok=true;
 if(private)*p=after;
 else ok=__atomic_compare_exchange_n(p,&before,after,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
#endif
 if(private || pa==rc.view.window.frames[0] || pa==rc.view.window.frames[1])rc.view.window.private_writes++;
 return (ok && rw_tick()) || rw_fault("store-failed");
}
static bool rw_invalidate(void){
 if(!rw_tick())return false;
 rc.view.window.invalidations++;
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 bool ok=aw_test_invalidate(AIR61_WINDOW_VA);
#else
 invlpg(AIR61_WINDOW_VA);bool ok=true;
#endif
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 return ok || rw_fault("invalidation");
}
static bool rw_parent_value(uint64_t expected){uint64_t v;return (rw_word(rc.view.window.pdpt,7,&v) && v==expected) || rw_fault("parent-readback");}
static bool rw_census_read(void *ctx,uint64_t pa,unsigned i,uint64_t *v){(void)ctx;return rw_word(pa,i,v);}
static uint64_t rw_census_now(void *ctx){(void)ctx;rc.census_ticket=false;if(!rw_tick())return 0;rc.census_ticket=true;return rc.last;}
static bool rw_known_table(uint64_t pa){for(unsigned i=0;i<rc.view.census.pages;i++)if(rc.view.census.table[i].pa==pa)return true;return false;}
static bool rw_owned(unsigned i){return rc.owned[i] && rc.view.window.frames[i] && rc.view.window.frames[i]!=rc.view.window.token.root && rc.view.window.frames[i]!=rc.view.window.pdpt && !rw_known_table(rc.view.window.frames[i]) && pmm_frame_has_tag(rc.view.window.frames[i],PMM_FRAME_PAGE_TABLE);}
static uint64_t rw_private_word(unsigned f,unsigned i){return f==0?(i==409?rc.view.window.frames[1]|AIR61_READ_PARENT_FLAGS:0):(i==80?AIR61_READ_LEAF:0);}
static bool rw_verify(bool linked,bool private,bool coalesced){
 if(!rc.snapshot)return false;
 if(coalesced){
  struct air61_window_token *a=&rc.source_ticket,*b=&rc.view.window.token;
  if(!rc.source_ticket_ready || a->owner!=b->owner || a->epoch!=b->epoch || a->root!=b->root)return rw_fault("source-ticket");
  rc.source_ticket_ready=false;rc.view.source_coalescings++;
  if(!rw_guard())return false;
 }else if(!rw_check(2))return false;
 uint64_t verify_begin=rc.last;
 for(unsigned i=0;i<512;i++){
  uint64_t a,b;
  if(!rw_word(rc.view.window.token.root,i,&a) || !rw_word(rc.view.window.pdpt,i,&b) || a!=rc.root[i] || b!=(i==7?(linked?rc.view.window.frames[0]|AIR61_READ_PARENT_FLAGS:0):rc.pdpt[i]))return rw_fault("borrowed-change");
  if(linked){rc.view.evidence.root_linked[i]=a;rc.view.evidence.pdpt_linked[i]=b;}
  else if(rc.view.window.unlink_attempts){rc.view.evidence.root_retired[i]=a;rc.view.evidence.pdpt_retired[i]=b;}
 }
 if(private)for(unsigned f=0;f<2;f++){
  if(!rw_owned(f))return rw_fault("frame-owner");
  for(unsigned i=0;i<512;i++){uint64_t v;if(!rw_word(rc.view.window.frames[f],i,&v) || v!=rw_private_word(f,i))return rw_fault("private-change");if(f)rc.view.evidence.pt[i]=v;else rc.view.evidence.pd[i]=v;}
 }
 if(!rw_tick())return false;
 if(rc.last<=verify_begin)return rw_fault("clock-stalled");
 if(linked)rc.view.evidence.linked=true;else if(rc.view.window.unlink_attempts)rc.view.evidence.retired=true;
 return true;
}
static bool rw_free(unsigned i){
 if(!rc.owned[i])return true;
 if(!rw_tick() || !rw_owned(i))return rw_fault("free-owner");
#ifdef SRVROS_AIR61_WINDOW_HOST_TEST
 if(!aw_test_free(rc.view.window.frames[i]))return rw_fault("free-failed");
#else
 pmm_free_frame(rc.view.window.frames[i]);
#endif
 rc.view.window.frees++;
 if(!pmm_frame_has_tag(rc.view.window.frames[i],PMM_FRAME_FREE))return rw_fault("free-tag");
 rc.owned[i]=false;return true;
}
static bool rw_refuse(void){
 if(rc.view.window.state==AIR61_WINDOW_POISONED || rc.view.window.parent_attempts)return rw_poison();
 if((rc.owned[0] || rc.owned[1]) && (!rw_verify(false,false,false) || !rw_free(1) || !rw_free(0)))return rw_poison();
 rc.view.window.claimed=false;rc.view.window.state=AIR61_WINDOW_REFUSED;return false;
}
static bool rw_reentry(void){rw_fault("reentry");rc.view.window.claimed=true;return rw_poison();}
static const uint32_t rw_offsets[AIR61_READ_SLOTS]={0x1c,0,0x0c,8,0x0c,0x14,0x10,0x14,0x24,0x20,0x24,0x1c};
static bool rw_wait_gap(void){
 if(!rw.current)return true;
 uint64_t previous=rw.pass[0].view.slot_end;
 for(;;){
  if(!rw_tick() || rc.last<previous)return rw_fault("gap-clock");
  uint64_t gap=rc.last-previous;
  if(gap>AIR61_READ_GAP_MAX_US)return rw_fault("gap-maximum");
  if(gap>=AIR61_READ_GAP_MIN_US)return true;
  if(rw.result.gap_spins==AIR61_READ_GAP_SPINS)return rw_fault("gap-spin-cap");
  rw.result.gap_spins++;
#ifndef SRVROS_AIR61_WINDOW_HOST_TEST
  __asm__ volatile("pause");
#endif
 }
}
bool vmm_air61_read_window_prime_room(void){
 if(!rw.result.busy || rw.current || !rc.checking || rc.snapshot || rc.in_census || rc.view.window.state!=AIR61_WINDOW_CLAIMED)return false;
 return rw_tick();
}
static bool rw_touch_next(void){
 /* First-touch next-pass private output inside pass0's existing lifetime.
  * Volatile same-value writes preserve every byte, including attempt state.
  * No page-table flags are masked or written, and pass1 still performs all
  * of its own bounded capture/preparation. */
 volatile uint8_t *p=(volatile uint8_t *)&rw.pass[1];
 for(unsigned i=0;i<sizeof(rw.pass[1]);i+=4096){if(!rw_tick())return false;p[i]=p[i];}
 if(!rw_tick())return false;
 p[sizeof(rw.pass[1])-1]=p[sizeof(rw.pass[1])-1];
 return rw_tick();
}
static bool rw_prepare(void){
 struct air61_window_result *r=&rc.view.window;
 r->state=AIR61_WINDOW_CLAIMED;r->claimed=true;
 r->token=(struct air61_window_token){rw.result.owner,rw.result.epoch+rw.current,rw.result.root};
 rc.hhdm=pmm_hhdm_offset();r->hhdm=rc.hhdm;r->clock_samples++;
 r->begin=rc.last=rw.io.now(rw.io.ctx);
 if(!rw.current)rw.result.begin=r->begin;
 if(!r->begin || r->token.epoch>r->begin){rw_fault("clock/epoch");goto refused;}
 if(!rw_tick() || !rw_check(0) || !rw_tick() || (!rw.current && !rw_touch_next()))goto refused;
 r->gate_end=rc.last;r->census_begin=rc.last;
 const struct hac_io census_io={.read=rw_census_read,.now=rw_census_now};
 rc.in_census=true;bool captured=hac_capture(&rc.view.census,&census_io,r->token.root);rc.in_census=false;rc.census_ticket=false;r->census_end=rc.last;
 if(!captured || rc.view.census.target_leaves || !rw_tick()){rw_fault("alias-census");goto refused;}
 uint64_t root_entry;
 if(!rw_word(r->token.root,384,&root_entry) || (root_entry&3)!=3 || !(root_entry&32) || (root_entry&~(RW_ADDR|0x800000000000003bull))){rw_fault("borrowed-path-A");goto refused;}
 r->pdpt=root_entry&RW_ADDR;
 for(unsigned i=0;i<512;i++)if(!rw_word(r->token.root,i,&rc.root[i]) || !rw_word(r->pdpt,i,&rc.pdpt[i]))goto refused;
 for(unsigned i=0;i<512;i++){rc.view.evidence.root_before[i]=rc.root[i];rc.view.evidence.pdpt_before[i]=rc.pdpt[i];}
 rc.snapshot=true;
 if(rc.pdpt[7]){rw_fault("existing-slot");goto refused;}
 for(unsigned f=0;f<2;f++){
  if(!rw_tick())goto refused;
  uint64_t pa=pmm_alloc_frames_above_tagged(1,VMM_PAGE_TABLE_MIN_PHYSICAL,PMM_FRAME_PAGE_TABLE);r->frames[f]=pa;
  if(!pa){rw_fault("allocation");goto refused;}
  if(!rw_ram_address(pa) || pa<VMM_PAGE_TABLE_MIN_PHYSICAL || rw_known_table(pa) || (f && pa==r->frames[0]) || !pmm_frame_has_tag(pa,PMM_FRAME_PAGE_TABLE) || !rw_ram(pa)){rw_fault("allocation-owner");rw_poison();goto refused;}
  rc.owned[f]=true;
  for(unsigned i=0;i<512;i++)if(!rw_store(pa,i,0,0,true))goto refused;
 }
 if(!rw_store(r->frames[1],80,0,AIR61_READ_LEAF,false) || !rw_store(r->frames[0],409,0,r->frames[1]|AIR61_READ_PARENT_FLAGS,false))goto refused;
 if(!rw_verify(false,true,false) || !rw_wait_gap() || !rw_check(1) || !rw_tick())goto refused;
 /* Only this immediate verification can consume phase1's full fresh source
  * result. No yield, store, callback or gap lies between issue and consume. */
 rc.source_ticket=r->token;rc.source_ticket_ready=true;
 if(!rw_verify(false,true,true))goto refused;
 uint64_t publish_begin=rc.last;r->parent_attempts++;
 if(!rw_store(r->pdpt,7,0,r->frames[0]|AIR61_READ_PARENT_FLAGS,false) || !rw_parent_value(r->frames[0]|AIR61_READ_PARENT_FLAGS) || !rw_invalidate() || !rw_verify(true,true,false))goto refused;
 if(rc.last<=publish_begin){rw_fault("clock-stalled");goto refused;}
 r->state=AIR61_WINDOW_ACTIVE;return true;
 refused:rw_refuse();return false;
}
static bool rw_path(uint64_t out[4]){
 const struct air61_window_result *r=&rc.view.window;
 if(!rw_owned(0) || !rw_owned(1))return rw_fault("path-owner");
 const uint64_t pages[]={r->token.root,r->pdpt,r->frames[0],r->frames[1]};
 const unsigned indices[]={384,7,409,80};
 const uint64_t expected[]={rc.root[384],r->frames[0]|AIR61_READ_PARENT_FLAGS,r->frames[1]|AIR61_READ_PARENT_FLAGS,AIR61_READ_LEAF};
 for(unsigned i=0;i<4;i++)if(!rw_word(pages[i],indices[i],out+i) || out[i]!=expected[i])return rw_fault("path-change");
 return true;
}
static bool rw_slots(void){
 for(unsigned i=0;i<AIR61_READ_SLOTS;i++){
  if(!rw_check(2) || !rw_path(rc.view.before[i]) || !rw_tick())return rw_poison();
  if(!i){
   rc.view.slot_begin=rc.last;
   if(rw.current){
    uint64_t previous=rw.pass[0].view.slot_end;
    if(rc.last<previous || rc.last-previous<AIR61_READ_GAP_MIN_US || rc.last-previous>AIR61_READ_GAP_MAX_US){rw_fault("slot-gap");return rw_poison();}
    rw.result.gap_us=rc.last-previous;
   }
  }
  /* Intentionally no target dereference. These are qualification markers,
   * not fabricated device samples or a callback hook for native reads. */
  rc.view.offsets[i]=rw_offsets[i];
  if(!rw_tick())return rw_poison();
  rc.view.slot_end=rc.last;
  if(!rw_path(rc.view.after[i]))return rw_poison();
  rc.view.slots++;
 }
 return true;
}
static bool rw_retire(void){
 struct air61_window_result *r=&rc.view.window;uint64_t begin=rc.last;
 if(!rw_verify(true,true,false))return rw_poison();
 if(rc.last<=begin){rw_fault("clock-stalled");return rw_poison();}
 uint64_t clear_begin=rc.last;r->unlink_attempts++;
 if(!rw_store(r->pdpt,7,r->frames[0]|AIR61_READ_PARENT_FLAGS,0,false) || !rw_parent_value(0) || !rw_invalidate() || !rw_verify(false,true,false))return rw_poison();
 if(rc.last<=clear_begin){rw_fault("clock-stalled");return rw_poison();}
 if(!rw_free(1) || !rw_free(0) || !rw_tick())return rw_poison();
 r->claimed=false;r->state=AIR61_WINDOW_RETIRED;return true;
}
const struct air61_read_series_result *vmm_air61_read_window_result(void){return &rw.result;}
const struct air61_read_pass_result *vmm_air61_read_window_pass(unsigned pass){return pass<AIR61_READ_PASSES?&rw.pass[pass].view:NULL;}
bool vmm_air61_read_window_qualify(const struct air61_read_window_env *io,uint64_t owner,uint64_t epoch){
 if(rw.result.busy)return rw_reentry();
 if(rw.result.attempted || !io || !io->check || !io->ram || !io->now || !owner || !epoch || epoch==UINT64_MAX)return false;
#ifdef SRVROS_AIR61_WINDOW
 if(vmm_air61_window_result()->state!=AIR61_WINDOW_NEW)return false;
#endif
 rw.result.attempted=rw.result.busy=true;rw.result.state=AIR61_READ_RUNNING;
 rw.result.owner=owner;rw.result.epoch=epoch;rw.result.root=kernel_cr3;rw.io=*io;
 for(rw.current=0;rw.current<AIR61_READ_PASSES;rw.current++){
  rw.result.started++;
  if(!rw_prepare())goto failed;
  if(!rw.current)rw.result.begin=rc.view.window.begin;
  if(!rw_slots() || !rw_retire())goto failed;
  rw.result.retired++;rw.result.end=rc.view.window.end;
 }
 rw.current=AIR61_READ_PASSES-1;
 rw.result.state=AIR61_READ_COMPLETE;rw.result.busy=false;return true;
 failed:
 if(rw.result.state!=AIR61_READ_POISONED)rw.result.state=AIR61_READ_REFUSED;
 rw.result.end=rc.view.window.end;rw.result.busy=false;return false;
}
#undef RW_ADDR
#undef rc
