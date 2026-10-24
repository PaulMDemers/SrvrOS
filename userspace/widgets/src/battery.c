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

struct desk_battery_controls desk_battery_controls(const struct srv_battery_status *s,const struct srv_battery_control *c) {
    struct desk_battery_controls v={.message="Battery controls are unavailable in this session."};
    if(!s || s->version!=SRV_BATTERY_VERSION || s->size!=sizeof(*s) ||
       !c || c->version!=SRV_BATTERY_CONTROL_VERSION || c->size!=sizeof(*c) || !c->available)return v;
    if(c->pending){v.message=c->pending==SRV_BATTERY_START?"Start requested…":"Stop requested…";return v;}
    if(s->epoch!=c->epoch){v.message="Updating battery state…";return v;}
    if(s->state==SRV_BATTERY_STARTING){v.stop=1;v.message="Starting battery monitoring…";return v;}
    if(s->state==SRV_BATTERY_STOPPING){v.message="Stopping battery monitoring…";return v;}
    if(s->state==SRV_BATTERY_ACTIVE){v.stop=1;v.message="Battery monitoring is running.";return v;}
    if(s->state==SRV_BATTERY_FAULTED || s->state>SRV_BATTERY_FAULTED){v.message="Battery monitoring is unavailable.";return v;}
    v.start=s->state==SRV_BATTERY_DISABLED || s->state==SRV_BATTERY_STOPPED || s->state==SRV_BATTERY_UNAVAILABLE;
    v.message=c->result<0?"Request failed. Refresh and try again.":s->state==SRV_BATTERY_UNAVAILABLE?"Battery monitoring could not start.":"Battery monitoring is stopped.";
    return v;
}

unsigned desk_battery_alert_update(struct desk_battery_alert *alert,const struct srv_battery_status *s) {
    if(!alert || !desk_battery_view(s).fresh)return 0;
    if((s->flags&SRV_BATTERY_AC_PRESENT) || s->soc>25) { alert->warned=0;return 0; }
    if(s->soc>10 && alert->warned==2)alert->warned=1;
    unsigned level=s->soc<=5?2:s->soc<=20?1:0;
    if(level<=alert->warned)return 0;
    alert->warned=level;return level;
}
