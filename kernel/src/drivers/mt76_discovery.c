#include <srvros/mt76_discovery.h>

static void scrub(void *p, size_t n) {
    volatile uint8_t *v=p;
    while(n--) *v++=0;
}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n) {
    for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;
    return true;
}
static void add(struct mt76_discovery *s,const struct mt76_rx_beacon *b) {
    unsigned i;
    for(i=0;i<s->count;i++)if(equal(s->row[i].bssid,b->bssid,6))break;
    struct mt76_discovery_row *r=&s->row[i];
    if(i==s->count) {
        /* At most one new row per input and at most eight inputs. */
        s->count++;
        *r=(struct mt76_discovery_row){.channel=b->channel,.interval=b->interval,
            .hidden=true,.privacy=b->privacy,.rsn=b->rsn_present,.wpa=b->wpa_present};
        for(unsigned n=0;n<6;n++)r->bssid[n]=b->bssid[n];
    } else if(r->privacy!=b->privacy || r->rsn!=b->rsn_present || r->wpa!=b->wpa_present)
        r->security_conflict=true;
    r->observations++;
    r->ds_present|=b->ds_present;
    r->beacon_seen|=!b->probe_response;r->probe_response_seen|=b->probe_response;
    /* Hidden observations never erase a revealed name. Retain the first
     * visible name on conflict; flag it instead of silently selecting one. */
    if(!b->hidden) {
        if(!r->hidden && (r->ssid_size!=b->ssid_size || !equal(r->ssid,b->ssid,b->ssid_size)))
            r->ssid_conflict=true;
        else if(r->hidden) {
            r->ssid_size=b->ssid_size;r->hidden=false;
            for(unsigned n=0;n<b->ssid_size;n++)r->ssid[n]=b->ssid[n];
        }
    }
}
static bool ledger(const struct mt76_rx_window *w) {
    if(!w || !mt76_rx_window_channel(&w->plan) || !w->attempted || !w->polled || !w->poll_ok || !w->finish_attempted ||
       !w->complete || w->failed || !w->stop_attempted || !w->stopped ||
       !w->attempts || w->attempts>MT76_RX_WINDOW_SLOTS || w->completions>w->attempts ||
       w->attempts-w->completions>1 || w->beacons>w->completions)return false;
    for(unsigned i=0;i<w->attempts;i++) {
        const struct mt76_rx_window_item *v=&w->item[i];
        if(!v->submitted || v->completed!=(i<w->completions))return false;
        if(!v->completed) {
            if(v->completion || v->remaining || v->actual || v->inspected ||
               v->view_status!=255 || v->beacon_status!=255 || v->consumed ||
               v->mpdu || v->frame_control || v->beacon)return false;
        } else {
            if((v->completion!=1 && v->completion!=13) || v->remaining>MT76_RX_WINDOW_BYTES ||
               v->actual!=MT76_RX_WINDOW_BYTES-v->remaining ||
               v->inspected!=(v->actual>0))return false;
            if(!v->actual && (v->view_status!=255 || v->beacon_status!=255 ||
                v->consumed || v->mpdu || v->frame_control || v->beacon))return false;
            if(v->actual && !w->plan.buffer[i])return false;
        }
    }
    return true;
}
bool mt76_discovery_collect(struct mt76_discovery *out,
                            const struct mt76_rx_window *w,
                            const struct mt76_discovery_io *io) {
    if(!out || out->attempted)return false;
    *out=(struct mt76_discovery){.attempted=true,.failed=true};
    if(!ledger(w) || !io || !io->owned || !io->owned(io->context,w))return false;
    const struct mt76_discovery_io f=*io;
    struct mt76_discovery next={.attempted=true};
    struct mt76_rx_beacon b={0};struct mt76_rx_view view={0};bool ok=true;
    for(unsigned i=0;i<w->completions && ok;i++) {
        const struct mt76_rx_window_item *v=&w->item[i];
        if(!f.owned(f.context,w)){ok=false;break;}
        next.inputs++;
        if(!v->actual){next.empty++;continue;}
        enum mt76_rx_status vs=mt76_rx_usb_view(w->plan.buffer[i],v->actual,&view);
        enum mt76_rx_status bs=MT76_RX_INVALID;
        if(vs==MT76_RX_OK)bs=mt76_rx_beacon(w->plan.buffer[i],v->actual,mt76_rx_window_channel(&w->plan),&b);
        if(vs!=v->view_status || (vs==MT76_RX_OK &&
           (view.consumed!=v->consumed || view.mpdu_size!=v->mpdu ||
            view.frame_control!=v->frame_control || bs!=v->beacon_status)) ||
           (vs!=MT76_RX_OK && (v->beacon_status!=255 || v->consumed || v->mpdu || v->frame_control)) ||
           v->beacon!=(vs==MT76_RX_OK && bs==MT76_RX_OK)) {ok=false;break;}
        if(v->beacon && (v->interval!=b.interval || v->ssid_size!=b.ssid_size ||
           v->privacy!=b.privacy || v->rsn!=b.rsn_present || v->wpa!=b.wpa_present ||
           v->ds_present!=b.ds_present || v->probe_response!=b.probe_response)) {ok=false;break;}
        if(!f.owned(f.context,w)){ok=false;break;}
        if(v->beacon){add(&next,&b);next.parsed++;}
        else if(vs==MT76_RX_UNSUPPORTED || (vs==MT76_RX_OK && bs==MT76_RX_UNSUPPORTED))next.unrelated++;
        else next.rejected++;
        scrub(&b,sizeof b);scrub(&view,sizeof view);
    }
    ok=ok && next.parsed==w->beacons && f.owned(f.context,w);
    if(ok){next.complete=true;*out=next;}
    scrub(&b,sizeof b);scrub(&view,sizeof view);scrub(&next,sizeof next);
    return ok;
}
