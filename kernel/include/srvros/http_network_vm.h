#ifndef SRVROS_HTTP_NETWORK_VM_H
#define SRVROS_HTTP_NETWORK_VM_H
#include <stdbool.h>
/* Explicit ram-storage,isolated-init guest fixture only; no physical recipe or
 * native Wi-Fi/key/USB acceptance. Permit is published by checked boot owner. */
/* require_real_dns is honored only with the explicit isolated VM permit.
 * It chooses the unchanged production DHCP/DNS/route bootstrap (zero override). */
void http_network_vm_enable(bool explicit_isolated_ready,bool require_real_dns);
bool http_network_vm_run(void);
#endif
