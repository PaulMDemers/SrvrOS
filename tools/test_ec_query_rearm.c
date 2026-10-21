#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/ec_query_rearm.h"
/* Latched-delivery model: once an enabled pending event latches an IRQ,
 * subsequently masking/clearing its source cannot withdraw that delivery. */
struct model {
    bool status,ec_pending,enabled,latched,queued;
    unsigned reads,enables,queues,arrival;
};
static void arrive(struct model *m) {
    m->status=true;m->ec_pending=true;
    if(m->enabled)m->latched=true;
}
static bool pending(void *p) {
    struct model *m=p;m->reads++;
    if(m->arrival==3 && m->reads==2)arrive(m);
    bool result=m->status||m->ec_pending;
    if(m->arrival==1 && m->reads==1)arrive(m); /* After precheck, before resume. */
    return result;
}
static void resume(void *p) {
    struct model *m=p;assert(!m->queued);m->enables++;m->enabled=true;
    if(m->status)m->latched=true;
    if(m->arrival==2)arrive(m); /* After enabling, before the postcheck. */
}
static void queue(void *p) {
    struct model *m=p;assert(!m->queued && (m->status||m->ec_pending));
    m->queues++;m->queued=true;m->enabled=false;m->status=false;
}
static void run(struct model *m) {
    const struct ec_query_rearm_ops ops={m,pending,resume,queue};ec_query_rearm(&ops);
}
int main(void) {
    /* Demonstrate the former ordering's stale-delivery mechanism. */
    struct model old={.status=true};resume(&old);if(pending(&old))queue(&old);
    assert(old.latched && !old.status && !old.enabled && old.queued);
    for(unsigned bits=1;bits<4;bits++) {
        struct model m={.status=(bits&1)!=0,.ec_pending=(bits&2)!=0};run(&m);
        assert(m.queued && m.queues==1 && m.enables==0 && m.reads==1 && !m.latched);
    }
    struct model quiet={0};run(&quiet);
    assert(quiet.enabled && quiet.enables==1 && quiet.reads==2 && !quiet.queued);
    for(unsigned arrival=1;arrival<=3;arrival++) {
        struct model m={.arrival=arrival};run(&m);
        assert(m.queued && m.queues==1 && m.enables==1 && m.reads==2);
        assert(!m.enabled && !m.status && m.latched); /* No false claim of eliminating this race. */
    }
    /* Work arriving after the postcheck stays enabled for normal IRQ delivery. */
    arrive(&quiet);assert(quiet.enabled && quiet.latched && !quiet.queued);
    puts("ec-query-rearm: PASS known pending avoids transient enable; quiet enables; late arrivals retained; latched-delivery limit explicit");
}
