#include <stddef.h>
#include <srvros/intel_bdw_blitter.h>
#include <srvros/intel_bdw_traffic.h>

enum { WIN_CLOCK = 1u << 5 };

static bool identity(struct bdw_command_hold *h)
{
    const struct scanout_transfer *t=h->transfer;
    return h->owner && &h->owner->traffic==h && t && t->object==t && t->command_hold==h &&
        h->grant.object==t && h->grant.owner==t->owner && h->grant.epoch==t->epoch && t->pinned &&
        (!t->device_publication || t->device_publication==&h->owner->pages) &&
        display_access_held(&t->exclusion) &&
        h->io.traffic.held_epoch(t,&h->owner->pages)==h->epoch &&
        h->io.widi_epoch(t,&h->owner->pages)==h->widi_epoch && intel_bdw_peer_ports_held(&h->routes);
}

static bool sample_read(void *ctx,uint32_t offset,uint32_t *word)
{
    struct bdw_command_hold *h=ctx;
    return identity(h) && h->io.traffic.read(h->transfer,offset,word) && *word!=UINT32_MAX &&
        (offset!=0x40d0 || !(*word&0xff00u)) && identity(h);
}

static bool select_bank(void *ctx,uint32_t from,uint32_t to)
{
    struct bdw_command_hold *h=ctx;
    return identity(h) && h->io.traffic.select_l3(h->transfer,from,to)==GT_WRITE_DONE && identity(h);
}

static bool clock_ok(struct bdw_command_hold *h,uint64_t begin)
{
    uint64_t at=h->now(h->clock_owner);
    if (!at || at<h->last || (begin && at-begin>BDW_TRAFFIC_BLOCK_US) || !identity(h)) return false;
    h->last=at;
    return true;
}

static bool plane_held(struct bdw_command_hold *h)
{
    uint32_t control,queued,live;
    if (!sample_read(h,0x70180,&control) || !(control&(1u<<31)) ||
        !sample_read(h,0x7019c,&queued) || !sample_read(h,0x701ac,&live) ||
        (live&4095) || (queued&~8u)!=live || live>=h->transfer->manifest.fence_end) return false;
    if (!h->plane) { h->plane=control; h->surface=live; }
    return control==h->plane && live==h->surface;
}

/* A flush observed before WiDi acknowledged GO=0 cannot retire that reader.
 * Bracket the L3 register itself, since the arbiter and client reply separately. */
static bool drain_read(void *ctx,uint32_t offset,uint32_t *word)
{
    uint32_t before,after;
    if (offset!=0x8038) return sample_read(ctx,offset,word);
    return sample_read(ctx,0x8014,&before) && sample_read(ctx,offset,word) &&
        sample_read(ctx,0x8014,&after) && (!(*word&1) || !((before|after)&0x11));
}

/* Read the independent WIN clock request (CGMSG) and its reply from the
 * clocking unit (MSG_CLKGATE_GCP). The device owner's WiDi lease excludes
 * the producer and changes to the request between these observations. */
static bool widi_clock_gated(struct bdw_command_hold *h)
{
    uint32_t requested,ack,closing;
    return sample_read(h,0x8104,&requested) && sample_read(h,0x802c,&ack) &&
        sample_read(h,0x8104,&closing) && (requested&WIN_CLOCK)==WIN_CLOCK &&
        (ack&WIN_CLOCK)==WIN_CLOCK && (closing&WIN_CLOCK)==WIN_CLOCK;
}

static bool executing(struct bdw_command_hold *h)
{
    /* Broadwell's GPM reopens all clients together. The independent WiDi
     * lease stays held, while BCS gets memory access for the first command. */
    static const struct {uint32_t offset,mask,value;} checks[]={
        {0x40d0,0xff5f,BDW_GAM_CLIENTS},{0x8028,0x5f,BDW_GAM_CLIENTS},{BDW_RSTFCTLMSG,BDW_PREP_RST_WIDI_ACK,0},
        {0x8000,0x30,0x10},{0x8004,0x30,0x10},{0x8008,0x30,0x10},
        {0x800c,0x30,0x10},{0x8010,0x30,0x10},{0x8038,3,0},
        {0x8028,0x5f,BDW_GAM_CLIENTS},{0x40d0,0xff5f,BDW_GAM_CLIENTS},{BDW_RSTFCTLMSG,BDW_PREP_RST_WIDI_ACK,0}
    };
    if (!h->widi_epoch || !widi_clock_gated(h)) return false;
    for (unsigned i=0;i<sizeof(checks)/sizeof(checks[0]);i++) {
        uint32_t word;
        if (!sample_read(h,checks[i].offset,&word) || (word&checks[i].mask)!=checks[i].value) return false;
    }
    return widi_clock_gated(h) && intel_bdw_routes_held(&h->routes,h->transfer);
}

static bool finish_drain(struct bdw_command_hold *h,uint64_t begin,bool after_execution)
{
    struct bdw_traffic_sample s={.ctx=h,.read=drain_read};
    if (after_execution && (!h->widi_epoch || !widi_clock_gated(h))) goto retained;
    for (unsigned i=0;i<GT_ACCESS_POLL_CAP;i++) {
        bool drained=false;
        if (!clock_ok(h,begin) || !intel_bdw_traffic_poll(&s,&drained)) break;
        if (!drained) continue;
        if ((after_execution && !widi_clock_gated(h)) || !intel_bdw_traffic_still_blocked(h,sample_read) || !plane_held(h) ||
            !clock_ok(h,begin) || h->last==begin) break;
        ++h->drains;
        h->phase=BDW_CMD_BLOCKED;
        return true;
    }
retained:
    h->phase=BDW_CMD_RETAINED;
    return false;
}

bool intel_bdw_command_acquire(struct bdw_blitter_mapping *m,struct scanout_transfer *t,
        const struct scanout_transfer_token *grant,const struct bdw_wireless_routes *routes,
        struct bdw_command_control io,
        void *owner,uint64_t (*now)(void *))
{
    if (!m || m->traffic.owner || !t || !grant || t->object!=t || t->command_hold || t->busy ||
        t->device_publication || !grant->epoch || grant->epoch!=t->epoch || grant->object!=t ||
        grant->owner!=t->owner || !t->pinned || t->manifest.pages!=BDW_BLITTER_GLOBAL_PAGES ||
        (t->state!=TRANSFER_RESERVED && t->state!=TRANSFER_ACTIVE) || !display_access_held(&t->exclusion) ||
        !intel_bdw_routes_held(routes,t) || !intel_bdw_peer_ports_held(routes) || !owner || !now || !io.traffic.read || !io.traffic.held_epoch || !io.traffic.request ||
        !io.traffic.release || !io.traffic.select_l3 || !io.isolate_widi || !io.widi_epoch || !io.set_execution) return false;
    if (t->state==TRANSFER_RESERVED ? !scanout_transfer_reservation_matches(t,grant,&t->reservation) :
                                     !scanout_transfer_check(t,grant)) return false;
    struct bdw_command_hold *h=&m->traffic;
    *h=(struct bdw_command_hold){.owner=m,.transfer=t,.grant=*grant,.io=io,.routes=*routes,
        .now=now,.clock_owner=owner,.phase=BDW_CMD_ACQUIRING};
    t->command_hold=h;
    struct bdw_traffic_sample s={.ctx=h,.read=sample_read};
    bool started=clock_ok(h,0);
    uint64_t begin=h->last;
    if (!started || !plane_held(h) || !intel_bdw_routes_held(&h->routes,h->transfer) || !intel_bdw_traffic_open(&s) || !clock_ok(h,begin)) {
        /* No steering or device request issued yet. Relinquish our software pin. */
        t->command_hold=NULL;
        h->phase=BDW_CMD_REFUSED;
        return false;
    }
    if (!intel_bdw_traffic_l3_policy(&s,select_bank) || !intel_bdw_traffic_open(&s) ||
        !clock_ok(h,begin)) goto retained;
    uint64_t epoch=0;
    if (io.traffic.request(t,&m->pages,&epoch)!=GT_WRITE_DONE || !epoch) goto retained;
    h->epoch=epoch;
    return finish_drain(h,begin,false);
retained:
    h->phase=BDW_CMD_RETAINED;
    return false;
}

bool intel_bdw_command_held(struct bdw_blitter_mapping *m)
{
    if (!m) return false;
    struct bdw_command_hold *h=&m->traffic;
    if (h->owner!=m || !h->epoch || !h->drains ||
        (h->phase!=BDW_CMD_BLOCKED && h->phase!=BDW_CMD_EXECUTING)) return false;
    if (clock_ok(h,0) && intel_bdw_routes_held(&h->routes,h->transfer) && plane_held(h) && (!h->widi_epoch || widi_clock_gated(h)) &&
        (h->phase==BDW_CMD_BLOCKED ? intel_bdw_traffic_still_blocked(h,sample_read) : executing(h)))
        return true;
    h->phase=BDW_CMD_RETAINED;
    return false;
}

bool intel_bdw_command_bcs(struct bdw_blitter_mapping *m,bool enable)
{
    if (!m || m->traffic.owner!=m || m->traffic.transfer->device_publication!=&m->pages ||
        (enable && (m->pages.state!=PUB_PUBLISHED || m->pages.readers ||
                    m->verified_pages[0]!=BDW_BLITTER_GLOBAL_PAGES || !m->invalidation[0].complete))) return false;
    struct bdw_command_hold *h=&m->traffic;
    if (h->phase!=(enable?BDW_CMD_BLOCKED:BDW_CMD_EXECUTING) || !intel_bdw_command_held(m))
        return false;
    uint64_t begin=h->last;
    if (!enable && !((m->request.state==BDW_BLITTER_RETIRED && !m->device.reset) ||
                     (m->request.state==BDW_BLITTER_STATUS_PINNED && m->detach_reset))) return false;
    h->phase=BDW_CMD_SWITCHING;
    if (enable) {
        uint64_t epoch=0;
        if (h->widi_epoch || !intel_bdw_routes_held(&h->routes,h->transfer) ||
            h->io.isolate_widi(h->transfer,&m->pages,h->epoch,&epoch)!=GT_WRITE_DONE || !epoch) goto retained;
        h->widi_epoch=epoch;
        if (!widi_clock_gated(h) || !intel_bdw_traffic_still_blocked(h,sample_read) ||
            !plane_held(h) || !clock_ok(h,begin)) goto retained;
    }
    if (!enable) {
        struct bdw_traffic_sample s={.ctx=h,.read=sample_read};
        if (!intel_bdw_traffic_l3_policy(&s,select_bank) || !executing(h) ||
            !plane_held(h) || !clock_ok(h,begin)) goto retained;
    }
    if (h->io.set_execution(h->transfer,&m->pages,h->epoch,enable)!=GT_WRITE_DONE) goto retained;
    if (!enable) return finish_drain(h,begin,true);
    /* The owner returned from the transition. Verify the actual GO replies. */
    if (!executing(h) || !plane_held(h) || !clock_ok(h,begin)) goto retained;
    h->phase=BDW_CMD_EXECUTING;
    return true;
retained:
    h->phase=BDW_CMD_RETAINED;
    return false;
}

bool intel_bdw_command_release(struct bdw_blitter_mapping *m)
{
    if (!m || m->traffic.phase!=BDW_CMD_BLOCKED) return false;
    const struct scanout_publication *attached=m->traffic.transfer->device_publication;
    if ((attached && (attached!=&m->pages || m->pages.state!=PUB_RETIRED)) ||
        (m->pages.exposed && (!attached || m->pages.readers || !m->invalidation[1].complete ||
                              m->verified_pages[1]!=BDW_BLITTER_GLOBAL_PAGES)) ||
        !intel_bdw_command_held(m)) return false;
    struct bdw_command_hold *h=&m->traffic;
    uint64_t begin=h->last;
    h->phase=BDW_CMD_SWITCHING;
    if (h->io.traffic.release(h->transfer,&m->pages,h->epoch)==GT_WRITE_DONE) {
        h->epoch=0;
        h->widi_epoch=0;
        struct bdw_traffic_sample s={.ctx=h,.read=sample_read};
        for (unsigned i=0;i<GT_ACCESS_POLL_CAP;i++) {
            bool open;
            if (!clock_ok(h,begin) || !bdw_traffic_reopened(&s,&open)) break;
            if (!open) continue;
            if (!plane_held(h) || !clock_ok(h,begin) || h->last==begin) break;
            h->phase=BDW_CMD_RETURNED;
            h->transfer->command_hold=NULL;
            return true;
        }
    }
    h->phase=BDW_CMD_RETAINED;
    return false;
}
