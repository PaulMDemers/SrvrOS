#include <srvros/scanout_transfer.h>

static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool geometry(const struct scanout_transfer_manifest *m) {
    return m && m->mmio && !(m->mmio&4095) && m->aperture && !(m->aperture&4095) &&
        m->aperture_bytes && !(m->aperture_bytes&4095) && m->aperture_bytes<=SCANOUT_SPACE_PAGES*4096ull &&
        m->aperture<=UINT64_MAX-m->aperture_bytes && !m->fence_begin && m->fence_end &&
        !(m->fence_end&4095) && m->fence_end<=m->aperture_bytes && m->source_epoch &&
        m->pages && m->pages<=SCANOUT_TRANSFER_PAGES && m->first>=m->fence_end/4096 &&
        m->first<m->aperture_bytes/4096 && m->pages<=m->aperture_bytes/4096-m->first &&
        !(m->assurances&~HANDOFF_ALL) && m->original && !((uintptr_t)m->original % _Alignof(uint64_t));
}
enum scanout_transfer_error scanout_transfer_admission(const struct scanout_transfer_manifest *m) {
    if (!geometry(m))
        return TRANSFER_INPUT;
    if (m->assurances != HANDOFF_ALL || m->obligations)
        return TRANSFER_ASSURANCE;
    return TRANSFER_OK;
}

static bool same_aperture(const struct scanout_transfer_manifest *a,
                          const struct scanout_transfer_manifest *b) {
    return a->mmio==b->mmio && a->aperture==b->aperture &&
           a->aperture_bytes==b->aperture_bytes && a->fence_begin==b->fence_begin &&
           a->fence_end==b->fence_end;
}

static bool same_range(const struct scanout_transfer_manifest *a,const struct scanout_transfer_manifest *b) {
    return same_aperture(a,b) && a->first==b->first && a->pages==b->pages && a->source_epoch==b->source_epoch;
}

static bool interval_held(const struct scanout_transfer *t) {
    const struct scanout_transfer_manifest *r=&t->reservation;
    return scanout_interval_held(&t->interval,r->mmio,r->aperture,r->aperture_bytes,
                                 r->fence_end,(struct scanout_space_range){r->first,r->pages});
}

static bool drop_range(struct scanout_transfer *t) {
    if(!interval_held(t) || !display_access_unpin(&t->exclusion))return false;
    scanout_interval_release(&t->interval);
    t->pinned=false;
    return true;
}

static bool auth(const struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    return t && token && t->object==t && !t->busy && t->epoch && t->owner &&
        token->object==t && token->epoch==t->epoch && token->owner==t->owner &&
        (t->state==TRANSFER_ACTIVE || t->state==TRANSFER_CLOSING);
}
static bool parent(const struct scanout_transfer *t) {
    return t->pinned && same_range(&t->manifest,&t->reservation) && t->manifest.original==t->original && display_access_held(&t->exclusion) && interval_held(t);
}
static bool arena(const struct scanout_transfer *t) {
    return t->space.object==&t->space && t->space.epoch==t->arena_epoch &&
        t->space.owner==t->owner && t->space.pages==t->manifest.aperture_bytes/4096 &&
        t->space.originals==t->original && t->space.original_range.first==t->manifest.first &&
        t->space.original_range.pages==t->manifest.pages &&
        (t->space.state==SPACE_ACTIVE || t->space.state==SPACE_CLOSING || t->space.state==SPACE_CLOSED);
}
static bool tick(struct scanout_transfer *t) {
    uint64_t now=t->io.now(t->io.ctx);
    if(!now || now<t->last_us || now<t->begin_us || now-t->begin_us>SCANOUT_TRANSFER_DEADLINE_US) {
        t->error=TRANSFER_CLOCK;return false;
    }
    t->last_us=now;return true;
}
static bool validate(struct scanout_transfer *t,enum scanout_transfer_phase phase) {
    if(!tick(t))return false;
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return false;}
    bool ok=t->io.revalidate(t->io.ctx,&t->manifest,phase);
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return false;}
    if(!tick(t))return false;
    if(!ok){t->error=TRANSFER_PROVIDER;return false;}
    if(t->last_us==t->begin_us){t->error=TRANSFER_CLOCK;return false;}
    return true;
}
static bool start(struct scanout_transfer *t) {
    uint64_t now=t->io.now(t->io.ctx);
    if(!now || now<t->last_us){t->error=TRANSFER_CLOCK;return false;}
    t->begin_us=t->last_us=now;return true;
}
static bool fault(struct scanout_transfer *t) {
    if(arena(t) && t->space.state==SPACE_ACTIVE)
        (void)scanout_space_close(&t->space,t->owner,t->arena_epoch);
    t->state=TRANSFER_FAULT;t->busy=false;return false;
}
static bool unwind(struct scanout_transfer *t) {
    /* No provider writer or child lease was ever exposed by this core. A
     * still-held exclusion can unwind read-only admission; lost ownership
     * cannot be repaired by clearing flags. */
    if(t->arena_epoch && (!arena(t) ||
       (t->space.state!=SPACE_CLOSED && !scanout_space_close(&t->space,t->owner,t->arena_epoch)) ||
       t->space.state!=SPACE_CLOSED))return fault(t);
    if(t->pinned) {
        if(!drop_range(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    }
    t->state=TRANSFER_CLOSED;t->busy=false;return false;
}
static bool reserve_range(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    const struct display_access_token *exclusion,const struct scanout_transfer_manifest *input,
    const struct scanout_interval *firmware,struct scanout_transfer_token *out) {
    if(!t || (t->object && t->object!=t) || t->busy || t->pinned ||
       (t->state!=TRANSFER_EMPTY && t->state!=TRANSFER_CLOSED) || !geometry(input) ||
       !exclusion || overlap(pool,sizeof(*pool),t,sizeof(*t)) ||
       overlap(out,sizeof(*out),t,sizeof(*t)) ||
       overlap(input->original,(uint64_t)input->pages*8,t,sizeof(*t)) ||
       overlap(input->original,(uint64_t)input->pages*8,pool,sizeof(*pool)) ||
       overlap(input->original,(uint64_t)input->pages*8,out,sizeof(*out)) ||
       overlap(input->original,(uint64_t)input->pages*8,exclusion,sizeof(*exclusion)) ||
       overlap(out,sizeof(*out),pool,sizeof(*pool)) ||
       overlap(out,sizeof(*out),exclusion,sizeof(*exclusion)) ||
       overlap(pool,sizeof(*pool),exclusion,sizeof(*exclusion)) ||
       pool->epoch==UINT64_MAX || pool->epoch<t->epoch)return false;
    struct scanout_transfer_manifest m=*input;
    struct display_access_token token=*exclusion;
    struct scanout_space_range pages={m.first,m.pages};
    bool claimed=firmware ? scanout_interval_borrow(&t->interval,firmware,m.mmio,m.aperture,m.aperture_bytes,m.fence_end,pages)
                          : scanout_interval_claim(&t->interval,m.mmio,m.aperture,m.aperture_bytes,m.fence_end,pages);
    if(!claimed)return false;
    if(!display_access_pin(&token)){scanout_interval_release(&t->interval);return false;}
    t->object=t;t->busy=true;t->pinned=true;t->state=TRANSFER_RESERVED;t->error=TRANSFER_OK;
    t->epoch=++pool->epoch;t->owner=token.owner;t->arena_epoch=0;
    t->exclusion=token;t->manifest=m;t->manifest.original=t->original;
    t->reservation=t->manifest;
    t->reservation_pool=pool;
    return true;
}

static bool reserve_originals(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    const struct display_access_token *exclusion,const struct scanout_transfer_manifest *input,
    const struct scanout_interval *firmware,struct scanout_transfer_token *out) {
    if(!geometry(input))return false;
    const uint64_t *entries=input->original;
    if(!reserve_range(t,pool,exclusion,input,firmware,out))return false;
    for(uint32_t i=0;i<t->manifest.pages;i++)t->original[i]=entries[i];
    t->busy=false;
    *out=(struct scanout_transfer_token){t,t->epoch,t->owner};
    return true;
}


bool scanout_transfer_reserve(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    const struct display_access_token *exclusion,const struct scanout_transfer_manifest *input,
    struct scanout_transfer_token *out) {
    return reserve_originals(t,pool,exclusion,input,0,out);
}

bool scanout_transfer_reserve_imported(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    const struct display_access_token *exclusion,const struct scanout_transfer_manifest *input,
    const struct scanout_interval *firmware,struct scanout_transfer_token *out) {
    if(!firmware || overlap(firmware,sizeof(*firmware),t,sizeof(*t)) ||
       overlap(firmware,sizeof(*firmware),pool,sizeof(*pool)) ||
       overlap(firmware,sizeof(*firmware),out,sizeof(*out)))return false;
    return reserve_originals(t,pool,exclusion,input,firmware,out);
}

static bool start_transfer(struct scanout_transfer *t,struct scanout_space_pool *spaces,
    struct scanout_transfer_manifest m,struct scanout_transfer_io io,bool check_original) {
    t->state=TRANSFER_CAPTURING;t->busy=true;t->io=io;
    t->manifest.assurances=m.assurances;t->manifest.obligations=m.obligations;
    if(!start(t))return false;
    for(uint32_t i=0;i<m.pages;i++) {
        if(check_original) {
            if(t->original[i]!=m.original[i]){t->error=TRANSFER_PROVIDER;return false;}
        } else t->original[i]=m.original[i];
    }
    if(!validate(t,TRANSFER_ADMIT))return false;
    struct scanout_space_range managed={m.first,m.pages},protected={0,(uint32_t)(m.fence_end/4096)};
    if(!scanout_space_open_transferred(&t->space,spaces,t->owner,(uint32_t)(m.aperture_bytes/4096),managed,protected,t->original)) {
        t->error=TRANSFER_SPACE;return false;
    }
    t->arena_epoch=t->space.epoch;
    if(!tick(t) || !parent(t)) {
        if(t->error==TRANSFER_OK)t->error=TRANSFER_EXCLUSION;
        return false;
    }
    t->state=TRANSFER_ACTIVE;t->busy=false;return true;
}

static bool reserved(const struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    return t && token && t->object==t && token->object==t && token->epoch==t->epoch &&
           token->owner==t->owner && t->pinned && !t->arena_epoch && t->state==TRANSFER_RESERVED && !t->busy;
}

bool scanout_transfer_reservation_matches(const struct scanout_transfer *t,const struct scanout_transfer_token *token,
    const struct scanout_transfer_manifest *source) {
    if(!reserved(t,token) || !parent(t) || !geometry(source) || !same_range(&t->reservation,source))return false;
    for(unsigned i=0;i<source->pages;i++)
        if(source->original[i]!=t->original[i])return false;
    return true;
}

bool scanout_transfer_activate(struct scanout_transfer *t,const struct scanout_transfer_token *token,
    struct scanout_space_pool *spaces,const struct scanout_transfer_manifest *m,struct scanout_transfer_io io) {
    if(!reserved(t,token) || scanout_transfer_admission(m)!=TRANSFER_OK || !io.now || !io.revalidate ||
       !same_range(&t->reservation,m) || overlap(spaces,sizeof(*spaces),t,sizeof(*t)) ||
       overlap(spaces,sizeof(*spaces),t->reservation_pool,sizeof(*t->reservation_pool)) ||
       overlap(spaces,sizeof(*spaces),token,sizeof(*token)) ||
       overlap(m->original,(uint64_t)m->pages*8,spaces,sizeof(*spaces)) ||
       (m->original!=t->original && overlap(m->original,(uint64_t)m->pages*8,t,sizeof(*t))))return false;
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    if(!scanout_transfer_reservation_matches(t,token,m))return false;
    /* An existing lender still owns this exclusion. Once external checks
     * begin, uncertainty must retain it until recovery. */
    if(!start_transfer(t,spaces,*m,io,true))return fault(t);
    return true;
}

bool scanout_transfer_open(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    struct scanout_space_pool *spaces,const struct display_access_token *exclusion,
    const struct scanout_transfer_manifest *input,struct scanout_transfer_io io,
    struct scanout_transfer_token *out) {
    if(scanout_transfer_admission(input)!=TRANSFER_OK || !io.revalidate || !io.now ||
       overlap(spaces,sizeof(*spaces),t,sizeof(*t)) || overlap(pool,sizeof(*pool),spaces,sizeof(*spaces)) ||
       overlap(input->original,(uint64_t)input->pages*8,spaces,sizeof(*spaces)) ||
       overlap(out,sizeof(*out),spaces,sizeof(*spaces)))return false;
    struct scanout_transfer_manifest m=*input;
    if(!reserve_range(t,pool,exclusion,&m,0,out))return false;
    if(!start_transfer(t,spaces,m,io,false))return unwind(t);
    *out=(struct scanout_transfer_token){t,t->epoch,t->owner};return true;
}
bool scanout_transfer_check(struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    if(!auth(t,token))return false;
    t->busy=true;
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    if(!start(t) || !validate(t,TRANSFER_LIVE))return fault(t);
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    t->busy=false;return true;
}
bool scanout_transfer_check_clocked(struct scanout_transfer *t,const struct scanout_transfer_token *token,
    uint64_t (*clock)(void *),void *ctx) {
    if(!auth(t,token) || !clock || !ctx)return false;
    t->busy=true;
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    uint64_t now=clock(ctx);
    if(!now || now<t->last_us){t->error=TRANSFER_CLOCK;return fault(t);}
    t->begin_us=t->last_us=now;
    now=clock(ctx);
    if(!now || now<t->last_us || now-t->begin_us>SCANOUT_TRANSFER_DEADLINE_US){t->error=TRANSFER_CLOCK;return fault(t);}
    t->last_us=now;
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    bool ok=t->io.revalidate(t->io.ctx,&t->manifest,TRANSFER_LIVE);
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    now=clock(ctx);
    if(!now || now<t->last_us || now-t->begin_us>SCANOUT_TRANSFER_DEADLINE_US){t->error=TRANSFER_CLOCK;return fault(t);}
    t->last_us=now;
    if(!ok){t->error=TRANSFER_PROVIDER;return fault(t);}
    if(t->last_us==t->begin_us){t->error=TRANSFER_CLOCK;return fault(t);}
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    t->busy=false;return true;
}
bool scanout_transfer_original(const struct scanout_transfer *t,const struct scanout_transfer_token *token,
    uint32_t index,uint64_t *out) {
    if(!auth(t,token) || !parent(t) || !arena(t) || index<t->manifest.first ||
       index-t->manifest.first>=t->manifest.pages || overlap(out,sizeof(*out),t,sizeof(*t)) ||
       overlap(out,sizeof(*out),token,sizeof(*token)))return false;
    *out=t->original[index-t->manifest.first];return true;
}
bool scanout_transfer_close(struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    if(t && t->command_hold)return false;
    if(reserved(t,token)) {
        if(!drop_range(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
        t->state=TRANSFER_CLOSED;return true;
    }
    if(!auth(t,token) || t->device_publication)return false;
    t->busy=true;t->state=TRANSFER_CLOSING;
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    if(t->space.state!=SPACE_CLOSED && !scanout_space_close(&t->space,t->owner,t->arena_epoch)) {
        t->error=TRANSFER_SPACE;return fault(t);
    }
    if(t->space.state!=SPACE_CLOSED){t->busy=false;return false;}
    if(!start(t) || !validate(t,TRANSFER_RETIRE))return fault(t);
    if(!arena(t) || t->space.state!=SPACE_CLOSED){t->error=TRANSFER_SPACE;return fault(t);}
    if(!drop_range(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    t->state=TRANSFER_CLOSED;t->busy=false;return true;
}
