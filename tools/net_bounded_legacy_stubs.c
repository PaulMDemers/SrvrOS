/* Host-only link support for historical net.c fixtures. Their strong modeled
 * callbacks win; an omitted guard dependency must remain dormant. Never link
 * this file into the kernel or the actual encrypted/bounded-probe fixtures. */
#include <srvros/net_device.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static _Noreturn void unexpected(const char *name) {
    fprintf(stderr,"legacy fixture entered an unbound probe callback: %s\n",name);
    abort();
}
__attribute__((weak)) uint64_t timer_ticks(void) { unexpected("timer_ticks"); }
__attribute__((weak)) uint64_t timer_monotonic_us(void) { unexpected("timer_monotonic_us"); }
__attribute__((weak)) bool net_device_link(struct net_device_link *out) { (void)out;unexpected("net_device_link"); }
__attribute__((weak)) const uint8_t *net_device_mac(void) { unexpected("net_device_mac"); }
__attribute__((weak)) bool process_should_exit_current(void) { unexpected("process_should_exit_current"); }
