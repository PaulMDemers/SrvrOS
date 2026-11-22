#include <srvros/intel_gt_access.h>
static bool fail(struct intel_gt_access *g,uint32_t reason) {
    g->reason|=reason;
    if(g->pinned)g->state=GT_ACCESS_FAULT;
    return false;
}
static bool clock_fail(struct intel_gt_access *g,uint64_t sample,uint64_t phase,uint32_t why) {
    if(!g->clock_cause){g->clock_cause=why;g->clock_sample=sample;g->clock_previous=g->last;g->clock_phase=phase;}
    return fail(g,GT_ACCESS_CLOCK);
}
static bool clock_ok(struct intel_gt_access *g,uint64_t phase) {
    uint64_t t=g->io.now(g->io.context);uint32_t why=0;
    if(!t)why|=GT_CLOCK_ZERO;
    if(t<g->last || t<phase || t<g->begin)why|=GT_CLOCK_BACKWARD;
    if(t>=phase && t-phase>GT_ACCESS_PHASE_US)why|=GT_CLOCK_PHASE;
    if(t>=g->begin && t-g->begin>GT_ACCESS_TOTAL_US)why|=GT_CLOCK_TOTAL;
    if(why)return clock_fail(g,t,phase,why);
    g->last=t;return true;
}
static bool guarded(struct intel_gt_access *g) {
    return g->object==g && display_access_held(&g->exclusion) && g->io.guard(g->io.context) ? true : fail(g,GT_ACCESS_GUARD);
}
static bool read_reg(struct intel_gt_access *g,uint32_t reg,uint32_t *v) {
    if(!guarded(g))return false;
    if(!g->io.read(g->io.context,reg,v) || *v==UINT32_MAX)return fail(g,GT_ACCESS_IO);
    return clock_ok(g,g->last);
}
static bool read_pair(struct intel_gt_access *g) {
    return read_reg(g,INTEL_GT_REQUEST,&g->requests) && read_reg(g,INTEL_GT_ACK,&g->ack);
}
static bool write_reg(struct intel_gt_access *g,uint32_t value) {
    if(!guarded(g) || !clock_ok(g,g->last))return false;
    g->write_attempts++;
    enum intel_gt_write result=g->io.write(g->io.context,INTEL_GT_REQUEST,value);
    if(result==GT_WRITE_DONE){g->writes++;return clock_ok(g,g->last);}
    /* Only a definitely unissued FIRST write can drop the pin immediately.
     * After initialization even a later zero-write refusal needs recovery. */
    if(result==GT_WRITE_NONE && !g->writes && g->write_attempts==1) {
        if(display_access_unpin(&g->exclusion)){g->pinned=false;g->state=GT_ACCESS_CLOSED;}
    }
    return fail(g,GT_ACCESS_IO);
}
static bool wait_state(struct intel_gt_access *g,uint32_t desired,bool c0) {
    uint64_t phase=g->last;
    for(unsigned i=0;i<GT_ACCESS_POLL_CAP;i++) {
        g->polls++;
        if(!clock_ok(g,phase) || !read_pair(g))return false;
        /* All other requesters were retired by the audited caller. Their
         * appearance is ownership loss, never a bit we may clear or borrow. */
        if((g->requests&0xffffu)!=desired || (g->ack&~1u))return fail(g,GT_ACCESS_FOREIGN);
        if(g->ack==desired) {
            if(!c0)return guarded(g) && clock_ok(g,phase);
            if(!read_reg(g,INTEL_GT_THREAD,&g->thread))return false;
            if(!(g->thread&7))return guarded(g) && clock_ok(g,phase);
        }
        g->io.pause(g->io.context);
    }
    return fail(g,GT_ACCESS_TIMEOUT);
}
static bool open_access(struct intel_gt_access *g,const struct display_access_token *t,
                        uint32_t grants,struct intel_gt_io io,bool guarded_snapshot) {
    if(!g)return false;
    if((g->object && g->object!=g) || (g->state!=GT_ACCESS_EMPTY && g->state!=GT_ACCESS_CLOSED))return fail(g,GT_ACCESS_STATE);
    uint32_t missing=GT_ACCESS_REQUIRED&~grants;
    if(!display_access_held(t))missing|=GT_ACCESS_EXCLUSION;
    if(grants&~GT_ACCESS_REQUIRED || !io.guard || !io.read || !io.write || !io.now || !io.pause ||
       (guarded_snapshot && !io.snapshot))missing|=GT_ACCESS_INPUT;
    if(missing){g->reason=missing;return false;}
    struct display_access_token saved=*t;
    *g=(struct intel_gt_access){.object=g,.exclusion=saved,.io=io,.guarded_snapshot=guarded_snapshot};
    g->begin=g->last=io.now(io.context);
    if(!g->begin || g->begin==UINT64_MAX)return clock_fail(g,g->begin,g->begin,g->begin?GT_CLOCK_SATURATED:GT_CLOCK_ZERO);
    if(!read_pair(g))return false;
    /* Deliberately narrower than the hardware: do not clear somebody else's
     * nonzero low-half request even if the caller mistakenly supplies all grant bits. */
    if((g->requests&0xffffu) || g->ack)return fail(g,GT_ACCESS_FOREIGN);
    if(!clock_ok(g,g->begin))return false;
    if(!display_access_pin(&g->exclusion))return fail(g,GT_ACCESS_EXCLUSION);
    g->pinned=true;g->state=GT_ACCESS_OPENING;
    if(!write_reg(g,0xffff0000u) || !wait_state(g,0,false) ||
       !write_reg(g,0x00010001u) || !wait_state(g,1,true))return false;
    if(g->last==g->begin)return clock_fail(g,g->last,g->begin,GT_CLOCK_FROZEN);
    g->state=GT_ACCESS_AWAKE;return true;
}
bool intel_gt_access_open(struct intel_gt_access *g,const struct display_access_token *t,uint32_t grants,struct intel_gt_io io) {
    return open_access(g,t,grants,io,false);
}
bool intel_gt_access_open_with_guarded_snapshot(struct intel_gt_access *g,const struct display_access_token *t,
                                               uint32_t grants,struct intel_gt_io io) {
    return open_access(g,t,grants,io,true);
}
bool intel_gt_access_check(struct intel_gt_access *g) {
    if(!g)return false;
    if(g->object!=g || g->state!=GT_ACCESS_AWAKE)return fail(g,GT_ACCESS_STATE);
    uint64_t phase=g->last;
    if(g->guarded_snapshot) {
        if(!g->io.snapshot || !g->pinned || !display_access_held(&g->exclusion))
            return fail(g,GT_ACCESS_GUARD);
    } else if(!guarded(g))return false;
    g->io.pause(g->io.context);
    if(!clock_ok(g,phase))return false;
    if(g->io.snapshot) {
        if(g->snapshots==UINT32_MAX)return fail(g,GT_ACCESS_STATE);
        if(g->guarded_snapshot && (g->object!=g || g->state!=GT_ACCESS_AWAKE ||
           !g->pinned || !display_access_held(&g->exclusion)))return fail(g,GT_ACCESS_GUARD);
        ++g->snapshots;
        if(!g->io.snapshot(g->io.context,&g->requests,&g->ack,&g->thread) ||
           g->requests==UINT32_MAX || g->ack==UINT32_MAX || g->thread==UINT32_MAX)return fail(g,GT_ACCESS_IO);
        if(!clock_ok(g,phase))return false;
    } else if(!read_pair(g) || !read_reg(g,INTEL_GT_THREAD,&g->thread))return false;
    if((g->requests&0xffffu)!=1 || g->ack!=1 || (g->thread&7))return fail(g,GT_ACCESS_FOREIGN);
    if(!guarded(g) || !clock_ok(g,phase))return false;
    return g->last>phase ? true : clock_fail(g,g->last,phase,GT_CLOCK_FROZEN);
}
bool intel_gt_access_close(struct intel_gt_access *g) {
    if(!g)return false;
    if(g->object!=g || g->borrows || g->dependents)return false;
    if(!intel_gt_access_check(g))return false;
    uint64_t phase=g->last;
    if(!write_reg(g,0x00010000u) || !wait_state(g,0,false))return false;
    if(g->last==phase)return clock_fail(g,g->last,phase,GT_CLOCK_FROZEN);
    if(!display_access_unpin(&g->exclusion))return fail(g,GT_ACCESS_EXCLUSION);
    g->pinned=false;g->state=GT_ACCESS_CLOSED;return true;
}
bool intel_gt_access_borrow(struct intel_gt_access *g,struct intel_gt_borrow *b) {
    if(!b || b->object)return false;
    *b=(struct intel_gt_borrow){.object=b};
    if(!g || g->borrows==UINT32_MAX || !intel_gt_access_check(g))return false;
    if(!display_access_pin(&g->exclusion))return false;
    ++g->borrows;b->parent=g;b->active=true;
    b->next=g->dependents;g->dependents=b;return true;
}
bool intel_gt_access_return(struct intel_gt_borrow *b) {
    if(!b || b->object!=b || !b->active || !b->parent)return false;
    struct intel_gt_access *g=b->parent;
    if(g->object!=g || !g->borrows)return false;
    struct intel_gt_borrow **link=&g->dependents;
    for(unsigned n=0;*link && *link!=b && n<g->borrows;n++) {
        if((*link)->object!=*link || (*link)->parent!=g || !(*link)->active)return false;
        link=&(*link)->next;
    }
    if(*link!=b || !intel_gt_access_check(g) || !display_access_unpin(&g->exclusion))return false;
    *link=b->next;--g->borrows;b->active=false;return true;
}
