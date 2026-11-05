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
    if(!h || h->active || !pool || !valid(s) || !io.revalidate)return HANDOFF_INVALID;
    uint32_t missing=HANDOFF_ALL&~s->assurances;
    if(!display_access_held(t))missing|=HANDOFF_EXCLUSION;
    if(missing)return missing;
    /* No populated entry is freed or overwritten here. Original values are
     * retained exactly; the provider must prove the interval was transferred. */
    if(!io.revalidate(io.context,s))return HANDOFF_SNAPSHOT;
    if(!display_access_pin(t))return HANDOFF_EXCLUSION;
    struct scanout_space_range managed={s->first,s->pages},protected={0,(uint32_t)(s->fence_end/4096)};
    if(!scanout_space_open(&h->space,pool,t->owner,(uint32_t)(s->aperture_bytes/4096),&managed,1,&protected,1)) {
        (void)display_access_unpin(t);return HANDOFF_SPACE;
    }
    h->snapshot=*s;h->exclusion=*t;h->active=true;return 0;
}
bool scanout_handoff_close(struct scanout_handoff *h,struct scanout_handoff_io io) {
    if(!h || !h->active || !display_access_held(&h->exclusion) || !io.revalidate ||
       !io.revalidate(io.context,&h->snapshot) ||
       (h->space.state!=SPACE_CLOSED && !scanout_space_close(&h->space,h->exclusion.owner,h->space.epoch)) ||
       h->space.state!=SPACE_CLOSED)return false;
    if(!display_access_unpin(&h->exclusion))return false;
    h->active=false;return true;
}
