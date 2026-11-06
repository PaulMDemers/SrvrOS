#include <srvros/intel_gt_access.h>
static bool fail(struct intel_gt_access *g,uint32_t reason) {
    g->reason|=reason;
    if(g->pinned)g->state=GT_ACCESS_FAULT;
    return false;
}
static bool clock_ok(struct intel_gt_access *g,uint64_t phase) {
    uint64_t t=g->io.now(g->io.context);
    if(!t || t<g->last || t<phase || t-phase>GT_ACCESS_PHASE_US || t-g->begin>GT_ACCESS_TOTAL_US)return fail(g,GT_ACCESS_CLOCK);
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
        if(g->requests!=desired || (g->ack&~1u))return fail(g,GT_ACCESS_FOREIGN);
        if(g->ack==desired) {
            if(!c0)return guarded(g) && clock_ok(g,phase);
            if(!read_reg(g,INTEL_GT_THREAD,&g->thread))return false;
            if(!(g->thread&7))return guarded(g) && clock_ok(g,phase);
        }
        g->io.pause(g->io.context);
    }
    return fail(g,GT_ACCESS_TIMEOUT);
}
bool intel_gt_access_open(struct intel_gt_access *g,const struct display_access_token *t,uint32_t grants,struct intel_gt_io io) {
    if(!g)return false;
    if((g->object && g->object!=g) || (g->state!=GT_ACCESS_EMPTY && g->state!=GT_ACCESS_CLOSED))return fail(g,GT_ACCESS_STATE);
    uint32_t missing=GT_ACCESS_REQUIRED&~grants;
    if(!display_access_held(t))missing|=GT_ACCESS_EXCLUSION;
    if(grants&~GT_ACCESS_REQUIRED || !io.guard || !io.read || !io.write || !io.now || !io.pause)missing|=GT_ACCESS_INPUT;
    if(missing){g->reason=missing;return false;}
    struct display_access_token saved=*t;
    *g=(struct intel_gt_access){.object=g,.exclusion=saved,.io=io};
    g->begin=g->last=io.now(io.context);
    if(!g->begin || g->begin==UINT64_MAX)return fail(g,GT_ACCESS_CLOCK);
    if(!read_pair(g))return false;
    /* Deliberately narrower than the hardware: do not clear somebody else's
     * nonzero request even if the caller mistakenly supplies all grant bits. */
    if(g->requests || g->ack)return fail(g,GT_ACCESS_FOREIGN);
    if(!clock_ok(g,g->begin))return false;
    if(!display_access_pin(&g->exclusion))return fail(g,GT_ACCESS_EXCLUSION);
    g->pinned=true;g->state=GT_ACCESS_OPENING;
    if(!write_reg(g,0xffff0000u) || !wait_state(g,0,false) ||
       !write_reg(g,0x00010001u) || !wait_state(g,1,true))return false;
    if(g->last==g->begin)return fail(g,GT_ACCESS_CLOCK);
    g->state=GT_ACCESS_AWAKE;return true;
}
bool intel_gt_access_check(struct intel_gt_access *g) {
    if(!g)return false;
    if(g->object!=g || g->state!=GT_ACCESS_AWAKE)return fail(g,GT_ACCESS_STATE);
    uint64_t phase=g->last;
    if(!guarded(g))return false;
    g->io.pause(g->io.context);
    if(!clock_ok(g,phase) || !read_pair(g) || !read_reg(g,INTEL_GT_THREAD,&g->thread))return false;
    if(g->requests!=1 || g->ack!=1 || (g->thread&7))return fail(g,GT_ACCESS_FOREIGN);
    if(!guarded(g) || !clock_ok(g,phase))return false;
    return g->last>phase ? true : fail(g,GT_ACCESS_CLOCK);
}
bool intel_gt_access_close(struct intel_gt_access *g) {
    if(!g)return false;
    if(!intel_gt_access_check(g))return false;
    uint64_t phase=g->last;
    if(!write_reg(g,0x00010000u) || !wait_state(g,0,false))return false;
    if(g->last==phase)return fail(g,GT_ACCESS_CLOCK);
    if(!display_access_unpin(&g->exclusion))return fail(g,GT_ACCESS_EXCLUSION);
    g->pinned=false;g->state=GT_ACCESS_CLOSED;return true;
}
