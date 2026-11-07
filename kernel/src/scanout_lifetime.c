#include <srvros/scanout_lifetime.h>
#include <stddef.h>
bool scanout_invariant(const struct scanout_lifetime *o) {
    if(!o || o->state>SCANOUT_FAULT || o->phase>SCANOUT_PENDING || o->front>2 || o->copy>SCANOUT_NONE ||
       o->ready>SCANOUT_NONE || o->target>SCANOUT_NONE || o->released>3)return false;
    if(o->state==SCANOUT_EMPTY)return !o->epoch && !o->owner && !o->phase && !o->front && !o->copy && !o->ready && !o->target && !o->released && !o->serial && !o->fault;
    if(!o->epoch || !o->owner || (o->profile.mode!=SCANOUT_SHADOW && o->profile.mode!=SCANOUT_HARDWARE))return false;
    if(o->copy!=SCANOUT_NONE && (!o->copy || o->copy==o->front || o->phase || o->ready!=SCANOUT_NONE || !o->copy_serial))return false;
    if(o->ready!=SCANOUT_NONE && (!o->ready || o->ready==o->front || o->copy!=SCANOUT_NONE))return false;
    if(o->phase && (o->target>2 || o->target==o->front || !o->request_serial || o->copy!=SCANOUT_NONE))return false;
    if(!o->phase && o->target!=SCANOUT_NONE)return false;
    if(o->state==SCANOUT_RETIRED && (o->phase || o->front || o->copy!=SCANOUT_NONE || o->ready!=SCANOUT_NONE))return false;
    if(o->released && o->state!=SCANOUT_RETIRED)return false;
    if(o->state==SCANOUT_FAULT && !o->fault)return false;
    return true;
}
static bool profile_valid(const struct scanout_profile *p) {
    if(!p || !p->width || p->width>4096 || !p->height || p->height>4096 ||
       p->pitch<p->width*4 || p->pitch>65536 || (p->pitch&63) || p->bytes!=(uint64_t)p->pitch*p->height ||
       p->bytes>16*1024*1024 || (p->assured&~SCANOUT_ASSURED_ALL) || !(p->assured&SCANOUT_RAM_PIN) ||
       (p->mode!=SCANOUT_SHADOW && p->mode!=SCANOUT_HARDWARE) ||
       (p->mode==SCANOUT_HARDWARE && p->assured!=SCANOUT_ASSURED_ALL))return false;
    uint64_t bytes=(p->bytes+4095)&~4095ull;
    for(unsigned i=0;i<3;i++) {
        if(!p->physical[i] || (p->physical[i]&4095) || p->physical[i]>UINT64_MAX-bytes)return false;
        for(unsigned j=0;j<i;j++)if(p->physical[i]<p->physical[j]+bytes && p->physical[j]<p->physical[i]+bytes)return false;
    }
    return true;
}
static bool live(const struct scanout_lifetime *o){return o->state==SCANOUT_ACTIVE || o->state==SCANOUT_CLOSING;}
static void retire(struct scanout_lifetime *o) {
    if(o->state==SCANOUT_CLOSING && !o->front && !o->phase && o->copy==SCANOUT_NONE) {
        o->ready=SCANOUT_NONE;o->state=SCANOUT_RETIRED;
    }
}
bool scanout_open(struct scanout_lifetime *o,struct scanout_pool *pool,const struct scanout_profile *p,uint64_t owner,uint64_t now) {
    if(!o || !pool || !owner || !profile_valid(p) || !scanout_invariant(o) ||
       (o->state!=SCANOUT_EMPTY && (o->state!=SCANOUT_RETIRED || o->released!=3)) || pool->epoch<o->epoch || pool->epoch==UINT64_MAX)return false;
    struct scanout_profile saved=*p;
    *o=(struct scanout_lifetime){.profile=saved,.epoch=++pool->epoch,.owner=owner,.state=SCANOUT_ACTIVE,
        .front=0,.copy=SCANOUT_NONE,.ready=SCANOUT_NONE,.target=SCANOUT_NONE,.last_us=now};
    return true;
}
bool scanout_poison(struct scanout_lifetime *o,uint64_t epoch,uint32_t reason) {
    if(!scanout_invariant(o) || !reason || epoch!=o->epoch || !live(o))return false;
    o->fault|=reason;o->state=SCANOUT_FAULT;return true;
}
bool scanout_tick(struct scanout_lifetime *o,uint64_t epoch,uint64_t now) {
    if(!scanout_invariant(o) || epoch!=o->epoch || !live(o))return false;
    if(now<o->last_us){(void)scanout_poison(o,o->epoch,SCANOUT_CLOCK);return false;}
    o->last_us=now;
    if(o->phase && now>=o->deadline_us){(void)scanout_poison(o,o->epoch,SCANOUT_TIMEOUT);return false;}
    return true;
}
static bool token(const struct scanout_lifetime *o,const struct scanout_token *t,uint64_t serial,unsigned target) {
    return t && t->object==o && t->epoch==o->epoch && t->serial && t->serial==serial && t->target==target && t->mode==o->profile.mode;
}
bool scanout_copy_begin(struct scanout_lifetime *o,uint64_t owner,uint64_t epoch,struct scanout_token *t) {
    if(!scanout_invariant(o) || o->state!=SCANOUT_ACTIVE || owner!=o->owner || epoch!=o->epoch || !t || o->phase ||
       o->copy!=SCANOUT_NONE || o->ready!=SCANOUT_NONE || o->serial==UINT64_MAX)return false;
    o->copy=o->front==1?2:1;o->copy_serial=++o->serial;
    *t=(struct scanout_token){o,o->epoch,o->copy_serial,o->copy,o->profile.mode};return true;
}
bool scanout_copy_end(struct scanout_lifetime *o,const struct scanout_token *t,bool success) {
    if(!scanout_invariant(o) || !live(o) || o->copy==SCANOUT_NONE || !token(o,t,o->copy_serial,o->copy))return false;
    unsigned target=o->copy;o->copy=SCANOUT_NONE;
    if(success && o->state==SCANOUT_ACTIVE)o->ready=target;
    retire(o);return true;
}
bool scanout_request(struct scanout_lifetime *o,uint64_t owner,uint64_t epoch,uint64_t now,struct scanout_token *t) {
    if(!t || !scanout_invariant(o) || !live(o) || owner!=o->owner || epoch!=o->epoch || !scanout_tick(o,o->epoch,now) || o->phase || o->copy!=SCANOUT_NONE || o->serial==UINT64_MAX ||
       now>UINT64_MAX-SCANOUT_DEADLINE_US)return false;
    unsigned target=o->state==SCANOUT_CLOSING?0:o->ready;
    if(target==SCANOUT_NONE || target==o->front)return false;
    o->phase=SCANOUT_SUBMITTING;o->target=target;o->request_serial=++o->serial;o->deadline_us=now+SCANOUT_DEADLINE_US;
    *t=(struct scanout_token){o,o->epoch,o->request_serial,target,o->profile.mode};return true;
}
bool scanout_issued(struct scanout_lifetime *o,const struct scanout_token *t,enum scanout_issue result,uint64_t now) {
    if(!scanout_invariant(o) || !live(o) || o->phase!=SCANOUT_SUBMITTING || !token(o,t,o->request_serial,o->target) || !scanout_tick(o,o->epoch,now))return false;
    if(result==SCANOUT_UNCERTAIN || (unsigned)result>SCANOUT_UNCERTAIN){(void)scanout_poison(o,o->epoch,SCANOUT_PUBLICATION);return false;}
    if(result==SCANOUT_REJECTED) {o->phase=SCANOUT_NO_REQUEST;o->target=SCANOUT_NONE;retire(o);return true;}
    o->phase=SCANOUT_PENDING;o->ready=SCANOUT_NONE;return true;
}
bool scanout_complete(struct scanout_lifetime *o,const struct scanout_token *t,uint32_t observed,uint32_t mode,uint64_t now) {
    if(!scanout_invariant(o) || !live(o) || o->phase!=SCANOUT_PENDING || !token(o,t,o->request_serial,o->target) ||
       observed!=o->target || mode!=o->profile.mode || !scanout_tick(o,o->epoch,now))return false;
    o->front=o->target;o->phase=SCANOUT_NO_REQUEST;o->target=SCANOUT_NONE;retire(o);return true;
}
bool scanout_close(struct scanout_lifetime *o,uint64_t owner,uint64_t epoch) {
    if(!scanout_invariant(o) || !live(o) || owner!=o->owner || epoch!=o->epoch)return false;
    o->state=SCANOUT_CLOSING;retire(o);return true;
}
bool scanout_take_release(struct scanout_lifetime *o,uint64_t epoch,unsigned target,uint64_t *physical) {
    if(!physical || !scanout_invariant(o) || o->state!=SCANOUT_RETIRED || epoch!=o->epoch || target<1 || target>2 || (o->released&(1u<<(target-1))))return false;
    *physical=o->profile.physical[target];o->released|=1u<<(target-1);return true;
}
