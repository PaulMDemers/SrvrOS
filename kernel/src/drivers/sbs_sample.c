#include <srvros/sbs_sample.h>
/* Mode and scale are bracketed so a battery mode change cannot relabel the
 * capacity values halfway through a sample. Reading status last also exposes
 * the battery's error and initialization flags without claiming atomicity. */
static const uint8_t commands[SBS_WORD_COUNT]={0x03,0x1a,0x09,0x0a,0x0d,0x0f,0x10,0x18,0x08,0x12,0x13,0x16,0x03,0x1a,0x16};
void sbs_sample_decode(struct sbs_snapshot *s) {
    s->valid=0;s->capacity_unit=0;s->voltage_uv=0;s->current_ua=0;
    s->remaining_capacity=s->full_capacity=s->design_capacity=0;s->temperature_mc=0;
    s->mode=s->raw[0];s->specification=s->raw[1];s->relative_soc=s->raw[4];
    s->status=s->raw[14];s->minutes_empty=s->raw[9];s->minutes_full=s->raw[10];
    if((s->raw[0]^s->raw[12])&0x8000 || s->raw[1]!=s->raw[13]) return;
    /* A battery error or uninitialized gauge does not yield valid capacity. */
    if(((s->status|s->raw[11])&15) || !(s->status&0x80) || !(s->raw[11]&0x80)) return;
    if(s->relative_soc<=100) s->valid|=SBS_VALID_SOC;
    if(s->minutes_empty!=65535) s->valid|=SBS_VALID_TIME_EMPTY;
    if(s->minutes_full!=65535) s->valid|=SBS_VALID_TIME_FULL;
    s->temperature_mc=(int32_t)s->raw[8]*100-273150;s->valid|=SBS_VALID_TEMPERATURE;
    unsigned spec=s->specification&255,v=(s->specification>>8)&15,i=(s->specification>>12)&15;
    /* SpecID values from the 1.1 erratum; 0x11 is the original 1.0 encoding. */
    if((spec!=0x10 && spec!=0x11 && spec!=0x21 && spec!=0x31)||v>3||i>3) return;
    static const uint32_t scale[]={1,10,100,1000};
    uint64_t vs=scale[v],is=scale[i];
    s->voltage_uv=(uint64_t)s->raw[2]*vs*1000;
    int32_t current=s->raw[3]<0x8000?s->raw[3]:(int32_t)s->raw[3]-65536;
    s->current_ua=(int64_t)current*(int64_t)is*1000;
    s->valid|=SBS_VALID_VOLTAGE|SBS_VALID_CURRENT;
    bool energy=(s->mode&0x8000)!=0;
    uint64_t factor=energy?10000*vs*is:1000*is;
    s->capacity_unit=energy?SBS_CAPACITY_UWH:SBS_CAPACITY_UAH;
    s->remaining_capacity=(uint64_t)s->raw[5]*factor;
    s->full_capacity=(uint64_t)s->raw[6]*factor;
    s->design_capacity=(uint64_t)s->raw[7]*factor;
    s->valid|=SBS_VALID_CAPACITY;
}
static int fail(struct sbs_sampler *s,enum sbs_error why,bool uncertain) {
    s->active=false;s->current=false;s->error=why;
    if(uncertain && s->waiting) { ec_transport_cancel(s->ec);s->ec->poisoned=true; }
    s->waiting=false;return -1;
}
bool sbs_sample_begin(struct sbs_sampler *s,struct ec_transport *ec,uint8_t base,uint64_t now) {
    if(!s||!ec||s->active||ec->poisoned||base>250||(ec->phase!=EC_IDLE && ec->phase!=EC_DONE)) return false;
    s->staging=(struct sbs_snapshot){.started_us=now};s->transaction=(struct ec_smbus){0};
    s->ec=ec;s->base=base;s->last_us=now;s->polls=0;s->index=0;s->active=true;s->waiting=false;s->error=SBS_OK;return true;
}
int sbs_sample_poll(struct sbs_sampler *s,uint64_t now) {
    if(!s||!s->ec) return -1;
    if(!s->active) return s->error==SBS_OK&&s->current?1:-1;
    if(now<s->last_us) return fail(s,SBS_CLOCK,true);
    s->last_us=now;
    if(now-s->staging.started_us>=20000000) return fail(s,SBS_DEADLINE,true);
    if(++s->polls>100000) return fail(s,SBS_POLL_LIMIT,true);
    if(!s->waiting) {
        if(!ec_smbus_read_word(&s->transaction,s->ec,s->base,0x0b,commands[s->index],now)) return fail(s,SBS_TRANSPORT,false);
        s->waiting=true;return 0;
    }
    int result=ec_smbus_poll(&s->transaction,now);
    if(result<0) return fail(s,SBS_TRANSPORT,true);
    if(!result) return 0;
    s->staging.raw[s->index++]=s->transaction.result;s->waiting=false;
    if(s->index<SBS_WORD_COUNT) return 0;
    if(((s->staging.raw[0]^s->staging.raw[12])&0x8000) || s->staging.raw[1]!=s->staging.raw[13]) return fail(s,SBS_CHANGED_MODE,false);
    s->staging.completed_us=now;s->staging.generation=s->sample.generation+1;
    sbs_sample_decode(&s->staging);s->sample=s->staging;s->current=true;s->active=false;return 1;
}
void sbs_sample_cancel(struct sbs_sampler *s) { if(s&&s->active) (void)fail(s,SBS_CANCELED,true); }
bool sbs_sample_fresh(const struct sbs_sampler *s,uint64_t now,uint64_t age) {
    return s&&s->current&&now>=s->sample.completed_us&&now-s->sample.completed_us<=age;
}
