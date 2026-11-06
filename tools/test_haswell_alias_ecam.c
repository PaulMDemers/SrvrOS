/* Actual PCI/VMM source, simulated RAM only: no PCI target reads or writes. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_VMM_HOST_TEST
#define SRVROS_PCI_HOST_TEST
#define SRVROS_X86_64_IO_H
static inline void outb(uint16_t p,uint8_t v) {(void)p;(void)v;assert(false);}
static inline void outw(uint16_t p,uint16_t v) {(void)p;(void)v;assert(false);}
static inline void outl(uint16_t p,uint32_t v) {(void)p;(void)v;assert(false);}
static inline uint32_t inl(uint16_t p) {(void)p;assert(false);return 0;}
static uint64_t vmm_test_cr3(void) {return 0x2000000;}
static void vmm_test_write_cr3(uint64_t v) {(void)v;assert(false);}
static uint64_t vmm_test_rflags(void) {return 2;}
static bool model_features;
static void vmm_test_cpuid(uint32_t l,uint32_t *a,uint32_t *b,uint32_t *c,uint32_t *d) {
 assert(model_features);*a=*b=*c=*d=0;
 if(l==0x80000000u)*a=0x80000001u;
 else {assert(l==0x80000001u);*d=1u<<20;}
}
static uint64_t vmm_test_efer(void) {assert(model_features);return 0xd00;}
static uint64_t vmm_test_irq_save(void) {assert(false);return 0;}
static void vmm_test_irq_restore(uint64_t v) {(void)v;assert(false);}
#include "../kernel/src/memory/vmm.c"
#include "../kernel/src/arch/x86_64/pci.c"
#include <srvros/haswell_alias_context.h>
static uint64_t pool[140][512],ticks;
static unsigned used;
static struct srv_acpi_mcfg_allocation allocation;
static struct hac_result result;
uint64_t acpi_mcfg_allocation_count(void) {return 1;}
const struct srv_acpi_mcfg_allocation *acpi_mcfg_allocation_at(uint64_t i) {
 assert(!i);return &allocation;
}
void *pmm_phys_to_virt(uint64_t p) {
 assert(p>=0x2000000 && p<0x2000000+used*4096);return (char *)pool+(p-0x2000000);
}
uint64_t pmm_alloc_frames_above_tagged(uint64_t n,uint64_t m,enum pmm_frame_tag t) {
 assert(n==1 && m==0x2000000 && t==PMM_FRAME_PAGE_TABLE && used<140);
 return 0x2000000+4096*used++;
}
void pmm_free_frame(uint64_t p) {(void)p;assert(false);}
void console_printf(const char *fmt,...) {(void)fmt;assert(false);}
static uint64_t now(void *c) {(void)c;return ++ticks;}
static bool read_word(void *c,uint64_t p,unsigned i,uint64_t *v) {
 (void)c;assert(i<512);assert(vmm_diagnostic_sole_kernel_root(0x2000000));
 *v=((uint64_t *)pmm_phys_to_virt(p))[i];return true;
}
static void check(unsigned buses,bool complete) {
 memset(pool,0,sizeof(pool));used=1;ticks=0;
 kernel_cr3=0x2000000;kernel_pml4=pool[0];kernel_mapping_count=0;
 memset(address_space_records,0,sizeof(address_space_records));address_space_count=0;
 memset(ecam_virtual_base,0,sizeof(ecam_virtual_base));
 allocation=(struct srv_acpi_mcfg_allocation){true,0xe0000000,0,0,buses-1};
 assert(map_ecam_windows());
 unsigned minimum=3+(buses+1)/2;
 assert(used==minimum && kernel_mapping_count==128);
 for(unsigned page=0;page<buses*256;page++) {
  uint64_t va=PCI_ECAM_VIRTUAL_BASE+page*4096;
  uint64_t pa=0;assert(vmm_virt_to_phys_in_space(kernel_cr3,va,&pa));
  assert(pa==0xe0000000+page*4096);
  uint64_t *pdpt=entry_table(kernel_pml4[pml4_index(va)]);
  uint64_t *pd=entry_table(pdpt[pdpt_index(va)]),*pt=entry_table(pd[pd_index(va)]);
  assert(pt[pt_index(va)]==(pa|VMM_PAGE_PRESENT|VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE));
 }
 static uint64_t saved[140][512];memcpy(saved,pool,sizeof(pool));
 const struct hac_io io={.read=read_word,.now=now};
 assert(hac_capture(&result,&io,kernel_cr3)==complete);
 assert(!memcmp(saved,pool,sizeof(pool)));
 if(complete)assert(result.status==1 && result.pages==minimum && result.reads[0]==512*minimum && result.reads[1]==result.reads[0]);
 else assert(result.status==2 && result.pages==64 && !result.reads[1] && result.bytes[0]<4096);
 printf("ecam-layout buses=%u graph_pages=%u complete=%u census_pages=%u reads=%u,%u\n",
        buses,used,complete,result.pages,result.reads[0],result.reads[1]);
}
static void check_large_proposal(void) {
 /* Only a synthetic staged layout: no new production mapper is called. */
 memset(pool,0,sizeof(pool));used=4;ticks=0;
 kernel_cr3=0x2000000;kernel_pml4=pool[0];kernel_mapping_count=0;
 memset(address_space_records,0,sizeof(address_space_records));address_space_count=0;
 const uint64_t va=PCI_ECAM_VIRTUAL_BASE,pa=0xe0000000;
 const uint64_t flags=VMM_PAGE_WRITABLE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_NO_EXECUTE;
 pool[0][pml4_index(va)]=0x2001003;pool[1][pdpt_index(va)]=0x2002003;
 for(unsigned i=0;i<78;i++)pool[2][i]=(pa+((uint64_t)i<<21))|flags|PAGE_LARGE|VMM_PAGE_PRESENT;
 for(unsigned i=0;i<39936;i++) {
  uint64_t v=va+(uint64_t)i*4096,physical=0,entry=0,leaf=0;
  assert(vmm_virt_to_phys_in_space(kernel_cr3,v,&physical) && physical==pa+(uint64_t)i*4096);
  assert(vmm_virt_to_phys_in_space(kernel_cr3,v+4095,&physical) && physical==pa+(uint64_t)i*4096+4095);
  assert(vmm_entry_for_address(kernel_cr3,v,&entry,&leaf) && leaf==(1ull<<21));
  assert(entry==pool[2][i/512] && !(entry&(1ull<<12)) && !(entry&VMM_PAGE_USER));
  register_kernel_mapping(v,pa+(uint64_t)i*4096,flags);
 }
 assert(kernel_mapping_count==128);
 const struct hac_io io={.read=read_word,.now=now};
 assert(hac_capture(&result,&io,kernel_cr3) && result.pages==3 && result.reads[0]==1536 && result.reads[1]==1536);
 static uint64_t saved[3][512];memcpy(saved,pool,sizeof(saved));
 address_space_records[0]=(struct address_space_record){.used=true,.root=0x2003000};
 address_spaces[0]=0x2003000;address_space_count=1;model_features=true;
 pool[3][0]=0x555000;
 assert(vmm_prepare_user_kernel_mappings(0x2003000));
 assert(pool[3][0]==0x555000 && !memcmp(pool[3]+256,pool[0]+256,2048));
 assert(!memcmp(saved,pool,sizeof(saved)));
 vmm_refresh_kernel_mappings(0x2003000);
 assert(pool[3][0]==0x555000 && !memcmp(pool[3]+256,pool[0]+256,2048));
 assert(used==4 && !memcmp(saved,pool,sizeof(saved)));
 pool[2][0]|=1ull<<12;assert(!vmm_prepare_user_kernel_mappings(0x2003000));pool[2][0]&=~(1ull<<12);
 pool[2][0]&=~VMM_PAGE_CACHE_DISABLE;assert(!vmm_prepare_user_kernel_mappings(0x2003000));
 model_features=false;
 puts("2MiB ECAM proposal layout: actual VMM translations/attributes/root preparation and 3-table census PASS; synthetic RAM only");
}
int main(void) {
 check(1,true);check(120,true);check(122,true);check(123,false);check(156,false);check(256,false);
 check_large_proposal();
 puts("actual PCI/VMM ECAM layout and unchanged census bounds PASS; simulated RAM only");
 return 0;
}
