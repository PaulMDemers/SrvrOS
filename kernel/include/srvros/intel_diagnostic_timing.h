#ifndef SRVROS_INTEL_DIAGNOSTIC_TIMING_H
#define SRVROS_INTEL_DIAGNOSTIC_TIMING_H
/* Software aggregate policy, not an individual hardware-operation timeout.
 * Only separately linked VT-d and full-GT images select the reviewed150ms policy.
 * Admission/source age, operation waits and enclosing owners remain unchanged. */
#if defined(SRVROS_INTEL_VTD_DRAIN) || defined(SRVROS_INTEL_GT_BOOTSTRAP_NATIVE)
#define INTEL_DIAGNOSTIC_PREPARE_US 150000ull
#else
#define INTEL_DIAGNOSTIC_PREPARE_US 100000ull
#endif
#endif
