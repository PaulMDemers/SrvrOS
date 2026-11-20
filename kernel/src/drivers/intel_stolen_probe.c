#include <srvros/intel_bdw_stolen_scanout.h>
#ifndef SRVROS_INTEL_STOLEN_CAPTURE
bool intel_stolen_probe_report(const char *cmdline) {(void)cmdline;return false;}
bool intel_stolen_probe_recipe(const char *cmdline) {(void)cmdline;return false;}
bool intel_stolen_probe_retained(void) {return false;}
#else
#include <srvros/intel_scanout_trial.h>
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
#define STOLEN_CENSUS_US 2000000u
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
  uint64_t epoch,digest;
  unsigned first,pages,checked,fences,closing_readers,missing;
  bool selected,verified;
 } range;
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
static bool parse_recipe(const char *s,bool *range) {
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
  if(!bit || (seen&bit) || ++tokens>10)return false;
  seen|=bit;
 }
 if(!((tokens==9 && seen==511) || (tokens==10 && seen==1023)))return false;
 if(range)*range=(seen&512)!=0;
 return true;
}
bool intel_stolen_probe_recipe(const char *s) {
 return parse_recipe(s,NULL);
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
 bool ok=phase==0?vmm_reserved_guard_begin(&stolen.guard.owner,stolen.root,pair,2,stolen.cache.cpu.physical_bits,
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

static bool observe_scanout_range(void) {
 const struct bdw_stolen_range *candidate=&stolen.layout.plan.range[1];
 if(!stolen.layout.classified || !stolen.guard.checked || stolen.guard.closed ||
    !stolen.guard.owner.epoch || !stolen.exclusion.epoch || !identity() ||
    !sole_root() || !intel_window_handoff_revalidate(&stolen.source))return refuse(32);
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
 if(!range_fences())return false;
 for(unsigned i=0;i<interval.pages;i++) {
  uint64_t raw;
  if((i%64==0 && !tick(stolen.begin[1])) ||
     !range_pair(0x800000u+(interval.first+i)*8,&raw))return false;
  if(raw!=interval.original[i] || raw!=stolen.pte[0][1][i])return refuse(32);
  stolen.range.digest=hash_word(stolen.range.digest,raw);++stolen.range.checked;
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
 return true;
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
 stolen.guard.closed=1;ok=observed;
done:
 if(stolen.guard.acquired && !stolen.guard.closed)stolen.uncertain=true;
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
 if(!parse_recipe(cmdline,&stolen.range.selected)){refuse(1);goto report;}
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
 (uint64_t)(stolen.range.selected?9:8),(uint64_t)stolen.reader_reads,(uint64_t)stolen.root_observation.phase,(uint64_t)stolen.root_observation.reason,stolen.root_observation.detail,
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
 if(stolen.range.selected)
  console_printf("intel-stolen-range: v=1 epoch=%u slot=%u pages=%u checked=%u fences=%u closing_readers=%u hash=%x verified=%u missing=%u assurances=0 obligations=255 writes=0\n",
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
