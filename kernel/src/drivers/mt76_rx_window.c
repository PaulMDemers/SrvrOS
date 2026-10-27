#include <srvros/mt76_rx_window.h>

static bool plan_valid(const struct mt76_rx_window_plan *p) {
    if (!p || !p->ring || (p->ring & 4095) || p->ring > UINT64_MAX-4095 ||
        !p->slot || p->slot > 255 || p->dci != 9) return false;
    for (unsigned i=0;i<MT76_RX_WINDOW_SLOTS;i++) {
        uint64_t a=p->physical[i]; uintptr_t v=(uintptr_t)p->buffer[i];
        if (!a || (a&4095) || a>UINT64_MAX-4095 || !v ||
            (v&4095) || v>UINTPTR_MAX-4095 || a==p->ring) return false;
        for (unsigned j=0;j<i;j++)
            if (a==p->physical[j] || v==(uintptr_t)p->buffer[j]) return false;
    }
    return true;
}
static void inspect(struct mt76_rx_window_item *s,const uint8_t *p) {
    if (!s->actual) return;
    struct mt76_rx_view v; struct mt76_rx_beacon b={0};
    s->inspected=true;s->view_status=mt76_rx_usb_view(p,s->actual,&v);
    if (s->view_status==MT76_RX_OK) {
        s->consumed=(unsigned)v.consumed;s->mpdu=(unsigned)v.mpdu_size;
        s->frame_control=v.frame_control;
        s->beacon_status=mt76_rx_beacon(p,s->actual,1,&b);
        if (s->beacon_status==MT76_RX_OK) {
            s->beacon=true;s->interval=b.interval;s->ssid_size=b.ssid_size;
            s->privacy=b.privacy;s->rsn=b.rsn_present;s->wpa=b.wpa_present;
            s->ds_present=b.ds_present;s->probe_response=b.probe_response;
        }
    }
    for (unsigned i=0;i<sizeof b;i++) ((volatile uint8_t *)&b)[i]=0;
}
bool mt76_rx_window_poll(struct mt76_rx_window *s,
                        const struct mt76_rx_window_plan *plan,
                        const struct mt76_rx_window_io *io) {
    if (!s || s->attempted) return false;
    *s=(struct mt76_rx_window){.attempted=true};
    for (unsigned i=0;i<MT76_RX_WINDOW_SLOTS;i++)
        s->item[i].view_status=s->item[i].beacon_status=255;
    if (!plan_valid(plan) || !io || !io->owned || !io->post || !io->poll ||
        !io->now) {s->failed=true;return false;}
    /* Copy callback/plan identities. The adapter must keep pointees and native
     * ring/endpoint ownership stable; owned() checks that before each action. */
    const struct mt76_rx_window_plan p=*plan;s->plan=p;
    const struct mt76_rx_window_io f=*io;
    uint64_t start=f.now(f.context),previous=start;
    unsigned stalled=0;bool pending=false,ok=true,finished=false;
    for (unsigned n=0;n<2000000;n++) {
        uint64_t now=f.now(f.context);
        if (now<previous || now<start || (now==previous && ++stalled>=1024) ||
            now-start>MT76_RX_WINDOW_US+100000) {ok=false;break;}
        if (now!=previous) stalled=0;
        previous=now;
        if (!f.owned(f.context,false)) {ok=false;break;}
        if (now-start>=MT76_RX_WINDOW_US ||
            (!pending && s->attempts==MT76_RX_WINDOW_SLOTS)) {finished=true;break;}
        if (!pending) {
            unsigned i=s->attempts++;
            /* A failed submission can still expose a TRB to hardware. Count
             * the attempt before the callback and stop; never retry its page. */
            pending=true;
            if (!f.post(f.context,i,p.ring+16*i,p.physical[i])) {ok=false;break;}
            s->item[i].submitted=true;
        }
        if (!f.owned(f.context,false)) {ok=false;break;}
        struct mt76_rx_window_event e={0};bool present=false;
        if (!f.poll(f.context,&e,&present)) {ok=false;break;}
        s->polls++;
        if (!present) continue;
        unsigned i=s->attempts-1;
        if (e.slot!=p.slot || e.dci!=p.dci || e.trb!=p.ring+16*i) {
            s->ignored++;continue;
        }
        /* Stopped/error events cannot supply payload, even residue zero. */
        if (e.event_data || (e.completion!=1 && e.completion!=13) ||
            e.remaining>MT76_RX_WINDOW_BYTES) {ok=false;break;}
        struct mt76_rx_window_item *v=&s->item[i];
        v->completed=true;v->completion=e.completion;v->remaining=e.remaining;
        v->actual=MT76_RX_WINDOW_BYTES-e.remaining;
        s->completions++;pending=false;
    }
    if (!finished) ok=false;
    uint64_t end=f.now(f.context);
    if (end<previous || end<start || end-start>MT76_RX_WINDOW_US+100000) ok=false;
    s->elapsed_us=end>=start?end-start:0;
    s->polled=true;s->poll_ok=ok;s->failed=!ok;
    return ok;
}
bool mt76_rx_window_finish(struct mt76_rx_window *s,
                          const struct mt76_rx_window_io *io,
                          const struct mt76_rx_window_stop *stop) {
    if (!s || !s->attempted || s->finish_attempted) return false;
    s->finish_attempted=true;
    bool ok=s->polled && s->poll_ok && !s->failed && plan_valid(&s->plan) &&
        io && io->owned && s->attempts && s->attempts<=MT76_RX_WINDOW_SLOTS;
    if (s->attempts && s->attempts<=MT76_RX_WINDOW_SLOTS) {
        s->stop_attempted=stop!=0;
        uint64_t current=s->plan.ring+16*(s->attempts-1);
        uint64_t dequeue=stop?stop->dequeue&~15ull:0;
        s->stopped=stop && stop->state==3 && stop->slot==s->plan.slot &&
            stop->dci==s->plan.dci && (stop->dequeue&15)==1 &&
            (dequeue==current || dequeue==current+16);
    }
    ok=ok && s->stopped;
    /* A failed poll/cleanup never licenses reading even a completed buffer. */
    if (ok) for (unsigned i=0;i<s->attempts;i++) {
        if (!io->owned(io->context,true)) {ok=false;break;}
        if (!s->item[i].completed) continue;
        __asm__ volatile("" ::: "memory");
        inspect(&s->item[i],s->plan.buffer[i]);
        if (s->item[i].beacon) s->beacons++;
    }
    s->failed=!ok;s->complete=ok;return ok;
}
bool mt76_rx_window_run(struct mt76_rx_window *s,
                        const struct mt76_rx_window_plan *plan,
                        const struct mt76_rx_window_io *io) {
    if (!s || s->attempted) return false;
    if (!io || !io->stop) {
        *s=(struct mt76_rx_window){.attempted=true,.failed=true};return false;
    }
    (void)mt76_rx_window_poll(s,plan,io);
    struct mt76_rx_window_stop stop={0};bool stopped=false;
    if (s->attempts) stopped=io->stop(io->context,&stop);
    bool ok=mt76_rx_window_finish(s,io,stopped?&stop:0);
    /* An attempted but failed callback is still an attempted cessation. */
    if(s->attempts)s->stop_attempted=true;
    return ok;
}
