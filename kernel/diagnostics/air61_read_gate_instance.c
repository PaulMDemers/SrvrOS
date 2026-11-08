/* Independent fresh gate for one lifetime. Never reset in native builds. */
#include <srvros/air61_read_window.h>
#include <srvros/ecam_boot.h>
#define J0(a,b) a##b
#define J(a,b) J0(a,b)
#define FN(a) J(a,AIR61_READ_GATE_INSTANCE)
#define SRVROS_AIR61_WINDOW_GATE
#define haswell_ecam_boot_gate_result FN(air61_read_gate_result)
#define haswell_ecam_boot_gate_diagnostic FN(air61_read_gate_diagnostic)
#define vmm_air61_window_copy_ram(pa,out,len) vmm_air61_read_window_copy_ram(AIR61_READ_GATE_INSTANCE,pa,out,len)
#include "haswell_ecam_boot.c"
#undef vmm_air61_window_copy_ram
#undef haswell_ecam_boot_gate_result
#undef haswell_ecam_boot_gate_diagnostic
const struct ecam_boot_gate_result *FN(air61_read_gate_result)(void){return &eg;}
bool FN(air61_read_gate_check)(unsigned phase){
#ifdef SRVROS_ECAM_GATE_HOST_TEST
 extern void air61_read_test_phase(unsigned,unsigned);air61_read_test_phase(AIR61_READ_GATE_INSTANCE,phase);
#endif
 if(!phase){if(eg_started)return false;eg_started=true;eg_root=vmm_kernel_address_space();}
 return eg_check(NULL,phase);
}
uint64_t FN(air61_read_gate_ram_steps)(void){return eg_ram_steps;}
bool FN(air61_read_gate_ram)(uint64_t pa){return eg_ram_page(NULL,pa);}
static bool touch_private(void *base,unsigned length){
 volatile uint8_t *p=base;
 for(unsigned at=0;at<length;at+=4096){if(!vmm_air61_read_window_prime_room())return false;p[at]=p[at];}
 if(!vmm_air61_read_window_prime_room())return false;
 p[length-1]=p[length-1];
 return true;
}
bool FN(air61_read_gate_touch)(void){
 return !eg_started && touch_private(&eg,sizeof(eg)) && touch_private(&eg_diagnostic,sizeof(eg_diagnostic)) &&
  touch_private(eg_ram,sizeof(eg_ram)) && touch_private(&eg_count,sizeof(eg_count)) && touch_private(&eg_phase,sizeof(eg_phase)) &&
  touch_private(eg_ram_index,sizeof(eg_ram_index)) && touch_private(&eg_ram_index_count,sizeof(eg_ram_index_count)) &&
  touch_private(&eg_ram_index_ready,sizeof(eg_ram_index_ready)) && touch_private(&eg_ram_steps,sizeof(eg_ram_steps)) &&
  touch_private(&eg_started,sizeof(eg_started)) && touch_private(&eg_broken,sizeof(eg_broken)) && touch_private(&eg_root,sizeof(eg_root)) &&
  touch_private(&eg_last_policy,sizeof(eg_last_policy)) && touch_private(eg_policy_ready,sizeof(eg_policy_ready));
}
#ifdef SRVROS_ECAM_GATE_HOST_TEST
#include <string.h>
void FN(air61_read_gate_reset)(void){
 memset(&eg,0,sizeof(eg));memset(&eg_diagnostic,0,sizeof(eg_diagnostic));
 memset(eg_policy_ready,0,sizeof(eg_policy_ready));eg_count=eg_phase=0;
 eg_started=eg_broken=false;eg_root=0;eg_ram_index_ready=false;eg_ram_index_count=0;eg_ram_steps=0;
}

#endif
