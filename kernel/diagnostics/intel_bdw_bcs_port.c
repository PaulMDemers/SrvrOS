#include <stddef.h>
#include <srvros/intel_bdw_bcs_port.h>

static const void *port_owner;

bool intel_bdw_bcs_port_claim(const void *owner)
{
    const void *available = NULL;
    return owner && __atomic_compare_exchange_n(&port_owner, &available, owner, false,
                                                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

bool intel_bdw_bcs_port_held(const void *owner)
{
    return owner && __atomic_load_n(&port_owner, __ATOMIC_ACQUIRE) == owner;
}

bool intel_bdw_bcs_port_release(const void *owner)
{
    return owner && __atomic_compare_exchange_n(&port_owner, &owner, NULL, false,
                                                __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}
