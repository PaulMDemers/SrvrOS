#ifndef DESK_POWER_TRIAL_H
#define DESK_POWER_TRIAL_H
#include <stdint.h>
/* Diagnostic timeline only. No changes to application or service deadlines. */
struct desk_power_trial {
    unsigned stage;
    uint64_t origin,last,began,start[3],end[3],first[3],generation[3];
};
static int desk_power_step(struct desk_power_trial *t,uint64_t now,int ready,int fresh,
                           uint64_t generation,int work_idle,unsigned cycles) {
    if(now<t->last || now<t->origin || now-t->origin>270000000)return -1;
    t->last=now;
    if(!t->stage) {
        if(now-t->origin>30000000)return -1;
        if(ready && fresh) { t->stage=1;t->began=now;return 1; }
        return 0;
    }
    if(t->stage==5)return 0;
    if(!fresh)return -1;
    uint64_t duration=t->stage==1?30000000:60000000;
    if(now-t->began<duration)return 0;
    if(now-t->began>duration+5000000)return -1;
    if(t->stage>=2) {
        unsigned i=t->stage-2;
        if(generation<=t->first[i] || (t->stage==3 && (!work_idle || cycles<6)))return -1;
        t->end[i]=now;t->generation[i]=generation;
    }
    t->stage++;t->began=now;
    if(t->stage<5) {t->start[t->stage-2]=now;t->first[t->stage-2]=generation;}
    return 1;
}
#endif
