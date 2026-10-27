#include <srvros/mt76_scan.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
unsigned mt76_scan_channel(unsigned i){static const uint8_t channels[3]={1,6,11};return i<3?channels[i]:0;}
static bool ledger(const struct mt76_scan_input *v,unsigned i){
    const struct mt76_channel_tune *t=&v->tune;
    if(v->channel!=mt76_scan_channel(i) || !v->complete || !v->stopped || !v->quiescent || !v->retained ||
       !v->ring || !v->physical || (v->ring&4095) || (v->physical&4095) || v->ring==v->physical ||
       v->ring>UINT64_MAX-4095 || v->physical>UINT64_MAX-4095 || !v->buffer ||
       !t->attempted || !t->complete || t->failed || t->status!=MT76_CHANNEL_OK || t->channel!=v->channel ||
       !t->target_complete || !t->observe_attempted || !t->observe_complete || !t->cleanup_attempted ||
       !t->cleanup_complete || !t->vco_attempted || !t->cleanup_vco_attempted || !t->target_vco_clear ||
       !t->cleanup_vco_clear || t->saved_mask!=8191 || t->touched_mask!=8191 || t->restored_mask!=8191 || t->steps!=21 ||
       !t->target_polls || t->target_polls>6 || !t->cleanup_polls || t->cleanup_polls>6 ||
       t->target_delay_us>100000 || t->cleanup_delay_us>100000 || v->events>1 || v->elapsed_us>10100000)return false;
    for(unsigned n=0;n<13;n++)if(t->saved[n]!=mt76_channel_snapshot_initial(n))return false;
    const unsigned delays[6]={0,10,100,1000,10000,100000};
    if(t->target_delay_us!=delays[t->target_polls-1] || t->cleanup_delay_us!=delays[t->cleanup_polls-1])return false;
    bool normal=v->events && (v->completion==1 || v->completion==13);
    if(normal)return v->remaining<=4096 && v->actual==4096-v->remaining;
    return !v->actual && !v->during_window && v->elapsed_us>=10000000 &&
        (v->events?(v->completion>=26 && v->completion<=28 && (v->completion==27 || v->remaining<=4096)):(!v->completion && !v->remaining));
}
static void add(struct mt76_scan *s,const struct mt76_rx_beacon *b){
    unsigned i;for(i=0;i<s->count;i++)if(equal(s->row[i].advertisement.bssid,b->bssid,6))break;
    struct mt76_scan_row *row=&s->row[i];struct mt76_discovery_row *r=&row->advertisement;
    if(i==s->count){s->count++;*r=(struct mt76_discovery_row){.channel=b->channel,.interval=b->interval,.hidden=true,.privacy=b->privacy,.rsn=b->rsn_present,.wpa=b->wpa_present};for(unsigned n=0;n<6;n++)r->bssid[n]=b->bssid[n];}
    else if(r->privacy!=b->privacy || r->rsn!=b->rsn_present || r->wpa!=b->wpa_present)r->security_conflict=true;
    row->channels|=(uint16_t)(1u<<(b->channel-1));row->channel_conflict|=r->channel!=b->channel;
    r->observations++;r->ds_present|=b->ds_present;r->beacon_seen|=!b->probe_response;r->probe_response_seen|=b->probe_response;
    if(!b->hidden){
        if(!r->hidden && (r->ssid_size!=b->ssid_size || !equal(r->ssid,b->ssid,b->ssid_size)))r->ssid_conflict=true;
        else if(r->hidden){r->hidden=false;r->ssid_size=b->ssid_size;for(unsigned n=0;n<b->ssid_size;n++)r->ssid[n]=b->ssid[n];}
    }
}
bool mt76_scan_collect(struct mt76_scan *out,const uint8_t country[2],const struct mt76_scan_input *input,unsigned count,const struct mt76_scan_io *io){
    if(!out || out->attempted)return false;
    *out=(struct mt76_scan){.attempted=true,.failed=true};
    if(!country || country[0]!='U' || country[1]!='S' || !input || count!=3 || !io || !io->owned)return false;
    const struct mt76_scan_io f=*io;
    for(unsigned i=0;i<3;i++){
        if(!ledger(&input[i],i))return false;
        for(unsigned j=0;j<i;j++)if(input[i].ring==input[j].ring || input[i].ring==input[j].physical || input[i].physical==input[j].ring || input[i].physical==input[j].physical || input[i].buffer==input[j].buffer)return false;
    }
    if(!f.owned(f.context,input,count))return false;
    struct mt76_scan next={.attempted=true};struct mt76_rx_beacon b={0};bool ok=true;
    for(unsigned i=0;i<3 && ok;i++){
        const struct mt76_scan_input *v=&input[i];if(!f.owned(f.context,input,count)){ok=false;break;}next.inputs++;
        if(!v->during_window)next.unobserved++;
        else if(!v->actual)next.empty++;
        else {
            enum mt76_rx_status status=mt76_rx_beacon(v->buffer,v->actual,v->channel,&b);
            if(status==MT76_RX_OK){add(&next,&b);next.parsed++;}
            else if(status==MT76_RX_UNSUPPORTED)next.unrelated++;
            else next.rejected++;
            scrub(&b,sizeof b);
        }
        ok=f.owned(f.context,input,count);
    }
    ok=ok && f.owned(f.context,input,count);
    if(ok){next.complete=true;*out=next;}
    scrub(&b,sizeof b);scrub(&next,sizeof next);return ok;
}
