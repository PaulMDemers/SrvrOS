#ifndef SRVROS_SCI_OBSERVE_H
#define SRVROS_SCI_OBSERVE_H
#include <stdint.h>
#include <stdbool.h>
/* Passive CPU0 observer: no I/O, allocation, logging or ownership decisions.
 * Only first successful reads within one callback are retained. Zero without
 * its validity bit is UNKNOWN. A bounded ring retains the most recent events. */
#define SCI_OBSERVE_CAPACITY 8
struct sci_observe_ports { uint16_t fixed_status,fixed_enable,ec_status,ec_enable; };
struct sci_controller_observation {
    uint32_t gsi,lapic_bits,lint0,route_low,route_high;
    uint16_t pic_masks;
    uint8_t valid,vector; /* availability: LAPIC=1, I/O APIC=2, PIC=4 */
};
struct sci_observation {
    struct sci_controller_observation before,after,post_eoi;
    uint64_t sequence,at_us,duration_us,previous_gap_us,post_eoi_elapsed_us;
    uint16_t fixed_status,fixed_enable;
    uint8_t ec_status,ec_enable,ec_raw_status,ec_raw_enable,valid,phase;
    bool handled,previous_valid,previous_handled,query_before,query_after,post_eoi_observed,query_post_eoi;
};
struct sci_observer {
    struct sci_observation rows[SCI_OBSERVE_CAPACITY],current;
    uint64_t total,previous_us;
    unsigned next,count;
    bool active,previous_valid,previous_handled,post_pending;
};
static inline bool sci_observe_begin(struct sci_observer *o,uint64_t now,uint8_t phase,bool query) {
    if(o->active)return false; /* Do not overwrite an outer observation. */
    o->post_pending=false; /* A missing completion remains explicitly unknown. */
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
static inline void sci_observe_controller(struct sci_observer *o,bool after,
    struct sci_controller_observation value) {
    if(!o->active)return;
    if(after)o->current.after=value;else o->current.before=value;
}
static inline void sci_observe_end(struct sci_observer *o,uint64_t now,bool handled,bool query) {
    if(!o->active)return;
    struct sci_observation *r=&o->current;
    if(o->total!=UINT64_MAX)o->total++;
    r->sequence=o->total;r->handled=handled;r->query_after=query;
    r->duration_us=now>=r->at_us?now-r->at_us:0;
    r->post_eoi.vector=r->before.vector;r->post_eoi.gsi=r->before.gsi;
    o->post_pending=true;
    o->rows[o->next]=*r;o->next=(o->next+1)%SCI_OBSERVE_CAPACITY;
    if(o->count<SCI_OBSERVE_CAPACITY)o->count++;
    o->previous_us=r->at_us;o->previous_valid=true;o->previous_handled=handled;o->active=false;
}
static inline void sci_observe_post_eoi(struct sci_observer *o,
    struct sci_controller_observation value,uint64_t now,bool query) {
    if(!o->post_pending || o->active || !o->count)return;
    struct sci_observation *r=&o->rows[(o->next+SCI_OBSERVE_CAPACITY-1)%SCI_OBSERVE_CAPACITY];
    r->post_eoi=value;r->post_eoi_observed=true;r->query_post_eoi=query;
    r->post_eoi_elapsed_us=now>=r->at_us?now-r->at_us:0;
    o->post_pending=false;
}
static inline const struct sci_observation *sci_observe_row(const struct sci_observer *o,unsigned i) {
    if(i>=o->count)return 0;
    return &o->rows[(o->next+SCI_OBSERVE_CAPACITY-o->count+i)%SCI_OBSERVE_CAPACITY];
}
#endif
