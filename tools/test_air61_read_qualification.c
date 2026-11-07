/* Actual native bridge + two production gate instances + exact encoder. */
#ifdef SRVROS_ECAM_GATE_HOST_TEST
#undef SRVROS_ECAM_GATE_HOST_TEST
#endif
#define AIR61_READ_WINDOW_LIBRARY
#include "test_air61_read_window.c"
#undef main
#include <srvros/air61_read_qualification.h>
extern void air61_read_bridge_test_reset(void),air61_read_report_test_reset(void);
static uint8_t output[AIR61_READ_RECEIPT_CAP];
int main(int argc,char **argv){
 assert(argc>=2);const char *mode=argv[1];
 rreset();air61_read_bridge_test_reset();air61_read_report_test_reset();irq_flags=0x202;
 bool maximum=!strcmp(mode,"maximum") || !strcmp(mode,"source-change");
 if(maximum){for(unsigned i=5;i<128;i++){map_entries[i]=(struct limine_memmap_entry){.base=0x100000000ull+i*8192,.length=4096,.type=1};map_ptrs[i]=map_entries+i;}map_response.entry_count=128;}
 if(!strcmp(mode,"ancestry")){pages[0][256]|=4;pages[2][0]|=4;pages[3][16]|=4;}
 if(!strcmp(mode,"foreign"))foreign_cpu=true;
 if(!strcmp(mode,"ap"))mp_input.cpu[1].launched=true;
 if(!strcmp(mode,"wp"))cr0_value&=~(1ull<<16);
 if(!strcmp(mode,"nx"))no_nx=true;
 if(!strcmp(mode,"clock"))no_reference=true;
 if(!strcmp(mode,"delay"))delay_in_census=true;
 if(!strcmp(mode,"borrowed-A"))pages[0][384]&=~32ull;
 if(!strcmp(mode,"boot"))boot.adoptions=0;
 if(!strcmp(mode,"cache-change"))read_phase_hook_mode=1;
 if(!strcmp(mode,"source-change"))read_phase_hook_mode=2;
 bool poison=!strcmp(mode,"poison0") || !strcmp(mode,"poison1");
 if(poison){terminal_expected=true;store_fail_at=!strcmp(mode,"poison0")?1027:2055;store_after=true;
  if(!setjmp(halted)){haswell_air61_read_qualify();assert(false);}
  assert(rw.result.state==AIR61_READ_POISONED && !(irq_flags&512));
 }else{
  bool ok=haswell_air61_read_qualify();assert(irq_flags==0x202);
  bool expected=!strcmp(mode,"normal") || (maximum && strcmp(mode,"source-change")) || !strcmp(mode,"ancestry") || !strcmp(mode,"small") || !strcmp(mode,"invalid-count") || !strcmp(mode,"oversized-census");
  assert(ok==expected);
  if(!ok){assert(!rw.result.retired || rw.result.retired==1);if(!strcmp(mode,"cache-change") || !strcmp(mode,"source-change"))assert(rw.result.retired==1 && allocs==2 && frees==2);}
 }
 assert(!haswell_air61_read_qualify());
 if(!strcmp(mode,"invalid-count")){struct ecam_boot_gate_result *g=(void *)air61_read_gate_result0();g->firmware[0].sample[0].count=129;}
 if(!strcmp(mode,"oversized-census"))rw.pass[0].view.census.bytes[0]=4097;
 unsigned nloads=loads,nstores=stores,nmsrs=msrs,nports=port_reads,nwrites=port_writes;uint64_t nticks=ticks;
 unsigned written=123;assert(!haswell_air61_read_encode("bad",output,sizeof(output),&written) && !written);
 bool ok=haswell_air61_read_encode("session-0000000000000001-00",output,!strcmp(mode,"small")?16:sizeof(output),&written);
 bool expected=!strcmp(mode,"normal") || !strcmp(mode,"maximum") || !strcmp(mode,"ancestry");
 assert(ok==expected && (ok?written>12:!written));
 if(ok){assert(argc==3);FILE *f=fopen(argv[2],"wb");assert(f);assert(fwrite(output,1,written,f)==written);assert(!fclose(f));printf("receipt=%u raw bounded; two fresh production gates PASS\n",written);}
 assert(loads==nloads && stores==nstores && msrs==nmsrs && port_reads==nports && port_writes==nwrites && ticks==nticks);
 written=123;assert(!haswell_air61_read_encode("session-0000000000000001-00",output,sizeof(output),&written) && !written);
 return 0;
}
