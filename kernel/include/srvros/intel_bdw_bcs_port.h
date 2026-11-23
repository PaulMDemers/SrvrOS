#ifndef SRVROS_INTEL_BDW_BCS_PORT_H
#define SRVROS_INTEL_BDW_BCS_PORT_H
#include <stdbool.h>

/* One physical blitter submission port on the reference integrated GPU.
 * Callers own the identity object for the entire exclusion. An uncertain
 * descriptor or mapping leaves the port claimed until device recovery. */
bool intel_bdw_bcs_port_claim(const void *owner);
bool intel_bdw_bcs_port_held(const void *owner);
bool intel_bdw_bcs_port_release(const void *owner);

/* Lend the claimed FIFO to one request. The parent stays visible to its
 * hardware guards and cannot release the port until that request returns. */
bool intel_bdw_bcs_port_lend(const void *owner,const void *request);
bool intel_bdw_bcs_port_lent(const void *owner,const void *request);
bool intel_bdw_bcs_port_return(const void *owner,const void *request);
#endif
