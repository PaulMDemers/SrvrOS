/* Private optional VM gate test; full run is deliberately not executed. */
#include <assert.h>
#include "../kernel/src/http_network_vm.c"
int main(void){
 http_network_vm_enable(false,true);assert(!enabled && !real_dns);
 http_network_vm_enable(true,false);assert(enabled && !real_dns);
 http_network_vm_enable(true,true);assert(enabled && real_dns);
 http_network_vm_enable(false,true);assert(!enabled && !real_dns);
 http_network_vm_enable(true,true);attempted=true;
 http_network_vm_enable(false,false);assert(enabled && real_dns);
 return 0;
}
