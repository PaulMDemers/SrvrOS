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
static bool eb_poison(void) {eb.r.state=ECAM_POISONED;return false;}
static bool eb_guard(void) {return vmm_diagnostic_sole_kernel_root(eb.r.root);}
static bool eb_check(unsigned phase) {
 if(eb.callback_active)return false;
 eb.callback_active=true;bool ok=eb.io.check(eb.io.ctx,phase);eb.callback_active=false;return ok;
}
static bool eb_tick(void) {
 uint64_t t=eb.io.now(eb.io.ctx);
 if(!t || t<eb.last || t<eb.r.begin || t-eb.r.begin>=ECAM_BOOT_LIMIT_US)return false;
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
 if(i>=512 || !pa || (pa&4095) || pa>=(1ull<<39) ||
    eb.r.words>=ECAM_BOOT_WORD_CAP || !eb_guard() || !eb_tick() || !eb.io.ram(eb.io.ctx,pa))return false;
 eb.r.words++;
#ifdef SRVROS_ECAM_HOST_TEST
 return ecam_test_load(pa,i,v);
#else
 volatile uint64_t *p=pmm_phys_to_virt(pa);
 if(!p || !is_canonical((uint64_t)(uintptr_t)p))return false;
 *v=p[i];return true;
#endif
}
static bool eb_address(uint64_t pa,unsigned i,volatile uint64_t **out) {
 if(i>=512 || !eb.io.ram(eb.io.ctx,pa))return false;
 uint64_t va=(uint64_t)(uintptr_t)pmm_phys_to_virt(pa+i*8);
#ifdef SRVROS_ECAM_HOST_TEST
 va=ecam_test_virtual(pa+i*8);
#endif
 if(!is_canonical(va))return false;
 uint64_t table=eb.r.root;
 const uint64_t addr=((1ull<<39)-1)&~4095ull;
 for(unsigned level=0;level<4;level++) {
  unsigned shift=39-9*level;uint64_t word;
  if(!eb_raw(table,(va>>shift)&511,&word) || !(word&1) ||
     (word&~(addr|0x8000000000000fffull)) ||
     (word&0x000ffffffffff000ull&~addr) || (!level && (word&128)))return false;
  if(level==3 || ((level==1 || level==2) && (word&128))) {
   unsigned bits=level==1?30:level==2?21:12;
   uint64_t low=(1ull<<bits)-1;
   if(level!=3 && (word&addr&low&~4096ull))return false;
   if(((word&addr&~low)|(va&low))!=pa+i*8)return false;
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
 }
 if(private_page) {
  if(!eb_owned())return false;
  for(unsigned i=0;i<512;i++){uint64_t word;if(!eb_word(eb.r.frame,i,&word) || word!=eb_leaf(i))return false;}
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
 if(eb.r.state!=ECAM_NEW)return false;
 eb.r.state=ECAM_REFUSED;
 if(!io || !io->check || !io->ram || !io->now)return false;
 eb.io=*io;eb.r.root=kernel_cr3;eb.r.begin=eb.last=io->now(io->ctx);
 if(!eb_guard() || !eb_tick() || !eb_check(0) || kernel_mapping_count>VMM_MAX_KERNEL_MAPPINGS)return false;
 eb.metadata_count=kernel_mapping_count;
 for(unsigned i=0;i<VMM_MAX_KERNEL_MAPPINGS;i++) {
  eb.metadata[i]=kernel_mappings[i];uint64_t va=kernel_mappings[i].virtual_address;
  if(va>=ECAM_BOOT_VA && va<ECAM_BOOT_VA+ECAM_BOOT_BYTES)return false;
 }
 for(unsigned i=0;i<512;i++)if(!eb_word(eb.r.root,i,&eb.root_words[i]))return false;
 uint64_t parent=eb.root_words[384],allowed=(((1ull<<39)-1)&~4095ull)|0x800000000000003bull;
 if((parent&3)!=3 || (parent&~allowed))return false;
 eb.r.pdpt=parent&(((1ull<<39)-1)&~4095ull);
 if(eb.r.pdpt==eb.r.root)return false;
 for(unsigned i=0;i<512;i++)if(!eb_word(eb.r.pdpt,i,&eb.pdpt_words[i]))return false;
 if(eb.pdpt_words[12])return false;
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
 eb.r.state=ECAM_READY;return true;
}
bool vmm_ecam_boot_retire(void) {
 if(eb.r.state!=ECAM_READY)return false;
 uint64_t start=eb.last;
 if(!eb_verify(true,true))return eb_poison();
 eb.r.state=ECAM_POISONED;eb.r.parent_attempts++;
 if(!eb_store(eb.r.pdpt,12,eb.r.frame|3,0) || !eb_invalidate() || !eb_verify(false,true) || eb.r.end<=start || !eb_free_detached())return false;
 eb.r.state=ECAM_RETIRED;return true;
}
bool vmm_ecam_boot_handoff(void) {
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
 }
 eb.r.state=ECAM_ACTIVE;return true;
}
const struct ecam_boot_result *vmm_ecam_boot_result(void) {return &eb.r;}
