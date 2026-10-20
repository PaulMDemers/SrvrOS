#ifndef SRVROS_BATTERY_CACHE_H
#define SRVROS_BATTERY_CACHE_H
#include <srvros/battery_service.h>
/* Pure cache model. Caller serializes writes and copies; no IO or allocation. */
struct battery_cache {
    struct battery_service_status value;
    uint64_t last_attempt_generation,last_attempt_end;
    bool last_attempt_ok;
};
void battery_cache_begin(struct battery_cache *,uint64_t epoch);
bool battery_cache_publish(struct battery_cache *,const struct sbs_snapshot *,
    bool ac,bool coherent,uint64_t ac_us,uint64_t now);
void battery_cache_read(const struct battery_cache *,uint64_t now,
    struct battery_service_status *);
#endif
