#include <srvros/display_access.h>
#define BLOCKED (1ull<<63)
static uint64_t state, epoch, owner, pins;
static bool fault;
bool display_access_enter(void) {
    uint64_t value=__atomic_load_n(&state,__ATOMIC_ACQUIRE);
    for (;;) {
        if(value&BLOCKED || value==BLOCKED-1) return false;
        if(__atomic_compare_exchange_n(&state,&value,value+1,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return true;
    }
}
void display_access_leave(void) {
    /* Only successful entrants may leave. A mismatched leave is a sticky
     * programming fault, never an implicit unpause. */
    uint64_t value=__atomic_load_n(&state,__ATOMIC_RELAXED);
    if(!value || value&BLOCKED) {fault=true;__atomic_store_n(&state,BLOCKED,__ATOMIC_RELEASE);return;}
    __atomic_fetch_sub(&state,1,__ATOMIC_RELEASE);
}
bool display_access_allowed(void) {return !(__atomic_load_n(&state,__ATOMIC_ACQUIRE)&BLOCKED);}
bool display_access_acquire(uint64_t who,struct display_access_token *out) {
    uint64_t expected=0;
    if(!who || !out || fault || epoch==UINT64_MAX ||
       !__atomic_compare_exchange_n(&state,&expected,BLOCKED,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return false;
    owner=who;++epoch;pins=0;*out=(struct display_access_token){epoch,owner};return true;
}
bool display_access_held(const struct display_access_token *t) {
    return t && !fault && owner && t->owner==owner && t->epoch==epoch &&
        __atomic_load_n(&state,__ATOMIC_ACQUIRE)==BLOCKED;
}
bool display_access_pin(const struct display_access_token *t) {
    if(!display_access_held(t) || pins==UINT64_MAX)return false;
    ++pins;return true;
}
bool display_access_unpin(const struct display_access_token *t) {
    if(!display_access_held(t) || !pins)return false;
    --pins;return true;
}
bool display_access_release(const struct display_access_token *t,bool clean) {
    if(!display_access_held(t))return false;
    if(!clean){fault=true;return false;}
    if(pins)return false;
    owner=0;__atomic_store_n(&state,0,__ATOMIC_RELEASE);return true;
}
