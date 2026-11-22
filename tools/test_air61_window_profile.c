#define AIR61_WINDOW_PROFILE_FIXTURE
#define main window_backend_fixture_main
#include "test_air61_window.c"
#undef main
#define SRVROS_ECAM_GATE_HOST_TEST
#include "../kernel/diagnostics/air61_window_gate.c"
#include <stdarg.h>
#include <setjmp.h>
#include "../kernel/diagnostics/air61_window_report.c"
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
static bool no_reference,foreign_cpu,foreign_identity,no_nx,wrong_width,changed_cache,changed_efer,mixed_mtrr,changed_catalog;
static uint64_t cr0_value,cr4_value;
static uint32_t feature_mask;
void eg_test_cpuid(uint32_t leaf,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 if(changed_catalog && eg.phases)map_entries[0].type=2;
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
 map_entries[0]=(struct limine_memmap_entry){.base=0x2000000,.length=5*4096,.type=0};
 map_entries[1]=(struct limine_memmap_entry){.base=0x2005000,.length=4096,.type=2};
 map_entries[2]=(struct limine_memmap_entry){.base=0x2006000,.length=2*4096,.type=0};
 map_entries[3]=(struct limine_memmap_entry){.base=0xfed1c000,.length=0x4000,.type=1};
 map_entries[4]=(struct limine_memmap_entry){.base=0xffe10000,.length=0x30000,.type=1};
 for(unsigned i=0;i<5;i++)map_ptrs[i]=map_entries+i;
 map_response=(struct limine_memmap_response){.entry_count=5,.entries=map_ptrs};memmap_request.response=&map_response;
 mp_input=(struct eg_mp_input){.count=4};
 for(unsigned i=0;i<4;i++){mp_input.cpu[i].id=i;mp_input.cpu[i].present=true;}
 table_info[0]=(struct acpi_table_info){.signature="DMAR",.physical_address=0x2005000,.length=136,.revision=1};
 table_info[1]=(struct acpi_table_info){.signature="MCFG",.physical_address=0x2005100,.length=60,.revision=1};
 allocation=(struct srv_acpi_mcfg_allocation){.present=true,.base_address=ECAM_BOOT_PA,.end_bus=155};
 memset(pages[5],0,4096);uint8_t *data=(uint8_t *)pages[5];memcpy(data,"DMAR",4);data[4]=136;data[8]=1;
 unsigned sum=0;for(unsigned i=0;i<136;i++)sum+=data[i];data[9]=(uint8_t)-sum;hfw_sha256(data,eg_test_dmar_pin);
 memcpy((uint8_t *)pages[5]+256,eg_mcfg_pin,60);
}

static struct ecam_boot_result boot={.state=ECAM_ACTIVE,.adoptions=1};
const struct ecam_boot_result *vmm_ecam_boot_result(void){return &boot;}
uint64_t eg_test_irq_save(void){uint64_t f=irq_flags;irq_flags&=~512ull;return f;}
void eg_test_irq_restore(uint64_t f){irq_flags=f;}
static jmp_buf halted;static bool terminal_expected;
void window_test_halt(void){assert(terminal_expected);longjmp(halted,1);}
void console_printf(const char *s,...){va_list a;va_start(a,s);for(;*s;s++){if(*s!='%'){putchar(*s);continue;}s++;if(*s=='s')fputs(va_arg(a,const char *),stdout);else if(*s=='x')printf("0x%016llx",(unsigned long long)va_arg(a,uint64_t));else {assert(*s=='u');printf("%llu",(unsigned long long)va_arg(a,uint64_t));}}va_end(a);}
void console_write(const char *s){fputs(s,stdout);}
void console_set_framebuffer_muted(bool b){assert(!b);}
void console_clear(void){}
bool console_sync(void){return true;}
#ifndef WINDOW_PROFILE_FIXTURE_LIBRARY
static bool ram_reference(uint64_t pa){
 if(!pa || (pa&4095) || pa>=(1ull<<39) || (pa>=ECAM_BOOT_PA && pa<ECAM_BOOT_PA+ECAM_BOOT_BYTES) || pa==0xfed90000 || pa==0xfed91000)return false;
 unsigned overlaps=0;bool good=false;
 for(unsigned i=0;i<eg_count;i++){
  const struct hfw_entry *p=eg_ram+i;
  if(p->base<pa+4096 && pa<p->base+p->length){overlaps++;good=p->base<=pa && pa+4096<=p->base+p->length && (p->type==0 || p->type==2 || p->type==3 || p->type==5 || p->type==6);}
 }
 return overlaps==1 && good;
}
static void ram_index_contracts(void){
 uint32_t random=0x61697261;unsigned queries=0;
 for(unsigned n=1;n<=128;n++){
  eg_count=n;
  for(unsigned i=0;i<n;i++)eg_ram[i]=(struct hfw_entry){.base=0x4000000ull+i*65536,.length=(i%4+1)*4096+(i%3?0:17),.type=i%9};
  for(unsigned i=n;i>1;i--){random=random*1664525+1013904223;unsigned j=random%i;struct hfw_entry tmp=eg_ram[i-1];eg_ram[i-1]=eg_ram[j];eg_ram[j]=tmp;}
  assert(eg_ram_index_prepare());
  for(unsigned i=0;i<n;i++)for(unsigned k=0;k<7;k++){
   uint64_t base=eg_ram[i].base,end=base+eg_ram[i].length;
   uint64_t pa=k<5?base+(uint64_t)k*4096:k==5?base-4096:end&~4095ull;
   assert(eg_ram_page(NULL,pa)==ram_reference(pa));queries++;
  }
  const uint64_t bad[]={0,1,0xfed90000,0xfed91000,ECAM_BOOT_PA,1ull<<39,(1ull<<39)-4096};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(eg_ram_page(NULL,bad[i])==ram_reference(bad[i]));
 }
 eg_count=1;eg_ram[0]=(struct hfw_entry){.base=0x4000000,.length=8192,.type=0};assert(eg_ram_index_prepare());
 assert(eg_ram_page(NULL,0x4000000));eg_ram[0].type=1;assert(!eg_ram_page(NULL,0x4000000));
 eg_ram[0].type=0;eg_ram[0].length=4095;assert(!eg_ram_page(NULL,0x4000000));
 for(unsigned bad=0;bad<4;bad++){
  eg_count=2;eg_ram[0]=(struct hfw_entry){0x4000000,8192,0};eg_ram[1]=(struct hfw_entry){0x5000000,4096,2};
  if(bad==0)eg_ram[1].base=0x4001000;
  if(bad==1)eg_ram[1].length=0;
  if(bad==2)eg_ram[1].type=9;
  if(bad==3){eg_ram[1].base=UINT64_MAX-1;eg_ram[1].length=4096;}
  assert(!eg_ram_index_prepare() && !eg_ram_page(NULL,0x4000000));
 }
 eg_count=0;assert(!eg_ram_index_prepare());eg_count=129;assert(!eg_ram_index_prepare());
 eg_count=1;eg_ram[0]=(struct hfw_entry){0x4000000,8192,0};assert(eg_ram_index_prepare());
 eg_count=2;assert(!eg_ram_page(NULL,0x4000000));eg_count=1;eg_ram_index[0]=255;assert(!eg_ram_page(NULL,0x4000000));
 printf("ram-index PASS %u source-row equivalence queries, unsorted/all-types/boundary/gap/max/malformed/stale-index and live row classification\n",queries);
}
int main(int argc,char **argv){
 if(argc>1 && !strcmp(argv[1],"index-contracts")){ram_index_contracts();return 0;}
 setup();irq_flags=0x202;
 if(argc>1 && !strcmp(argv[1],"max")){for(unsigned i=5;i<128;i++){map_entries[i]=(struct limine_memmap_entry){.base=0x100000000ull+i*8192,.length=4096,.type=1};map_ptrs[i]=map_entries+i;}map_response.entry_count=128;}
 bool refusal=argc>1 && strcmp(argv[1],"max") && strcmp(argv[1],"poison") && strcmp(argv[1],"changed-cache") && strcmp(argv[1],"changed-catalog");
 if(argc>1 && !strcmp(argv[1],"delayed"))delay_in_census=true;
 if(argc>1 && !strcmp(argv[1],"refuse"))foreign_cpu=true;
 if(argc>1 && !strcmp(argv[1],"no-nx"))no_nx=true;
 if(argc>1 && !strcmp(argv[1],"wp"))cr0_value&=~(1ull<<16);
 if(argc>1 && !strcmp(argv[1],"ap"))mp_input.cpu[1].launched=true;
 if(argc>1 && !strcmp(argv[1],"cr4"))cr4_value|=1ull<<12;
 if(argc>1 && !strcmp(argv[1],"gap"))map_entries[3].length-=4096;
 if(argc>1 && !strcmp(argv[1],"mcfg"))((uint8_t *)pages[5])[256+9]^=1;
 if(argc>1 && (!strcmp(argv[1],"poison") || !strcmp(argv[1],"changed-cache") || !strcmp(argv[1],"changed-catalog"))){terminal_expected=true;if(!strcmp(argv[1],"poison")){store_fail_at=1027;store_after=true;}else if(!strcmp(argv[1],"changed-cache"))changed_cache=true;else changed_catalog=true;if(!setjmp(halted)){haswell_air61_window_run();assert(false);}assert(aw.r.state==AIR61_WINDOW_POISONED && aw.r.claimed && !frees && !(irq_flags&512));return 0;}
 bool ok=haswell_air61_window_run();assert(irq_flags==0x202);
 if(!refusal){assert(ok && aw.r.state==AIR61_WINDOW_RETIRED && frees==2 && eg.phases==2 && eg.cache_reads[0]==49 && eg.cache_reads[1]==49 && eg.firmware_bytes==1840);assert(aw.evidence.linked && aw.evidence.retired);}
 else assert(!ok && !frees && !allocs);
 if(argc>1 && !strcmp(argv[1],"delayed"))assert(aw.r.gate_end && aw.census.prepare_reads && !aw.census.reads[0] && !strcmp(aw.r.fault,"clock/deadline"));
 unsigned n=loads;bool reported=haswell_air61_window_report("session-0000000000000001-00");assert(reported==ok);
 haswell_air61_window_reference("session-0000000000000001-00",true);assert(loads==n);
 puts(ok?"laptop-capture: completed=yes":"laptop-capture: completed=NO");return 0;
}
#endif
