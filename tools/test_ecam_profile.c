/* Real gate, mapper, adoption and formatter; fixture hardware only. */
#define ECAM_PROFILE_FIXTURE
#define ECAM_GATE_FIXTURE_LIBRARY
#include "test_ecam_boot_gate.c"
#include <stdarg.h>
#include <setjmp.h>
#include "../kernel/diagnostics/haswell_ecam_report.c"
static jmp_buf terminal_display;
static bool terminal_expected;
static unsigned clears,syncs;
void console_printf(const char *s,...) {
 va_list a;va_start(a,s);
 for(;*s;s++) {
  if(*s!='%'){putchar(*s);continue;}
  s++;
  if(*s=='s')fputs(va_arg(a,const char *),stdout);
  else if(*s=='x')printf("0x%016llx",(unsigned long long)va_arg(a,uint64_t));
  else {assert(*s=='u');printf("%llu",(unsigned long long)va_arg(a,uint64_t));}
 }
 va_end(a);
}
void console_write(const char *s){fputs(s,stdout);}
void console_set_framebuffer_muted(bool b){assert(terminal_expected && !b);}
void console_clear(void){assert(terminal_expected);clears++;}
bool console_sync(void){assert(terminal_expected);syncs++;return true;}
_Noreturn void halt_forever(void){assert(terminal_expected);longjmp(terminal_display,1);}
#ifndef ECAM_PROFILE_FIXTURE_LIBRARY
int main(int argc,char **argv) {
 setup();unsigned count=argc>1 && !strcmp(argv[1],"max")?128:4;
 if(argc>1 && !strcmp(argv[1],"2m"))pages[3][16]=0x2000083;
 if(argc>1 && !strcmp(argv[1],"1g"))pages[2][0]=0x83;
 map_entries[2]=(struct limine_memmap_entry){.base=0xfed1c000,.length=0x4000,.type=1};
 map_entries[3]=(struct limine_memmap_entry){.base=0xffe10000,.length=0x30000,.type=1};
 for(unsigned i=4;i<count;i++)map_entries[i]=(struct limine_memmap_entry){.base=0x100000000ull+i*8192,.length=4096,.type=1};
 for(unsigned i=0;i<count;i++)map_ptrs[i]=map_entries+i;
 map_response.entry_count=count;
 assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff());
 if(argc>1 && !strncmp(argv[1],"failure",7)) {
  if(!strcmp(argv[1],"failure-deadline")){stop_clock=true;frozen_time=eb.r.begin+100000;}
  else if(!strcmp(argv[1],"failure-clock")){stop_clock=true;frozen_time=eb.last-1;}
  else pages[1][12]^=32;
  assert(!haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
  unsigned before=loads,writes_before=stores;uint64_t ticks_before=clock_ticks;
  struct ecam_boot_diagnostic first=eb_diagnostic;
  terminal_expected=true;
  if(!setjmp(terminal_display))haswell_ecam_boot_fail_closed("fixture adoption failed");
  assert(loads==before && stores==writes_before && clock_ticks==ticks_before+(stop_clock?0:1) && !frees);
  assert(clears==1 && syncs==1 && !memcmp(&first,&eb_diagnostic,sizeof(first)));
  assert(eb.r.state==ECAM_POISONED && !eb.r.adoptions);
  return 0;
 }
 assert(haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
 assert(eb.r.adoptions==1 && eb.r.words<65536 && !frees && invalidates==39936);
 unsigned n=loads;assert(!haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));assert(loads==n);
 clock_ticks+=16000000;
 assert(!haswell_ecam_boot_report(NULL));
 assert(!haswell_ecam_boot_report("session-x"));
 assert(haswell_ecam_boot_report("/work/srvros-work/Logs/session-0000000000000001-00"));
 n=total;assert(!haswell_ecam_boot_report("session-0000000000000001-00") && total==n);
 haswell_ecam_boot_reference("session-0000000000000001-01",true);
 haswell_ecam_boot_reference("session-0000000000000001-00",true);
 return 0;
}

#endif
