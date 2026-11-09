#include <srvros/intel_takeover_preflight.h>
static bool census_valid(const struct intel_takeover_result *r) {
    const struct intel_completion_result *c=r->census;
    return c && c->object==c && c->parent==r->parent && c->engine_epoch==r->epoch &&
        c->complete && c->stable && c->begin_us && c->end_us>c->begin_us &&
        c->end_us-c->begin_us<=100000 && c->samples==2 && c->groups==10 &&
        c->contexts==2 && c->reads==92 && c->batches==2;
}
static bool parent_valid(const struct intel_takeover_result *r) {
    const struct intel_gt_access *g=r->parent;
    return g && g->object==g && g->state==GT_ACCESS_AWAKE && g->pinned &&
        g->engine_epoch==r->epoch && display_access_held(&g->exclusion) && census_valid(r) &&
        r->borrow.object==&r->borrow && r->borrow.parent==g && r->borrow.active &&
        g->borrows==1 && g->dependents==&r->borrow && !r->borrow.next;
}
static bool clock_ok(struct intel_takeover_result *r,struct intel_takeover_io io) {
    uint64_t t=io.now(io.ctx);
    if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>100000) {
        r->reason|=INTEL_TAKEOVER_CLOCK;return false;
    }
    r->last_us=t;return true;
}
static bool guard(struct intel_takeover_result *r,struct intel_takeover_io io) {
    if(!clock_ok(r,io))return false;
    if(!parent_valid(r)){r->reason|=INTEL_TAKEOVER_PARENT;return false;}
    bool valid=io.guard(io.ctx);
    if(!parent_valid(r)){r->reason|=INTEL_TAKEOVER_PARENT;return false;}
    if(!valid){r->reason|=INTEL_TAKEOVER_GUARD;return false;}
    return true;
}
bool intel_takeover_observe(struct intel_takeover_result *r,struct intel_gt_access *g,
    const struct intel_completion_result *c,struct intel_takeover_io io) {
    if(!r || r->object)return false;
    *r=(struct intel_takeover_result){.object=r,.parent=g,.census=c};
    if(!g || !io.guard || !io.sample || !io.now){r->reason=INTEL_TAKEOVER_INPUT;return false;}
    r->epoch=g->engine_epoch;
    if(!census_valid(r) || g->borrows || g->dependents || !intel_gt_access_check(g)) {
        r->reason=INTEL_TAKEOVER_INPUT;return false;
    }
    r->begin_us=r->last_us=io.now(io.ctx);
    if(!r->begin_us || r->begin_us<=c->end_us || r->begin_us-c->end_us>100000) {
        r->reason=INTEL_TAKEOVER_CLOCK;return false;
    }
    if(!intel_gt_access_borrow(g,&r->borrow)){r->reason=INTEL_TAKEOVER_PARENT;return false;}
    unsigned attempts=g->write_attempts;
    r->blocked=!intel_gt_access_close(g) && g->write_attempts==attempts;
    if(!r->blocked || !parent_valid(r)){r->reason=INTEL_TAKEOVER_PARENT;goto done;}
    for(unsigned n=0;n<2;n++) {
        if(!guard(r,io))goto done;
        r->attempts++;
        bool ok=io.sample(io.ctx,r->words[n],&r->counts[n]);
        if(r->counts[n]>INTEL_TAKEOVER_WORDS) {
            r->counts[n]=0;r->reason|=INTEL_TAKEOVER_COUNT;goto done;
        }
        r->reads+=r->counts[n];
        for(unsigned i=0;i<r->counts[n];i++)
            if(r->words[n][i]==UINT32_MAX)r->reason|=INTEL_TAKEOVER_FLOAT;
        if(!ok || r->counts[n]!=INTEL_TAKEOVER_WORDS)r->reason|=INTEL_TAKEOVER_READ;
        if(r->counts[n]==INTEL_TAKEOVER_WORDS)r->samples++;
        if(!guard(r,io) || r->reason)goto done;
    }
    r->stable=true;
    for(unsigned i=0;i<INTEL_TAKEOVER_WORDS;i++)
        if(r->words[0][i]!=r->words[1][i])r->stable=false;
    if(!r->stable){r->reason=INTEL_TAKEOVER_CHANGED;goto done;}
    for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
        /* Upper16 bits are retained write-mask observations, not active state. */
        if(r->words[0][e]&3u)r->busy|=1u<<e;
        if(r->words[0][e]&0xfffcu)r->reserved|=1u<<e;
        const uint32_t *w=c->words[0][e];
        if(!w[0] && !w[1] && !w[2] && !w[3] && w[4]==1 && !w[5])r->geometry|=1u<<e;
    }
    if(r->words[0][5]&~0xdfu)r->reserved|=32u;
done:
    /* No writes were issued by this observer. A still-valid registered parent
     * can return after a read-only failure; foreign epoch/identity stays pinned. */
    if(parent_valid(r))r->returned=intel_gt_access_return(&r->borrow);
    if(!r->returned)r->reason|=INTEL_TAKEOVER_RETURN;
    r->end_us=io.now(io.ctx);
    if(!r->end_us || r->end_us<r->last_us || r->end_us<=r->begin_us ||
       r->end_us-r->begin_us>100000)r->reason|=INTEL_TAKEOVER_CLOCK;
    if(r->returned && (g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned ||
       g->engine_epoch!=r->epoch || !census_valid(r) || !display_access_held(&g->exclusion) ||
       g->borrows || g->dependents))r->reason|=INTEL_TAKEOVER_PARENT;
    r->complete=!r->reason && r->stable && r->samples==2 && r->returned;
    r->quiescent=r->complete && !r->busy && !r->reserved && !r->words[0][5] &&
        r->geometry==31 && !c->enabled && !c->pending && !c->flip_wait &&
        !c->tlb_pending && c->parser_idle==3;
    return r->complete;
}
