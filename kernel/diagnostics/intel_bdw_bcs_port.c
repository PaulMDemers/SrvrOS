#include <stddef.h>
#include <srvros/intel_bdw_bcs_port.h>

static const void *port_owner;
static const void *borrower;
static const char releasing;

static bool exchange_borrower(const void *from,const void *to)
{
    return __atomic_compare_exchange_n(&borrower,&from,to,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED);
}

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
    if (!intel_bdw_bcs_port_held(owner) || !exchange_borrower(NULL,&releasing)) return false;
    bool released=__atomic_compare_exchange_n(&port_owner,&owner,NULL,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED);
    __atomic_store_n(&borrower,NULL,__ATOMIC_RELEASE);
    return released;
}

bool intel_bdw_bcs_port_lend(const void *owner,const void *request)
{
    if (!request || owner==request || !intel_bdw_bcs_port_held(owner) ||
        !exchange_borrower(NULL,request)) return false;
    if (intel_bdw_bcs_port_held(owner)) return true;
    exchange_borrower(request,NULL);
    return false;
}

bool intel_bdw_bcs_port_lent(const void *owner,const void *request)
{
    return request && intel_bdw_bcs_port_held(owner) &&
           __atomic_load_n(&borrower,__ATOMIC_ACQUIRE)==request;
}

bool intel_bdw_bcs_port_return(const void *owner,const void *request)
{
    return intel_bdw_bcs_port_lent(owner,request) && exchange_borrower(request,NULL);
}
