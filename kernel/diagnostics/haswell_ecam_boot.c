/* Exact Air6,1 pre-PCI admission. AP ELF only; explicit v5 pre-PCI dispatcher.
 * Legacy CF8 selection is restored; no PCI config-data/firmware/policy writes.
 * These observations authorize only equivalent ECAM representation, not VT-d. */
#include <srvros/ecam_boot.h>
#include <srvros/acpi.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <stddef.h>
static struct ecam_boot_gate_result eg;
static struct ecam_boot_gate_diagnostic eg_diagnostic;
static bool eg_fail(const char *stage,unsigned details,unsigned index,unsigned component,uint64_t expected,uint64_t observed) {
 if(!eg_diagnostic.stage)eg_diagnostic=(struct ecam_boot_gate_diagnostic){stage,index,component,details,expected,observed};
 return false;
}
const struct ecam_boot_gate_diagnostic *haswell_ecam_boot_gate_diagnostic(void){return &eg_diagnostic;}
static struct hfw_entry eg_ram[HFW_MAP_CAP];
static unsigned eg_count,eg_phase;
static bool eg_started,eg_broken;
static uint64_t eg_root;
static struct ecam_boot_policy eg_last_policy;
static bool eg_policy_ready[2];
struct eg_mp_input {
 uint64_t revision,flags,count,bsp;
 struct {uint32_t id;bool present,launched;} cpu[4];
};
static const uint8_t eg_dmar_pin[32]={0xaa,0xfa,0x82,0x6c,0xe5,0x0c,0x10,0x38,0x8f,0x2c,0x41,0x98,0xde,0x9a,0x42,0x9a,0xb5,0x1a,0x55,0x73,0xad,0x1c,0x5d,0x28,0x56,0xea,0x32,0xed,0x07,0x8b,0x63,0x07};
static const uint8_t eg_mcfg_pin[60]={
 0x4d,0x43,0x46,0x47,0x3c,0,0,0,1,0x58,0x41,0x50,0x50,0x4c,0x45,0x20,
 0x41,0x70,0x70,0x6c,0x65,0x30,0x30,0,1,0,0,0,0x4c,0x6f,0x6b,0x69,
 0x5f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xe0,0,0,0,0,0,0,0,0x9b,0,0,0,0};
#ifdef SRVROS_ECAM_GATE_HOST_TEST
extern void eg_test_cpuid(uint32_t,uint32_t *,uint32_t *,uint32_t *,uint32_t *);
extern uint64_t eg_test_cr0(void),eg_test_cr4(void),eg_test_flags(void);
extern bool eg_test_msr(uint32_t,uint64_t *);
extern uint32_t eg_test_port_read(uint16_t);
extern void eg_test_port_write(uint16_t,uint32_t);
extern uint8_t eg_test_dmar_pin[32];
extern bool eg_test_mp_input(struct eg_mp_input *);
#define eg_cpuid eg_test_cpuid
#define eg_cr0 eg_test_cr0
#define eg_cr4 eg_test_cr4
#define eg_flags eg_test_flags
#define eg_port_read eg_test_port_read
#define eg_port_write eg_test_port_write
#define eg_mp_input_read eg_test_mp_input
#else
#include "../src/arch/x86_64/io.h"
static void eg_cpuid(uint32_t n,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d){__asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(n),"c"(0));}
static uint64_t eg_cr0(void){uint64_t v;__asm__ volatile("mov %%cr0,%0":"=r"(v));return v;}
static uint64_t eg_cr4(void){uint64_t v;__asm__ volatile("mov %%cr4,%0":"=r"(v));return v;}
static uint64_t eg_flags(void){uint64_t v;__asm__ volatile("pushfq; popq %0":"=r"(v));return v;}
#define eg_port_read inl
#define eg_port_write outl
extern volatile struct limine_mp_request ap_probe_request;
static bool eg_mp_input_read(struct eg_mp_input *out) {
 const struct limine_mp_response *r=ap_probe_request.response;
 if(!r || r->revision || r->flags || !r->cpus || r->cpu_count!=4 || r->bsp_lapic_id)return false;
 *out=(struct eg_mp_input){.revision=r->revision,.flags=r->flags,.count=r->cpu_count,.bsp=r->bsp_lapic_id};
 unsigned mask=0;
 for(unsigned i=0;i<4;i++) {
  const struct limine_mp_info *p=r->cpus[i];
  if(!p || p->lapic_id>=4 || (mask&(1u<<p->lapic_id)) || (p->lapic_id && p->goto_address))return false;
  mask|=1u<<p->lapic_id;
  out->cpu[i].present=true;out->cpu[i].id=p->lapic_id;out->cpu[i].launched=p->goto_address!=NULL;
 }
 return true;
}
#endif
static bool eg_mp(struct ecam_boot_policy *p) {
 struct eg_mp_input r;
 if(!eg_mp_input_read(&r) || r.revision || r.flags || r.count!=4 || r.bsp)return false;
 unsigned mask=0;
 p->revision=r.revision;p->flags=r.flags;p->count=r.count;p->bsp=r.bsp;
 for(unsigned i=0;i<4;i++) {
  unsigned id=r.cpu[i].id;
  if(!r.cpu[i].present || id>=4 || (mask&(1u<<id)) || (id && r.cpu[i].launched))return false;
  mask|=1u<<id;
  p->ids[i]=id;p->present_mask|=(unsigned)r.cpu[i].present<<i;p->launch_mask|=(unsigned)r.cpu[i].launched<<i;
 }
 return mask==15;
}
static uint64_t eg_now(void *ctx){(void)ctx;return timer_monotonic_us();}
static bool eg_cpu(void *ctx,struct x86_cache_cpu *cpu) {
 (void)ctx;uint32_t a,b,c,d;eg_cpuid(0,&a,&b,&c,&d);if(a<1)return false;
 cpu->vendor[0]=b;cpu->vendor[1]=d;cpu->vendor[2]=c;
 if(b!=0x756e6547 || d!=0x49656e69 || c!=0x6c65746e)return false;
 eg_cpuid(1,&a,&b,&c,&d);cpu->signature=a;cpu->features=d;
 if(a!=0x40651 || b>>24 || (d&0x11028)!=0x11028)return false;
 struct ecam_boot_policy p={.apic=b>>24};
 eg_cpuid(0x80000000,&a,&b,&c,&d);if(a<0x80000008)return false;p.extended_max=a;
 eg_cpuid(0x80000001,&a,&b,&c,&d);if((d&((1u<<20)|(1u<<26)))!=((1u<<20)|(1u<<26)))return false;p.nx_edx=d;
 eg_cpuid(0x80000008,&a,&b,&c,&d);cpu->physical_bits=a&255;cpu->cr0=eg_cr0();
 p.cr4=eg_cr4();p.irq=eg_flags();
 if(cpu->physical_bits!=39 || cpu->cr0&(3ull<<29) || (cpu->cr0&0x80010001ull)!=0x80010001ull || p.cr4!=0x620 || (p.irq&512) || !eg_mp(&p))return false;
 if(eg_phase<2 && eg_policy_ready[eg_phase]) {
  const struct ecam_boot_policy *old=eg.policy+eg_phase;
  if(p.extended_max!=old->extended_max || p.nx_edx!=old->nx_edx || p.apic!=old->apic || p.cr4!=old->cr4 || p.revision!=old->revision || p.flags!=old->flags || p.count!=old->count || p.bsp!=old->bsp || p.present_mask!=old->present_mask || p.launch_mask!=old->launch_mask)return false;
  for(unsigned i=0;i<4;i++)if(p.ids[i]!=old->ids[i])return false;
 }
 eg_last_policy=p;return true;
}
static bool eg_msr(void *ctx,uint32_t n,uint64_t *v) {
 (void)ctx;
 bool efer=n==0xc0000080u;
 if(!efer && n!=0xfe && n!=0x277 && n!=0x2ff && !(n>=0x200 && n<=0x21f) && n!=0x250 && n!=0x258 && n!=0x259 && !(n>=0x268 && n<=0x26f))return false;
 if(eg_phase>=2 || (efer ? eg.efer_reads>=2 : eg.cache_reads[eg_phase]>=49))return false;
 if(efer)eg.efer_reads++;else eg.cache_reads[eg_phase]++;
#ifdef SRVROS_ECAM_GATE_HOST_TEST
 return eg_test_msr(n,v);
#else
 uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(n));*v=a|((uint64_t)d<<32);return true;
#endif
}
static bool eg_catalog(void *ctx,struct hfw_entry *out,unsigned *count) {
 (void)ctx;extern volatile struct limine_memmap_request memmap_request;
 const struct limine_memmap_response *r=memmap_request.response;
 if(!r || !r->entries || !r->entry_count || r->entry_count>HFW_MAP_CAP)return false;
 *count=r->entry_count;
 for(unsigned i=0;i<*count;i++){const struct limine_memmap_entry *p=r->entries[i];if(!p)return false;out[i]=(struct hfw_entry){p->base,p->length,p->type};}
 return hfw_catalog_valid(out,*count);
}
static bool eg_ram_page(void *ctx,uint64_t pa) {
 (void)ctx;
 if(!pa || (pa&4095) || pa>=(1ull<<39) || (pa>=ECAM_BOOT_PA && pa<ECAM_BOOT_PA+ECAM_BOOT_BYTES) || pa==0xfed90000 || pa==0xfed91000)return false;
 unsigned overlaps=0;bool good=false;
 for(unsigned i=0;i<eg_count;i++) {
  const struct hfw_entry *p=eg_ram+i;
  if(p->base<pa+4096 && pa<p->base+p->length){overlaps++;good=p->base<=pa && pa+4096<=p->base+p->length && (p->type==0 || p->type==2 || p->type==3 || p->type==5 || p->type==6);}
 }
 return overlaps==1 && good;
}
static bool eg_copy(uint64_t pa,uint8_t *out,unsigned length) {
 if(length>4096-eg.firmware_bytes)return eg_fail("firmware-byte-cap",1,0,0,4096,eg.firmware_bytes+length);
 eg.firmware_bytes+=length;return vmm_ecam_boot_copy_ram(pa,out,length);
}
static const struct acpi_table_info *eg_find(const char *signature,unsigned length) {
 const struct acpi_table_info *found=NULL;
 if(acpi_table_count()>32)return NULL;
 for(unsigned i=0;i<acpi_table_count();i++) {
  const struct acpi_table_info *t=acpi_table_at(i);if(!t)return NULL;
  bool match=true;for(unsigned j=0;j<4;j++)if(t->signature[j]!=signature[j])match=false;
  if(match){if(found || t->length!=length || t->revision!=1)return NULL;found=t;}
 }
 return found;
}
static bool eg_table(void *ctx,const struct hfw_entry *e,unsigned n,uint8_t *out,uint64_t *pa) {
 (void)ctx;const struct acpi_table_info *t=eg_find("DMAR",136);
 if(!t || !hfw_table_covered(e,n,t->physical_address) || !eg_copy(t->physical_address,out,136))return false;
 *pa=t->physical_address;return true;
}
static bool eg_identity(void *ctx,uint32_t out[5]) {
 (void)ctx;const unsigned devices[5]={2,2,2,0,31},offsets[5]={0,8,0x2c,0,0};
 for(unsigned i=0;i<5;i++) {
  if(eg.config_reads>=20 || eg.selector_writes>38)return false;
  if(eg_flags()&512)return false;
  uint32_t old=eg_port_read(0xcf8);eg_port_write(0xcf8,0x80000000u|(devices[i]<<11)|offsets[i]);
  out[i]=eg_port_read(0xcfc);eg_port_write(0xcf8,old);
  eg.config_reads++;eg.selector_writes+=2;
 }
 return true;
}
static bool eg_mcfg(unsigned phase) {
 const struct acpi_table_info *t=eg_find("MCFG",60);
 const struct srv_acpi_mcfg_allocation *a=acpi_mcfg_allocation_at(0);
 if(!t || acpi_mcfg_allocation_count()!=1 || !a || !a->present || a->base_address!=ECAM_BOOT_PA || a->pci_segment || a->start_bus || a->end_bus!=155)return false;
 /* MCFG is admitted ACPI RAM, never an ECAM data read. */
 bool covered=false;unsigned overlaps=0;
 if(t->physical_address>UINT64_MAX-60)return false;
 for(unsigned i=0;i<eg_count;i++)if(eg_ram[i].base<t->physical_address+60 && t->physical_address<eg_ram[i].base+eg_ram[i].length){overlaps++;covered=eg_ram[i].base<=t->physical_address && t->physical_address+60<=eg_ram[i].base+eg_ram[i].length && (eg_ram[i].type==2 || eg_ram[i].type==3);}
 if(overlaps!=1 || !covered || !eg_copy(t->physical_address,eg.mcfg[phase],60))return false;
 eg.mcfg_pa[phase]=t->physical_address;
 for(unsigned i=0;i<60;i++)if(eg.mcfg[phase][i]!=eg_mcfg_pin[i])return false;
 return true;
}
static bool eg_stable(void) {
 struct x86_cache_cpu cpu={0};
 if(!eg_cpu(NULL,&cpu))return eg_fail("stable-cpu-controls",0,0,0,0,0);
 if(!vmm_diagnostic_sole_kernel_root(eg_root))return eg_fail("stable-sole-root",0,0,0,0,0);
 const struct x86_cache_cpu *old=&eg.cache[0].cpu;
 const uint64_t expected[]={old->signature,old->features,old->physical_bits,old->cr0};
 const uint64_t actual[]={cpu.signature,cpu.features,cpu.physical_bits,cpu.cr0};
 for(unsigned i=0;i<4;i++)if(expected[i]!=actual[i])return eg_fail("stable-cpu-state",1,0,i,expected[i],actual[i]);
 struct hfw_entry entries[HFW_MAP_CAP];unsigned count;
 if(!eg_catalog(NULL,entries,&count))return eg_fail("stable-catalog-source",0,0,0,0,0);
 if(count!=eg_count)return eg_fail("stable-catalog-count",1,0,0,eg_count,count);
 for(unsigned i=0;i<count;i++) {
  const uint64_t want[]={eg_ram[i].base,eg_ram[i].length,eg_ram[i].type};
  const uint64_t got[]={entries[i].base,entries[i].length,entries[i].type};
  for(unsigned j=0;j<3;j++)if(want[j]!=got[j])return eg_fail("stable-catalog-word",1,i,j,want[j],got[j]);
 }
 const struct acpi_table_info *m=eg_find("MCFG",60),*d=eg_find("DMAR",136);
 if(!m || !d)return eg_fail("stable-table-identity",0,0,0,0,0);
 if(m->physical_address!=eg.mcfg_pa[0])return eg_fail("stable-MCFG-address",1,0,0,eg.mcfg_pa[0],m->physical_address);
 if(d->physical_address!=eg.firmware[0].sample[0].table_pa)return eg_fail("stable-DMAR-address",1,0,0,eg.firmware[0].sample[0].table_pa,d->physical_address);
 uint8_t bytes[136];
 if(!eg_copy(m->physical_address,bytes,60))return eg_fail("stable-MCFG-copy",0,0,0,0,0);
 for(unsigned i=0;i<60;i++)if(bytes[i]!=eg.mcfg[0][i])return eg_fail("stable-MCFG-byte",1,i,0,eg.mcfg[0][i],bytes[i]);
 if(!eg_copy(d->physical_address,bytes,136))return eg_fail("stable-DMAR-copy",0,0,0,0,0);
 for(unsigned i=0;i<136;i++)if(bytes[i]!=eg.firmware[0].sample[0].dmar[i])return eg_fail("stable-DMAR-byte",1,i,0,eg.firmware[0].sample[0].dmar[i],bytes[i]);
 return true;
}
static bool eg_check_inner(void *ctx,unsigned phase) {
 (void)ctx;
 if(!timer_reference_ready())return eg_fail("reference-clock",0,0,0,0,0);
 if(phase==2)return eg.phases>=1 && eg_stable();
 if(phase>1 || phase!=eg.phases)return false;
 eg_phase=phase;struct x86_cache_cpu cpu={0};
 if(!eg_cpu(NULL,&cpu) || !vmm_diagnostic_sole_kernel_root(eg_root))return false;
 eg.policy[phase]=eg_last_policy;eg_policy_ready[phase]=true;
 if(!phase && !eg_catalog(NULL,eg_ram,&eg_count))return false;
 const struct hfw_io fw={.identity=eg_identity,.catalog=eg_catalog,.table=eg_table,.now=eg_now};
 const struct x86_cache_io cache={.cpu=eg_cpu,.msr=eg_msr,.now=eg_now};
 const uint8_t *pin=eg_dmar_pin;
#ifdef SRVROS_ECAM_GATE_HOST_TEST
 pin=eg_test_dmar_pin;
#endif
 if(!hfw_capture(&eg.firmware[phase],&fw,pin) || !eg_mcfg(phase) ||
    !x86_cache_snapshot(&eg.cache[phase],&cache) || !eg_msr(NULL,0xc0000080,&eg.efer[phase]) ||
    (eg.efer[phase]&~0xd01ull) || (eg.efer[phase]&0xd00)!=0xd00)return false;
 for(unsigned leaf=0;leaf<ECAM_BOOT_LEAVES;leaf++) {
  unsigned m0=255,p0=255,e0=255;
  for(unsigned page=0;page<512;page++) {
   unsigned m,p,e;uint64_t pa=ECAM_BOOT_PA+((uint64_t)leaf<<21)+page*4096ull;
   if(!x86_cache_page(&eg.cache[phase],pa,2,&m,&p,&e) || e)return false;
   if(!page){m0=m;p0=p;e0=e;}else if(m!=m0 || p!=p0 || e!=e0)return false;
  }
 }
 if(phase) {
  if(!x86_cache_equal(eg.cache,eg.cache+1) || eg.efer[0]!=eg.efer[1] || eg.mcfg_pa[0]!=eg.mcfg_pa[1])return false;
  const struct hfw_sample *a=&eg.firmware[0].sample[0],*b=&eg.firmware[1].sample[0];
  if(a->count!=b->count || a->table_pa!=b->table_pa)return false;
  for(unsigned i=0;i<a->count;i++)if(a->entries[i].base!=b->entries[i].base || a->entries[i].length!=b->entries[i].length || a->entries[i].type!=b->entries[i].type)return false;
  for(unsigned i=0;i<136;i++)if(a->dmar[i]!=b->dmar[i])return false;
 }
 eg.phases++;return phase==0 || eg_stable();
}
static bool eg_check(void *ctx,unsigned phase) {
 if(eg_broken)return false;
 bool ok=eg_check_inner(ctx,phase);
 if(!ok){eg_fail("admission-phase",1,phase,0,1,0);eg_broken=true;}
 return ok;
}
bool haswell_ecam_boot_prepare(void) {
 if(eg_started)return false;
 eg_started=true;
 eg_root=vmm_kernel_address_space();
 const struct ecam_boot_env env={.check=eg_check,.ram=eg_ram_page,.now=eg_now};
 return vmm_ecam_boot_prepare(&env);
}
bool haswell_ecam_boot_handoff(void){return eg_started && vmm_ecam_boot_handoff();}
bool haswell_ecam_boot_retire(void){return eg_started && vmm_ecam_boot_retire();}
const struct ecam_boot_gate_result *haswell_ecam_boot_gate_result(void){return &eg;}
bool haswell_ecam_boot_adopt(uint64_t va,uint64_t pa,uint64_t bytes){return eg_started && !eg_broken && vmm_ecam_boot_adopt(va,pa,bytes);}
