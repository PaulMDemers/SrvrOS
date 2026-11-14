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
        !(m->assurances&~HANDOFF_ALL) && m->original;
}
static bool auth(const struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    return t && token && t->object==t && !t->busy && t->epoch && t->owner &&
        token->object==t && token->epoch==t->epoch && token->owner==t->owner &&
        (t->state==TRANSFER_ACTIVE || t->state==TRANSFER_CLOSING);
}
static bool parent(const struct scanout_transfer *t) {
    return t->pinned && t->manifest.original==t->original && display_access_held(&t->exclusion);
}
static bool arena(const struct scanout_transfer *t) {
    return t->space.object==&t->space && t->space.epoch==t->arena_epoch &&
        t->space.owner==t->owner && t->space.pages==t->manifest.aperture_bytes/4096 &&
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
        if(!display_access_unpin(&t->exclusion)){t->error=TRANSFER_EXCLUSION;return fault(t);}
        t->pinned=false;
    }
    t->state=TRANSFER_CLOSED;t->busy=false;return false;
}
bool scanout_transfer_open(struct scanout_transfer *t,struct scanout_transfer_pool *pool,
    struct scanout_space_pool *spaces,const struct display_access_token *exclusion,
    const struct scanout_transfer_manifest *input,struct scanout_transfer_io io,
    struct scanout_transfer_token *out) {
    if(!t || (t->object && t->object!=t) || t->busy || t->pinned ||
       (t->state!=TRANSFER_EMPTY && t->state!=TRANSFER_CLOSED) || !geometry(input) ||
       !io.revalidate || !io.now || !exclusion ||
       overlap(pool,sizeof(*pool),t,sizeof(*t)) || overlap(spaces,sizeof(*spaces),t,sizeof(*t)) ||
       overlap(out,sizeof(*out),t,sizeof(*t)) || overlap(input->original,(uint64_t)input->pages*8,t,sizeof(*t)) ||
       overlap(input->original,(uint64_t)input->pages*8,pool,sizeof(*pool)) ||
       overlap(input->original,(uint64_t)input->pages*8,spaces,sizeof(*spaces)) ||
       overlap(input->original,(uint64_t)input->pages*8,out,sizeof(*out)) ||
       overlap(input->original,(uint64_t)input->pages*8,exclusion,sizeof(*exclusion)) ||
       overlap(pool,sizeof(*pool),spaces,sizeof(*spaces)) || overlap(out,sizeof(*out),pool,sizeof(*pool)) ||
       overlap(out,sizeof(*out),spaces,sizeof(*spaces)) ||
       pool->epoch==UINT64_MAX || pool->epoch<t->epoch)return false;
    /* The small descriptor/token are copied before changing owned storage.
     * The complete raw vector lives in the original object, not on the stack. */
    struct scanout_transfer_manifest m=*input;
    struct display_access_token token=*exclusion;
    if(m.assurances!=HANDOFF_ALL || m.obligations)return false;
    if(!display_access_held(&token))return false;
    t->object=t;t->busy=true;t->state=TRANSFER_CAPTURING;t->error=TRANSFER_OK;
    t->epoch=++pool->epoch;t->owner=token.owner;t->arena_epoch=0;
    t->exclusion=token;t->io=io;t->manifest=m;t->manifest.original=t->original;
    if(!display_access_pin(&token)){t->error=TRANSFER_EXCLUSION;return unwind(t);}
    t->pinned=true;
    if(!start(t))return unwind(t);
    for(uint32_t i=0;i<m.pages;i++)t->original[i]=m.original[i];
    if(!validate(t,TRANSFER_ADMIT))return unwind(t);
    struct scanout_space_range managed={m.first,m.pages},protected={0,(uint32_t)(m.fence_end/4096)};
    if(!scanout_space_open(&t->space,spaces,t->owner,(uint32_t)(m.aperture_bytes/4096),&managed,1,&protected,1)) {
        t->error=TRANSFER_SPACE;return unwind(t);
    }
    t->arena_epoch=t->space.epoch;
    if(!tick(t) || !parent(t)) {
        if(t->error==TRANSFER_OK)t->error=TRANSFER_EXCLUSION;
        return unwind(t);
    }
    t->state=TRANSFER_ACTIVE;t->busy=false;*out=(struct scanout_transfer_token){t,t->epoch,t->owner};return true;
}
bool scanout_transfer_check(struct scanout_transfer *t,const struct scanout_transfer_token *token) {
    if(!auth(t,token))return false;
    t->busy=true;
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    if(!start(t) || !validate(t,TRANSFER_LIVE))return fault(t);
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
    if(!auth(t,token))return false;
    t->busy=true;t->state=TRANSFER_CLOSING;
    if(!parent(t)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    if(!arena(t)){t->error=TRANSFER_SPACE;return fault(t);}
    if(t->space.state!=SPACE_CLOSED && !scanout_space_close(&t->space,t->owner,t->arena_epoch)) {
        t->error=TRANSFER_SPACE;return fault(t);
    }
    if(t->space.state!=SPACE_CLOSED){t->busy=false;return false;}
    if(!start(t) || !validate(t,TRANSFER_RETIRE))return fault(t);
    if(!arena(t) || t->space.state!=SPACE_CLOSED){t->error=TRANSFER_SPACE;return fault(t);}
    if(!display_access_unpin(&t->exclusion)){t->error=TRANSFER_EXCLUSION;return fault(t);}
    t->pinned=false;t->state=TRANSFER_CLOSED;t->busy=false;return true;
}
