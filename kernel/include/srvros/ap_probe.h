#pragma once
#include <stdbool.h>
#include <stdint.h>
#define AP_PROBE_ROUNDS 32u
#define AP_PROBE_LIMIT_US 20000u
#define AP_PROBE_STOP UINT64_MAX
/* Pure receipt classifier. Late/future/stale acknowledgements never pass. */
static inline int ap_probe_observe(uint64_t expected, uint64_t ack, uint64_t fault,
                                  uint64_t elapsed, bool clock_ok) {
    if (!clock_ok || fault || elapsed > AP_PROBE_LIMIT_US || ack > expected) return -1;
    return ack == expected ? 1 : 0;
}
#ifdef SRVROS_AP_PROBE
bool ap_probe_run(const char *cmdline);
#endif
