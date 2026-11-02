#include <srvros/mt76_scan_batch.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool empty_item(const struct mt76_rx_window_item *v){
    return !v->submitted && !v->completed && !v->inspected && !v->beacon &&
        !v->completion && !v->remaining && !v->actual && !v->consumed && !v->mpdu &&
        v->view_status==255 && v->beacon_status==255 && !v->frame_control && !v->interval &&
        !v->ssid_size && !v->privacy && !v->rsn && !v->wpa && !v->ds_present && !v->probe_response;
}
static bool plans_valid(const struct mt76_scan_batch_visit *v){
    for(unsigned i=0;i<MT76_SCAN_VISITS;i++){
        const struct mt76_rx_window *w=&v[i].window;const struct mt76_rx_window_plan *p=&w->plan;
        if(!mt76_scan_tune_valid(&v[i].tune,mt76_scan_channel(i)) ||
           !mt76_rx_window_plan_valid(p) || p->country[0]!='U' || p->country[1]!='S' ||
           p->channel!=mt76_scan_channel(i) || p->budget_us!=MT76_RX_WINDOW_LONG_US ||
           !w->attempted || !w->polled || !w->poll_ok || !w->finish_attempted || !w->complete ||
           w->failed || !w->stop_attempted || !w->stopped ||
           !w->attempts || w->attempts>MT76_RX_WINDOW_SLOTS || w->completions>w->attempts ||
           w->attempts-w->completions>1 || w->polls<w->completions || !w->polls || w->polls>20000000 ||
           w->elapsed_us>MT76_RX_WINDOW_LONG_US+100000 ||
           (w->completions<MT76_RX_WINDOW_SLOTS && w->elapsed_us<MT76_RX_WINDOW_LONG_US))return false;
        /* Check all preallocated resources, including unsubmitted pages. */
        for(unsigned j=0;j<i;j++){
            const struct mt76_rx_window_plan *q=&v[j].window.plan;
            if(p->slot!=q->slot || p->ring==q->ring)return false;
            for(unsigned a=0;a<MT76_RX_WINDOW_SLOTS;a++){
                if(p->physical[a]==q->ring || p->ring==q->physical[a])return false;
                for(unsigned b=0;b<MT76_RX_WINDOW_SLOTS;b++)
                    if(p->physical[a]==q->physical[b] || p->buffer[a]==q->buffer[b])return false;
            }
        }
        for(unsigned n=0;n<w->attempts;n++){
            const struct mt76_rx_window_item *item=&w->item[n];
            if(!item->beacon && (item->interval || item->ssid_size || item->privacy || item->rsn ||
               item->wpa || item->ds_present || item->probe_response))return false;
        }
        for(unsigned n=w->attempts;n<MT76_RX_WINDOW_SLOTS;n++)if(!empty_item(&w->item[n]))return false;
    }
    return true;
}
struct collect_context {
    const struct mt76_scan_batch_visit *visits;
    const struct mt76_scan_batch_io *io;
    unsigned index;
};
static bool owned(void *context,const struct mt76_rx_window *w){
    struct collect_context *c=context;
    return w==&c->visits[c->index].window && c->io->owned(c->io->context,c->visits,MT76_SCAN_VISITS);
}
static void add(struct mt76_scan_batch *s,const struct mt76_discovery_row *v){
    unsigned i;for(i=0;i<s->count;i++)if(equal(s->row[i].advertisement.bssid,v->bssid,6))break;
    struct mt76_scan_row *row=&s->row[i];struct mt76_discovery_row *r=&row->advertisement;
    if(i==s->count){s->count++;*r=*v;}
    else{
        r->observations+=v->observations;r->ds_present|=v->ds_present;
        r->beacon_seen|=v->beacon_seen;r->probe_response_seen|=v->probe_response_seen;
        r->ssid_conflict|=v->ssid_conflict;r->security_conflict|=v->security_conflict ||
            r->privacy!=v->privacy || r->rsn!=v->rsn || r->wpa!=v->wpa;
        if(!v->hidden){
            if(!r->hidden && (r->ssid_size!=v->ssid_size || !equal(r->ssid,v->ssid,v->ssid_size)))r->ssid_conflict=true;
            else if(r->hidden){r->hidden=false;r->ssid_size=v->ssid_size;for(unsigned n=0;n<v->ssid_size;n++)r->ssid[n]=v->ssid[n];}
        }
    }
    row->channels|=(uint16_t)(1u<<(v->channel-1));row->channel_conflict|=r->channel!=v->channel;
}
bool mt76_scan_batch_collect(struct mt76_scan_batch *out,
    const struct mt76_scan_batch_visit *visits,unsigned count,const struct mt76_scan_batch_io *io){
    if(!out || out->attempted)return false;
    *out=(struct mt76_scan_batch){.attempted=true,.failed=true};
    if(!visits || count!=MT76_SCAN_VISITS || !io || !io->owned || !plans_valid(visits))return false;
    const struct mt76_scan_batch_io f=*io;
    if(!f.owned(f.context,visits,count))return false;
    struct mt76_scan_batch next={.attempted=true};struct mt76_discovery result={0};
    struct collect_context c={visits,&f,0};const struct mt76_discovery_io d={&c,owned};bool ok=true;
    for(unsigned i=0;i<count && ok;i++){
        c.index=i;result=(struct mt76_discovery){0};
        ok=mt76_discovery_collect(&result,&visits[i].window,&d);
        if(!ok)break;
        next.visits++;next.inputs+=result.inputs;next.pending+=visits[i].window.attempts-visits[i].window.completions;
        next.empty+=result.empty;next.parsed+=result.parsed;next.unrelated+=result.unrelated;next.rejected+=result.rejected;
        for(unsigned n=0;n<result.count;n++)add(&next,&result.row[n]);
        scrub(&result,sizeof result);
    }
    ok=ok && next.visits==count && f.owned(f.context,visits,count);
    if(ok){next.complete=true;*out=next;}
    scrub(&result,sizeof result);scrub(&next,sizeof next);return ok;
}
