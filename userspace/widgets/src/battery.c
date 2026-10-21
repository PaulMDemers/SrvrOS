#include <srvros/desk_battery.h>
#include <stdio.h>
#include <string.h>
struct desk_battery_view desk_battery_view(const struct srv_battery_status *s) {
    struct desk_battery_view v={0};
    strcpy(v.label,"Battery —");strcpy(v.detail,"Battery information is unavailable.");
    if(!s || s->version!=SRV_BATTERY_VERSION || s->size!=sizeof(*s))return v;
    switch(s->state) {
    case SRV_BATTERY_DISABLED:case SRV_BATTERY_STOPPED:
        strcpy(v.label,"Battery off");strcpy(v.detail,"Battery monitoring is stopped.");return v;
    case SRV_BATTERY_STARTING:
        strcpy(v.label,"Battery …");strcpy(v.detail,"Starting battery monitoring…");return v;
    case SRV_BATTERY_STOPPING:
        strcpy(v.label,"Stopping …");strcpy(v.detail,"Stopping battery monitoring…");return v;
    case SRV_BATTERY_ACTIVE:break;
    default:return v;
    }
    if((s->flags&(SRV_BATTERY_VALID|SRV_BATTERY_FRESH|SRV_BATTERY_STALE))!=(SRV_BATTERY_VALID|SRV_BATTERY_FRESH) ||
       !(s->valid&SRV_BATTERY_SOC_VALID) || s->soc>100) {
        strcpy(v.label,"Battery stale");strcpy(v.detail,"Waiting for a fresh battery reading.");return v;
    }
    v.fresh=1;
    int ac=(s->flags&SRV_BATTERY_AC_PRESENT)!=0;
    snprintf(v.label,sizeof(v.label),"%s%u%%",ac?"AC · ":"",s->soc);
    const char *state=ac?"On AC power":"On battery";
    if((s->valid&SRV_BATTERY_CURRENT_VALID) && s->current_ua>0)state="Charging";
    else if((s->valid&SRV_BATTERY_CURRENT_VALID) && s->current_ua<0)state="Discharging";
    snprintf(v.detail,sizeof(v.detail),"%u%% · %s",s->soc,state);
    return v;
}
