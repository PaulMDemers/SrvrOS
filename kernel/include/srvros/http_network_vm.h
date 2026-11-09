#ifndef SRVROS_HTTP_NETWORK_VM_H
#define SRVROS_HTTP_NETWORK_VM_H
#include <stdbool.h>
/* Explicit ram-storage,isolated-init guest fixture only; no physical recipe or
 * native Wi-Fi/key/USB acceptance. Permit is published by checked boot owner. */
void http_network_vm_enable(bool explicit_isolated_ready);
bool http_network_vm_run(void);
#endif
