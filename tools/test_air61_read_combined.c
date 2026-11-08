/* Same fixture hardware for early ECAM and the real later window gate/backend. */
#define ECAM_TEST_FRAMES 10
#define WINDOW_COMBINED_FIXTURE
#define READ_COMBINED_FIXTURE
#define READ_ONLY_COMBINED_FIXTURE
#define ECAM_PROFILE_FIXTURE_LIBRARY
#include "test_ecam_profile.c"
#include <srvros/air61_read_qualification.h>
void air61_read_test_phase(unsigned pass,unsigned phase){(void)pass;eg_phase=phase;}
uint64_t eg_test_irq_save(void){uint64_t f=irq_flags;irq_flags&=~512ull;return f;}
void eg_test_irq_restore(uint64_t f){irq_flags=f;}
#ifndef AIR61_REGISTER_SAMPLE_FIXTURE
void window_test_halt(void){fflush(stdout);fprintf(stderr,"fault=%s\n",rw.result.fault);abort();}
#endif
#ifdef AIR61_REGISTER_SAMPLE_FIXTURE
int sample_producer_main(int argc,char **argv){
#else
int main(int argc,char **argv){
#endif
 setup();memset(&rw,0,sizeof(rw));
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 extern void air61_sample_test_setup_text(void);air61_sample_test_setup_text();
#endif
 unsigned count=argc>1 && !strcmp(argv[1],"max")?128:5;
 map_entries[0]=(struct limine_memmap_entry){.base=0x2000000,.length=8*4096,.type=0};
 map_entries[1]=(struct limine_memmap_entry){.base=0x2008000,.length=4096,.type=2};
 map_entries[2]=(struct limine_memmap_entry){.base=0x2009000,.length=4096,.type=0};
 map_entries[3]=(struct limine_memmap_entry){.base=0xfed1c000,.length=0x4000,.type=1};
 map_entries[4]=(struct limine_memmap_entry){.base=0xffe10000,.length=0x30000,.type=1};
 for(unsigned i=5;i<count;i++)map_entries[i]=(struct limine_memmap_entry){.base=0x100000000ull+i*8192,.length=4096,.type=1};
 for(unsigned i=0;i<count;i++)map_ptrs[i]=map_entries+i;
 map_response.entry_count=count;
 memcpy(pages[8],pages[6],136);memcpy((uint8_t *)pages[8]+256,pages[7],60);
 tags[6]=tags[7]=PMM_FRAME_FREE;
 table_info[0].physical_address=0x2008000;table_info[1].physical_address=0x2008100;
 if(argc>1 && !strcmp(argv[1],"native-hhdm")){
  pages[0][256]|=4;pages[2][0]|=4;pages[3][16]|=4;
 }
 pages[0][384]|=32;
 assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff() && haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
 if(argc>1 && !strcmp(argv[1],"ad-transition"))pages[4][2]|=32;
 clock_ticks+=160000;
 terminal_expected=true;
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 assert(haswell_air61_register_sample());
#else
 assert(haswell_air61_read_qualify());
#endif
 assert(rw.result.state==AIR61_READ_COMPLETE && rw.result.retired==2 && frees==4);
 clock_ticks+=16000000;
 assert(haswell_ecam_boot_report("session-0000000000000001-00"));
#ifdef SRVROS_AIR61_REGISTER_SAMPLE
 if(argc>1 && !strcmp(argv[1],"small-wire")){
  uint8_t tiny[16];unsigned written=9;
  assert(!haswell_air61_read_encode("session-0000000000000001-00",tiny,sizeof(tiny),&written) && !written);
  written=9;assert(!haswell_air61_read_encode("session-0000000000000001-00",tiny,sizeof(tiny),&written) && !written);
  puts("sample-fixture: bounded encoder refuses overflow and retry");return 0;
 }
#endif
 unsigned oldloads=loads;assert(haswell_air61_read_report("session-0000000000000001-00") && oldloads==loads);
 haswell_air61_read_reference("session-0000000000000001-00",true);
 haswell_ecam_boot_reference("session-0000000000000001-00",true);
 puts("laptop-capture: boot options=srvros.capture=haswell-firmware srvros.hsw-cache=alias-nx-preinit srvros.ecam=air61-2m-test srvros.vtd-window=air61-two-pass-no-read");
 puts("laptop-capture: completed=yes");return 0;
}
