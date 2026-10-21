#ifndef SRVROS_SCI_OBSERVE_H
#define SRVROS_SCI_OBSERVE_H
#include <stdint.h>
#include <stdbool.h>
/* Passive CPU0 observer: no I/O, allocation, logging or ownership decisions.
 * Only first successful reads within one callback are retained. Zero without
 * its validity bit is UNKNOWN. A bounded ring retains the most recent events. */
#define SCI_OBSERVE_CAPACITY 8
struct sci_observe_ports { uint16_t fixed_status,fixed_enable,ec_status,ec_enable; };
struct sci_observation {
    uint64_t sequence,at_us,duration_us,previous_gap_us;
    uint16_t fixed_status,fixed_enable;
    uint8_t ec_status,ec_enable,ec_raw_status,ec_raw_enable,valid,phase;
    bool handled,previous_valid,previous_handled,query_before,query_after;
};
struct sci_observer {
    struct sci_observation rows[SCI_OBSERVE_CAPACITY],current;
    uint64_t total,previous_us;
    unsigned next,count;
    bool active,previous_valid,previous_handled;
};
static inline bool sci_observe_begin(struct sci_observer *o,uint64_t now,uint8_t phase,bool query) {
    if(o->active)return false; /* Do not overwrite an outer observation. */
    o->current=(struct sci_observation){.at_us=now,.phase=phase,.query_before=query,
        .previous_valid=o->previous_valid,.previous_handled=o->previous_handled,
        .previous_gap_us=o->previous_valid&&now>=o->previous_us?now-o->previous_us:0};
    o->active=true;return true;
}
static inline void sci_observe_read(struct sci_observer *o,const struct sci_observe_ports *p,
    uint64_t port,unsigned bits,uint32_t raw,uint32_t visible) {
    if(!o->active)return;
    struct sci_observation *r=&o->current;
    if(bits==16 && port==p->fixed_status && !(r->valid&1)) {
        r->fixed_status=visible;r->valid|=1;
    } else if(bits==16 && port==p->fixed_enable && !(r->valid&2)) {
        r->fixed_enable=visible;r->valid|=2;
    } else if(bits==8 && port==p->ec_status && !(r->valid&4)) {
        r->ec_status=visible;r->ec_raw_status=raw;r->valid|=4;
    } else if(bits==8 && port==p->ec_enable && !(r->valid&8)) {
        r->ec_enable=visible;r->ec_raw_enable=raw;r->valid|=8;
    }
}
static inline void sci_observe_end(struct sci_observer *o,uint64_t now,bool handled,bool query) {
    if(!o->active)return;
    struct sci_observation *r=&o->current;
    if(o->total!=UINT64_MAX)o->total++;
    r->sequence=o->total;r->handled=handled;r->query_after=query;
    r->duration_us=now>=r->at_us?now-r->at_us:0;
    o->rows[o->next]=*r;o->next=(o->next+1)%SCI_OBSERVE_CAPACITY;
    if(o->count<SCI_OBSERVE_CAPACITY)o->count++;
    o->previous_us=r->at_us;o->previous_valid=true;o->previous_handled=handled;o->active=false;
}
static inline const struct sci_observation *sci_observe_row(const struct sci_observer *o,unsigned i) {
    if(i>=o->count)return 0;
    return &o->rows[(o->next+SCI_OBSERVE_CAPACITY-o->count+i)%SCI_OBSERVE_CAPACITY];
}
#endif
