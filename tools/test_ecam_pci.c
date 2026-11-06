/* Actual PCI adoption route; reaching config ports or fallback maps aborts. */
#define ECAM_PROFILE_FIXTURE_LIBRARY
#define haswell_ecam_boot_fail_closed ecam_formatter_terminal
#include "test_ecam_profile.c"
#undef haswell_ecam_boot_fail_closed
#include <setjmp.h>
#define SRVROS_PCI_HOST_TEST
#define SRVROS_X86_64_IO_H
static void outb(uint16_t p,uint8_t v){(void)p;(void)v;abort();}
static void outw(uint16_t p,uint16_t v){(void)p;(void)v;abort();}
static void outl(uint16_t p,uint32_t v){(void)p;(void)v;abort();}
static uint32_t inl(uint16_t p){(void)p;abort();}
static jmp_buf terminal;
_Noreturn void haswell_ecam_boot_fail_closed(const char *reason){assert(reason);longjmp(terminal,1);}
#include "../kernel/src/arch/x86_64/pci.c"
int main(void) {
 setup();assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff());
 unsigned n=stores,alloc_before=allocs;
 assert(map_ecam_windows() && ecam_virtual_base[0]==ECAM_BOOT_VA);
 assert(stores==n && allocs==alloc_before && eb.r.adoptions==1 && !frees);
 if(!setjmp(terminal)){(void)map_ecam_windows();assert(false);}
 for(volatile unsigned mode=0;mode<10;mode++) {
  setup();memset(ecam_virtual_base,0,sizeof(ecam_virtual_base));
  assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff());
  if(mode==0)allocation.present=false;
  if(mode==1)allocation.pci_segment=1;
  if(mode==2)allocation.start_bus=1;
  if(mode==3)allocation.end_bus=154;
  if(mode==4)allocation.base_address+=4096;
  if(mode==5)eb.r.state=ECAM_READY;
  if(mode==6)eb.r.state=ECAM_POISONED;
  if(mode==7)pages[5][1]^=64;
  if(mode==8)kernel_mappings[0].flags^=1;
  if(mode==9)cr4_value|=1ull<<12;
  if(!setjmp(terminal)){(void)map_ecam_windows();assert(false);}
  assert(!ecam_virtual_base[0] && !eb.r.adoptions && !frees && allocs==1 && stores==513);
 }
 puts("actual PCI adoption PASS: no remap/config access; duplicate, geometry, state, table, registry and fresh-control failures terminate");
}
