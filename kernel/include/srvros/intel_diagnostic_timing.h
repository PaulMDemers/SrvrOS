#ifndef SRVROS_INTEL_DIAGNOSTIC_TIMING_H
#define SRVROS_INTEL_DIAGNOSTIC_TIMING_H
/* Software aggregate policy, not an individual hardware-operation timeout.
 * Only the separately linked VT-d image selects the reviewed150ms policy.
 * Admission/source age, operation waits and enclosing owners remain unchanged. */
#ifdef SRVROS_INTEL_VTD_DRAIN
#define INTEL_DIAGNOSTIC_PREPARE_US 150000ull
#else
#define INTEL_DIAGNOSTIC_PREPARE_US 100000ull
#endif
#endif
