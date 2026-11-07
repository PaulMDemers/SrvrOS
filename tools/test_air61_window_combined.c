/* Same fixture hardware for early ECAM and the real later window gate/backend. */
#define ECAM_TEST_FRAMES 10
#define WINDOW_COMBINED_FIXTURE
#define ECAM_PROFILE_FIXTURE_LIBRARY
#include "test_ecam_profile.c"
#include <srvros/air61_window.h>
uint64_t eg_test_irq_save(void){uint64_t f=irq_flags;irq_flags&=~512ull;return f;}
void eg_test_irq_restore(uint64_t f){irq_flags=f;}
void window_test_halt(void){assert(false);}
int main(int argc,char **argv){
 setup();memset(&aw,0,sizeof(aw));
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
 table_info[0].physical_address=0x2008000;table_info[1].physical_address=0x2008100;
 if(argc>1 && !strcmp(argv[1],"native-hhdm")){
  pages[0][256]|=4;pages[2][0]|=4;pages[3][16]|=4;
 }
 assert(haswell_ecam_boot_prepare() && haswell_ecam_boot_handoff() && haswell_ecam_boot_adopt(ECAM_BOOT_VA,ECAM_BOOT_PA,ECAM_BOOT_BYTES));
 if(argc>1 && !strcmp(argv[1],"ad-transition"))pages[4][2]|=32;
 clock_ticks+=160000;
 assert(haswell_air61_window_run());
 assert(aw.r.state==AIR61_WINDOW_RETIRED && aw.r.frees==2 && aw.evidence.retired);
 clock_ticks+=16000000;
 assert(haswell_ecam_boot_report("session-0000000000000001-00"));
 unsigned oldloads=loads;assert(haswell_air61_window_report("session-0000000000000001-00") && oldloads==loads);
 haswell_air61_window_reference("session-0000000000000001-00",true);
 haswell_ecam_boot_reference("session-0000000000000001-00",true);
 puts("laptop-capture: completed=yes");return 0;
}
