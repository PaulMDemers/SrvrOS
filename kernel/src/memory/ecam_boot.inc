/* Private AP-ELF boot mapper. Ordinary VMM callers/semantics are unchanged. */
#include <srvros/ecam_boot.h>
static struct {
 struct ecam_boot_result r;
 struct ecam_boot_env io;
 uint64_t root_words[512],pdpt_words[512],last;
 struct kernel_mapping metadata[VMM_MAX_KERNEL_MAPPINGS];
 uint64_t metadata_count;
 bool callback_active;
} eb;
static struct ecam_boot_evidence eb_evidence;
static struct ecam_boot_diagnostic eb_diagnostic;
static const char *eb_stage="none";
static bool eb_fault(const char *reason,unsigned details,uint64_t table,unsigned index,
                     unsigned component,uint64_t expected,uint64_t observed) {
 if(!eb_diagnostic.reason) {
  eb_diagnostic.stage=eb_stage;eb_diagnostic.reason=reason;eb_diagnostic.sample_us=eb.last;
  eb_diagnostic.table=table;eb_diagnostic.index=index;eb_diagnostic.component=component;
  eb_diagnostic.expected=expected;eb_diagnostic.observed=observed;
  eb_diagnostic.details=details;eb_diagnostic.words=eb.r.words;
  eb_diagnostic.sample_clock_valid=eb.r.begin && eb.last>=eb.r.begin;
 }
 return false;
}
const struct ecam_boot_diagnostic *vmm_ecam_boot_diagnostic(void){return &eb_diagnostic;}
static bool eb_poison(void) {eb.r.state=ECAM_POISONED;return false;}
static bool eb_guard(void) {return vmm_diagnostic_sole_kernel_root(eb.r.root) || eb_fault("sole-root",0,0,0,0,0,0);}
static bool eb_check(unsigned phase) {
 if(eb.callback_active)return eb_fault("recursive-gate",0,0,0,0,0,0);
 eb.callback_active=true;bool ok=eb.io.check(eb.io.ctx,phase);eb.callback_active=false;return ok || eb_fault("fresh-gate",1,0,0,0,1,0);
}
static bool eb_tick(void) {
 uint64_t t=eb.io.now(eb.io.ctx);
 if(!t || t<eb.last || t<eb.r.begin || t-eb.r.begin>=ECAM_BOOT_LIMIT_US) {
  bool first=!eb_diagnostic.reason;
  eb_fault(!t || t<eb.last || t<eb.r.begin ? "clock-invalid" : "lifetime-deadline",1,0,0,0,eb.last,t);
  if(first){eb_diagnostic.sample_us=t;eb_diagnostic.rejected_clock=true;eb_diagnostic.sample_clock_valid=t && t>=eb.last && t>=eb.r.begin;}
  return false;
 }
 eb.last=t;eb.r.end=t;return true;
}
static bool eb_event(unsigned event,unsigned n) {
#ifdef SRVROS_ECAM_HOST_TEST
 return ecam_test_event(event,n);
#else
 (void)event;(void)n;return true;
#endif
}
/* Read only admitted RAM. All hardware table words, including HHDM walk words,
 * are charged to this operation's separate fixed bound. Bootstrap HHDM is the
 * same trusted RAM accessor used by PMM; no target/MMIO data load is provided. */
static bool eb_raw(uint64_t pa,unsigned i,uint64_t *v) {
 if(i>=512 || !pa || (pa&4095) || pa>=(1ull<<39))return eb_fault("read-address",2,pa,i,0,0,0);
 if(eb.r.words>=ECAM_BOOT_WORD_CAP)return eb_fault("word-cap",3,pa,i,0,ECAM_BOOT_WORD_CAP,eb.r.words);
 if(!eb_guard() || !eb_tick())return false;
 if(!eb.io.ram(eb.io.ctx,pa))return eb_fault("RAM-source",2,pa,i,0,0,0);
 eb.r.words++;
#ifdef SRVROS_ECAM_HOST_TEST
 return ecam_test_load(pa,i,v) || eb_fault("read-failed",2,pa,i,0,0,0);
#else
 volatile uint64_t *p=pmm_phys_to_virt(pa);
 if(!p || !is_canonical((uint64_t)(uintptr_t)p))return eb_fault("RAM-pointer",2,pa,i,0,0,0);
 *v=p[i];return true;
#endif
}
static bool eb_address(uint64_t pa,unsigned i,volatile uint64_t **out) {
 if(i>=512 || !eb.io.ram(eb.io.ctx,pa))return eb_fault("RAM-source",2,pa,i,0,0,0);
 uint64_t va=(uint64_t)(uintptr_t)pmm_phys_to_virt(pa+i*8);
#ifdef SRVROS_ECAM_HOST_TEST
 va=ecam_test_virtual(pa+i*8);
#endif
 if(!eb.r.hhdm || eb.r.hhdm>UINT64_MAX-(pa+i*8))return eb_fault("HHDM-offset",6,pa,i,0,0,eb.r.hhdm);
 if(va!=eb.r.hhdm+pa+i*8)return eb_fault("HHDM-pointer",3,pa,i,0,eb.r.hhdm+pa+i*8,va);
 if(!is_canonical(va))return eb_fault("HHDM-canonical",6,pa,i,0,0,va);
 uint64_t table=eb.r.root;
 const uint64_t addr=((1ull<<39)-1)&~4095ull;
 for(unsigned level=0;level<4;level++) {
  unsigned shift=39-9*level;uint64_t word;
  unsigned slot=(va>>shift)&511;
  if(!eb_raw(table,slot,&word))return false;
  if(!(word&1) || (word&~(addr|0x8000000000000fffull)) ||
     (word&0x000ffffffffff000ull&~addr) || (!level && (word&128)))return eb_fault("HHDM-entry",6,table,slot,level,0,word);
  if(level==3 || ((level==1 || level==2) && (word&128))) {
   unsigned bits=level==1?30:level==2?21:12;
   uint64_t low=(1ull<<bits)-1;
   if(level!=3 && (word&addr&low&~4096ull))return eb_fault("HHDM-alignment",6,table,slot,level,0,word);
   if(((word&addr&~low)|(va&low))!=pa+i*8)return eb_fault("HHDM-translation",3,table,slot,level,pa+i*8,(word&addr&~low)|(va&low));
   *out=(volatile uint64_t *)(uintptr_t)va;return true;
  }
  table=word&addr;
 }
 return false;
}
static bool eb_word(uint64_t pa,unsigned i,uint64_t *v) {
 volatile uint64_t *p;
 if(!eb_address(pa,i,&p))return false;
 (void)p;return eb_raw(pa,i,v);
}
bool vmm_ecam_boot_copy_ram(uint64_t pa,void *output,unsigned length) {
 if(!output || !length || length>136 || pa>=(1ull<<39) || length>(1ull<<39)-pa ||
    !eb.callback_active || !eb.io.ram || !eb.io.now || !eb_guard())return false;
 uint8_t *out=output;
 for(unsigned at=0;at<length;) {
  uint64_t address=pa+at,word;
  if(!eb_word(address&~4095ull,(address&4095)/8,&word))return false;
  unsigned offset=address&7;
  for(;offset<8 && at<length;offset++,at++)out[at]=(uint8_t)(word>>(8*offset));
 }
 return true;
}
static bool eb_store(uint64_t pa,unsigned i,uint64_t before,uint64_t after) {
 volatile uint64_t *p;uint64_t observed;
 if(!eb_address(pa,i,&p) || !eb_raw(pa,i,&observed) || observed!=before || !eb_event(1,i))return false;
#ifdef SRVROS_ECAM_HOST_TEST
 return ecam_test_store(pa,i,before,after);
#else
 return __atomic_compare_exchange_n(p,&before,after,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
#endif
}
static uint64_t eb_leaf(unsigned i) {
 return i<ECAM_BOOT_LEAVES ? (ECAM_BOOT_PA+((uint64_t)i<<21))|0x8000000000000093ull : 0;
}
static bool eb_owned(void) {
 return eb.r.frame && eb.r.frame!=eb.r.root && eb.r.frame!=eb.r.pdpt &&
        pmm_frame_has_tag(eb.r.frame,PMM_FRAME_PAGE_TABLE);
}
static bool eb_metadata(void) {
 if(kernel_mapping_count!=eb.metadata_count)return false;
 for(unsigned i=0;i<VMM_MAX_KERNEL_MAPPINGS;i++)
  if(kernel_mappings[i].virtual_address!=eb.metadata[i].virtual_address ||
     kernel_mappings[i].physical_address!=eb.metadata[i].physical_address ||
     kernel_mappings[i].flags!=eb.metadata[i].flags)return false;
 return true;
}
static bool eb_verify(bool linked,bool private_page) {
 if(!eb_guard() || !eb_metadata() || !eb_check(2))return false;
 for(unsigned i=0;i<512;i++) {
  uint64_t a,b;
  if(!eb_word(eb.r.root,i,&a) || !eb_word(eb.r.pdpt,i,&b) || a!=eb.root_words[i] ||
     b!=(i==12 ? (linked?eb.r.frame|3:0) : eb.pdpt_words[i]))return false;
  if(linked){eb_evidence.root_after[i]=a;eb_evidence.pdpt_after[i]=b;}
 }
 if(private_page) {
  if(!eb_owned())return false;
  for(unsigned i=0;i<512;i++){uint64_t word;if(!eb_word(eb.r.frame,i,&word) || word!=eb_leaf(i))return false;eb_evidence.pd[i]=word;}
 }
 return true;
}
static bool eb_free_detached(void) {
 if(!eb_guard() || !eb_owned() || !eb_event(4,0))return false;
 pmm_free_frame(eb.r.frame);eb.r.frees++;
 return pmm_frame_has_tag(eb.r.frame,PMM_FRAME_FREE);
}
static bool eb_refuse(void) {
 if(eb.r.parent_attempts)return eb_poison();
 if(eb.r.frame && (!eb_verify(false,false) || !eb_free_detached()))return eb_poison();
 eb.r.state=ECAM_REFUSED;return false;
}
static bool eb_invalidate(void) {
 for(unsigned i=0;i<ECAM_BOOT_GRANULES;i++) {
  if(!eb_guard() || !eb_tick() || !eb_event(2,i))return false;
#ifdef SRVROS_ECAM_HOST_TEST
  if(!ecam_test_invalidate(ECAM_BOOT_VA+(uint64_t)i*4096))return false;
#else
  invlpg(ECAM_BOOT_VA+(uint64_t)i*4096);
#endif
  eb.r.invalidations++;
 }
 return true;
}
bool vmm_ecam_boot_prepare(const struct ecam_boot_env *io) {
 eb_stage="prepare";
 if(eb.r.state!=ECAM_NEW)return false;
 eb.r.state=ECAM_REFUSED;
 if(!io || !io->check || !io->ram || !io->now)return false;
 eb.io=*io;eb.r.root=kernel_cr3;eb.r.hhdm=pmm_hhdm_offset();eb.r.begin=eb.last=io->now(io->ctx);
 if(!eb_guard() || !eb_tick() || !eb_check(0) || kernel_mapping_count>VMM_MAX_KERNEL_MAPPINGS)return false;
 eb.metadata_count=kernel_mapping_count;
 for(unsigned i=0;i<VMM_MAX_KERNEL_MAPPINGS;i++) {
  eb.metadata[i]=kernel_mappings[i];uint64_t va=kernel_mappings[i].virtual_address;
  eb_evidence.metadata_before[i][0]=va;eb_evidence.metadata_before[i][1]=kernel_mappings[i].physical_address;eb_evidence.metadata_before[i][2]=kernel_mappings[i].flags;
  if(va>=ECAM_BOOT_VA && va<ECAM_BOOT_VA+ECAM_BOOT_BYTES)return false;
 }
 for(unsigned i=0;i<512;i++)if(!eb_word(eb.r.root,i,&eb.root_words[i]))return false;
 uint64_t parent=eb.root_words[384],allowed=(((1ull<<39)-1)&~4095ull)|0x800000000000003bull;
 if((parent&3)!=3 || (parent&~allowed))return false;
 eb.r.pdpt=parent&(((1ull<<39)-1)&~4095ull);
 if(eb.r.pdpt==eb.r.root)return false;
 for(unsigned i=0;i<512;i++)if(!eb_word(eb.r.pdpt,i,&eb.pdpt_words[i]))return false;
 if(eb.pdpt_words[12])return false;
 eb_evidence.count_before=eb.metadata_count;
 for(unsigned i=0;i<512;i++){eb_evidence.root_before[i]=eb.root_words[i];eb_evidence.pdpt_before[i]=eb.pdpt_words[i];}
 eb.r.frame=pmm_alloc_frames_above_tagged(1,VMM_PAGE_TABLE_MIN_PHYSICAL,PMM_FRAME_PAGE_TABLE);
 if(!eb.r.frame)return false;
 if(!eb_owned() || !eb.io.ram(eb.io.ctx,eb.r.frame))return eb_poison();
 for(unsigned i=0;i<512;i++) {
  uint64_t old;if(!eb_word(eb.r.frame,i,&old) || !eb_store(eb.r.frame,i,old,eb_leaf(i)))return eb_refuse();
  eb.r.private_writes++;
 }
 if(!eb_check(1) || !eb_verify(false,true) || eb.r.end<=eb.r.begin)return eb_refuse();
 /* Terminal retention is set BEFORE the first potentially visible store. */
 eb.r.state=ECAM_POISONED;eb.r.parent_attempts++;
 if(!eb_store(eb.r.pdpt,12,0,eb.r.frame|3) || !eb_invalidate() || !eb_verify(true,true))return false;
 eb.r.state=ECAM_READY;eb_diagnostic.prepared_us=eb.r.end;return true;
}
bool vmm_ecam_boot_retire(void) {
 eb_stage="retire";
 if(eb.r.state!=ECAM_READY)return false;
 uint64_t start=eb.last;
 if(!eb_verify(true,true))return eb_poison();
 eb.r.state=ECAM_POISONED;eb.r.parent_attempts++;
 if(!eb_store(eb.r.pdpt,12,eb.r.frame|3,0) || !eb_invalidate() || !eb_verify(false,true) || eb.r.end<=start || !eb_free_detached())return false;
 eb.r.state=ECAM_RETIRED;return true;
}
bool vmm_ecam_boot_handoff(void) {
 eb_stage="handoff";
 if(eb.r.state!=ECAM_READY)return false;
 uint64_t start=eb.last;
 if(!eb_verify(true,true) || eb.r.end<=start)return eb_poison();
 eb.r.state=ECAM_POISONED;
 const uint64_t flags=VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE;
 for(unsigned i=0;i<ECAM_BOOT_GRANULES;i++) {
  if(!eb_event(3,i) || (i%512==0 && (!eb_tick() || !eb_guard())))return false;
  register_kernel_mapping(ECAM_BOOT_VA+(uint64_t)i*4096,ECAM_BOOT_PA+(uint64_t)i*4096,flags);
  eb.r.metadata_attempts++;
 }
 if(kernel_mapping_count!=VMM_MAX_KERNEL_MAPPINGS || !eb_guard() || !eb_tick() || eb.r.end<=start || !eb_check(2))return false;
 for(unsigned i=0;i<VMM_MAX_KERNEL_MAPPINGS;i++) {
  struct kernel_mapping want=i<eb.metadata_count ? eb.metadata[i] : (struct kernel_mapping){ECAM_BOOT_VA+(i-eb.metadata_count)*4096,ECAM_BOOT_PA+(i-eb.metadata_count)*4096,flags};
  if(kernel_mappings[i].virtual_address!=want.virtual_address || kernel_mappings[i].physical_address!=want.physical_address || kernel_mappings[i].flags!=want.flags)return false;
  eb_evidence.metadata_after[i][0]=kernel_mappings[i].virtual_address;eb_evidence.metadata_after[i][1]=kernel_mappings[i].physical_address;eb_evidence.metadata_after[i][2]=kernel_mappings[i].flags;
 }
 eb_evidence.count_after=kernel_mapping_count;
 eb.r.state=ECAM_ACTIVE;eb_diagnostic.handoff_us=eb.r.end;return true;
}
const struct ecam_boot_result *vmm_ecam_boot_result(void) {return &eb.r;}
const struct ecam_boot_evidence *vmm_ecam_boot_evidence(void){return &eb_evidence;}
bool vmm_ecam_boot_adopt(uint64_t va,uint64_t pa,uint64_t bytes) {
 eb_stage="adopt-enter";
 if(eb.r.state!=ECAM_ACTIVE || eb.r.adoptions)return eb_fault("state-or-consumed",1,0,0,0,ECAM_ACTIVE,eb.r.state);
 eb_diagnostic.adopt_entry_last_us=eb.last;
 if(va!=ECAM_BOOT_VA || pa!=ECAM_BOOT_PA || bytes!=ECAM_BOOT_BYTES) {
  unsigned n=va!=ECAM_BOOT_VA?0:pa!=ECAM_BOOT_PA?1:2;
  eb_fault("geometry",1,0,0,n,n==0?ECAM_BOOT_VA:n==1?ECAM_BOOT_PA:ECAM_BOOT_BYTES,n==0?va:n==1?pa:bytes);return eb_poison();
 }
 if(!eb_guard())return eb_poison();
 if(!eb_owned()){eb_fault("frame-ownership",2,eb.r.frame,0,0,0,0);return eb_poison();}
 eb_stage="adopt-gate";
 if(!eb_check(2))return eb_poison();
 eb_stage="adopt-metadata";
 for(unsigned i=0;i<128;i++) {
  const uint64_t actual[]={kernel_mappings[i].virtual_address,kernel_mappings[i].physical_address,kernel_mappings[i].flags};
  for(unsigned j=0;j<3;j++)if(actual[j]!=eb_evidence.metadata_after[i][j]){eb_fault("metadata-word",1,0,i,j,eb_evidence.metadata_after[i][j],actual[j]);return eb_poison();}
 }
 if(kernel_mapping_count!=eb_evidence.count_after){eb_fault("metadata-count",1,0,0,0,eb_evidence.count_after,kernel_mapping_count);return eb_poison();}
 uint64_t start=eb.last;
 eb_stage="adopt-tables";
 for(unsigned i=0;i<512;i++) {
  uint64_t a,b,c;
  if(!eb_word(eb.r.root,i,&a) || !eb_word(eb.r.pdpt,i,&b) || !eb_word(eb.r.frame,i,&c))return eb_poison();
  if(a!=eb_evidence.root_after[i]){eb_fault("root-word",3,eb.r.root,i,0,eb_evidence.root_after[i],a);return eb_poison();}
  if(b!=eb_evidence.pdpt_after[i]){eb_fault("pdpt-word",3,eb.r.pdpt,i,0,eb_evidence.pdpt_after[i],b);return eb_poison();}
  if(c!=eb_evidence.pd[i]){eb_fault("PD-word",3,eb.r.frame,i,0,eb_evidence.pd[i],c);return eb_poison();}
 }
 eb_stage="adopt-finish";
 if(!eb_guard() || !eb_tick())return eb_poison();
 if(eb.r.end<=start){eb_fault("clock-no-progress",1,0,0,0,start,eb.r.end);eb_diagnostic.sample_clock_valid=false;return eb_poison();}
 eb.r.adopted_us=eb.r.end;eb.r.adoptions=1;return true;
}
