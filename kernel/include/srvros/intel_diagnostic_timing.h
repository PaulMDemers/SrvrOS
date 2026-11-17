#ifndef SRVROS_INTEL_DIAGNOSTIC_TIMING_H
#define SRVROS_INTEL_DIAGNOSTIC_TIMING_H
/* Software aggregate policy, not an individual hardware-operation timeout.
 * Only separately linked VT-d and full-GT images select the reviewed150ms policy.
 * The explicit seconds variant widens only aggregate parent/software clocks.
 * Source age, individual hardware waits and poll caps remain unchanged. */
#ifdef SRVROS_INTEL_GT_SECONDS
#ifndef SRVROS_INTEL_GT_BOOTSTRAP_NATIVE
#error "Multi-second policy belongs only to the separate full-GT diagnostic"
#endif
#define INTEL_DIAGNOSTIC_PREPARE_US 3000000ull
#define INTEL_DIAGNOSTIC_GT_PHASE_US 3000000ull
#define INTEL_DIAGNOSTIC_GT_TOTAL_US 4000000ull
#define INTEL_DIAGNOSTIC_HDA_US 5000000ull
#define INTEL_DIAGNOSTIC_OUTER_US 6000000ull
#elif defined(SRVROS_INTEL_VTD_DRAIN) || defined(SRVROS_INTEL_GT_BOOTSTRAP_NATIVE)
#define INTEL_DIAGNOSTIC_PREPARE_US 150000ull
#else
#define INTEL_DIAGNOSTIC_PREPARE_US 100000ull
#endif
#ifndef SRVROS_INTEL_GT_SECONDS
#define INTEL_DIAGNOSTIC_GT_PHASE_US 100000ull
#define INTEL_DIAGNOSTIC_GT_TOTAL_US 300000ull
#define INTEL_DIAGNOSTIC_HDA_US 400000ull
#define INTEL_DIAGNOSTIC_OUTER_US 500000ull
#endif
#endif
