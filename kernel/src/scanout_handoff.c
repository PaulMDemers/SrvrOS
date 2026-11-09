#include <srvros/scanout_handoff.h>
static bool valid(const struct scanout_handoff_snapshot *s) {
    return s && s->mmio && !(s->mmio&4095) && s->aperture && !(s->aperture&4095) &&
        s->aperture_bytes && !(s->aperture_bytes&4095) && s->aperture_bytes<=SCANOUT_SPACE_PAGES*4096ull &&
        s->aperture<=UINT64_MAX-s->aperture_bytes && !s->fence_begin && s->fence_end &&
        !(s->fence_end&4095) && s->fence_end<=s->aperture_bytes &&
        s->pages && s->pages<=HANDOFF_PAGES && s->first>=s->fence_end/4096 &&
        s->first<s->aperture_bytes/4096 && s->pages<=s->aperture_bytes/4096-s->first &&
        !(s->assurances&~HANDOFF_ALL);
}
uint32_t scanout_handoff_open(struct scanout_handoff *h,struct scanout_space_pool *pool,
    const struct display_access_token *t,const struct scanout_handoff_snapshot *s,struct scanout_handoff_io io) {
    if(!h || (h->object && h->object!=h) || h->active || h->busy || h->pinned ||
       !pool || !valid(s) || !io.revalidate)return HANDOFF_INVALID;
    uint32_t missing=HANDOFF_ALL&~s->assurances;
    if(!display_access_held(t))missing|=HANDOFF_EXCLUSION;
    if(missing)return missing;
    /* Capture inputs before a callback can change shared caller storage. The
     * pin covers validation itself, and busy rejects nested opens/closes. */
    const struct scanout_handoff_snapshot captured=*s;
    const struct display_access_token token=*t;
    h->object=h;h->busy=true;h->snapshot=captured;h->exclusion=token;h->provider=io;
    if(!display_access_pin(&token)){h->busy=false;return HANDOFF_EXCLUSION;}
    h->pinned=true;
    uint32_t error=HANDOFF_SNAPSHOT;
    /* No populated entry is freed or overwritten here. The provider must
     * establish a transfer; neither a snapshot nor this pin supplies it. */
    if(io.revalidate(io.context,&captured)) {
        struct scanout_space_range managed={captured.first,captured.pages},
            protected={0,(uint32_t)(captured.fence_end/4096)};
        error=HANDOFF_SPACE;
        if(display_access_held(&token) &&
           scanout_space_open(&h->space,pool,token.owner,(uint32_t)(captured.aperture_bytes/4096),&managed,1,&protected,1)) {
            h->active=true;h->busy=false;return 0;
        }
    }
    /* Validation is read-only. A clean failure can return its pin, but a lost
     * exclusion cannot be repaired or made reusable by clearing software flags. */
    if(display_access_unpin(&token))h->pinned=false;
    else error=HANDOFF_EXCLUSION;
    h->busy=false;return error;
}
bool scanout_handoff_close(struct scanout_handoff *h,struct scanout_handoff_io io) {
    if(!h || h->object!=h || !h->active || h->busy || !h->pinned ||
       !display_access_held(&h->exclusion) || !io.revalidate ||
       io.context!=h->provider.context || io.revalidate!=h->provider.revalidate)return false;
    h->busy=true;
    /* Stop new reservations before calling the provider, and never validate
     * retirement while an address lease still needs to drain. */
    if((h->space.state!=SPACE_CLOSED && !scanout_space_close(&h->space,h->exclusion.owner,h->space.epoch)) ||
       h->space.state!=SPACE_CLOSED){h->busy=false;return false;}
    const struct scanout_handoff_snapshot captured=h->snapshot;
    const struct display_access_token token=h->exclusion;
    bool clean=io.revalidate(io.context,&captured) && display_access_held(&token);
    if(!clean || !display_access_unpin(&token)){h->busy=false;return false;}
    h->pinned=false;h->active=false;h->busy=false;return true;
}
