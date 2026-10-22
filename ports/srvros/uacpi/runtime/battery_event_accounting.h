#ifndef SRVROS_BATTERY_EVENT_ACCOUNTING_H
#define SRVROS_BATTERY_EVENT_ACCOUNTING_H
#include <stdbool.h>
#include <stdint.h>
/* Telemetry only, never an event admission budget or an ownership sequence.
 * CPU0 callers serialize record/take. Fixed storage and at most one periodic
 * record per interval, even after long scheduling gaps; no catch-up burst. */
#define BATTERY_EVENT_LOG_INTERVAL_US 5000000ull
struct battery_event_totals {
    uint64_t events,alarms,acknowledgements,unknown,invalidations;
    bool saturated;
};
struct battery_event_accounting {
    struct battery_event_totals totals;
    uint64_t last_log_us;
    bool dirty;
};
static inline void battery_event_increment(uint64_t *value,bool *saturated) {
    if(*value==UINT64_MAX)*saturated=true;else ++*value;
}
static inline void battery_event_record(struct battery_event_accounting *a,
    bool alarm,bool acknowledged,bool recognized) {
    battery_event_increment(&a->totals.events,&a->totals.saturated);
    if(alarm)battery_event_increment(&a->totals.alarms,&a->totals.saturated);
    if(acknowledged)battery_event_increment(&a->totals.acknowledgements,&a->totals.saturated);
    if(alarm)battery_event_increment(recognized?&a->totals.invalidations:&a->totals.unknown,&a->totals.saturated);
    a->dirty=true;
}
static inline bool battery_event_take(struct battery_event_accounting *a,
    uint64_t now,bool final,struct battery_event_totals *out) {
    if(!final && (!a->dirty || now<a->last_log_us || now-a->last_log_us<BATTERY_EVENT_LOG_INTERVAL_US))return false;
    *out=a->totals;a->last_log_us=now;a->dirty=false;return true;
}
#endif
