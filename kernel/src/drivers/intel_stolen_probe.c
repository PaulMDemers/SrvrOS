#include <srvros/intel_bdw_stolen_scanout.h>
#ifndef SRVROS_INTEL_STOLEN_CAPTURE
bool intel_stolen_probe_report(const char *cmdline) {(void)cmdline;return false;}
bool intel_stolen_probe_retained(void) {return false;}
#else
#include <srvros/intel_scanout_trial.h>
#include <srvros/intel_handoff.h>
#include <limine.h>
extern volatile struct limine_memmap_request memmap_request;
#include <srvros/scanout_handoff.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/x86_cache.h>
#include <srvros/pci.h>
#include <srvros/vmm.h>
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
 const struct pci_device *gpu,*host;
 uint64_t root,last,begin[2],end[2],physical,hash[2][3],clock_at,clock_previous;
 uint64_t pte[2][3][16384];
 uint32_t aux[2][3],count[2][3],reads[2][3],meta_reads;
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
static bool recipe(const char *s) {
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
  if(!bit || (seen&bit) || ++tokens>9)return false;
  seen|=bit;
 }
 return tokens==9 && seen==511;
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
 if(page==0x108000 || page==0x43000 || page==0x6f000)return true;
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
 if(!checkpoint(11) || !tick(stolen.begin[n]))return false;
 if(!intel_window_handoff_revalidate(&stolen.source))return refuse(11);
 if(!identity() || !tick(stolen.begin[n]))return false;
 stolen.end[n]=stolen.last;return stolen.end[n]>stolen.begin[n] || refuse(18);
}
bool intel_stolen_probe_retained(void){return stolen.uncertain || (stolen.acquired && !stolen.released);}
bool intel_stolen_probe_report(const char *cmdline) {
 if(stolen.attempted)return false;
 stolen.attempted=true;stolen.mtrr=stolen.pat=stolen.effective=255;
 console_write("intel-stolen: wire4 root-observed UC3 alias-preflight read-only complete inherited/candidate translation census; two bounded2s passes, no input, no GPU writes or allocation grant\n");
 if(!recipe(cmdline)){refuse(1);goto report;}
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
cleanup:
 trace(13,0);
 if(unmap() && !stolen.uncertain && intel_window_handoff_clean())stolen.released=display_access_release(&stolen.exclusion,true);
 if(!stolen.released){stolen.uncertain=true;stolen.reason|=8;refuse(23);}
report:
 trace(14,1);
 console_printf("intel-stolen-summary: v=4 root_phase=%u root_reason=%u root_detail=%u preflight=%u fail=%u offset=%u cache_index=3 mtrr=%u pat=%u effective=%u complete=%u phase=%u reason=%u acquired=%u released=%u retained=%u maps=%u retired=%u cache_checks=%u alias_checks=%u meta_reads=%u classified=%u checked=%u clock=%u previous=%u writes=0 ownership=none\n",
 (uint64_t)stolen.root_observation.phase,(uint64_t)stolen.root_observation.reason,stolen.root_observation.detail,
 (uint64_t)stolen.preflight,(uint64_t)stolen.fail,(uint64_t)stolen.fail_offset,(uint64_t)stolen.mtrr,(uint64_t)stolen.pat,(uint64_t)stolen.effective,
 (uint64_t)stolen.complete,(uint64_t)stolen.phase,(uint64_t)stolen.reason,(uint64_t)stolen.acquired,(uint64_t)stolen.released,
 (uint64_t)intel_stolen_probe_retained(),(uint64_t)stolen.maps,(uint64_t)stolen.retired,(uint64_t)stolen.cache_checks,
 (uint64_t)stolen.alias_checks,(uint64_t)stolen.meta_reads,(uint64_t)stolen.layout.classified,(uint64_t)stolen.layout.checked,
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
 return stolen.complete && !intel_stolen_probe_retained();
}
#endif
