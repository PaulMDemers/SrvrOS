#include <srvros/mt76_selection.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y?y-x<an:x-y<bn;
}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n){
    for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;
    return true;
}
static bool row_valid(const struct mt76_scan_row *v){
    const struct mt76_discovery_row *r=&v->advertisement;
    unsigned nonzero=0;for(unsigned i=0;i<6;i++)nonzero|=r->bssid[i];
    if(!nonzero || (r->bssid[0]&1) || r->ssid_size>32 || r->hidden!=(r->ssid_size==0) ||
       !r->interval || !r->observations || r->observations>MT76_SCAN_BATCH_ROWS ||
       (!r->beacon_seen && !r->probe_response_seen) ||
       (r->channel!=1 && r->channel!=6 && r->channel!=11) ||
       !v->channels || (v->channels&~UINT16_C(0x0421)) ||
       !(v->channels&(1u<<(r->channel-1))) ||
       v->channel_conflict!=((v->channels&(v->channels-1))!=0))return false;
    return true;
}
static bool batch_valid(const struct mt76_scan_batch *b){
    if(!b || !b->attempted || !b->complete || b->failed || b->visits!=3 ||
       b->inputs>24 || b->pending>3 || b->inputs+b->pending>24 ||
       b->count>24 || b->parsed>24 || b->empty>24 || b->unrelated>24 || b->rejected>24 ||
       b->parsed+b->empty+b->unrelated+b->rejected!=b->inputs ||
       b->count>b->parsed || (b->count==0)!=(b->parsed==0))return false;
    unsigned observations=0;
    for(unsigned i=0;i<b->count;i++){
        if(!row_valid(&b->row[i]))return false;
        observations+=b->row[i].advertisement.observations;
        for(unsigned j=0;j<i;j++)if(equal(b->row[i].advertisement.bssid,b->row[j].advertisement.bssid,6))return false;
    }
    return observations==b->parsed;
}
static struct mt76_scan_row copy_row(const struct mt76_scan_row *v){
    const struct mt76_discovery_row *r=&v->advertisement;
    struct mt76_scan_row next={.channels=v->channels,.channel_conflict=v->channel_conflict,
        .advertisement={.ssid_size=r->ssid_size,.channel=r->channel,.interval=r->interval,
        .observations=r->observations,.hidden=r->hidden,.privacy=r->privacy,.rsn=r->rsn,.wpa=r->wpa,
        .ds_present=r->ds_present,.beacon_seen=r->beacon_seen,.probe_response_seen=r->probe_response_seen,
        .ssid_conflict=r->ssid_conflict,.security_conflict=r->security_conflict}};
    for(unsigned i=0;i<6;i++)next.advertisement.bssid[i]=r->bssid[i];
    for(unsigned i=0;i<r->ssid_size;i++)next.advertisement.ssid[i]=r->ssid[i];
    return next;
}
bool mt76_selection_invalidate(struct mt76_selection *s){
    if(!s)return false;
    uint64_t old=s->generation;scrub(s,sizeof *s);
    s->generation=old==UINT64_MAX?old:old+1;
    return old!=UINT64_MAX;
}
bool mt76_selection_publish(struct mt76_selection *s,const struct mt76_scan_batch *b,
                             const struct mt76_selection_io *io){
    if(!s)return false;
    bool alias=(b && overlap(s,sizeof *s,b,sizeof *b)) || (io && overlap(s,sizeof *s,io,sizeof *io));
    if(!mt76_selection_invalidate(s) || alias || !b || !io || !io->owned)return false;
    const struct mt76_selection_io f=*io;
    if(!f.owned(f.context,b) || !batch_valid(b))return false;
    struct mt76_selection next={.generation=s->generation,.complete=true,.count=b->count};
    for(unsigned i=0;i<b->count;i++)next.row[i]=copy_row(&b->row[i]);
    bool ok=f.owned(f.context,b);
    if(ok)*s=next;
    scrub(&next,sizeof next);return ok;
}
enum mt76_selection_status mt76_selection_choose(const struct mt76_selection *s,
    uint64_t generation,unsigned index,struct mt76_selection_choice *out){
    if(!out || (s && overlap(s,sizeof *s,out,sizeof *out)))return MT76_SELECTION_INVALID;
    scrub(out,sizeof *out);
    if(!s)return MT76_SELECTION_INVALID;
    if(!generation || generation!=s->generation)return MT76_SELECTION_STALE;
    if(!s->complete)return MT76_SELECTION_UNAVAILABLE;
    if(s->count>24 || index>=s->count || !row_valid(&s->row[index]))return MT76_SELECTION_INVALID;
    const struct mt76_scan_row *v=&s->row[index];
    if(v->channel_conflict || v->advertisement.ssid_conflict || v->advertisement.security_conflict)return MT76_SELECTION_CONFLICT;
    if(v->advertisement.hidden)return MT76_SELECTION_HIDDEN;
    *out=(struct mt76_selection_choice){.generation=generation,.index=index,.country={'U','S'},.bandwidth=20,.row=copy_row(v)};
    return MT76_SELECTION_OK;
}
bool mt76_selection_name(const uint8_t *name,unsigned size,char *out,size_t cap){
    if(!out || !cap)return false;
    if(name && overlap(name,size,out,cap))return false;
    out[0]=0;
    if(size>32 || (!name && size))return false;
    size_t needed=1;
    for(unsigned i=0;i<size;i++)needed+=(name[i]>=32 && name[i]<=126 && name[i]!='\\')?1:4;
    if(needed>cap)return false;
    static const char hex[]="0123456789abcdef";unsigned n=0;
    for(unsigned i=0;i<size;i++){
        uint8_t x=name[i];
        if(x>=32 && x<=126 && x!='\\')out[n++]=(char)x;
        else{out[n++]='\\';out[n++]='x';out[n++]=hex[x>>4];out[n++]=hex[x&15];}
    }
    out[n]=0;return true;
}
