#ifndef SRVROS_BATTERY_TRANSITION_H
#define SRVROS_BATTERY_TRANSITION_H
#include <srvros/sbs_sample.h>
#define BATTERY_TRANSITION_PHASES 3u
#define BATTERY_TRANSITION_STABLE 3u
#define BATTERY_TRANSITION_WAIT_US 120000000ull
struct battery_transition {
    unsigned phase,stable;
    uint64_t phase_started,last_now,last_generation,last_end,first_generation,first_us;
    bool failed;
};
/* Returns -1 for timeout/invalid evidence, 0 while waiting, 1 after all phases.
 * The caller retires normally on -1; a hardware/worker fault uses containment. */
static inline int battery_transition_step(struct battery_transition *t,
    const struct sbs_snapshot *s,bool ac,bool coherent,uint64_t ac_us,uint64_t now) {
    if(t->failed)return -1;
    if(t->phase==BATTERY_TRANSITION_PHASES)return 1;
    const unsigned required=SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CURRENT|SBS_VALID_CAPACITY;
    if(now<t->last_now||now<t->phase_started||now-t->phase_started>=BATTERY_TRANSITION_WAIT_US||
       (s->valid&required)!=required||s->relative_soc>100||!s->voltage_uv||!s->full_capacity||
       (s->capacity_unit!=SBS_CAPACITY_UAH&&s->capacity_unit!=SBS_CAPACITY_UWH)||
       s->generation!=t->last_generation+1||!ac_us||ac_us>s->started_us||
       s->started_us>s->completed_us||s->completed_us>now||
       now-s->completed_us>1000000||ac_us<=t->last_end) {
        t->failed=true;return -1;
    }
    t->last_now=now;t->last_generation=s->generation;t->last_end=s->completed_us;
    bool matches=coherent&&(t->phase==1?(!ac&&s->current_ua<0):(ac&&s->current_ua>=0));
    if(!matches){t->stable=0;t->first_generation=t->first_us=0;return 0;}
    if(!t->stable){t->first_generation=s->generation;t->first_us=ac_us;}
    if(++t->stable==BATTERY_TRANSITION_STABLE) {
        t->phase++;t->stable=0;t->phase_started=now;
    }
    return t->phase==BATTERY_TRANSITION_PHASES?1:0;
}
#endif
