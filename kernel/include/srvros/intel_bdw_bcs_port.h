#ifndef SRVROS_INTEL_BDW_BCS_PORT_H
#define SRVROS_INTEL_BDW_BCS_PORT_H
#include <stdbool.h>

/* One physical blitter submission port on the reference integrated GPU.
 * Callers own the identity object for the entire exclusion. An uncertain
 * descriptor or mapping leaves the port claimed until device recovery. */
bool intel_bdw_bcs_port_claim(const void *owner);
bool intel_bdw_bcs_port_held(const void *owner);
bool intel_bdw_bcs_port_release(const void *owner);

#endif
