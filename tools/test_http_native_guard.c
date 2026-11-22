/* Link the actual HTTP ownership check into the native xHCI HOST fixture.
 * This does not model an ELF or scheduler lifetime; it closes the previous
 * gap between the real native event handler and the generic OWNER latch. */
#define SRVROS_HTTP_NETWORK_HOST_TEST
#include "../kernel/src/http_network_probe.c"
uint64_t http_network_test_irq_save(void){return 0;}
void http_network_test_irq_restore(uint64_t flags){(void)flags;}
bool http_native_test_current(struct http_network_probe *s){return current(s);}
