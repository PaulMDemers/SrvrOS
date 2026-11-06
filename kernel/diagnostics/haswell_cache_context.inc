/* Uses the parent's exact firmware/PCI gates. CPU table RAM only; no data alias
 * dereference, map, MSR/control write or VT-d register read. */
#include <srvros/haswell_cache_context.h>
#define HCC_UNIT 0xfed90000ull
#define HCC_WINDOW 0xffffc001f3250000ull
static struct hfw_result hcc_fw[2];
static struct x86_cache_snapshot hcc_cpu[2];
static struct hcc_walk hcc_walks[2][2][2]; /* phase, before/after, HHDM/window */
static uint64_t hcc_cr3[2],hcc_cr4[2],hcc_hhdm[2];
static unsigned hcc_table_reads,hcc_pte_reads[2];
static uint64_t hcc_bounds[2][2];
#ifdef SRVROS_HCC_HOST_TEST
extern bool hcc_test_cpu(void *,struct x86_cache_cpu *);
extern bool hcc_test_msr(void *,uint32_t,uint64_t *);
extern uint64_t hcc_test_cr3(void),hcc_test_cr4(void);
extern bool hcc_test_entry(uint64_t,unsigned,uint64_t *);
#define hcc_read_cpu hcc_test_cpu
#define hcc_read_msr hcc_test_msr
#define hcc_root hcc_test_cr3
#define hcc_control hcc_test_cr4
#else
static void hcc_cpuid(uint32_t n,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){__asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(n),"c"(0));}
static bool hcc_read_cpu(void *ctx,struct x86_cache_cpu *cpu) {
 (void)ctx;uint32_t a,b,c,d;hcc_cpuid(0,&a,&b,&c,&d);if(a<1)return false;
 cpu->vendor[0]=b;cpu->vendor[1]=d;cpu->vendor[2]=c;
 hcc_cpuid(1,&a,&b,&c,&d);cpu->signature=a;cpu->features=d;
 hcc_cpuid(0x80000000,&a,&b,&c,&d);if(a<0x80000008)return false;
 hcc_cpuid(0x80000008,&a,&b,&c,&d);cpu->physical_bits=a&255;
 __asm__ volatile("mov %%cr0,%0":"=r"(cpu->cr0));
 return cpu->signature==0x40651 && cpu->physical_bits==39 && (cpu->features&0x20);
}
static bool hcc_read_msr(void *ctx,uint32_t n,uint64_t *v) {
 (void)ctx;if(n!=0xfe && n!=0x277 && n!=0x2ff && !(n>=0x200 && n<=0x21f) && n!=0x250 && n!=0x258 && n!=0x259 && !(n>=0x268 && n<=0x26f))return false;
 uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));*v=a|((uint64_t)d<<32);return true;
}
static uint64_t hcc_root(void){uint64_t v;__asm__ volatile("mov %%cr3,%0":"=r"(v));return v;}
static uint64_t hcc_control(void){uint64_t v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
#endif
static bool hcc_resource_catalog(void) {
 const struct hfw_sample *s=&hcc_fw[0].sample[0];
 const struct hfw_entry *lo=NULL,*hi=NULL;
 for(unsigned i=0;i<s->count;i++) {
  const struct hfw_entry *e=s->entries+i;uint64_t end=e->base+e->length;
  if(e->base<HCC_UNIT+8192 && HCC_UNIT<end)return false;
  if(end<=HCC_UNIT && (!lo || end>lo->base+lo->length))lo=e;
  if(e->base>=HCC_UNIT+4096 && (!hi || e->base<hi->base))hi=e;
 }
 return lo && hi && lo->base==0xfed1c000 && lo->length==0x4000 && lo->type==1 && hi->base==0xffe10000 && hi->length==0x30000 && hi->type==1;
}
static bool hcc_ram_page(uint64_t pa) {
 const struct hfw_sample *s=&hcc_fw[0].sample[0];
 if((pa&4095) || !pa || pa==HCC_UNIT || pa==HCC_UNIT+4096 || pa>UINT64_MAX-4096)return false;
 unsigned overlap=0;bool covered=false;
 for(unsigned i=0;i<s->count;i++){const struct hfw_entry *e=s->entries+i;
  if(e->base<pa+4096 && pa<e->base+e->length){overlap++;covered=e->base<=pa && pa+4096<=e->base+e->length && (e->type==0 || e->type==2 || e->type==3 || e->type==5 || e->type==6);}}
 return overlap==1 && covered;
}
static bool hcc_entry(void *ctx,uint64_t pa,unsigned index,uint64_t *out) {
 (void)ctx;if(index>=512 || hcc_table_reads>=16 || !hcc_ram_page(pa) || hcc_root()!=vmm_kernel_address_space())return false;
#ifdef SRVROS_HCC_HOST_TEST
 if(!hcc_test_entry(pa,index,out))return false;
#else
 uint64_t address=pa+index*8,translated;
 volatile uint64_t *p=pmm_phys_to_virt(address);
 if(!p || !vmm_virt_to_phys_in_space(vmm_kernel_address_space(),(uint64_t)(uintptr_t)p,&translated) || translated!=address)return false;
 *out=*p;
#endif
 hcc_table_reads++;return true;
}
static bool hcc_fw_equal(void) {
 const struct hfw_sample *a=&hcc_fw[0].sample[0],*b=&hcc_fw[1].sample[0];
 if(a->count!=b->count || a->table_pa!=b->table_pa)return false;
 for(unsigned i=0;i<HFW_DMAR_BYTES;i++)if(a->dmar[i]!=b->dmar[i])return false;
 for(unsigned i=0;i<a->count;i++)if(a->entries[i].base!=b->entries[i].base || a->entries[i].length!=b->entries[i].length || a->entries[i].type!=b->entries[i].type)return false;
 return true;
}
static void hcc_report(unsigned status) {
 console_printf("hsw-cache: v=1 status=%u samples=%u msr_cap=49 pte_cap=16 limit_us=100000 mappings=0 writes=0 ownership=0\n",(uint64_t)status,(uint64_t)(status==1?2:0));
 if(status!=1)return;
 static uint8_t bytes[1024];static char data[1400];
 for(unsigned phase=0;phase<2;phase++) {
  const struct x86_cache_snapshot *s=&hcc_cpu[phase];unsigned n=0;
  for(unsigned i=0;i<3;i++)put(bytes,&n,s->cpu.vendor[i],4);
  put(bytes,&n,s->cpu.signature,4);put(bytes,&n,s->cpu.features,4);put(bytes,&n,s->cpu.physical_bits,4);
  const uint64_t values[]={s->cpu.cr0,s->pat,s->cap,s->def_type,s->begin_us,s->end_us,hcc_cr3[phase],hcc_cr4[phase],hcc_hhdm[phase],hcc_bounds[phase][0],hcc_bounds[phase][1]};
  for(unsigned i=0;i<11;i++)put(bytes,&n,values[i],8);
  put(bytes,&n,s->reads,4);put(bytes,&n,s->count,4);put(bytes,&n,s->fixed_count,4);put(bytes,&n,hcc_pte_reads[phase],4);
  for(unsigned i=0;i<16;i++){put(bytes,&n,s->base[i],8);put(bytes,&n,s->mask[i],8);}
  for(unsigned i=0;i<11;i++)put(bytes,&n,s->fixed[i],8);
  b64(bytes,n,data);console_printf("hsw-cache-cpu: v=1 phase=%u data=%s\n",(uint64_t)phase,data);
  for(unsigned edge=0;edge<2;edge++)for(unsigned alias=0;alias<2;alias++) {
   const struct hcc_walk *w=&hcc_walks[phase][edge][alias];n=0;
   put(bytes,&n,w->va,8);put(bytes,&n,w->pa,8);
   for(unsigned i=0;i<4;i++){put(bytes,&n,w->table[i],8);put(bytes,&n,w->entry[i],8);}
   put(bytes,&n,w->count,4);put(bytes,&n,w->shift,4);put(bytes,&n,w->state,4);
   b64(bytes,n,data);console_printf("hsw-cache-walk: v=1 phase=%u edge=%u alias=%u data=%s\n",(uint64_t)phase,(uint64_t)edge,(uint64_t)alias,data);
  }
 }
}
#include "haswell_alias_adapter.inc"
static bool hcc_run(bool alias) {
 unsigned status=2;const struct hfw_io fw_io={.identity=identity,.catalog=catalog,.table=table,.now=now,.progress=progress};
 console_write("hsw-cache-progress: read-only Air6,1 CPU/cache and page-table RAM; VT-d target never dereferenced\n");
 if(!hfw_capture(&hcc_fw[0],&fw_io,pin))goto done;
 hfw_report(&hcc_fw[0]);status=3;
 if(!hcc_resource_catalog())goto done;
 for(unsigned phase=0;phase<2;phase++) {
  console_printf("hsw-cache-progress: context=%u/2 no mappings or policy writes\n",(uint64_t)phase+1);
  if(alias && phase==1 && !hac_run())goto done;
  hcc_bounds[phase][0]=timer_monotonic_us();
  if(phase && (hcc_bounds[phase][0]<hcc_bounds[0][1] || hcc_bounds[phase][0]-hcc_bounds[0][1]>100000))goto done;
  hcc_cr3[phase]=hcc_root();hcc_cr4[phase]=hcc_control();hcc_hhdm[phase]=pmm_hhdm_offset();
  if(hcc_cr3[phase]!=vmm_kernel_address_space() || hcc_cr4[phase]&(1ull<<12) || !hcc_hhdm[phase] || hcc_hhdm[phase]>UINT64_MAX-HCC_UNIT)goto done;
  struct x86_cache_cpu cpu={0};if(!hcc_read_cpu(NULL,&cpu) || cpu.signature!=0x40651 || cpu.physical_bits!=39 || (cpu.features&0x11020)!=0x11020)goto done;
  hcc_table_reads=0;
  for(unsigned edge=0;edge<2;edge++) {
   if(edge){const struct x86_cache_io io={.cpu=hcc_read_cpu,.msr=hcc_read_msr,.now=now};if(!x86_cache_snapshot(&hcc_cpu[phase],&io))goto done;}
   for(unsigned alias=0;alias<2;alias++)if(!hcc_walk(&hcc_walks[phase][edge][alias],hcc_cr3[phase],alias?HCC_WINDOW:hcc_hhdm[phase]+HCC_UNIT,39,hcc_entry,NULL))goto done;
  }
  hcc_bounds[phase][1]=timer_monotonic_us();hcc_pte_reads[phase]=hcc_table_reads;
  if(!hcc_bounds[phase][0] || hcc_bounds[phase][1]<=hcc_bounds[phase][0] || hcc_bounds[phase][1]-hcc_bounds[phase][0]>100000)goto done;
  if(hcc_root()!=hcc_cr3[phase] || hcc_control()!=hcc_cr4[phase] || pmm_hhdm_offset()!=hcc_hhdm[phase])goto done;
  if(hcc_walks[phase][0][1].state!=1 || (hcc_walks[phase][0][0].state==2 && hcc_walks[phase][0][0].pa!=HCC_UNIT))goto done;
  for(unsigned alias=0;alias<2;alias++)if(!hcc_walk_equal(&hcc_walks[phase][0][alias],&hcc_walks[phase][1][alias]))goto done;
 }
 if(!x86_cache_equal(&hcc_cpu[0],&hcc_cpu[1]) || hcc_cr3[0]!=hcc_cr3[1] || hcc_cr4[0]!=hcc_cr4[1] || hcc_hhdm[0]!=hcc_hhdm[1])goto done;
 for(unsigned alias=0;alias<2;alias++)if(!hcc_walk_equal(&hcc_walks[0][0][alias],&hcc_walks[1][0][alias]))goto done;
 status=4;if(!hfw_capture(&hcc_fw[1],&fw_io,pin) || !hcc_fw_equal())goto done;
 hfw_report_named(&hcc_fw[1],"hcache-firmware-post");status=1;
 done: if(status==2)hfw_report(&hcc_fw[0]);hcc_report(status);if(alias)hac_report(status==1);return status==1;
}

bool haswell_cache_context_run(void){return hcc_run(false);}
bool haswell_alias_context_run(void){return hcc_run(true);}
