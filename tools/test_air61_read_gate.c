/* Two independent instances of the actual gate; host hardware primitives only. */
#include <string.h>
#include <srvros/air61_read_window.h>
#include <srvros/ecam_boot.h>
#define J0(a,b) a##b
#define J(a,b) J0(a,b)
#define FN(a) J(a,AIR61_READ_GATE_PASS)
#define SRVROS_ECAM_GATE_HOST_TEST
#define SRVROS_AIR61_WINDOW_GATE
#define haswell_ecam_boot_gate_result FN(test_read_gate_result)
#define haswell_ecam_boot_gate_diagnostic FN(test_read_gate_diagnostic)
#define vmm_air61_window_copy_ram(pa,out,len) vmm_air61_read_window_copy_ram(AIR61_READ_GATE_PASS,pa,out,len)
#include "../kernel/diagnostics/haswell_ecam_boot.c"
#undef vmm_air61_window_copy_ram
#undef haswell_ecam_boot_gate_result
#undef haswell_ecam_boot_gate_diagnostic
const struct ecam_boot_gate_result *FN(test_read_gate_result)(void){return &eg;}
bool FN(test_read_gate_check)(unsigned phase){
 if(!phase){if(eg_started)return false;eg_started=true;eg_root=vmm_kernel_address_space();}
 return eg_check(NULL,phase);
}
bool FN(test_read_gate_ram)(uint64_t pa){return eg_ram_page(NULL,pa);}
void FN(test_read_gate_reset)(void){
 memset(&eg,0,sizeof(eg));memset(&eg_diagnostic,0,sizeof(eg_diagnostic));
 memset(eg_policy_ready,0,sizeof(eg_policy_ready));eg_count=eg_phase=0;
 eg_started=eg_broken=false;eg_root=0;eg_ram_index_ready=false;eg_ram_index_count=0;eg_ram_steps=0;
}
