#include <srvros/battery_cache.h>

void battery_cache_begin(struct battery_cache *c,uint64_t epoch) {
    *c=(struct battery_cache){.value={.state=BATTERY_SERVICE_STARTING,.epoch=epoch}};
}
bool battery_cache_publish(struct battery_cache *c,const struct sbs_snapshot *s,
    bool ac,bool coherent,uint64_t ac_us,uint64_t now) {
    if(c->value.state!=BATTERY_SERVICE_ACTIVE)return false;
    c->value.attempts++;
    c->last_attempt_ok=false;
    const unsigned required=SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CURRENT|SBS_VALID_CAPACITY;
    bool ordered=s->generation&&s->generation>c->last_attempt_generation&&
        ac_us&&ac_us>c->last_attempt_end&&ac_us<=s->started_us&&
        s->started_us<=s->completed_us&&s->completed_us<=now;
    if(ordered) {
        c->last_attempt_generation=s->generation;
        c->last_attempt_end=s->completed_us;
    }
    if(!ordered||now-s->completed_us>1000000||(s->valid&required)!=required||
       s->relative_soc>100||!s->voltage_uv||!s->full_capacity||
       (s->capacity_unit!=SBS_CAPACITY_UAH&&s->capacity_unit!=SBS_CAPACITY_UWH)) {
        c->value.error=BATTERY_SERVICE_BAD_SAMPLE;c->value.rejected++;return false;
    }
    if(!coherent) {
        c->value.error=BATTERY_SERVICE_AC_CHANGED;c->value.rejected++;return false;
    }
    c->value.sample=*s;c->value.ac_sampled_us=ac_us;
    c->value.flags=(c->value.flags&BATTERY_CACHE_SAMPLING)|BATTERY_CACHE_VALID|
        (ac?BATTERY_CACHE_AC_PRESENT:0);
    c->value.error=BATTERY_SERVICE_OK;c->last_attempt_ok=true;
    return true;
}
void battery_cache_read(const struct battery_cache *c,uint64_t now,
    struct battery_service_status *out) {
    *out=c->value;
    out->flags&=~(BATTERY_CACHE_FRESH|BATTERY_CACHE_STALE);
    out->age_us=UINT64_MAX;
    if(!(out->flags&BATTERY_CACHE_VALID))return;
    if(now>=out->sample.completed_us)out->age_us=now-out->sample.completed_us;
    if(out->state==BATTERY_SERVICE_ACTIVE&&c->last_attempt_ok&&out->age_us<=BATTERY_SERVICE_FRESH_US)
        out->flags|=BATTERY_CACHE_FRESH;
    else out->flags|=BATTERY_CACHE_STALE;
}
