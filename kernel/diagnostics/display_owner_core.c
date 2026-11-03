#include <srvros/display_owner.h>
#include <stddef.h>
static bool profile_valid(const struct display_owner_profile *p) {
    return p && p->access_valid && p->gpu==0x0a268086 && p->pch==0x9c438086 &&
        p->hda==0x0a0c8086 && p->subsystem==0x011a106b && !(p->assured&~DISPLAY_OWNER_REQUIRED);
}
static bool live(const struct display_owner *o) {
    return o && (o->state==DISPLAY_OWNER_OBSERVED || o->state==DISPLAY_OWNER_ADOPTED);
}
static bool invariant(const struct display_owner *o) {
    if(!o || (unsigned)o->state>DISPLAY_OWNER_FAULT || o->guard>1)return false;
    if((o->state==DISPLAY_OWNER_EMPTY || o->state==DISPLAY_OWNER_RETIRED)?o->guard!=0:(!o->epoch || o->guard!=1))return false;
    unsigned refs[2]={0},busy=0;
    for(unsigned i=0;i<DISPLAY_OWNER_SLOTS;i++) {
        const struct display_owner_ref *r=&o->slots[i];
        if(r->live) {if(r->domain>DISPLAY_OWNER_AUDIO || !r->serial)return false;refs[r->domain]++;if(r->busy)busy++;}
        else if(r->busy)return false;
    }
    if((o->state==DISPLAY_OWNER_EMPTY || o->state==DISPLAY_OWNER_RETIRED) && (refs[0] || refs[1] || busy))return false;
    return o->refs[0]==refs[0] && o->refs[1]==refs[1] && o->inflight==busy;
}
static bool same(const struct display_owner_profile *a,const struct display_owner_profile *b) {
    if(a->gpu!=b->gpu || a->pch!=b->pch || a->hda!=b->hda || a->subsystem!=b->subsystem ||
       a->assured!=b->assured || a->active!=b->active || a->access_valid!=b->access_valid)return false;
    for(unsigned i=0;i<DISPLAY_OWNER_WORDS;i++)if(a->raw[i]!=b->raw[i])return false;
    return true;
}
static struct display_owner_ref *token_ref(struct display_owner *o,const struct display_owner_token *t) {
    if(!o || !t || t->owner!=o || !t->epoch || t->epoch!=o->epoch || t->slot>=DISPLAY_OWNER_SLOTS ||
       !invariant(o) || o->state==DISPLAY_OWNER_EMPTY || o->state==DISPLAY_OWNER_RETIRED)return NULL;
    struct display_owner_ref *r=&o->slots[t->slot];
    return r->live && t->serial && r->serial==t->serial?r:NULL;
}
static void finish_close(struct display_owner *o) {
    if(o->state==DISPLAY_OWNER_CLOSING && !o->refs[0] && !o->refs[1] && !o->inflight) {
        o->guard=0;o->state=DISPLAY_OWNER_RETIRED;
    }
}
bool display_owner_open(struct display_owner *o,struct display_owner_pool *pool,const struct display_owner_profile *p) {
    if(!o || !pool || !profile_valid(p) || (o->state!=DISPLAY_OWNER_EMPTY && o->state!=DISPLAY_OWNER_RETIRED) ||
       !invariant(o) || o->guard || pool->epoch==UINT64_MAX)return false;
    struct display_owner_profile saved=*p; /* Allow input to alias retired storage. */
    *o=(struct display_owner){.profile=saved,.epoch=++pool->epoch,.guard=1,
        .state=(saved.assured&DISPLAY_OWNER_BACKING)?DISPLAY_OWNER_ADOPTED:DISPLAY_OWNER_OBSERVED};
    return true;
}
bool display_owner_hold(struct display_owner *o,uint64_t epoch,unsigned domain,struct display_owner_token *t) {
    if(!live(o) || !invariant(o) || !t || !epoch || epoch!=o->epoch || domain>DISPLAY_OWNER_AUDIO ||
       o->serial==UINT64_MAX || !o->guard || o->fault)return false;
    for(unsigned i=0;i<DISPLAY_OWNER_SLOTS;i++)if(!o->slots[i].live) {
        o->slots[i]=(struct display_owner_ref){.serial=++o->serial,.domain=domain,.live=true};
        o->refs[domain]++;*t=(struct display_owner_token){.owner=o,.epoch=epoch,.serial=o->serial,.slot=i};return true;
    }
    return false;
}
bool display_owner_begin(struct display_owner *o,const struct display_owner_token *t) {
    struct display_owner_ref *r=token_ref(o,t);
    if(!r || !live(o) || r->busy || o->fault || !o->guard)return false;
    r->busy=true;o->inflight++;return true;
}
bool display_owner_end(struct display_owner *o,const struct display_owner_token *t) {
    struct display_owner_ref *r=token_ref(o,t);if(!r || !r->busy)return false;
    r->busy=false;o->inflight--;return true;
}
bool display_owner_release(struct display_owner *o,const struct display_owner_token *t) {
    struct display_owner_ref *r=token_ref(o,t);if(!r || r->busy)return false;
    o->refs[r->domain]--;*r=(struct display_owner_ref){0};finish_close(o);return true;
}
bool display_owner_close(struct display_owner *o,uint64_t epoch) {
    if(!live(o) || !invariant(o) || !epoch || epoch!=o->epoch || !o->guard || o->fault)return false;
    o->state=DISPLAY_OWNER_CLOSING;finish_close(o);return true;
}
bool display_owner_fault(struct display_owner *o,uint64_t epoch,uint32_t reason) {
    if(!o || !invariant(o) || !reason || !epoch || epoch!=o->epoch || !o->guard ||
       o->state==DISPLAY_OWNER_EMPTY || o->state==DISPLAY_OWNER_RETIRED)return false;
    o->fault|=reason;o->state=DISPLAY_OWNER_FAULT;return true;
}
uint32_t display_owner_check(const struct display_owner *o,uint64_t epoch,const struct display_owner_profile *p) {
    if(!o || !epoch || epoch!=o->epoch)return DISPLAY_OWNER_STALE;
    uint32_t reasons=0;
    if(!invariant(o) || !live(o) || !o->guard)reasons|=DISPLAY_OWNER_STATE;
    if(!profile_valid(p))return reasons|DISPLAY_OWNER_PROFILE;
    reasons|=DISPLAY_OWNER_REQUIRED&~p->assured;
    if(!same(&o->profile,p))reasons|=DISPLAY_OWNER_CHANGED;
    if(p->active)reasons|=DISPLAY_OWNER_ACTIVE;
    if(o->refs[0] || o->refs[1])reasons|=DISPLAY_OWNER_REFS;
    if(o->inflight)reasons|=DISPLAY_OWNER_INFLIGHT;
    if(o->fault)reasons|=DISPLAY_OWNER_POISON;
    return reasons; /* Even zero is a predicate in a model, never write authority. */
}
