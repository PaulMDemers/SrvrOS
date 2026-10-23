#ifndef SRVROS_DESKTOP_BATTERY_TRIAL_H
#define SRVROS_DESKTOP_BATTERY_TRIAL_H
#include <srvros/battery_abi.h>
#define DESK_BATTERY_PHASE_US 120000000ull
/* Read-only acceptance state. Duplicate cached reads cannot advance a phase.
 * A screenshot must finish before the caller advances to the next instruction. */
struct desk_battery_trial {
    unsigned phase,stable;
    uint64_t began,last_now,epoch,generation,sampled,first_generation;
};
static inline int desk_battery_trial_step(struct desk_battery_trial *t,
    const struct srv_battery_status *s,uint64_t now) {
    if(t->phase>=3)return 1;
    if(now<t->last_now || now<t->began || now-t->began>=DESK_BATTERY_PHASE_US)return -1;
    t->last_now=now;
    unsigned required=SRV_BATTERY_SOC_VALID|SRV_BATTERY_VOLTAGE_VALID|SRV_BATTERY_CURRENT_VALID|SRV_BATTERY_CAPACITY_VALID;
    if(!s || s->version!=SRV_BATTERY_VERSION || s->size!=sizeof(*s) || s->state!=SRV_BATTERY_ACTIVE ||
       (s->flags&(SRV_BATTERY_VALID|SRV_BATTERY_FRESH|SRV_BATTERY_STALE))!=(SRV_BATTERY_VALID|SRV_BATTERY_FRESH) ||
       (s->valid&required)!=required || s->age_us>5000000 || !s->generation || !s->epoch ||
       s->soc>100 || !s->voltage_uv || !s->full_capacity ||
       (s->capacity_unit!=SRV_BATTERY_CAPACITY_UAH && s->capacity_unit!=SRV_BATTERY_CAPACITY_UWH) ||
       !s->ac_sampled_us || s->ac_sampled_us>s->sampled_us || s->sampled_us>now) {
        t->stable=0;return 0;
    }
    if(t->epoch && (s->epoch!=t->epoch || s->generation<t->generation || s->sampled_us<t->sampled))return -1;
    t->epoch=s->epoch;
    if(s->generation==t->generation)return 0;
    if(t->generation && (s->generation!=t->generation+1 || s->ac_sampled_us<=t->sampled))t->stable=0;
    t->generation=s->generation;t->sampled=s->sampled_us;
    int ac=!!(s->flags&SRV_BATTERY_AC_PRESENT);
    if(t->phase==1?(ac || s->current_ua>=0):(!ac || s->current_ua<0)){t->stable=0;return 0;}
    if(!t->stable)t->first_generation=s->generation;
    return ++t->stable>=3;
}
static inline void desk_battery_trial_advance(struct desk_battery_trial *t,uint64_t now) {
    t->phase++;t->stable=0;t->began=now;
}
#endif
