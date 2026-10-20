#include "battery_event_hold.h"
static bool valid(const struct battery_event_hold *h,struct battery_event_ops io) {
    return h&&io.read&&io.write&&h->status&&h->enable&&h->status!=h->enable&&
        h->mask&&!(h->original&~h->mask);
}
bool battery_event_hold_begin(struct battery_event_hold *h,struct battery_event_ops io) {
    if(!h)return false;
    if(!h->mask)return !h->active&&!h->poisoned&&!h->status&&!h->enable&&!h->original;
    if(!valid(h,io)||h->active||h->poisoned)return false;
    /* Recheck after platform preflight; do not displace an unexpected owner. */
    if(io.read(io.ctx,h->enable)!=h->original)return false;
    h->active=true;
    io.write(io.ctx,h->enable,0);
    if(io.read(io.ctx,h->enable)){h->poisoned=true;return false;}
    return true;
}
bool battery_event_hold_restore(struct battery_event_hold *h,struct battery_event_ops io) {
    if(!h)return false;
    if(!h->mask)return !h->active&&!h->poisoned&&!h->status&&!h->enable&&!h->original;
    if(!valid(h,io)||!h->active||h->poisoned)return false;
    if(io.read(io.ctx,h->enable)){h->poisoned=true;return false;}
    /* Only after worker, interpreter and SCI retirement and mode restoration. */
    io.write(io.ctx,h->enable,h->original);
    if(io.read(io.ctx,h->enable)!=h->original){h->poisoned=true;return false;}
    h->active=false;return true;
}
uint8_t battery_event_hold_filter(struct battery_event_hold *h,uint16_t port,bool write,uint8_t value) {
    if(h&&h->active&&port==h->status) {
        if(write&&(value&h->mask))h->protected_writes++;
        return value&~h->mask;
    }
    return value;
}
