#include <srvros/intel_bdw_stolen_scanout.h>
#ifndef SRVROS_INTEL_STOLEN_CAPTURE
bool intel_stolen_probe_report(const char *cmdline) {(void)cmdline;return false;}
bool intel_stolen_probe_recipe(const char *cmdline) {(void)cmdline;return false;}
bool intel_stolen_probe_retained(void) {return false;}
#else
#include <srvros/intel_scanout_trial.h>
#include <srvros/intel_bdw_bcs_context.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/intel_bdw_private_vm.h>
#include <srvros/intel_bdw_blitter.h>
#include <srvros/intel_reader_audit.h>
#include <srvros/intel_completion_audit.h>
#include <srvros/intel_pci_requester.h>
#include <srvros/intel_handoff.h>
#include <limine.h>
extern volatile struct limine_memmap_request memmap_request;
#include <srvros/scanout_transfer.h>
#include <srvros/scanout_memory.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/x86_cache.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>
#include <srvros/pmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <srvros/workspace_log.h>

/* Separate read-only census budget: at most23 GGTT pages/pass and three
 * stable DWORD loads for every entry, with fresh PCI/root/alias checks. This
 * changes no100ms GT maintenance/publication deadline or ownership policy. */
#define OWNED_PAGE_SLOT 12291u
#define STOLEN_CENSUS_US 2000000u
/* Three disjoint GPU-only command windows, below the Gen8 32-bit limit.
 * None can be reached by indexing the CPU aperture. */
static const uint32_t command_slots[] = {0x10000, 0x80000, 0xffe00};
#define COMMAND_PAGES 32u
#define STOLEN_VA 0xffffc001f51a0000ull
static struct {
 struct scanout_handoff_snapshot source;
 struct scanout_profile profile;
 struct display_access_token exclusion;
 struct bdw_stolen_facts facts[2];
 struct bdw_stolen_plan plan;
 struct bdw_stolen_layout layout;
 struct bdw_stolen_vectors vectors;
 struct x86_cache_snapshot cache;
 struct vmm_range_observation root_observation;
 struct {
  uint64_t begin,end;
  unsigned attempted,observed,admitted,ram_count,pmm_checks,unmapped;
  struct {uint64_t physical;unsigned pages,checked,reserved,framebuffer,other,uncovered,tagged;} span[2];
  struct vmm_alias_ram ram[VMM_ALIAS_RAM_LIMIT];
  struct vmm_alias_inventory inventory;
  struct vmm_unmapped_detail detail;
 } cpu;
 struct {
  struct vmm_reserved_guard owner;
  struct vmm_alias_inventory inventory[3];
  struct vmm_unmapped_detail detail;
  uint64_t begin,end;
  unsigned attempted,pinned,acquired,checked,closed,unpinned;
 } guard;
 struct {
  uint64_t begin,end;
  uint32_t config[2][INTEL_PCI_REQUESTER_WORDS+2];
  uint32_t readers[42];
  unsigned config_reads[2],reader_reads;
 } handoff;
 struct {
  struct scanout_transfer transfer;
  struct scanout_transfer_pool pool;
  struct scanout_transfer_token token;
  uint64_t epoch,digest,excluded_aperture_page;
  unsigned first,pages,checked,fences,closing_readers,missing,protected_pages;
  uint32_t frame[2],frame_reads;
  bool selected,verified,aperture_alias;
 } range;
 struct {
  struct scanout_interval lease;
  unsigned begin, first, scanned, nonzero, verified, batches, maps;
  bool selected, scanning, claimed, returned, exhausted, reader_conflict;
 } private_range;
 struct {
  struct scanout_interval lease;
  uint64_t original[SCANOUT_TRANSFER_PAGES];
  unsigned first, pages, sampled, matched, occupied, present, same_first;
  bool reading, claimed, returned, conflict;
 } gpu_range;
 struct {
  struct scanout_interval firmware;
  struct scanout_transfer page;
  struct scanout_transfer_token token;
  uint64_t observed[2];
  unsigned blocked, samples;
  bool selected, imported, returned, submission;
  unsigned ports, wd_samples;
  uint32_t wd[2][4], strap[2];
  struct {uint32_t well_before, codec_before, control, codec_after, well_after;} audio[2];
  unsigned audio_samples;
  struct scanout_interval commands;
  uint32_t command_page, occupied[3], first, tried, checked;
  bool command_returned;
 } import;
 struct {
  struct scanout_transfer transfer;
  struct scanout_transfer_token token;
  uint64_t entries[2][3]; /* left, selected slot, right */
  uint64_t aperture, table_backing, register_page;
  unsigned guard_checks, reader_pages, reader_words;
  uint32_t frame[2], frame_reads;
  unsigned samples, conflicts;
  bool reading, returned;
 } owned_page;
 struct {
  struct scanout_transfer transfer;
  struct scanout_transfer_token token;
  struct scanout_transfer_manifest manifest;
  struct bdw_blitter_image image;
  uint64_t dma_base;
  struct intel_bdw_ggtt_page planned[7];
  uint64_t pdp0;
  unsigned flush_lines;
  bool dma_freed;
  struct intel_bdw_rcs_indirect rcs;
  uint64_t descriptor;
  uint64_t aperture, table_backing, register_page;
  unsigned guard_checks;
  uint32_t frame[2], frame_reads;
  unsigned samples, conflicts;
  bool returned;
 } command_tail;
 const struct pci_device *gpu,*host;
 uint64_t root,last,begin[2],end[2],physical,hash[2][3],clock_at,clock_previous;
 uint64_t pte[2][3][16384];
 uint32_t aux[2][3],count[2][3],reads[2][3],meta_reads;
 uint32_t reader[2][3][13],reader_count[2][3],reader_reads;
 struct {uint64_t physical;unsigned pages,queried,ok,mtrr,pat,effective,checks;} policy[2][2][8];
 unsigned maps,retired,cache_checks,alias_checks,phase,reason,fail,fail_offset,active_offset;
 unsigned mtrr,pat,effective;
 uint32_t page;
 struct {unsigned seq,step,edge,phase,offset,maps,retired,held,mapped;} trace[16];
 unsigned sequence;
 const char *command;
 char progress[4096];
 bool attempted,mapped,uncertain,unmap_failed,acquired,released,complete,preflight;
} stolen;
/* Wire2/3: preserve the first refusal, even if cleanup subsequently fails.
 * Values13/14 distinguish resource admission from actual PAT/MTRR policy. */
static bool refuse(unsigned code) {
 if(!stolen.fail){stolen.fail=code;stolen.fail_offset=stolen.active_offset;}
 return false;
}
/* RAM trace never writes the framebuffer or storage through a live alias.
 * A durable entry names a pending operation, not its successful completion.
 * Persisting inside a pass consumes that original pass's existing budget. */
static void trace(unsigned step,unsigned edge) {
 unsigned seq=++stolen.sequence;
 stolen.trace[(seq-1)%16]=(__typeof__(stolen.trace[0])){seq,step,edge,stolen.phase,
  stolen.active_offset,stolen.maps,stolen.retired,stolen.acquired && !stolen.released,stolen.mapped};
}
static bool append(char **out,const char *text) {
 while(*text) {if(*out>=stolen.progress+sizeof(stolen.progress)-1)return false;*(*out)++=*text++;}return true;
}
static bool number(char **out,uint64_t value) {
 char digits[20];unsigned n=0;do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);
 while(n){char t[2]={digits[--n],0};if(!append(out,t))return false;}return true;
}
static bool checkpoint(unsigned step) {
 trace(step,0);
 if(stolen.mapped || stolen.uncertain)return refuse(27);
 if(stolen.acquired && !stolen.released &&
    (!display_access_held(&stolen.exclusion) || vmm_current_address_space()!=stolen.root))return refuse(27);
 char *out=stolen.progress;
 if(!append(&out,"laptop-capture: boot options=") || !append(&out,stolen.command) || !append(&out,"\n"))return refuse(27);
 unsigned start=stolen.sequence>16?stolen.sequence-15:1;
 for(unsigned seq=start;seq<=stolen.sequence;seq++) {
  const __typeof__(stolen.trace[0]) *t=&stolen.trace[(seq-1)%16];
  const char *keys[]={"seq","step","edge","phase","offset","maps","retired","held","mapped"};
  const unsigned values[]={t->seq,t->step,t->edge,t->phase,t->offset,t->maps,t->retired,t->held,t->mapped};
  if(!append(&out,"intel-stolen: progress v=1"))return refuse(27);
  for(unsigned i=0;i<9;i++)if(!append(&out," ") || !append(&out,keys[i]) || !append(&out,"=") || !number(&out,values[i]))return refuse(27);
  if(!append(&out,"\n"))return refuse(27);
 }
 *out=0;
 if(!stolen.acquired || stolen.released)console_printf("intel-stolen: pending step=%u; saving diagnostic progress\n",(uint64_t)step);
 if(!workspace_log_stolen_progress(stolen.progress,(uint64_t)(out-stolen.progress)))return refuse(27);
 if(stolen.acquired && !stolen.released &&
    (!display_access_held(&stolen.exclusion) || vmm_current_address_space()!=stolen.root))return refuse(27);
 return true;
}
static bool equal_bytes(const void *a,const void *b,unsigned n) {
 const unsigned char *x=a,*y=b;for(unsigned i=0;i<n;i++)if(x[i]!=y[i])return false;return true;
}
static bool parse_recipe(const char *s,bool *range,bool *private_range,bool *import,bool *submission) {
 static const char *const fixed[]={"srvros.storage=ram","srvros.capture=intel-display","srvros.intel-backing=read-only",
 "srvros.intel-cache=read-only","srvros.intel-window=read-only","srvros.intel-stolen=read-only","srvros.capture-failure=reboot"};
 unsigned seen=0,tokens=0;
 if(!s)return false;
 while(*s) {
  while(*s==' ' || *s=='\t' || *s=='\n' || *s=='\r')++s;
  if(!*s)break;
  const char *start=s;while(*s && *s!=' ' && *s!='\t' && *s!='\n' && *s!='\r')++s;
  unsigned len=(unsigned)(s-start),bit=0;
  for(unsigned i=0;i<7;i++) {
   unsigned n=0;while(fixed[i][n])++n;
   if(n==len && equal_bytes(start,fixed[i],n))bit=1u<<i;
  }
  const char *prefix[]={"srvros.workspace-volume=","srvros.desktop-release="};
  for(unsigned i=0;i<2;i++) {
   unsigned n=0;while(prefix[i][n])++n;
   unsigned count=i?16:8;
   if(len==n+count && equal_bytes(start,prefix[i],n)) {
    bool valid=true;for(unsigned k=n;k<len;k++)if(!((start[k]>='0' && start[k]<='9') || (start[k]>='a' && start[k]<='f')))valid=false;
    if(valid)bit=1u<<(7+i);
   }
  }
  static const char exact_range[]="srvros.intel-scanout=range-observation";
  if(len==sizeof(exact_range)-1 && equal_bytes(start,exact_range,len))bit=512;
  static const char allocator[]="srvros.intel-scanout=private-range";
  if(len==sizeof(allocator)-1 && equal_bytes(start,allocator,len))bit=1024;
  static const char imported[]="srvros.intel-scanout=firmware-import";
  if(len==sizeof(imported)-1 && equal_bytes(start,imported,len))bit=2048;
  static const char submit[]="srvros.intel-scanout=submission-guard";
  if(len==sizeof(submit)-1 && equal_bytes(start,submit,len))bit=4096;
  if(!bit || (seen&bit) || ++tokens>10)return false;
  seen|=bit;
 }
 if(!((tokens==9 && seen==511) || (tokens==10 && (seen==1023 || seen==1535 || seen==2559 || seen==4607))))return false;
 if(range)*range=(seen&(512|1024|2048|4096))!=0;
 if(import)*import=(seen&(2048|4096))!=0;
 if(submission)*submission=(seen&4096)!=0;
 if(private_range)*private_range=(seen&1024)!=0;
 return true;
}
bool intel_stolen_probe_recipe(const char *s) {
 return parse_recipe(s,NULL,NULL,NULL,NULL);
}
static bool tick(uint64_t begin) {
 uint64_t at=timer_monotonic_us();
 if(!at || at<stolen.last || at<begin || at-begin>STOLEN_CENSUS_US){stolen.clock_at=at;stolen.clock_previous=stolen.last;stolen.reason|=2;return refuse(18);}
 stolen.last=at;return true;
}
/* The leaf observer requires a sole kernel root, not merely current==kernel.
 * Admit that condition before installing an alias that cleanup must validate.
 * Repeat per mapping; never infer later retirement from this early check. */
static bool sole_root(void) {
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 bool ok=vmm_diagnostic_sole_kernel_root_observe(stolen.root,&stolen.root_observation);
#else
 uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
 bool ok=vmm_diagnostic_sole_kernel_root_observe(stolen.root,&stolen.root_observation);
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 return ok || refuse(5);
}
static bool identity(void) {
 if(!stolen.acquired || stolen.released || !display_access_held(&stolen.exclusion) ||
 vmm_current_address_space()!=stolen.root || stolen.root!=vmm_kernel_address_space() ||
 !intel_boot_check(INTEL_BOOT_OBSERVER,stolen.gpu))return refuse(9);
 uint32_t lo=pci_read_config32(stolen.gpu,0x10),hi=pci_read_config32(stolen.gpu,0x14);
 bool ok=pci_read_config32(stolen.gpu,0)==0x16268086 && pci_read_config32(stolen.gpu,0x2c)==0x011b106b &&
 ((uint64_t)hi<<32 | (lo&~15u))==stolen.source.mmio && (pci_read_config32(stolen.gpu,4)&7)==7;
 return ok || refuse(9);
}
static bool allowed(uint32_t page) {
 if(stolen.owned_page.reading && stolen.import.selected && !stolen.import.submission &&
    stolen.import.imported && stolen.guard.checked && !stolen.guard.closed &&
    page==((0x800000u+8*OWNED_PAGE_SLOT)&~4095u))return true;
 if(page==0x42000 && stolen.import.selected && stolen.guard.checked && !stolen.guard.closed)return true;
 if(stolen.import.submission && stolen.guard.checked && !stolen.guard.closed) {
  for(unsigned i=0;i<3;i++)
   if(page==stolen.import.command_page && page==0x800000u+8*command_slots[i])return true;
 }
 if(stolen.gpu_range.reading && stolen.guard.checked && !stolen.guard.closed &&
    page>=0x800000u+8*stolen.gpu_range.first &&
    page<0x800000u+8*(stolen.gpu_range.first+stolen.gpu_range.pages))return true;
 if(stolen.private_range.scanning && stolen.guard.checked && !stolen.guard.closed &&
    page>=0x800000u+8*stolen.private_range.begin && page<0x800000u+8*SCANOUT_SPACE_PAGES)return true;
 if((page==0x6e000 || page==0x65000 || page==0x45000) && stolen.import.selected && stolen.guard.pinned &&
    (!stolen.guard.acquired || stolen.guard.closed))return true;
 if(page==0x100000 && stolen.range.selected && stolen.guard.checked && !stolen.guard.closed)return true;
 if(page==0x108000 || page==0x43000 || page==0x6f000 ||
    page==0x70000 || page==0x71000 || page==0x72000)return true;
 if(!stolen.plan.range[0].pages)return false;
 for(unsigned r=0;r<3;r++) {
  uint64_t first=0x800000+stolen.plan.range[r].gpu/4096*8;
  uint64_t end=first+(uint64_t)stolen.plan.range[r].pages*8;
  if(page+4096ull>first && page<end)return true;
 }
 return false;
}
/* Reject every overlap with boot RAM/reclaimable/firmware memory. A gap in
 * the firmware map is permitted only inside the exact previously admitted
 * 16MiB PCI BAR; this is an MMIO observation, never a RAM reservation. */
static bool reserved_page(uint64_t physical) {
 const struct limine_memmap_response *m=memmap_request.response;
 if(!m || !m->entries || !m->entry_count || m->entry_count>4096 ||
 physical<stolen.source.mmio || physical-stolen.source.mmio>0x1000000-4096)return false;
 for(uint64_t i=0;i<m->entry_count;i++) {
  const struct limine_memmap_entry *e=m->entries[i];
  if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
  if(e->base<physical+4096 && physical<e->base+e->length && e->type!=LIMINE_MEMMAP_RESERVED)return false;
 }
 return true;
}
static bool policy(uint64_t physical) {
 struct x86_cache_snapshot fresh;unsigned mtrr,pat,effective;
 if(!identity())return false;
 if(!reserved_page(physical))return refuse(13);
 if(!intel_cache_probe_context(&fresh) || !x86_cache_equal(&fresh,&stolen.cache))return refuse(10);
 /* Captured native PAT: index2 is UC-minus(7), index3 is UC(0).
  * Require actual UC at every layer, rather than weakening the old type check. */
 if(!x86_cache_page(&fresh,physical,3,&mtrr,&pat,&effective))return refuse(14);
 stolen.mtrr=mtrr;stolen.pat=pat;stolen.effective=effective;
 if(mtrr || pat || effective)return refuse(14);
 ++stolen.cache_checks;return identity();
}
static bool alias(void) {
 uint64_t physical;
 if(!stolen.mapped || !identity() || !vmm_virt_to_phys_in_space(stolen.root,STOLEN_VA,&physical) || physical!=stolen.physical)return refuse(16);
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 bool ok=intel_stolen_test_alias(stolen.root,STOLEN_VA,physical);
#else
 uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
 struct vmm_alias_observation a={0};
 bool ok=vmm_diagnostic_kernel_alias(stolen.root,STOLEN_VA,physical,1,stolen.cache.cpu.physical_bits,&a,NULL) &&
 a.pages==1 && a.leaves==1 && a.pat_index==3 && !a.writable && !a.user && a.no_execute==1 && a.leaf_sizes==1;
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 if(ok)++stolen.alias_checks;
 return (ok || refuse(16)) && identity();
}
static bool unmap(void) {
 if(!stolen.mapped)return !stolen.uncertain;
 if(stolen.unmap_failed)return false;
 uint64_t physical;
 if(!alias() || !vmm_unmap_page_in_space(stolen.root,STOLEN_VA,&physical) || physical!=stolen.physical ||
 vmm_virt_to_phys_in_space(stolen.root,STOLEN_VA,&physical)) {
  stolen.uncertain=stolen.unmap_failed=true;stolen.reason|=8;return refuse(22);
 }
 stolen.mapped=false;++stolen.retired;return true;
}
static bool map(uint32_t page) {
 if(stolen.mapped && stolen.page==page)return true;
 if(!unmap())return false;
 if(!allowed(page))return refuse(26);
 if(!identity() || !sole_root())return false;
 uint64_t physical=stolen.source.mmio+page,old;
 if(physical<stolen.source.mmio || vmm_virt_to_phys_in_space(stolen.root,STOLEN_VA,&old))return refuse(15);
 trace(16,0);
 if(!policy(physical))return false;
 trace(16,1);
 trace(15,0);
 if(!vmm_map_page_in_space(stolen.root,STOLEN_VA,physical,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_WRITE_THROUGH)) {
  if(vmm_virt_to_phys_in_space(stolen.root,STOLEN_VA,&old))stolen.uncertain=true;
  return refuse(15);
 }
 stolen.mapped=true;stolen.page=page;stolen.physical=physical;++stolen.maps;
 trace(15,1);trace(17,0);
 bool ok=alias() && policy(physical);if(ok)trace(17,1);return ok;
}
static bool load(uint32_t offset,uint32_t *out) {
 /* alias() brackets the actual sole-root leaf walk with fresh identity
  * checks. Repeating the same PCI census immediately before it adds no check. */
 stolen.active_offset=offset;
 if(offset&3)return refuse(17);
 bool first=stolen.meta_reads==0 && stolen.maps<=1;
 if(!map(offset&~4095u) || !alias())return false;
 if(first)trace(18,0);
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 bool ok=intel_stolen_test_read(offset,out);if(ok && first)trace(18,1);return ok || refuse(17);
#else
 __asm__ volatile("mfence":::"memory");*out=*(volatile const uint32_t *)(STOLEN_VA+(offset&4095));
 __asm__ volatile("mfence":::"memory");if(first)trace(18,1);return true;
#endif
}
static bool facts(unsigned n) {
 if(n>=2 || !identity())return false;
 struct bdw_stolen_facts f={.host=pci_read_config32(stolen.host,0),.gpu=pci_read_config32(stolen.gpu,0),
 .subsystem=pci_read_config32(stolen.gpu,0x2c),.ggc=pci_read_config32(stolen.host,0x50)&65535,
 .bdsm=pci_read_config32(stolen.host,0xb0),.bgsm=pci_read_config32(stolen.host,0xb4),.tolud=pci_read_config32(stolen.host,0xbc),
 .aperture_bytes=stolen.source.aperture_bytes,.ggtt_bytes=0x800000,.fence_begin=stolen.source.fence_begin,
 .fence_end=stolen.source.fence_end,.width=stolen.profile.width,.height=stolen.profile.height,.stride=stolen.profile.pitch};
 if((pci_read_config32(stolen.gpu,0x50)&65535)!=f.ggc ||
 (pci_read_config32(stolen.gpu,0x5c)&0xfff00000u)!=(f.bdsm&0xfff00000u))return refuse(12);
 /* Qualify the actual RO UC3 alias lifetime before the first register load.
  * No storage crosses a live alias, and no count/clock is reset between this
  * preflight and the real read. A retained alias must still halt. */
 if(!stolen.preflight) {
  stolen.active_offset=0x1082c0;
  if(!checkpoint(16) || !tick(stolen.begin[n]))return false;
  if(!policy(stolen.source.mmio+0x108000) || !tick(stolen.begin[n]))return false;
  if(!checkpoint(15) || !tick(stolen.begin[n]))return false;
  if(!map(0x108000) || !unmap() || !identity())return false;
  stolen.preflight=true;
  if(!checkpoint(18) || !tick(stolen.begin[n]))return false;
 }
 if(!load(0x1082c0,&f.reserved))return false;
 ++stolen.meta_reads;
 const uint32_t off[]={0x43208,0x6f800,0x6f840};
 for(unsigned i=0;i<3;i++){if(!load(off[i],&stolen.aux[n][i]))return false;++stolen.meta_reads;}
 if(!unmap() || !identity())return false;
 const struct bdw_stolen_facts *old=&stolen.facts[0];
 if(n && (f.host!=old->host || f.gpu!=old->gpu || f.subsystem!=old->subsystem || f.ggc!=old->ggc ||
 f.bdsm!=old->bdsm || f.bgsm!=old->bgsm || f.tolud!=old->tolud || f.reserved!=old->reserved ||
 f.aperture_bytes!=old->aperture_bytes || f.ggtt_bytes!=old->ggtt_bytes || f.fence_begin!=old->fence_begin ||
 f.fence_end!=old->fence_end || f.width!=old->width || f.height!=old->height || f.stride!=old->stride))return refuse(24);
 stolen.facts[n]=f;return true;
}
/* Same thirteen documented read-only words as intel_reader_audit.c. These
 * bases do not describe a complete tiled/stereo/scaled reader footprint, and
 * a quiet snapshot is not maintained exclusion. Never turn this into rights. */
static bool readers(unsigned n) {
 for(unsigned p=0;p<3;p++) {
  if(!tick(stolen.begin[n]))return false;
  for(unsigned w=0;w<13;w++) {
   uint32_t value;
   if(!load(0x70000+0x1000*p+intel_reader_offsets[w],&value))return false;
   ++stolen.reader_reads;
   if(value==UINT32_MAX)return refuse(28);
   stolen.reader[n][p][w]=value;++stolen.reader_count[n][p];
  }
 }
 return unmap() && identity() && tick(stolen.begin[n]);
}
/* Entire physical candidate spans at every hypothetical PAT slot. The actual
 * translation census remains mandatory. No target RAM load, alias, reservation,
 * flush or visibility claim is made. Nonuniform/undefined policy stays unknown. */
static bool candidate_policy(unsigned n) {
 struct x86_cache_snapshot fresh;
 if(!identity() || !intel_cache_probe_context(&fresh) || !x86_cache_equal(&fresh,&stolen.cache))return refuse(10);
 for(unsigned r=0;r<2;r++)for(unsigned i=0;i<8;i++) {
  if(!tick(stolen.begin[n]))return false;
  __typeof__(stolen.policy[0][0][0]) *q=&stolen.policy[n][r][i];
  q->physical=stolen.pte[n][r+1][0]&~4095ull;q->pages=stolen.plan.range[r+1].pages;
  q->mtrr=q->pat=q->effective=255;q->queried=1;
  q->ok=x86_cache_range(&fresh,q->physical,q->pages,i,&q->mtrr,&q->pat,&q->effective,&q->checks);
 }
 return identity() && tick(stolen.begin[n]);
}
static uint64_t hash_word(uint64_t hash,uint64_t value) {
 for(unsigned i=0;i<8;i++){hash=(hash^(value&255))*1099511628211ull;value>>=8;}return hash;
}
static bool capture(unsigned n) {
 if(!unmap())return false;
 if(!checkpoint(8) || !tick(stolen.begin[n]))return false;
 if(!intel_window_handoff_revalidate(&stolen.source))return refuse(11);
 if(!checkpoint(9) || !tick(stolen.begin[n]))return false;
 if(!facts(n) || !tick(stolen.begin[n]))return false;
 if(!checkpoint(10) || !tick(stolen.begin[n]))return false;
 if(!n && !intel_bdw_stolen_scanout_plan(&stolen.facts[0],&stolen.plan))return refuse(19);
 if(!checkpoint(19) || !tick(stolen.begin[n]) || !readers(n))return false;
 for(unsigned r=0;r<3;r++) {
  stolen.hash[n][r]=14695981039346656037ull;
  stolen.vectors.pte[n][r]=stolen.pte[n][r];stolen.vectors.count[n][r]=stolen.plan.range[r].pages;
  for(unsigned i=0;i<stolen.plan.range[r].pages;i++) {
   uint32_t off=(uint32_t)(0x800000+stolen.plan.range[r].gpu/4096*8+8*i),high,low,again;
   if(!load(off+4,&high))return false;
   ++stolen.reads[n][r];
   if(!load(off,&low))return false;
   ++stolen.reads[n][r];
   if(!load(off+4,&again))return false;
   ++stolen.reads[n][r];
   if(high!=again)return refuse(20);
   uint64_t value=(uint64_t)high<<32|low;stolen.pte[n][r][i]=value;
   stolen.hash[n][r]=hash_word(stolen.hash[n][r],value);++stolen.count[n][r];
   if((i&63)==63 && !tick(stolen.begin[n]))return false;
  }
 }
 if(!unmap())return false;
 if(!checkpoint(20) || !tick(stolen.begin[n]) || !candidate_policy(n))return false;
 if(!checkpoint(11) || !tick(stolen.begin[n]))return false;
 if(!intel_window_handoff_revalidate(&stolen.source))return refuse(11);
 if(!identity() || !tick(stolen.begin[n]))return false;
 stolen.end[n]=stolen.last;return stolen.end[n]>stolen.begin[n] || refuse(18);
}
/* CPU-side preparation only. A reserved OS frame and absence of CPU aliases
 * do not transfer firmware memory or stop any GPU/DMA/display reader. */
static bool cpu_backing(void) {
 stolen.cpu.attempted=1;
 if(!tick(stolen.begin[1]))return false;
 stolen.cpu.begin=stolen.last;
 if(!identity() || !sole_root() || !intel_window_handoff_revalidate(&stolen.source))return refuse(29);
 const struct limine_memmap_response *m=memmap_request.response;
 if(!m || !m->entries || !m->entry_count || m->entry_count>4096)return refuse(29);
 uint64_t limit=1ull<<stolen.cache.cpu.physical_bits;
 for(uint64_t i=0;i<m->entry_count;i++) {
  const struct limine_memmap_entry *e=m->entries[i];
  if(!e || !e->length || e->base>=limit || e->length>limit-e->base)return refuse(29);
  bool ram=e->type==LIMINE_MEMMAP_USABLE || e->type==LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
   e->type==LIMINE_MEMMAP_ACPI_NVS || e->type==LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE || e->type==LIMINE_MEMMAP_EXECUTABLE_AND_MODULES;
  if(!ram)continue;
  unsigned n=stolen.cpu.ram_count,at=0;
  if(n==VMM_ALIAS_RAM_LIMIT)return refuse(29);
  while(at<n && stolen.cpu.ram[at].base<e->base)++at;
  if((at && stolen.cpu.ram[at-1].base+stolen.cpu.ram[at-1].length>e->base) ||
     (at<n && e->base+e->length>stolen.cpu.ram[at].base))return refuse(29);
  for(unsigned j=n;j>at;j--)stolen.cpu.ram[j]=stolen.cpu.ram[j-1];
  stolen.cpu.ram[at]=(struct vmm_alias_ram){e->base,e->length};++stolen.cpu.ram_count;
 }
 if(!stolen.cpu.ram_count)return refuse(29);
 struct vmm_physical_span spans[2];bool reserved=true;
 for(unsigned r=0;r<2;r++) {
  __typeof__(stolen.cpu.span[0]) *q=&stolen.cpu.span[r];
  q->physical=stolen.layout.physical[r+1];q->pages=stolen.plan.range[r+1].pages;
  spans[r]=(struct vmm_physical_span){q->physical,q->pages};
  for(unsigned page=0;page<q->pages;page++) {
   if(!(page&63) && (!identity() || !tick(stolen.begin[1])))return false;
   uint64_t physical=q->physical+4096ull*page,type=0;unsigned cover=0;
   for(uint64_t i=0;i<m->entry_count;i++) {
    const struct limine_memmap_entry *e=m->entries[i];
    if(e->base<=physical && e->length>=4096 && physical-e->base<=e->length-4096){++cover;type=e->type;}
   }
   if(cover>1)return refuse(29);
   if(!cover)++q->uncovered;
   else if(type==LIMINE_MEMMAP_RESERVED)++q->reserved;
   else if(type==LIMINE_MEMMAP_FRAMEBUFFER)++q->framebuffer;
   else ++q->other;
   if(pmm_frame_has_tag(physical,PMM_FRAME_RESERVED))++q->tagged;
   ++q->checked;++stolen.cpu.pmm_checks;
  }
  if(q->reserved!=q->pages || q->tagged!=q->pages)reserved=false;
 }
 if(!identity() || !tick(stolen.begin[1]))return false;
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 bool absent=vmm_diagnostic_kernel_physical_unmapped(stolen.root,spans,2,stolen.cache.cpu.physical_bits,
  stolen.cpu.ram,stolen.cpu.ram_count,&stolen.cpu.inventory,&stolen.cpu.detail);
#else
 uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
 bool absent=vmm_diagnostic_kernel_physical_unmapped(stolen.root,spans,2,stolen.cache.cpu.physical_bits,
  stolen.cpu.ram,stolen.cpu.ram_count,&stolen.cpu.inventory,&stolen.cpu.detail);
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 if(!absent && stolen.cpu.detail.walk.reason!=VMM_ALIAS_PRESENT)return refuse(29);
 stolen.cpu.unmapped=absent;
 if(!absent)stolen.cpu.inventory=stolen.cpu.detail.prefix;
 struct x86_cache_snapshot fresh;
 if(!identity() || !sole_root() || !intel_window_handoff_revalidate(&stolen.source) ||
    !intel_cache_probe_context(&fresh) || !x86_cache_equal(&fresh,&stolen.cache))return refuse(29);
 if(!tick(stolen.begin[1]))return false;
 stolen.cpu.end=stolen.last;stolen.end[1]=stolen.last;
 if(stolen.cpu.end<=stolen.cpu.begin)return refuse(18);
 stolen.cpu.admitted=reserved && absent;stolen.cpu.observed=1;
 return true;
}
/* Maintain CPU mapping exclusion in this original census/exclusion lifetime.
 * No native hardware right or target access is issued by this guard. */
static bool cpu_guard_operation(unsigned phase,const struct vmm_physical_span *pair) {
#ifndef SRVROS_INTEL_STOLEN_HOST_TEST
 uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
 bool ok;
 if(phase==0 && stolen.range.selected) {
  uint64_t offset=(uint64_t)stolen.source.first*4096;
  uint64_t table_offset=((uint64_t)stolen.source.first*8) & ~4095ull;
  const struct bdw_stolen_plan *plan=&stolen.layout.plan;
  ok=stolen.layout.classified && plan->gtt_end>plan->gtt_begin &&
     table_offset<plan->gtt_end-plan->gtt_begin && stolen.source.mmio<=UINT64_MAX-0x800000-table_offset &&
     offset==stolen.source.fence_end && offset<=stolen.source.aperture_bytes-4096 &&
     stolen.source.aperture<=UINT64_MAX-offset &&
     (!stolen.import.submission || (stolen.source.aperture_bytes>=5*4096 &&
       offset<=stolen.source.aperture_bytes-5*4096 && stolen.source.aperture+offset<=UINT64_MAX-4*4096));
  if(ok) {
   uint64_t aperture=stolen.source.aperture+offset, backing=plan->gtt_begin+table_offset;
   uint64_t reg=stolen.source.mmio+0x800000+table_offset;
   if(stolen.import.submission) {
    /* One owner covers the five aperture pages and every submission port. */
    uint64_t ports[BDW_SUBMISSION_PORT_COUNT+4];
    for(unsigned i=0;i<BDW_SUBMISSION_PORT_COUNT;i++)ports[i]=stolen.source.mmio+intel_bdw_submission_ports[i];
    for(unsigned i=0;i<4;i++)ports[BDW_SUBMISSION_PORT_COUNT+i]=aperture+(i+1)*4096;
    ok=vmm_reserved_guard_begin_submission_window(&stolen.guard.owner,stolen.root,pair,2,stolen.cache.cpu.physical_bits,
       stolen.cpu.ram,stolen.cpu.ram_count,aperture,backing,reg,ports,BDW_SUBMISSION_PORT_COUNT+4,&stolen.guard.inventory[0],&stolen.guard.detail);
    if(ok)stolen.import.ports=BDW_SUBMISSION_PORT_COUNT;
   } else if(stolen.import.selected) {
    /* Make the candidate's aperture and GGTT backing the primary guard
     * window. Keep the inherited aperture, table backing and all eight CPU
     * submission/WiDi ports excluded in the very same VMM owner. */
    const uint64_t page_offset=(uint64_t)OWNED_PAGE_SLOT*4096;
    const uint64_t table_page=(8ull*OWNED_PAGE_SLOT)&~4095ull;
    uint64_t ports[BDW_SUBMISSION_PORT_COUNT+2];
    for(unsigned i=0;i<BDW_SUBMISSION_PORT_COUNT;i++)ports[i]=stolen.source.mmio+intel_bdw_submission_ports[i];
    ports[BDW_SUBMISSION_PORT_COUNT]=aperture;
    ports[BDW_SUBMISSION_PORT_COUNT+1]=backing;
    ok=page_offset<=stolen.source.aperture_bytes-4096 && stolen.source.aperture<=UINT64_MAX-page_offset &&
       plan->gtt_end-plan->gtt_begin>=table_page+4096 && stolen.source.mmio<=UINT64_MAX-0x800000-table_page &&
       pmm_frame_has_tag(backing,PMM_FRAME_RESERVED);
    if(ok) {
     uint64_t target=stolen.source.aperture+page_offset, target_backing=plan->gtt_begin+table_page;
     uint64_t target_reg=stolen.source.mmio+0x800000+table_page;
     ok=vmm_reserved_guard_begin_submission_window(&stolen.guard.owner,stolen.root,pair,2,stolen.cache.cpu.physical_bits,
        stolen.cpu.ram,stolen.cpu.ram_count,target,target_backing,target_reg,ports,BDW_SUBMISSION_PORT_COUNT+2,
        &stolen.guard.inventory[0],&stolen.guard.detail);
     if(ok) {
      stolen.owned_page.aperture=target;stolen.owned_page.table_backing=target_backing;
      stolen.owned_page.register_page=target_reg;
     }
    }
   } else ok=vmm_reserved_guard_begin_register_window(&stolen.guard.owner,stolen.root,pair,2,stolen.cache.cpu.physical_bits,
       stolen.cpu.ram,stolen.cpu.ram_count,aperture,backing,reg,&stolen.guard.inventory[0],&stolen.guard.detail);
  }
  if(!ok && stolen.guard.detail.walk.reason==VMM_ALIAS_PRESENT) {
   const struct vmm_unmapped_detail *d=&stolen.guard.detail;
   uint64_t page=stolen.source.aperture+offset;
   stolen.range.aperture_alias=page>=d->physical && page-d->physical<d->bytes;
  }
 } else ok=phase==0?vmm_reserved_guard_begin(&stolen.guard.owner,stolen.root,pair,2,stolen.cache.cpu.physical_bits,
  stolen.cpu.ram,stolen.cpu.ram_count,&stolen.guard.inventory[0],&stolen.guard.detail):
  phase==1?vmm_reserved_guard_check(&stolen.guard.owner,&stolen.guard.inventory[1],&stolen.guard.detail):
  vmm_reserved_guard_close(&stolen.guard.owner,&stolen.guard.inventory[2],&stolen.guard.detail);
#ifndef SRVROS_INTEL_STOLEN_HOST_TEST
 if(flags&512)__asm__ volatile("sti":::"memory");
#endif
 return ok;
}

/* Keep the firmware endpoint and requester observation in the same CPU
 * guard lifetime as the reader snapshot. Config words are observational.
 * Bus mastering stays enabled. The exact captured capability chain admits
 * each successive read, and no OpRegion backing or mailbox is accessed. */
static bool handoff_config(unsigned sample) {
 uint32_t *words=stolen.handoff.config[sample];
 for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS+2;i++) {
  if(!tick(stolen.begin[1]) || !identity())return false;
  uint8_t offset=i<INTEL_PCI_REQUESTER_WORDS ? intel_pci_requester_offsets[i] :
     i==INTEL_PCI_REQUESTER_WORDS ? 0xfc : 0xe8;
  uint32_t raw=pci_read_config32(stolen.gpu,offset);
  if(raw==UINT32_MAX)return refuse(31);
  words[i]=i==INTEL_PCI_REQUESTER_WORDS+1 ? raw&0xffffu : raw;
  ++stolen.handoff.config_reads[sample];
  if(i<INTEL_PCI_REQUESTER_WORDS && !intel_pci_requester_prefix(words,i+1))return refuse(31);
 }
 return true;
}

/* PIPE_FRMCOUNT_A shares the active plane's already admitted register page.
 * Observe frame progress inside the aperture guard, using the original pass
 * clock and a bounded poll even if that clock stops advancing. */
static bool sample_frame(bool closing,uint32_t frames[2],uint32_t *reads) {
 uint64_t begin=stolen.last;
 for(unsigned i=0;i<4096;i++) {
  if(!tick(stolen.begin[1]) || stolen.last-begin>50000)return refuse(32);
  uint32_t frame;
  if(!load(0x70040,&frame))return false;
  ++*reads;
  frames[closing]=frame;
  if(frame==UINT32_MAX)return refuse(32);
  if(!closing)return true;
  uint32_t advanced=frame-frames[0];
  if(advanced)return advanced<=240 || refuse(32);
 }
 return refuse(32);
}

static bool handoff_readers(bool closing) {
 const uint32_t auxiliary[]={0x43208,0x6f800,0x6f840};
 for(unsigned i=0;i<42;i++) {
  if(!tick(stolen.begin[1]))return false;
  unsigned plane_word=i>=3?i-3:0;
  uint32_t offset=i<3?auxiliary[i]:
    0x70000+0x1000*(plane_word/INTEL_READER_WORDS)+intel_reader_offsets[plane_word%INTEL_READER_WORDS];
  uint32_t raw;
  if(!load(offset,&raw))return false;
  if(raw==UINT32_MAX)return refuse(31);
  if(stolen.range.selected && i>=3) {
   unsigned pipe=plane_word/INTEL_READER_WORDS, word=plane_word%INTEL_READER_WORDS;
   if(raw!=stolen.reader[1][pipe][word])return refuse(32);
   /* Programmed and live primary-A addresses must still select boot offset zero. */
   if(!pipe && word>=1 && word<=4 && (raw&0xfffff000u))return refuse(32);
  }
  if(stolen.range.selected && i==3) {
   if(!(raw&(1u<<31)) || !sample_frame(closing,stolen.range.frame,&stolen.range.frame_reads))return refuse(32);
  }
  if(closing) {
   if(raw!=stolen.handoff.readers[i])return refuse(32);
   ++stolen.range.closing_readers;
  } else {
   stolen.handoff.readers[i]=raw;
   ++stolen.handoff.reader_reads;
  }
 }
 return true;
}

/* Read the complete original translation interval for an OS-backed X-tiled
 * surface. The source is usable only inside this display/VMM guard. The
 * inherited 16-entry observation is the anchor, never the range length. */
static bool range_pair(uint32_t offset,uint64_t *value) {
 uint32_t hi,lo,again;
 if(!load(offset+4,&hi) || !load(offset,&lo) || !load(offset+4,&again))return false;
 if(hi!=again)return refuse(20);
 *value=((uint64_t)hi<<32)|lo;
 return true;
}

static bool range_fences(void) {
 for(unsigned i=0;i<32;i++) {
  uint64_t raw;
  if(!tick(stolen.begin[1]) || !range_pair(0x100000u+8*i,&raw))return false;
  if(raw!=(i==0?0x021ff02f00000001ull:0))return refuse(32);
  ++stolen.range.fences;
 }
 return true;
}

/* Only the inherited primary on A may fetch at these checkpoints. A cursor,
 * sprite, queued surface or second primary requires its own footprint import. */
static bool private_display_clear(void) {
 for(unsigned p=0;p<3;p++) {
  const uint32_t *w=&stolen.handoff.readers[3+INTEL_READER_WORDS*p];
  if((w[0]>>31)!=(unsigned)(p==0) || (w[5]&63) || (w[8]>>31))return false;
  for(unsigned i=1;i<=4;i++)if(w[i]&0xfffff000u)return false;
 }
 return true;
}

/* Read up to 64 PTEs from one RO UC page per guarded batch. A populated
 * scratch entry, an encoded non-present entry and an unstable read are all
 * ineligible. No access through the aperture or the sampled backing. */
static bool private_batch(unsigned first,unsigned count,uint64_t *values) {
 uint32_t offset=0x800000u+first*8;
 if(!count || count>64 || ((first&511)+count)>512 || !tick(stolen.begin[1]) ||
    !map(offset&~4095u) || !alias())return false;
 for(unsigned i=0;i<count;i++,offset+=8) {
  uint32_t hi,lo,again;
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
  if(!intel_stolen_test_read(offset+4,&hi) || !intel_stolen_test_read(offset,&lo) ||
     !intel_stolen_test_read(offset+4,&again))return refuse(17);
#else
  volatile const uint32_t *pte=(volatile const uint32_t *)(STOLEN_VA+(offset&4095));
  hi=pte[1];lo=pte[0];again=pte[1];
#endif
  if(hi!=again || (hi==UINT32_MAX && lo==UINT32_MAX))return refuse(20);
  values[i]=((uint64_t)hi<<32)|lo;
 }
 if(!alias() || !tick(stolen.begin[1]))return false;
 if(stolen.private_range.scanning)++stolen.private_range.batches;
 return true;
}

/* A display-sized window in GPU address space, outside the CPU aperture.
 * Compare both complete vectors, including occupied entries. Never read
 * through the translations or treat a common scratch PTE as available. */
static bool observe_gpu_surface(unsigned pages) {
 const struct bdw_stolen_facts *f=&stolen.facts[1];
 unsigned first=f->aperture_bytes/4096;
 if(first!=SCANOUT_SPACE_PAGES || !pages || pages>SCANOUT_TRANSFER_PAGES ||
    first+pages>f->ggtt_bytes/8 || !private_display_clear() ||
    !intel_window_handoff_revalidate(&stolen.source))return refuse(32);
 stolen.gpu_range.first=first;
 stolen.gpu_range.pages=pages;
 struct scanout_space_range span={first,pages};
 for(unsigned pass=0;pass<2;pass++) {
  if(pass && !stolen.gpu_range.occupied) {
   stolen.gpu_range.claimed=scanout_interval_claim_gpu_only(&stolen.gpu_range.lease,
      stolen.source.mmio,stolen.source.aperture,f->aperture_bytes,f->fence_end,f->ggtt_bytes,span);
   stolen.gpu_range.conflict=!stolen.gpu_range.claimed;
  }
  stolen.gpu_range.reading=true;
  for(unsigned i=0;i<pages;i+=64) {
   uint64_t batch[64];
   unsigned count=pages-i<64?pages-i:64;
   if(!private_batch(first+i,count,batch))goto fail;
   for(unsigned j=0;j<count;j++) {
    uint64_t raw=batch[j];
    if(!pass) {
     stolen.gpu_range.original[i+j]=raw;
     ++stolen.gpu_range.sampled;
     stolen.gpu_range.occupied+=raw!=0;
     stolen.gpu_range.present+=raw&1;
     stolen.gpu_range.same_first+=raw==stolen.gpu_range.original[0];
    } else {
     if(raw!=stolen.gpu_range.original[i+j])goto fail;
     ++stolen.gpu_range.matched;
    }
   }
  }
  if(!unmap())goto fail;
  stolen.gpu_range.reading=false;
  if(!intel_window_handoff_revalidate(&stolen.source) ||
     (stolen.gpu_range.claimed && !scanout_interval_held(&stolen.gpu_range.lease,
      stolen.source.mmio,stolen.source.aperture,f->aperture_bytes,f->fence_end,span)))goto fail;
 }
 return true;
fail:
 stolen.gpu_range.reading=false;
 return refuse(32);
}

static bool reserve_private_range(unsigned pages) {
 struct scanout_interval *lease=&stolen.private_range.lease;
 const struct bdw_stolen_range *last=&stolen.plan.range[2];
 /* Start at the first GGTT register page beyond both firmware candidates. */
 uint64_t after=(last->gpu+last->bytes)/4096;
 if(after>SCANOUT_SPACE_PAGES || !pages || pages>SCANOUT_SPACE_PAGES ||
    stolen.source.aperture_bytes!=4096ull*SCANOUT_SPACE_PAGES)return false;
 unsigned begin=(after+511u)&~511u;
 stolen.private_range.begin=begin;
 if(!private_display_clear()){stolen.private_range.reader_conflict=true;return true;}
 if(!unmap())return false;
 unsigned maps_before=stolen.maps, run=0;
 bool ok=false;
 uint64_t values[64];
 stolen.private_range.scanning=true;
 for(unsigned slot=begin;slot<SCANOUT_SPACE_PAGES && !stolen.private_range.claimed;slot+=64) {
  if(!private_batch(slot,64,values))goto done;
  for(unsigned i=0;i<64;i++) {
   ++stolen.private_range.scanned;
   if(values[i]){++stolen.private_range.nonzero;run=0;continue;}
   unsigned pos=slot+i;
   if(!run && (pos&63u))continue;
   if(++run<pages)continue;
   unsigned first=pos+1-pages;
   struct scanout_space_range range={first,pages};
   if(scanout_interval_claim(lease,stolen.source.mmio,stolen.source.aperture,
        stolen.source.aperture_bytes,stolen.source.fence_end,range)){
    stolen.private_range.first=first;stolen.private_range.claimed=true;break;
   }
   /* Slide to the next 256 KiB aligned start within the same zero run. */
   run=pages>=64?pages-64:0;
  }
 }
 if(!stolen.private_range.claimed){stolen.private_range.exhausted=true;ok=true;goto done;}
 for(unsigned i=0;i<pages;) {
  unsigned count=pages-i<64?pages-i:64;
  if(!scanout_interval_held(lease,stolen.source.mmio,stolen.source.aperture,stolen.source.aperture_bytes,
      stolen.source.fence_end,(struct scanout_space_range){stolen.private_range.first,pages}) ||
     !private_batch(stolen.private_range.first+i,count,values))goto done;
  for(unsigned j=0;j<count;j++){if(values[j])goto done;++stolen.private_range.verified;}
  i+=count;
 }
 ok=true;
done:
 if(!unmap())ok=false;
 stolen.private_range.scanning=false;
 stolen.private_range.maps=stolen.maps-maps_before;
 return ok || refuse(32);
}

/* The VMM validates the active root and guard with local IRQs masked.
 * The guard remains pinned across the scan. Only this observation needs CLI. */
static bool aperture_guard_held(void) {
 uint64_t flags;
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 flags=intel_stolen_test_irq_save();
#else
 __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
 uint64_t inherited=stolen.source.aperture+stolen.source.fence_end;
 bool held;
 if(stolen.owned_page.aperture) {
  held=vmm_reserved_guard_register_window_held(&stolen.guard.owner,stolen.root,stolen.owned_page.aperture,
              stolen.owned_page.table_backing,stolen.owned_page.register_page) &&
       vmm_reserved_guard_excludes_io_page(&stolen.guard.owner,stolen.root,inherited) &&
       vmm_reserved_guard_excludes_io_page(&stolen.guard.owner,stolen.root,
          stolen.layout.plan.gtt_begin+((stolen.source.first*8ull)&~4095ull));
  for(unsigned i=0;held && i<stolen.owned_page.reader_pages;i++)
   held=vmm_reserved_guard_excludes_io_page(&stolen.guard.owner,stolen.root,stolen.source.mmio+0x71000+4096*i);
  for(unsigned i=0;held && i<BDW_SUBMISSION_PORT_COUNT;i++)
   held=vmm_reserved_guard_excludes_io_page(&stolen.guard.owner,stolen.root,stolen.source.mmio+intel_bdw_submission_ports[i]);
 } else held=vmm_reserved_guard_io_page_held(&stolen.guard.owner,stolen.root,inherited);
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 intel_stolen_test_irq_restore(flags);
#else
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 return held;
}

/* Both firmware candidates have two classified, complete PTE vectors. Keep
 * their contiguous GPU span occupied in the common allocator until reboot. */
static bool import_firmware(void) {
 uint32_t end=intel_bdw_stolen_import_end(&stolen.layout,stolen.source.fence_end,stolen.source.aperture_bytes);
 uint32_t first=(uint32_t)(stolen.source.fence_end/4096);
 if(!end || !private_display_clear())return false;
 struct scanout_space_range pages={first,end-first};
 stolen.import.imported=scanout_interval_import_firmware(&stolen.import.firmware,stolen.source.mmio,
     stolen.source.aperture,stolen.source.aperture_bytes,stolen.source.fence_end,pages);
 return stolen.import.imported;
}

static bool check_import_conflict(const struct scanout_transfer_manifest *m) {
 struct scanout_interval other={0};
 if(scanout_interval_claim(&other,m->mmio,m->aperture,m->aperture_bytes,m->fence_end,
                           (struct scanout_space_range){m->first,1})){
  scanout_interval_release(&other);return false;
 }
 ++stolen.import.blocked;
 return true;
}

/* Read one candidate under the native display/CPU guard. A nonzero invalid
 * entry is occupied too. No CPU mapping of the backing or GGTT write. */
static bool command_occupancy(unsigned first, uint32_t *mask) {
 uint64_t values[COMMAND_PAGES];
 if(first<stolen.facts[1].aperture_bytes/4096 ||
    first+COMMAND_PAGES>stolen.facts[1].ggtt_bytes/8)return false;
 stolen.import.command_page=0x800000u+8*first;
 bool ok=private_batch(first,COMMAND_PAGES,values);
 stolen.import.command_page=0;
 if(!unmap() || !ok)return false;
 *mask=0;
 for(unsigned i=0;i<COMMAND_PAGES;i++)if(values[i])*mask|=1u<<i;
 return true;
}

/* First four pages are the BCS hardware image. RCS executes from page four
 * and saves the old L3 policy in page five. Keep metadata out of both. */
#define TAIL_GLOBAL_PAGES BDW_BLITTER_GLOBAL_PAGES
#define TAIL_DMA_PAGES (TAIL_GLOBAL_PAGES+BDW_PRIVATE_PAGES)
static bool command_frames_owned(void)
{
 uint64_t base=stolen.command_tail.dma_base;
 if(!base || stolen.command_tail.dma_freed)return false;
 for(unsigned i=0;i<TAIL_DMA_PAGES;i++)
  if(!pmm_frame_has_tag(base+4096ull*i,PMM_FRAME_DMA))return false;
 return true;
}
static bool command_ram_alias(uint64_t base)
{
 struct vmm_alias_observation alias;
 uint64_t va=(uint64_t)pmm_phys_to_virt(base);
 if(!va || (va&4095) || base<0x100000 || base>0x100000000ull-4096*TAIL_DMA_PAGES ||
    !command_frames_owned() || !stolen.layout.classified ||
    (base<stolen.plan.data_end && base+4096*TAIL_DMA_PAGES>stolen.plan.gtt_begin))return false;
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 uint64_t flags=intel_stolen_test_irq_save();
#else
 uint64_t flags;__asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
 bool ok=vmm_diagnostic_kernel_alias(stolen.root,va,base,TAIL_DMA_PAGES,
                                     stolen.cache.cpu.physical_bits,&alias,NULL);
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 intel_stolen_test_irq_restore(flags);
#else
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 unsigned mtrr,pat,effective,checks;
 return ok && alias.pages==TAIL_DMA_PAGES && alias.writable && !alias.user &&
        x86_cache_range(&stolen.cache,base,TAIL_DMA_PAGES,alias.pat_index,&mtrr,&pat,&effective,&checks) &&
        mtrr==6 && pat==6 && effective==6;
}
static bool prepare_command_frames(const struct scanout_transfer_manifest *m,const struct intel_bdw_bcs_context_config *bcs)
{
 if(stolen.command_tail.dma_base)return false;
#ifndef SRVROS_INTEL_STOLEN_HOST_TEST
 uint32_t eax=1,ebx,ecx,edx;
 __asm__ volatile("cpuid":"+a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));
 if(!(edx&(1u<<19)) || ((ebx>>8)&255u)!=8)return false;
#endif
 uint64_t base=pmm_alloc_frames_tagged(TAIL_DMA_PAGES,PMM_FRAME_DMA);
 stolen.command_tail.dma_base=base;
 if(!base || !command_ram_alias(base))return false;
 for(unsigned i=0;i<TAIL_GLOBAL_PAGES;i++)
  if(!intel_bdw_ggtt_plan_entry(m,m->first+i,base+i*4096,&stolen.command_tail.planned[i]))return false;
 volatile uint32_t *ram=pmm_phys_to_virt(base);
 for(unsigned i=0;i<(TAIL_GLOBAL_PAGES-4)*1024;i++)
  ram[BDW_BCS_IMAGE_WORDS+i]=i<32 ? stolen.command_tail.rcs.words[i] : 0;
 uint64_t vm_base=base+TAIL_GLOBAL_PAGES*4096;
 if(!intel_bdw_blitter_prepare(&stolen.command_tail.image,bcs,ram,pmm_phys_to_virt(vm_base),
      base,stolen.command_tail.token.epoch))return false;
 stolen.command_tail.descriptor=stolen.command_tail.image.descriptor;
 stolen.command_tail.pdp0=vm_base+BDW_PRIVATE_PD*4096;
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 for(unsigned i=0;i<TAIL_DMA_PAGES*4096/64;i++) {
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
  intel_stolen_test_flush((volatile unsigned char *)ram+i*64,i);
#else
  __asm__ volatile("clflush (%0)"::"r"((volatile unsigned char *)ram+i*64):"memory");
#endif
  ++stolen.command_tail.flush_lines;
 }
 __atomic_thread_fence(__ATOMIC_SEQ_CST);
 return true;
}
static bool check_command_frames(const struct scanout_transfer_manifest *m)
{
 uint64_t base=stolen.command_tail.dma_base;
 if(!command_ram_alias(base) || stolen.command_tail.flush_lines!=TAIL_DMA_PAGES*64)return false;
 for(unsigned i=0;i<TAIL_GLOBAL_PAGES;i++) {
  struct intel_bdw_ggtt_page expected;
  const struct intel_bdw_ggtt_page *saved=&stolen.command_tail.planned[i];
  if(!intel_bdw_ggtt_plan_entry(m,m->first+i,base+i*4096,&expected) ||
     expected.replacement!=saved->replacement || expected.original!=saved->original ||
     expected.gpu_address!=saved->gpu_address || expected.mmio_offset!=saved->mmio_offset ||
     expected.index!=saved->index || expected.physical!=saved->physical)return false;
 }
 volatile const uint32_t *ram=pmm_phys_to_virt(base);
 const struct bdw_blitter_image *image=&stolen.command_tail.image;
 if(image->physical!=base || image->global!=ram || image->epoch!=stolen.command_tail.token.epoch ||
    image->descriptor!=stolen.command_tail.descriptor ||
    image->config.ring_gpu!=(uint64_t)m->first*4096 || image->config.aperture_bytes!=m->aperture_bytes ||
    image->config.fence_begin!=m->fence_begin || image->config.fence_end!=m->fence_end ||
    image->config.context_id!=1)return false;
 for(unsigned i=0;i<(TAIL_GLOBAL_PAGES-4)*1024;i++)
  if(ram[BDW_BCS_IMAGE_WORDS+i]!=(i<32 ? stolen.command_tail.rcs.words[i] : 0))return false;
 uint64_t vm_base=base+TAIL_GLOBAL_PAGES*4096;
 return stolen.command_tail.pdp0==vm_base+BDW_PRIVATE_PD*4096 &&
        image->vm==pmm_phys_to_virt(vm_base) && intel_bdw_blitter_verify(image);
}

/* Borrow only the alignment padding in the second imported buffer. Keep the
 * complete tiled image and its 128-PTE fetch allowance outside this interval.
 * The lender pins the original translations; the lease alone permits no I/O. */
static bool borrow_command_tail(struct scanout_transfer_manifest source) {
 const struct bdw_stolen_range *buffer=&stolen.layout.plan.range[2];
 if(!stolen.import.imported || !stolen.layout.classified || !private_display_clear() ||
    buffer->pages<COMMAND_PAGES || source.pages>buffer->pages-COMMAND_PAGES ||
    buffer->pages!=stolen.count[1][2] || buffer->pages!=stolen.count[0][2])return false;
 unsigned offset=buffer->pages-COMMAND_PAGES;
 source.first=buffer->gpu/4096+offset;
 source.pages=COMMAND_PAGES;
 source.original=&stolen.pte[1][2][offset];
 if(scanout_transfer_admission(&source)!=TRANSFER_ASSURANCE ||
    !scanout_transfer_reserve_imported(&stolen.command_tail.transfer,&stolen.range.pool,&stolen.exclusion,
      &source,&stolen.import.firmware,&stolen.command_tail.token))return false;
 stolen.command_tail.manifest=source;
 uint64_t gpu=(uint64_t)source.first*4096;
 const struct intel_bdw_bcs_context_config bcs={
  .ring_gpu=gpu,.context_gpu=gpu+4096,.completion_gpu=gpu+3*4096,
  .aperture_bytes=source.aperture_bytes,.fence_begin=source.fence_begin,.fence_end=source.fence_end,
  .nonce=stolen.command_tail.token.epoch,.context_id=1,
  .vendor=stolen.facts[1].gpu&0xffff,.device=stolen.facts[1].gpu>>16};
 return intel_bdw_rcs_indirect_build(source.first,source.pages,&stolen.command_tail.rcs) &&
        prepare_command_frames(&source,&bcs);
}

static bool check_command_tail(void) {
 struct scanout_transfer_manifest fresh=stolen.command_tail.manifest;
 struct scanout_interval other={0};
 uint64_t entries[COMMAND_PAGES];
 bool observed=private_batch(fresh.first,COMMAND_PAGES,entries);
 bool unmapped=unmap();
 fresh.original=entries;
 /* A lost or changed GGTT vector can expose the DMA allocation to a GPU
  * reader. Closing a software reservation cannot retire that reader. */
 if(!observed || !unmapped ||
    !scanout_transfer_reservation_matches(&stolen.command_tail.transfer,&stolen.command_tail.token,&fresh)) {
  stolen.uncertain=true;
  return refuse(34);
 }
 if(!intel_bdw_rcs_indirect_verify(fresh.first,fresh.pages,&stolen.command_tail.rcs) ||
    !check_command_frames(&fresh))
  return false;
 ++stolen.command_tail.samples;
 /* A second child of the same firmware lender must also be excluded. */
 struct scanout_space_range last={fresh.first+fresh.pages-1,1};
 if(scanout_interval_borrow(&other,&stolen.import.firmware,fresh.mmio,fresh.aperture,
      fresh.aperture_bytes,fresh.fence_end,last)) {
  scanout_interval_release(&other);
  return false;
 }
 ++stolen.command_tail.conflicts;
 return true;
}

static bool reserve_commands(struct scanout_transfer_manifest source) {
 const struct bdw_stolen_facts *f=&stolen.facts[1];
 for(unsigned i=0;i<3;i++) {
  unsigned first=command_slots[i];
  if(!command_occupancy(first,&stolen.import.occupied[i]))return false;
  ++stolen.import.tried;
  if(stolen.import.occupied[i])continue;
  if(!scanout_interval_claim_gpu_only(&stolen.import.commands,stolen.source.mmio,stolen.source.aperture,
     f->aperture_bytes,f->fence_end,f->ggtt_bytes,(struct scanout_space_range){first,COMMAND_PAGES}))return false;
  stolen.import.first=first;
  return true;
 }
 return borrow_command_tail(source);
}

static bool check_command_lease(void) {
 if(!stolen.import.first)return check_command_tail();
 struct scanout_space_range pages={stolen.import.first,COMMAND_PAGES};
 uint32_t mask;
 if(!scanout_interval_held(&stolen.import.commands,stolen.source.mmio,stolen.source.aperture,
     stolen.source.aperture_bytes,stolen.source.fence_end,pages) ||
    !command_occupancy(pages.first,&mask) || mask)return false;
 ++stolen.import.checked;
 return true;
}

static bool check_video_fuse(unsigned i) {
 uint32_t *strap=&stolen.import.strap[i];
 return tick(stolen.begin[1]) && load(0x42014,strap) && unmap() && intel_bdw_air_wireless_video_disabled(*strap) &&
        (!i || *strap==stolen.import.strap[0]);
}

static bool inactive_pipe_sample(void) {
 for(unsigned pipe=1;pipe<3;pipe++) {
  for(unsigned i=0;i<INTEL_READER_WORDS;i++) {
   uint32_t word;
   if(!tick(stolen.begin[1]) || !load(0x70000+4096*pipe+intel_reader_offsets[i],&word) ||
      word!=stolen.handoff.readers[3+pipe*INTEL_READER_WORDS+i]) {
    unmap();return refuse(32);
   }
   ++stolen.owned_page.reader_words;
  }
 }
 return unmap();
}
static bool hold_inactive_pipes(void) {
 if(!inactive_pipe_sample())return false;
 uint64_t flags;
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 flags=intel_stolen_test_irq_save();
#else
 __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
 bool held=vmm_reserved_guard_exclude_io_range(&stolen.guard.owner,stolen.root,stolen.source.mmio+0x71000,2);
#ifdef SRVROS_INTEL_STOLEN_HOST_TEST
 intel_stolen_test_irq_restore(flags);
#else
 if(flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
 if(held)stolen.owned_page.reader_pages=2;
 return held || refuse(32);
}
static bool owned_plane_sample(bool closing) {
 const unsigned indices[]={0,1,2,3,4,5,8};
 for(unsigned n=0;n<sizeof(indices)/sizeof(indices[0]);n++) {
  unsigned i=indices[n];
  uint32_t word;
  if(!load(0x70000+intel_reader_offsets[i],&word) || word!=stolen.handoff.readers[3+i])
   return refuse(32);
 }
 return sample_frame(closing,stolen.owned_page.frame,&stolen.owned_page.frame_reads) && unmap();
}

/* The driver can reserve an address while its original mapping is populated.
 * Hold only a software reservation until a source can grant hardware access. */
static bool observe_owned_page(struct scanout_transfer_manifest m, unsigned sample) {
 const struct scanout_interval *fw=&stolen.import.firmware;
 uint32_t end=intel_bdw_stolen_import_end(&stolen.layout,m.fence_end,m.aperture_bytes);
 if(sample>1 || sample!=stolen.owned_page.samples || !end || end>OWNED_PAGE_SLOT-1 ||
    OWNED_PAGE_SLOT+1>=m.aperture_bytes/4096 || !fw->firmware ||
    (uint64_t)fw->pages.first+fw->pages.pages!=end ||
    !scanout_interval_held(fw,m.mmio,m.aperture,m.aperture_bytes,m.fence_end,fw->pages) ||
    !private_display_clear() || !aperture_guard_held())return false;
 if(!intel_cache_probe_owned_pte_uc(stolen.owned_page.register_page) ||
    (!sample && (!hold_inactive_pipes() || !owned_plane_sample(false))))return false;
 uint64_t *entries=stolen.owned_page.entries[sample];
 stolen.owned_page.reading=true;
 bool read=private_batch(OWNED_PAGE_SLOT-1,3,entries);
 stolen.owned_page.reading=false;
 if(!unmap() || !read)return false;
 ++stolen.owned_page.samples;
 m.first=OWNED_PAGE_SLOT;m.pages=1;m.original=&entries[1];
 if(scanout_transfer_admission(&m)!=TRANSFER_ASSURANCE)return false;
 if(!sample && !scanout_transfer_reserve(&stolen.owned_page.transfer,&stolen.range.pool,
     &stolen.exclusion,&m,&stolen.owned_page.token))return false;
 if(!scanout_transfer_reservation_matches(&stolen.owned_page.transfer,&stolen.owned_page.token,&m) ||
    (sample && !equal_bytes(entries,stolen.owned_page.entries[0],3*sizeof(*entries))))return false;
 struct scanout_interval other={0};
 if(scanout_interval_claim(&other,m.mmio,m.aperture,m.aperture_bytes,m.fence_end,
                           (struct scanout_space_range){m.first,1})) {
  scanout_interval_release(&other);return false;
 }
 ++stolen.owned_page.conflicts;
 if((sample && !owned_plane_sample(true)) || !aperture_guard_held())return false;
 ++stolen.owned_page.guard_checks;
 return true;
}

static bool check_imported_page(struct scanout_transfer_manifest m) {
 if(!scanout_transfer_close(&stolen.range.transfer,&stolen.range.token))return false;
 m.pages=1;
 if(!check_import_conflict(&m) ||
    !scanout_transfer_reserve_imported(&stolen.import.page,&stolen.range.pool,&stolen.exclusion,
                                       &m,&stolen.import.firmware,&stolen.import.token))return false;
 for(unsigned i=0;i<2;i++) {
  if(!tick(stolen.begin[1]) || !aperture_guard_held() ||
     !scanout_transfer_reservation_matches(&stolen.import.page,&stolen.import.token,&m) ||
     !check_video_fuse(i) || (stolen.import.submission && !check_command_lease()) ||
     !range_pair(0x800000u+m.first*8,&stolen.import.observed[i]))return false;
  ++stolen.import.samples;
  if(stolen.import.observed[i]!=m.original[0] || !unmap())return false;
  if(!stolen.import.submission && !observe_owned_page(m,i))return false;
  if(!i && (!check_import_conflict(&m) || !intel_window_handoff_revalidate(&stolen.source)))return false;
 }
 /* The software lender cannot grant permission to write the live PTE. */
 if(scanout_transfer_admission(&m)!=TRANSFER_ASSURANCE ||
    !scanout_transfer_close(&stolen.import.page,&stolen.import.token))return false;
 stolen.import.returned=true;
 if(stolen.command_tail.token.object) {
  if(!check_command_tail() || !scanout_transfer_close(&stolen.command_tail.transfer,&stolen.command_tail.token))
   return false;
  stolen.command_tail.returned=true;
 }
 if(!check_import_conflict(&m))return false;
 if(stolen.owned_page.token.object) {
  if(!aperture_guard_held() || !scanout_transfer_close(&stolen.owned_page.transfer,&stolen.owned_page.token))return false;
  stolen.owned_page.returned=true;
  ++stolen.owned_page.guard_checks;
 }
 return true;
}

static bool observe_scanout_range(void) {
 const struct bdw_stolen_range *candidate=&stolen.layout.plan.range[1];
 if(!stolen.layout.classified || !stolen.guard.checked || stolen.guard.closed ||
    !stolen.guard.owner.epoch || !stolen.exclusion.epoch || !identity() ||
    !sole_root())return refuse(32);
 if(!intel_window_handoff_revalidate(&stolen.source))return refuse(11);
 if(!aperture_guard_held())return refuse(33);
 stolen.range.excluded_aperture_page=stolen.source.aperture+stolen.source.fence_end;
 if(!scanout_interval_protect_boot_framebuffer(stolen.source.mmio,stolen.source.aperture,
      stolen.source.aperture_bytes,stolen.source.fence_end))return refuse(32);
 stolen.range.protected_pages=stolen.source.fence_end/4096;
 /* Profile admission above fixes width, height and stride. X tiling rounds
  * rows to eight. The 128 tail translations must also have recoverable PTEs. */
 struct scanout_memory_layout image;
 if(!scanout_memory_layout(stolen.profile.width,stolen.profile.height,stolen.profile.pitch,true,&image))return refuse(32);
 uint32_t pages=image.pages+128;
 if(candidate->gpu!=(uint64_t)stolen.source.first*4096 ||
    candidate->gpu<stolen.source.fence_end || pages>candidate->pages ||
    pages>SCANOUT_TRANSFER_PAGES || pages>stolen.count[0][1] || pages>stolen.count[1][1])return refuse(32);
 /* This source does not implement firmware/engine/DMA/reader/cache transfer.
  * Keep the complete original vector private to the current observation. */
 const struct scanout_transfer_manifest interval={
  .mmio=stolen.source.mmio,.aperture=stolen.source.aperture,.aperture_bytes=stolen.source.aperture_bytes,
  .fence_begin=stolen.source.fence_begin,.fence_end=stolen.source.fence_end,
  .first=stolen.source.first,.pages=pages,.original=stolen.pte[1][1],
  .source_epoch=stolen.exclusion.epoch,.assurances=stolen.source.assurances,.obligations=INTEL_COMPLETION_OPEN};
 stolen.range.epoch=interval.source_epoch;stolen.range.first=interval.first;stolen.range.pages=interval.pages;
 stolen.range.digest=14695981039346656037ull;
 if(stolen.import.selected) {
  if(!import_firmware() ||
     !scanout_transfer_reserve_imported(&stolen.range.transfer,&stolen.range.pool,&stolen.exclusion,
                                        &interval,&stolen.import.firmware,&stolen.range.token))return refuse(34);
 } else if(!scanout_transfer_reserve(&stolen.range.transfer,&stolen.range.pool,&stolen.exclusion,
                                     &interval,&stolen.range.token))return refuse(32);
 if(!range_fences())return false;
 for(unsigned i=0;i<interval.pages;i++) {
  uint64_t raw;
  if((i%64==0 && !tick(stolen.begin[1])) ||
     !range_pair(0x800000u+(interval.first+i)*8,&raw))return false;
  if(raw!=stolen.range.transfer.original[i] || raw!=stolen.pte[0][1][i])return refuse(32);
  stolen.range.digest=hash_word(stolen.range.digest,raw);++stolen.range.checked;
 }
 if(stolen.private_range.selected) {
  if(!reserve_private_range(pages))return false;
  if(stolen.private_range.exhausted && !observe_gpu_surface(pages))return false;
 }
 if(!range_fences() || !unmap() || !handoff_readers(true) || !unmap() ||
    !handoff_config(1) || !equal_bytes(stolen.handoff.config[0],stolen.handoff.config[1],sizeof(stolen.handoff.config[0])) ||
    !identity() || !sole_root() || !intel_window_handoff_revalidate(&stolen.source) ||
    !tick(stolen.begin[1]))return refuse(32);
 /* Ask the same admission function used by the mapping transaction. Any
  * other outcome conflicts with this read-only source and its open obligations. */
 if(scanout_transfer_admission(&interval)!=TRANSFER_ASSURANCE)return refuse(32);
 stolen.range.missing=HANDOFF_ALL & ~interval.assurances;
 stolen.range.verified=true;
 if(stolen.import.submission && !reserve_commands(interval))return refuse(34);
 bool imported=!stolen.import.selected || check_imported_page(interval);
 if(stolen.import.first && imported) {
  scanout_interval_release(&stolen.import.commands);
  stolen.import.command_returned=true;
 }
 return imported || refuse(34);
}

static bool observe_handoff(void) {
 if(!tick(stolen.begin[1]))return false;
 stolen.handoff.begin=stolen.last;
 if(!handoff_config(0) || !handoff_readers(false) || !unmap() ||
    !(stolen.range.selected ? observe_scanout_range() : handoff_config(1)) ||
    !tick(stolen.begin[1]))return false;
 if(stolen.last<=stolen.handoff.begin)return refuse(31);
 stolen.handoff.end=stolen.last;
 return true;
}

/* Power-gated display MMIO can read zero. Identify the live display codec
 * on both sides of the control read and bracket the page with well status. */
static bool sample_wireless_audio(unsigned sample) {
 __typeof__(stolen.import.audio[0]) *a=&stolen.import.audio[sample];
 const uint32_t powered=1u<<30, wireless_enabled=1u<<30, codec=0x80862808;
 if(!tick(stolen.begin[1]) ||
    !load(0x45404,&a->well_before) || a->well_before==UINT32_MAX || !(a->well_before & powered) ||
    !load(0x65020,&a->codec_before) || a->codec_before!=codec ||
    !load(0x65800,&a->control) || (a->control & wireless_enabled) ||
    !load(0x65020,&a->codec_after) || a->codec_after!=codec ||
    !load(0x45404,&a->well_after) || a->well_after==UINT32_MAX || !(a->well_after & powered)) {
  refuse(31);
  unmap();
  return false;
 }
 if(!unmap() || !tick(stolen.begin[1]))return false;
 ++stolen.import.audio_samples;
 return true;
}

/* WD_SURF arms wireless capture. Sample the capture page on either side
 * of CPU exclusion; the VMM refuses even a read alias while the loan is live. */
static bool sample_wireless_capture(unsigned sample) {
 const uint32_t offsets[]={0x6e514,0x6e520,0x6e558,0x6e400}; /* SURF, TAIL_CFG, STATUS, FUNC_CTL */
 for(unsigned i=0;i<4;i++) {
  uint32_t *word=&stolen.import.wd[sample][i];
  if(!tick(stolen.begin[1]) || !load(offsets[i],word) || *word==UINT32_MAX) {
   refuse(31);
   unmap();
   return false;
  }
 }
 if(!unmap() || !tick(stolen.begin[1]))return false;
 ++stolen.import.wd_samples;
 if(sample)
  for(unsigned i=0;i<4;i++)if(stolen.import.wd[0][i]!=stolen.import.wd[1][i])return refuse(31);
 if(stolen.import.wd[sample][3] & (1u<<31))return refuse(31);
 return sample_wireless_audio(sample);
}

static bool cpu_guard(void) {
 if(!stolen.cpu.admitted)return true;
 if(!checkpoint(23))return false;
 stolen.guard.attempted=1;
 if(!tick(stolen.begin[1]))return false;
 stolen.guard.begin=stolen.last;
 if(!identity() || !sole_root() || !intel_window_handoff_revalidate(&stolen.source))return refuse(30);
 if(!display_access_pin(&stolen.exclusion))return refuse(30);
 stolen.guard.pinned=1;
 struct vmm_physical_span pair[2];
 for(unsigned i=0;i<2;i++)pair[i]=(struct vmm_physical_span){stolen.cpu.span[i].physical,stolen.cpu.span[i].pages};
 bool ok=false;
 if(stolen.import.selected && !sample_wireless_capture(0))goto done;
 if(!tick(stolen.begin[1]) || !cpu_guard_operation(0,pair)){refuse(30);goto done;}
 stolen.guard.acquired=1;
 if(!tick(stolen.begin[1]) || !identity() || !sole_root() || !intel_window_handoff_revalidate(&stolen.source))goto done;
 if(!cpu_guard_operation(1,pair)){refuse(30);goto done;}
 stolen.guard.checked=1;
 bool observed=observe_handoff();
 /* A read refusal can return after the alias and original guard have been
  * proved clean. Context, time or alias uncertainty retains exclusion. */
 if(!unmap())goto done;
 if(!tick(stolen.begin[1]) || !identity() || !sole_root() ||
    !intel_window_handoff_revalidate(&stolen.source))goto done;
 if(!cpu_guard_operation(2,pair)){refuse(30);goto done;}
 stolen.guard.closed=1;
 ok=observed && (!stolen.import.selected || sample_wireless_capture(1)) &&
    (!stolen.owned_page.reader_pages || inactive_pipe_sample());
done:
 if(stolen.guard.acquired && !stolen.guard.closed)stolen.uncertain=true;
 if(stolen.import.first && !stolen.import.command_returned && !stolen.uncertain) {
  scanout_interval_release(&stolen.import.commands);
  stolen.import.command_returned=true;
 }
 if(stolen.owned_page.token.object && !stolen.owned_page.returned && !stolen.uncertain) {
  stolen.owned_page.returned=scanout_transfer_close(&stolen.owned_page.transfer,&stolen.owned_page.token);
  if(!stolen.owned_page.returned){stolen.uncertain=true;ok=false;refuse(34);}
 }
 if(stolen.import.token.object && !stolen.import.returned && !stolen.uncertain) {
  stolen.import.returned=scanout_transfer_close(&stolen.import.page,&stolen.import.token);
  if(!stolen.import.returned){stolen.uncertain=true;ok=false;refuse(34);}
 }
 if(stolen.command_tail.token.object && !stolen.command_tail.returned && !stolen.uncertain) {
  stolen.command_tail.returned=scanout_transfer_close(&stolen.command_tail.transfer,&stolen.command_tail.token);
  if(!stolen.command_tail.returned){stolen.uncertain=true;ok=false;refuse(34);}
 }
 /* These frames have never been installed in any GPU translation. Only the
  * owner that returned the tail loan can give them back to the PMM. */
 if(stolen.command_tail.dma_base && stolen.command_tail.returned && !stolen.uncertain) {
  if(command_frames_owned()) {
   pmm_free_frame_range(stolen.command_tail.dma_base,TAIL_DMA_PAGES);
   stolen.command_tail.dma_freed=true;
  } else {stolen.uncertain=true;ok=false;refuse(34);}
 }
 if(stolen.range.token.object && stolen.range.transfer.state!=TRANSFER_CLOSED && !stolen.uncertain &&
    !scanout_transfer_close(&stolen.range.transfer,&stolen.range.token)){
  stolen.uncertain=true;ok=false;refuse(32);
 }
 if(stolen.gpu_range.claimed && !stolen.uncertain) {
  scanout_interval_release(&stolen.gpu_range.lease);
  stolen.gpu_range.returned=true;
 }
 if(stolen.private_range.claimed && !stolen.uncertain) {
  scanout_interval_release(&stolen.private_range.lease);
  stolen.private_range.returned=true;
 }
 if(!stolen.uncertain) {
  if(display_access_unpin(&stolen.exclusion))stolen.guard.unpinned=1;
  else {stolen.uncertain=true;ok=false;refuse(30);}
 }
 if(!ok)return refuse(30);
 struct x86_cache_snapshot fresh;
 if(!identity() || !intel_cache_probe_context(&fresh) || !x86_cache_equal(&fresh,&stolen.cache))return refuse(30);
 if(!tick(stolen.begin[1]))return false;
 stolen.guard.end=stolen.last;stolen.end[1]=stolen.last;
 return stolen.guard.end>stolen.guard.begin || refuse(18);
}
bool intel_stolen_probe_retained(void){return stolen.uncertain || (stolen.acquired && !stolen.released);}
bool intel_stolen_probe_report(const char *cmdline) {
 if(stolen.attempted)return false;
 stolen.attempted=true;stolen.mtrr=stolen.pat=stolen.effective=255;
 console_write("intel-stolen: hardware ownership gate under maintained CPU mapping exclusion; two bounded2s passes, no input, no GPU writes or allocation grant\n");
 if(!parse_recipe(cmdline,&stolen.range.selected,&stolen.private_range.selected,&stolen.import.selected,&stolen.import.submission)){refuse(1);goto report;}
 if(!timer_reference_ready()){refuse(2);goto report;}
 stolen.command=cmdline;
 if(!checkpoint(1))goto report;
 if(!intel_scanout_trial_profile(&stolen.profile) || stolen.profile.width!=1440 ||
 stolen.profile.height!=900 || stolen.profile.pitch!=6144){refuse(3);goto report;}
 if(!checkpoint(2))goto report;
 if(!intel_window_handoff_snapshot(&stolen.source) || stolen.source.assurances || stolen.source.pages!=16 ||
 stolen.source.first!=8704 || stolen.source.fence_begin || stolen.source.fence_end!=0x2200000 ||
 stolen.source.aperture_bytes!=0x10000000 || !intel_window_handoff_clean()){refuse(4);goto report;}
 if(!checkpoint(3))goto report;
 stolen.root=vmm_current_address_space();
 if(!stolen.root){stolen.root_observation=(struct vmm_range_observation){.phase=1,.reason=VMM_RANGE_ROOT};refuse(5);goto report;}
 if(stolen.root!=vmm_kernel_address_space()){stolen.root_observation=(struct vmm_range_observation){.phase=1,.reason=VMM_RANGE_KERNEL};refuse(5);goto report;}
 if(!sole_root())goto report;
 for(uint64_t i=0;i<pci_device_count();i++) {
  const struct pci_device *d=pci_device_at(i);if(!d || d->bus || d->function)continue;
  if(!d->device)stolen.host=d;else if(d->device==2)stolen.gpu=d;
 }
 if(!stolen.host || !stolen.gpu){refuse(6);goto report;}
 if(!checkpoint(4))goto report;
 if(!console_sync()){refuse(7);goto report;}
 if(!checkpoint(5))goto report;
 if(!display_access_acquire(0x53544f4c454eull,&stolen.exclusion)){refuse(8);goto report;}
 stolen.acquired=true;
 if(!checkpoint(6))goto cleanup;
 if(!identity())goto cleanup;
 if(!checkpoint(7))goto cleanup;
 if(!intel_cache_probe_context(&stolen.cache)){refuse(10);goto cleanup;}
 for(unsigned n=0;n<2;n++) {
  stolen.phase=n+1;stolen.begin[n]=timer_monotonic_us();
  if(!stolen.begin[n] || stolen.begin[n]<stolen.last){stolen.clock_at=stolen.begin[n];stolen.clock_previous=stolen.last;stolen.reason|=2;refuse(18);goto cleanup;}
  stolen.last=stolen.begin[n];
  if(!capture(n)){stolen.reason|=4;goto cleanup;}
 }
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<HANDOFF_PAGES;i++)
  if(stolen.pte[n][1][i]!=stolen.source.original[i]){stolen.reason|=32;refuse(25);goto cleanup;}
 stolen.phase=3;if(!checkpoint(12))goto cleanup;
 stolen.complete=intel_bdw_stolen_scanout_check(&stolen.facts[0],&stolen.vectors,&stolen.layout);
 if(!stolen.complete){stolen.reason|=16;refuse(21);}
 if(stolen.complete && (!checkpoint(21) || !cpu_backing() || !cpu_guard()))stolen.complete=false;
cleanup:
 trace(13,0);
 if(unmap() && !stolen.uncertain && intel_window_handoff_clean())stolen.released=display_access_release(&stolen.exclusion,true);
 if(!stolen.released){stolen.uncertain=true;stolen.reason|=8;refuse(23);}
report:
 trace(14,1);
 console_printf("intel-stolen-summary: v=%u reader_reads=%u root_phase=%u root_reason=%u root_detail=%u preflight=%u fail=%u offset=%u cache_index=3 mtrr=%u pat=%u effective=%u complete=%u phase=%u reason=%u acquired=%u released=%u retained=%u maps=%u retired=%u cache_checks=%u alias_checks=%u meta_reads=%u classified=%u checked=%u clock=%u previous=%u writes=0 ownership=none\n",
 (uint64_t)(stolen.import.submission?15:stolen.import.selected?14:stolen.private_range.selected?13:stolen.range.selected?12:8),(uint64_t)stolen.reader_reads,(uint64_t)stolen.root_observation.phase,(uint64_t)stolen.root_observation.reason,stolen.root_observation.detail,
 (uint64_t)stolen.preflight,(uint64_t)stolen.fail,(uint64_t)stolen.fail_offset,(uint64_t)stolen.mtrr,(uint64_t)stolen.pat,(uint64_t)stolen.effective,
 (uint64_t)stolen.complete,(uint64_t)stolen.phase,(uint64_t)stolen.reason,(uint64_t)stolen.acquired,(uint64_t)stolen.released,
 (uint64_t)intel_stolen_probe_retained(),(uint64_t)stolen.maps,(uint64_t)stolen.retired,(uint64_t)stolen.cache_checks,
 (uint64_t)stolen.alias_checks,(uint64_t)stolen.meta_reads,(uint64_t)(stolen.complete && stolen.layout.classified),(uint64_t)stolen.layout.checked,
 stolen.clock_at,stolen.clock_previous);
 for(unsigned n=0;n<2;n++) {
  const struct bdw_stolen_facts *f=&stolen.facts[n];
  console_printf("intel-stolen-facts: v=1 sample=%u begin=%u end=%u host=%x gpu=%x subsystem=%x ggc=%x bdsm=%x bgsm=%x tolud=%x reserved=%x width=%u height=%u stride=%u aperture=%u ggtt=%u fence_begin=%u fence_end=%u fbc=%x psr_ctl=%x psr_status=%x\n",
  (uint64_t)n,stolen.begin[n],stolen.end[n],(uint64_t)f->host,(uint64_t)f->gpu,(uint64_t)f->subsystem,(uint64_t)f->ggc,
  (uint64_t)f->bdsm,(uint64_t)f->bgsm,(uint64_t)f->tolud,(uint64_t)f->reserved,(uint64_t)f->width,(uint64_t)f->height,(uint64_t)f->stride,f->aperture_bytes,f->ggtt_bytes,f->fence_begin,f->fence_end,(uint64_t)stolen.aux[n][0],(uint64_t)stolen.aux[n][1],(uint64_t)stolen.aux[n][2]);
  for(unsigned r=0;r<3;r++) {
   unsigned count=stolen.count[n][r],bad=count;
   uint64_t bad_value=0,first=count?stolen.pte[n][r][0]:0;
   for(unsigned i=0;i<count;i++)if((first&0xffffffff00000fffull)!=1 || stolen.pte[n][r][i]!=first+4096ull*i) {
    bad=i;bad_value=stolen.pte[n][r][i];break;
   }
   console_printf("intel-stolen-run: v=1 sample=%u range=%u gpu=%u bytes=%u pages=%u count=%u reads=%u first=%x last=%x hash=%x bad_index=%u bad_value=%x\n",
    (uint64_t)n,(uint64_t)r,stolen.plan.range[r].gpu,stolen.plan.range[r].bytes,(uint64_t)stolen.plan.range[r].pages,
    (uint64_t)count,(uint64_t)stolen.reads[n][r],count?stolen.pte[n][r][0]:0,count?stolen.pte[n][r][count-1]:0,stolen.hash[n][r],(uint64_t)bad,bad_value);
  }
 }
 for(unsigned n=0;n<2;n++) {
  for(unsigned p=0;p<3;p++) {
   console_printf("intel-stolen-reader: v=1 sample=%u pipe=%u count=%u",(uint64_t)n,(uint64_t)p,(uint64_t)stolen.reader_count[n][p]);
   for(unsigned w=0;w<13;w++)console_printf(" w%u=%x",(uint64_t)w,(uint64_t)stolen.reader[n][p][w]);
   console_write("\n");
  }
  for(unsigned r=0;r<2;r++)for(unsigned i=0;i<8;i++) {
   const __typeof__(stolen.policy[0][0][0]) *q=&stolen.policy[n][r][i];
   console_printf("intel-stolen-policy: v=1 sample=%u range=%u index=%u physical=%x pages=%u queried=%u ok=%u mtrr=%u pat=%u effective=%u checks=%u\n",
    (uint64_t)n,(uint64_t)(r+1),(uint64_t)i,q->physical,(uint64_t)q->pages,(uint64_t)q->queried,(uint64_t)q->ok,
    (uint64_t)q->mtrr,(uint64_t)q->pat,(uint64_t)q->effective,(uint64_t)q->checks);
  }
 }
 for(unsigned r=0;r<2;r++) {
  const __typeof__(stolen.cpu.span[0]) *q=&stolen.cpu.span[r];
  console_printf("intel-stolen-cpu-span: v=1 range=%u physical=%x pages=%u checked=%u reserved=%u framebuffer=%u other=%u uncovered=%u tagged=%u\n",
   (uint64_t)(r+1),q->physical,(uint64_t)q->pages,(uint64_t)q->checked,(uint64_t)q->reserved,(uint64_t)q->framebuffer,
   (uint64_t)q->other,(uint64_t)q->uncovered,(uint64_t)q->tagged);
 }
 const struct vmm_unmapped_detail *d=&stolen.cpu.detail;
 const struct vmm_alias_inventory *i=&stolen.cpu.inventory;
 console_printf("intel-stolen-cpu: v=1 attempted=%u observed=%u admitted=%u begin=%u end=%u ram_ranges=%u bits=%u pmm_checks=%u unmapped=%u reason=%u level=%u root_phase=%u root_reason=%u root_detail=%u tables=%u entries=%u leaves=%u span=%u virtual=%x physical=%x bytes=%u entry=%x writes=0 authority=none\n",
  (uint64_t)stolen.cpu.attempted,(uint64_t)stolen.cpu.observed,(uint64_t)stolen.cpu.admitted,stolen.cpu.begin,stolen.cpu.end,
  (uint64_t)stolen.cpu.ram_count,(uint64_t)(stolen.cpu.attempted?stolen.cache.cpu.physical_bits:0),(uint64_t)stolen.cpu.pmm_checks,(uint64_t)stolen.cpu.unmapped,(uint64_t)d->walk.reason,(uint64_t)d->walk.level,
  (uint64_t)d->walk.root.phase,(uint64_t)d->walk.root.reason,d->walk.root.detail,(uint64_t)i->tables,(uint64_t)i->entries,(uint64_t)i->leaves,
  (uint64_t)d->span,d->virtual_base,d->physical,d->bytes,d->entry);
 console_printf("intel-stolen-guard: v=1 attempted=%u begin=%u end=%u pinned=%u acquired=%u checked=%u closed=%u unpinned=%u reason=%u writes=0 authority=none\n",
  (uint64_t)stolen.guard.attempted,stolen.guard.begin,stolen.guard.end,(uint64_t)stolen.guard.pinned,
  (uint64_t)stolen.guard.acquired,(uint64_t)stolen.guard.checked,(uint64_t)stolen.guard.closed,(uint64_t)stolen.guard.unpinned,
  (uint64_t)stolen.guard.detail.walk.reason);
 for(unsigned n=0;n<3;n++) {
  const struct vmm_alias_inventory *g=&stolen.guard.inventory[n];
  console_printf("intel-stolen-guard-walk: v=1 phase=%u tables=%u entries=%u leaves=%u\n",(uint64_t)n,
   (uint64_t)g->tables,(uint64_t)g->entries,(uint64_t)g->leaves);
 }
 if(stolen.import.submission) {
  console_printf("intel-submission-guard: v=9 pages=%u closed=%u samples=%u surface0=%x tail0=%x status0=%x surface1=%x tail1=%x status1=%x func0=%x func1=%x strap0=%x strap1=%x tried=%u mask0=%x mask1=%x mask2=%x first=%u checked=%u returned=%u tail_slot=%u tail_pages=%u tail_pte=%x descriptor=%x tail_samples=%u tail_conflicts=%u tail_returned=%u rcs_indirect=%x dma_base=%x cpu_flush_lines=%u dma_freed=%u pdp0=%x engine_hwsp=%x audio_samples=%u aperture_pages=%u\n",
   (uint64_t)stolen.import.ports,(uint64_t)stolen.guard.closed,(uint64_t)stolen.import.wd_samples,
   (uint64_t)stolen.import.wd[0][0],(uint64_t)stolen.import.wd[0][1],(uint64_t)stolen.import.wd[0][2],
   (uint64_t)stolen.import.wd[1][0],(uint64_t)stolen.import.wd[1][1],(uint64_t)stolen.import.wd[1][2],
   (uint64_t)stolen.import.wd[0][3],(uint64_t)stolen.import.wd[1][3],(uint64_t)stolen.import.strap[0],(uint64_t)stolen.import.strap[1],
   (uint64_t)stolen.import.tried,(uint64_t)stolen.import.occupied[0],(uint64_t)stolen.import.occupied[1],(uint64_t)stolen.import.occupied[2],
   (uint64_t)stolen.import.first,(uint64_t)stolen.import.checked,(uint64_t)stolen.import.command_returned,
   (uint64_t)stolen.command_tail.manifest.first,(uint64_t)stolen.command_tail.manifest.pages,
   stolen.command_tail.transfer.original[0],stolen.command_tail.descriptor,(uint64_t)stolen.command_tail.samples,
   (uint64_t)stolen.command_tail.conflicts,(uint64_t)stolen.command_tail.returned,(uint64_t)stolen.command_tail.rcs.pointer,
   stolen.command_tail.dma_base,(uint64_t)stolen.command_tail.flush_lines,(uint64_t)stolen.command_tail.dma_freed,stolen.command_tail.pdp0,stolen.command_tail.planned[6].gpu_address,
   (uint64_t)stolen.import.audio_samples,(uint64_t)(stolen.guard.acquired?5:0));
 }
 if(stolen.import.selected) {
  for(unsigned n=0;n<2;n++) {
   if(!stolen.import.submission) {
    const uint32_t *wd=stolen.import.wd[n];
    console_printf("intel-owned-widi: v=1 sample=%u surface=%x tail=%x status=%x function=%x strap=%x\n",
      (uint64_t)n,(uint64_t)wd[0],(uint64_t)wd[1],(uint64_t)wd[2],(uint64_t)wd[3],(uint64_t)stolen.import.strap[n]);
   }
   const __typeof__(stolen.import.audio[0]) *a=&stolen.import.audio[n];
   console_printf("intel-wireless-audio: v=1 sample=%u well_before=%x codec_before=%x control=%x codec_after=%x well_after=%x\n",
    (uint64_t)n,(uint64_t)a->well_before,(uint64_t)a->codec_before,(uint64_t)a->control,
    (uint64_t)a->codec_after,(uint64_t)a->well_after);
  }
 }
 if(stolen.import.selected)
  console_printf("intel-firmware-import: v=%u first=%u pages=%u borrowed=%u samples=%u original=%x last=%x returned=%u conflicts=%u writes=0 grants=0\n",
    (uint64_t)(stolen.import.submission?1:2),(uint64_t)stolen.import.firmware.pages.first,(uint64_t)stolen.import.firmware.pages.pages,
    (uint64_t)(stolen.import.token.object!=NULL),(uint64_t)stolen.import.samples,
    stolen.import.observed[0],stolen.import.observed[1],(uint64_t)stolen.import.returned,(uint64_t)stolen.import.blocked);
 if(stolen.import.selected && !stolen.import.submission) {
  const uint64_t *a=stolen.owned_page.entries[0],*b=stolen.owned_page.entries[1];
  console_printf("intel-owned-page: v=4 video_samples=%u audio_samples=%u reader_pages=%u reader_words=%u aperture=%x table_backing=%x register_page=%x guard_checks=%u slot=%u samples=%u epoch=%u returned=%u conflicts=%u frame0=%u frame1=%u frame_reads=%u left0=%x pte0=%x right0=%x left1=%x pte1=%x right1=%x writes=0 grants=0\n",
   (uint64_t)stolen.import.wd_samples,(uint64_t)stolen.import.audio_samples,
   (uint64_t)stolen.owned_page.reader_pages,(uint64_t)stolen.owned_page.reader_words,
   stolen.owned_page.aperture,stolen.owned_page.table_backing,stolen.owned_page.register_page,
   (uint64_t)stolen.owned_page.guard_checks,(uint64_t)OWNED_PAGE_SLOT,(uint64_t)stolen.owned_page.samples,stolen.owned_page.token.epoch,
   (uint64_t)stolen.owned_page.returned,(uint64_t)stolen.owned_page.conflicts,
   (uint64_t)stolen.owned_page.frame[0],(uint64_t)stolen.owned_page.frame[1],(uint64_t)stolen.owned_page.frame_reads,a[0],a[1],a[2],b[0],b[1],b[2]);
 }
 if(stolen.private_range.selected)
  console_printf("intel-private-range: v=2 begin=%u first=%u scanned=%u occupied=%u checked=%u batches=%u maps=%u claimed=%u returned=%u exhausted=%u reader_conflict=%u writes=0\n",
   (uint64_t)stolen.private_range.begin,(uint64_t)stolen.private_range.first,(uint64_t)stolen.private_range.scanned,
   (uint64_t)stolen.private_range.nonzero,(uint64_t)stolen.private_range.verified,(uint64_t)stolen.private_range.batches,
   (uint64_t)stolen.private_range.maps,(uint64_t)stolen.private_range.claimed,(uint64_t)stolen.private_range.returned,
   (uint64_t)stolen.private_range.exhausted,(uint64_t)stolen.private_range.reader_conflict);
 if(stolen.private_range.selected)
  console_printf("intel-gpu-surface: v=1 first=%u pages=%u sampled=%u matched=%u occupied=%u present=%u first_pte=%x same_first=%u claimed=%u conflict=%u returned=%u writes=0\n",
   (uint64_t)stolen.gpu_range.first,(uint64_t)stolen.gpu_range.pages,(uint64_t)stolen.gpu_range.sampled,
   (uint64_t)stolen.gpu_range.matched,(uint64_t)stolen.gpu_range.occupied,(uint64_t)stolen.gpu_range.present,
   stolen.gpu_range.original[0],(uint64_t)stolen.gpu_range.same_first,(uint64_t)stolen.gpu_range.claimed,
   (uint64_t)stolen.gpu_range.conflict,(uint64_t)stolen.gpu_range.returned);
 if(stolen.range.selected)
  console_printf("intel-stolen-range: v=4 aperture_page=%x aperture_alias=%u frame0=%u frame1=%u frame_reads=%u protected_pages=%u reservation=%u returned=%u epoch=%u slot=%u pages=%u checked=%u fences=%u closing_readers=%u hash=%x verified=%u missing=%u assurances=0 obligations=255 writes=0\n",
   stolen.range.excluded_aperture_page,(uint64_t)stolen.range.aperture_alias,(uint64_t)stolen.range.frame[0],(uint64_t)stolen.range.frame[1],
   (uint64_t)stolen.range.frame_reads,(uint64_t)stolen.range.protected_pages,stolen.range.token.epoch,(uint64_t)(stolen.range.transfer.state==TRANSFER_CLOSED),
   stolen.range.epoch,(uint64_t)stolen.range.first,(uint64_t)stolen.range.pages,(uint64_t)stolen.range.checked,
   (uint64_t)stolen.range.fences,(uint64_t)stolen.range.closing_readers,stolen.range.digest,(uint64_t)stolen.range.verified,(uint64_t)stolen.range.missing);
 console_printf("intel-stolen-hardware-gate: v=1 begin=%u end=%u firmware_transfer=0 first_unmet=%s writes=0\n",
  stolen.handoff.begin,stolen.handoff.end,stolen.handoff.end?"firmware-transfer":
  stolen.cpu.observed && !stolen.cpu.admitted?"cpu-backing":"unresolved");
 for(unsigned sample=0;sample<2;sample++) {
  console_printf("intel-stolen-hardware-config: v=1 sample=%u count=%u",(uint64_t)sample,(uint64_t)stolen.handoff.config_reads[sample]);
  for(unsigned word=0;word<INTEL_PCI_REQUESTER_WORDS+2;word++)
   console_printf(" w%u=%x",(uint64_t)word,(uint64_t)stolen.handoff.config[sample][word]);
  console_write("\n");
 }
 console_printf("intel-stolen-hardware-readers: v=1 count=%u",(uint64_t)stolen.handoff.reader_reads);
 for(unsigned word=0;word<42;word++)console_printf(" w%u=%x",(uint64_t)word,(uint64_t)stolen.handoff.readers[word]);
 console_write("\n");
 return stolen.complete && !intel_stolen_probe_retained();
}
#endif
