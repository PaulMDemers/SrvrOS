#include <srvros/scanout_publish.h>

bool scanout_linear_storage(uint32_t width,uint32_t height,uint32_t pitch,uint64_t *bytes,uint32_t *pages) {
    if(!bytes || !pages || !width || !height || !pitch || (pitch&63) || (uint64_t)width*4>pitch)return false;
    uint64_t size=(uint64_t)pitch*height,count=size/4096+!!(size&4095);
    if(!count || count>SCANOUT_PUBLISH_PAGES)return false;
    *bytes=size;*pages=(uint32_t)count;return true;
}
static bool range_valid(uint64_t owner,const struct scanout_space *s,const struct scanout_space_token *r,uint32_t pages,unsigned use,bool active) {
    if(!s || !s->epoch || !s->pages || s->pages>SCANOUT_SPACE_PAGES || r->object!=s || r->epoch!=s->epoch || owner!=s->owner || !r->serial || r->slot>=SCANOUT_SPACE_LEASES ||
       (s->state!=SPACE_ACTIVE && (active || s->state!=SPACE_CLOSING)))return false;
    const struct scanout_space_entry *e=&s->leases[r->slot];
    return e->use==use && e->serial==r->serial && e->first==r->first && e->pages==r->pages &&
           pages==r->pages && r->first<s->pages && r->pages<=s->pages-r->first;
}
static bool range_live(const struct scanout_publication *p,unsigned use,bool active) {
    return range_valid(p->owner,p->space,&p->range,p->pages,use,active);
}
static bool active_phase(const struct scanout_publication *p) {
    return p->state==PUB_PREPARING || p->state==PUB_PREPARED || p->state==PUB_WRITING;
}
static bool auth(const struct scanout_publication *p,const struct scanout_publish_token *t) {
    return p && t && p->object==p && !p->busy && t->object==p && t->epoch==p->epoch && t->owner==p->owner && p->epoch && p->owner;
}
static bool fail(struct scanout_publication *p,unsigned reason) {
    p->error=reason;p->busy=false;
    if(p->exposed) {
        p->state=PUB_FAULT;
        (void)scanout_space_quarantine(p->space,&p->range);
    } else p->state=PUB_CANCELLED;
    return false;
}
static bool tick(struct scanout_publication *p) {
    uint64_t now=p->io.now(p->io.ctx);
    if(!now || now<p->last_us || now-p->phase_us>SCANOUT_PUBLISH_DEADLINE_US)return fail(p,PUB_CLOCK);
    p->last_us=now;return true;
}
static bool guard(struct scanout_publication *p,bool active) {
    if(!tick(p))return false;
    if(!range_live(p,p->exposed?SPACE_EXPOSED:SPACE_RESERVED,active))return fail(p,PUB_LEASE);
    if(!p->io.guard(p->io.ctx))return fail(p,PUB_GUARD);
    /* A callback may cause close/quarantine; revalidate before progressing. */
    if(!range_live(p,p->exposed?SPACE_EXPOSED:SPACE_RESERVED,active))return fail(p,PUB_LEASE);
    return tick(p);
}
static bool start(struct scanout_publication *p,bool active) {
    p->busy=true;
    uint64_t now=p->io.now(p->io.ctx);
    if(!now || now<p->last_us)return fail(p,PUB_CLOCK);
    p->phase_us=p->last_us=now;
    return guard(p,active);
}
static bool read_entry(struct scanout_publication *p,uint32_t i,uint64_t *value) {
    if(!tick(p))return false;
    if(!range_live(p,p->exposed?SPACE_EXPOSED:SPACE_RESERVED,active_phase(p)))return fail(p,PUB_LEASE);
    p->reads++;
    if(!p->io.read(p->io.ctx,p->range.first+i,value))return fail(p,PUB_IO);
    return tick(p);
}
static bool match(struct scanout_publication *p,const uint64_t *expected) {
    for(uint32_t i=0;i<p->pages;i++) {
        uint64_t value;
        if(!read_entry(p,i,&value))return false;
        if(value!=expected[i])return fail(p,PUB_CHANGED);
    }
    return true;
}
static bool write_entries(struct scanout_publication *p,const uint64_t *values) {
    for(uint32_t i=0;i<p->pages;i++) {
        if(!tick(p))return false;
        if(!range_live(p,SPACE_EXPOSED,active_phase(p)))return fail(p,PUB_LEASE);
        /* Exposure precedes the FIRST callback, including an uncertain failure. */
        p->writes++;
        if(!p->io.write(p->io.ctx,p->range.first+i,values[i]))return fail(p,PUB_IO);
        if(!tick(p))return false;
    }
    if(!tick(p))return false;
    if(!range_live(p,SPACE_EXPOSED,active_phase(p)))return fail(p,PUB_LEASE);
    p->flushes++;
    if(!p->io.flush(p->io.ctx))return fail(p,PUB_IO);
    return tick(p);
}
bool scanout_publish_prepare(struct scanout_publication *p,struct scanout_publish_pool *pool,uint64_t owner,
                            struct scanout_space *space,const struct scanout_space_token *range,
                            const uint64_t *entries,uint32_t pages,const struct scanout_publish_io *io,
                            struct scanout_publish_token *out) {
    if(!p || (p->object && p->object!=p) || !pool || !owner || !range || !entries || !pages || pages>SCANOUT_PUBLISH_PAGES || !io || !out ||
       !io->guard || !io->read || !io->write || !io->flush || !io->readers_retired || !io->now ||
       pool->epoch==UINT64_MAX || pool->epoch<p->epoch || p->busy || (p->state!=PUB_EMPTY && p->state!=PUB_CANCELLED && p->state!=PUB_RETIRED))return false;
    /* Copy small arguments before replacing metadata; large vectors must not
     * alias this object. No page-table format/authority is inferred from values. */
    uintptr_t base=(uintptr_t)p,end=base+sizeof(*p),input=(uintptr_t)entries,output=(uintptr_t)out;
    if(end<base || input>UINTPTR_MAX-pages*sizeof(*entries) || output>UINTPTR_MAX-sizeof(*out) ||
       (input<end && base<input+pages*sizeof(*entries)) || (output<end && base<output+sizeof(*out)))return false;
    if(!range_valid(owner,space,range,pages,SPACE_RESERVED,true))return false;
    struct scanout_publish_io callbacks=*io;struct scanout_space_token lease=*range;
    p->object=p;p->epoch=++pool->epoch;p->owner=owner;p->space=space;p->range=lease;p->io=callbacks;
    p->state=PUB_PREPARING;p->error=PUB_OK;p->pages=pages;p->reads=p->writes=p->flushes=0;
    p->busy=false;p->exposed=false;p->last_us=p->phase_us=0;
    for(uint32_t i=0;i<pages;i++)p->after[i]=entries[i];
    if(!start(p,true))return false;
    for(uint32_t i=0;i<pages;i++)if(!read_entry(p,i,&p->before[i]))return false;
    if(!guard(p,true))return false;
    if(p->last_us==p->phase_us)return fail(p,PUB_CLOCK);
    p->state=PUB_PREPARED;p->busy=false;
    *out=(struct scanout_publish_token){p,p->epoch,p->owner};return true;
}
bool scanout_publish_apply(struct scanout_publication *p,const struct scanout_publish_token *t) {
    if(!auth(p,t) || p->state!=PUB_PREPARED)return false;
    if(!start(p,true) || !match(p,p->before) || !guard(p,true))return false;
    if(p->last_us==p->phase_us)return fail(p,PUB_CLOCK);
    if(!scanout_space_expose(p->space,&p->range))return fail(p,PUB_LEASE);
    p->exposed=true;p->state=PUB_WRITING;
    if(!write_entries(p,p->after) || !match(p,p->after) || !guard(p,true))return false;
    if(p->last_us==p->phase_us)return fail(p,PUB_CLOCK);
    p->state=PUB_PUBLISHED;p->busy=false;return true;
}
bool scanout_publish_restore(struct scanout_publication *p,const struct scanout_publish_token *t) {
    if(!auth(p,t) || p->state!=PUB_PUBLISHED)return false;
    if(!start(p,false))return false;
    if(!tick(p))return false;
    if(!p->io.readers_retired(p->io.ctx)) {
        if(!tick(p) || !guard(p,false))return false;
        p->busy=false;return false;
    }
    if(!tick(p) || !match(p,p->after) || !guard(p,false))return false;
    if(p->last_us==p->phase_us)return fail(p,PUB_CLOCK);
    p->state=PUB_RESTORING;
    if(!write_entries(p,p->before) || !match(p,p->before) || !guard(p,false))return false;
    if(!tick(p))return false;
    if(!p->io.readers_retired(p->io.ctx))return fail(p,PUB_GUARD);
    if(!tick(p) || p->last_us==p->phase_us)return fail(p,PUB_CLOCK);
    if(!range_live(p,SPACE_EXPOSED,false) || !scanout_space_retire(p->space,&p->range,SCANOUT_SPACE_RETIRE_ALL))return fail(p,PUB_LEASE);
    p->state=PUB_RETIRED;p->busy=false;return true;
}
bool scanout_publish_cancel(struct scanout_publication *p,const struct scanout_publish_token *t) {
    if(!auth(p,t) || p->state!=PUB_PREPARED)return false;
    p->state=PUB_CANCELLED;return true;
}
