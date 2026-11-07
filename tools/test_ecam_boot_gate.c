/* Same real backend plus real pre-PCI adapter; only CPU/ports/RAM are fixtures.
 * A synthetic DMAR pin is HOST_TEST-only and never hardware acceptance. */
#define main ecam_backend_fixture_main
#include "test_ecam_boot.c"
#undef main
#ifndef SRVROS_ECAM_GATE_HOST_TEST
#define SRVROS_ECAM_GATE_HOST_TEST
#endif
#include "../kernel/diagnostics/haswell_ecam_boot.c"
volatile struct limine_memmap_request memmap_request;
uint8_t eg_test_dmar_pin[32];
static struct limine_memmap_entry map_entries[128];
static struct limine_memmap_entry *map_ptrs[128];
static struct limine_memmap_response map_response;
static struct eg_mp_input mp_input;
static struct acpi_table_info table_info[2];
static struct srv_acpi_mcfg_allocation allocation;
static uint32_t selector;
static unsigned msrs,port_reads,port_writes;
static bool no_reference,foreign_cpu,foreign_identity,no_nx,wrong_width,changed_cache,changed_efer,mixed_mtrr;
static uint64_t cr0_value,cr4_value;
static uint32_t feature_mask;
void eg_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 *a=*b=*c=*d=0;
 if(!leaf){*a=1;*b=0x756e6547;*d=0x49656e69;*c=0x6c65746e;}
 else if(leaf==1){*a=foreign_cpu?0x306d4:0x40651;*d=feature_mask;}
 else if(leaf==0x80000000)*a=0x80000008;
 else if(leaf==0x80000001)*d=no_nx?0:(1u<<20)|(1u<<26);
 else {assert(leaf==0x80000008);*a=wrong_width?40:39;}
}
uint64_t eg_test_cr0(void){return cr0_value;}
uint64_t eg_test_cr4(void){return cr4_value;}
uint64_t eg_test_flags(void){return irq_flags;}
bool eg_test_mp_input(struct eg_mp_input *out){*out=mp_input;return true;}
bool eg_test_msr(uint32_t n,uint64_t *v) {
 msrs++;*v=0;
 if(n==0xfe)*v=0x510; /* Maximum16 variable pairs and11 fixed words. */
 else if(n==0x277)*v=0x0000010500070406ull ^ (changed_cache && eg_phase ? 2ull : 0);
 else if(n==0x2ff)*v=0xc06;
 else if(n==0x200)*v=mixed_mtrr?ECAM_BOOT_PA:0xc0000000;
 else if(n==0x201)*v=(((1ull<<39)-1)&~((mixed_mtrr?4096ull:1ull<<30)-1))|0x800;
 else if(n==0xc0000080)*v=0xd00 | (changed_efer && eg_phase ? 1 : 0);
 else assert((n>=0x202 && n<=0x21f) || n==0x250 || n==0x258 || n==0x259 || (n>=0x268 && n<=0x26f));
 return true;
}
uint32_t eg_test_port_read(uint16_t port) {
 if(port==0xcf8)return selector;
 assert(port==0xcfc && (selector&0x80000000u));port_reads++;
 if(foreign_identity)return 0xffffffff;
 unsigned device=(selector>>11)&31,offset=selector&255;
 if(!device){assert(!offset);return 0x0a048086;}
 if(device==31){assert(!offset);return 0x9c438086;}
 assert(device==2);
 if(!offset)return 0x0a268086;
 if(offset==8)return 0x03000009;
 assert(offset==0x2c);return 0x011a106b;
}
void eg_test_port_write(uint16_t port,uint32_t value){assert(port==0xcf8);port_writes++;selector=value;}
bool timer_reference_ready(void){return !no_reference;}
uint64_t timer_monotonic_us(void){return now(NULL);}
uint64_t acpi_table_count(void){return 2;}
const struct acpi_table_info *acpi_table_at(uint64_t i){assert(i<2);return table_info+i;}
uint64_t acpi_mcfg_allocation_count(void){return 1;}
const struct srv_acpi_mcfg_allocation *acpi_mcfg_allocation_at(uint64_t i){assert(!i);return &allocation;}
static void setup(void) {
 reset();memset(&eg,0,sizeof(eg));memset(&eg_diagnostic,0,sizeof(eg_diagnostic));memset(eg_policy_ready,0,sizeof(eg_policy_ready));eg_count=eg_phase=0;eg_started=eg_broken=false;eg_root=0;
 no_reference=foreign_cpu=foreign_identity=no_nx=wrong_width=changed_cache=changed_efer=mixed_mtrr=false;
 cr0_value=0x80010033;cr4_value=0x620;feature_mask=0x11028;msrs=port_reads=port_writes=0;selector=0x12340000;
 map_entries[0]=(struct limine_memmap_entry){.base=0x2000000,.length=6*4096,.type=0};
 map_entries[1]=(struct limine_memmap_entry){.base=0x2006000,.length=2*4096,.type=2};
 for(unsigned i=0;i<2;i++)map_ptrs[i]=map_entries+i;
 map_response=(struct limine_memmap_response){.entry_count=2,.entries=map_ptrs};memmap_request.response=&map_response;
 mp_input=(struct eg_mp_input){.count=4};
 for(unsigned i=0;i<4;i++){mp_input.cpu[i].id=i;mp_input.cpu[i].present=true;}
 table_info[0]=(struct acpi_table_info){.signature="DMAR",.physical_address=0x2006000,.length=136,.revision=1};
 table_info[1]=(struct acpi_table_info){.signature="MCFG",.physical_address=0x2007000,.length=60,.revision=1};
 allocation=(struct srv_acpi_mcfg_allocation){.present=true,.base_address=ECAM_BOOT_PA,.end_bus=155};
 memset(pages[6],0,4096);uint8_t *data=(uint8_t *)pages[6];memcpy(data,"DMAR",4);data[4]=136;data[8]=1;
 unsigned sum=0;for(unsigned i=0;i<136;i++)sum+=data[i];data[9]=(uint8_t)-sum;hfw_sha256(data,eg_test_dmar_pin);
 memcpy(pages[7],eg_mcfg_pin,60);
}
#ifndef ECAM_GATE_FIXTURE_LIBRARY
int main(void) {
 setup();assert(haswell_ecam_boot_prepare());
 assert(eg.phases==2 && eg.cache_reads[0]==49 && eg.cache_reads[1]==49 && eg.efer_reads==2 && msrs==100);
 assert(eg.config_reads==20 && eg.selector_writes==40 && port_reads==20 && port_writes==40 && selector==0x12340000);
 assert(haswell_ecam_boot_retire() && eb.r.state==ECAM_RETIRED && frees==1 && eb.r.words<ECAM_BOOT_WORD_CAP && eg.firmware_bytes<=4096);
 assert(!haswell_ecam_boot_prepare());
 setup();assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff());assert(eb.r.state==ECAM_ACTIVE && !frees);
 for(unsigned mode=0;mode<15;mode++) {
  setup();if(mode==0)foreign_cpu=true;if(mode==1)no_nx=true;if(mode==2)wrong_width=true;
  if(mode==3)cr4_value|=1ull<<12;
  if(mode==4)cr0_value&=~(1ull<<16);
  if(mode==5)mp_input.cpu[1].launched=true;
  if(mode==6)mp_input.cpu[2].id=1;
  if(mode==7)mp_input.bsp=1;
  if(mode==8)no_reference=true;
  if(mode==9)irq_flags|=512;
  if(mode==10)foreign_identity=true;
  if(mode>=11){const unsigned bits[]={3,5,12,16};feature_mask&=~(1u<<bits[mode-11]);}
  assert(!haswell_ecam_boot_prepare() && !allocs && !stores && !msrs);
  assert(selector==0x12340000);
 }
 for(unsigned i=0;i<60;i++) {
  setup();((uint8_t *)pages[7])[i]^=1;
  assert(!haswell_ecam_boot_prepare() && !allocs && !stores && selector==0x12340000);
 }
 for(unsigned mode=0;mode<5;mode++) {
  setup();if(mode==0)allocation.end_bus=154;if(mode==1)map_entries[1].type=1;
  if(mode==2)table_info[0].physical_address=0xfed90000;
  if(mode==3)((uint8_t *)pages[6])[100]^=1;
  if(mode==4)mixed_mtrr=true;
  assert(!haswell_ecam_boot_prepare() && !allocs && !stores);
 }
 setup();changed_cache=true;assert(!haswell_ecam_boot_prepare() && !frees && !pages[1][12]);retained();
 setup();changed_efer=true;assert(!haswell_ecam_boot_prepare() && !frees && !pages[1][12]);retained();
 setup();assert(haswell_ecam_boot_prepare());((uint8_t *)pages[7])[0]^=1;
 assert(!haswell_ecam_boot_retire());retained();
 for(unsigned mode=0;mode<6;mode++) {
  setup();assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff());
  if(mode==0)cr4_value|=1ull<<12;
  if(mode==1)((uint8_t *)pages[7])[23]^=1;
  if(mode==2)((uint8_t *)pages[6])[101]^=1;
  if(mode==3)map_entries[0].length-=4096;
  if(mode==4)load_fault=loads+1;
  if(mode==5){stop_clock=true;frozen_time=eb.r.begin+100000;}
  assert(!haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES) && !eb.r.adoptions && !frees);
  const char *stage[]={"stable-cpu-controls","stable-MCFG-byte","stable-DMAR-byte","stable-catalog-word","stable-MCFG-copy","stable-MCFG-copy"};
  assert(eg_diagnostic.stage && !strcmp(eg_diagnostic.stage,stage[mode]));
  if(mode==1 || mode==2)assert(eg_diagnostic.index==(mode==1?23:101) && (eg_diagnostic.expected^eg_diagnostic.observed)==1);
  assert(eb_diagnostic.reason && !strcmp(eb_diagnostic.reason,mode==4?"read-failed":mode==5?"lifetime-deadline":"fresh-gate"));
  struct ecam_boot_gate_diagnostic first=eg_diagnostic;
  assert(!haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
  assert(!memcmp(&first,&eg_diagnostic,sizeof(first)));
 }
 puts("actual pre-PCI gate PASS: maximum49+49 cache MSRs,2 EFER reads,20 exact legacy IDs, CF8 restored, pinned MCFG/DMAR, BSP/parked APs, RAM sources, cache uniformity and fresh-change refusals; synthetic DMAR pin");
 return 0;
}
#endif
