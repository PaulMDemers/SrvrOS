#include <srvros/mt76_discovery5.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool equal(const uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static enum mt76_discovery5_status choose(struct mt76_discovery5_choice *out,
 const struct mt76_discovery *s,const struct mt76_rx_window *w,const struct mt76_discovery_io *io,
 uint64_t generation,const uint8_t *name,unsigned size){
 if(!s || !w || !io || !io->owned || !generation || !name || !size || size>32 ||
 !s->attempted || !s->complete || s->failed || s->count>8 || !mt76_rx5_window_plan_valid(&w->plan) ||
 !w->complete || !w->stopped || w->failed || w->completions>8)return MT76_DISC5_INVALID;
 if(!io->owned(io->context,w))return MT76_DISC5_OWNERSHIP;
 /* Rebuild the whole cache from the same immutable retained ledger, rather
  * than trusting externally supplied row/count flags. */
 struct mt76_discovery verified={0};
 if(!mt76_discovery5_collect(&verified,w,io))return MT76_DISC5_OWNERSHIP;
 bool identical=verified.count==s->count && verified.parsed==s->parsed && verified.inputs==s->inputs && verified.empty==s->empty && verified.unrelated==s->unrelated && verified.rejected==s->rejected;
 for(unsigned i=0;i<s->count && identical;i++){
  const struct mt76_discovery_row *a=&verified.row[i],*b=&s->row[i];
  identical=a->ssid_size==b->ssid_size && a->channel==b->channel && a->interval==b->interval &&
   a->observations==b->observations && a->hidden==b->hidden && a->privacy==b->privacy && a->rsn==b->rsn &&
   a->wpa==b->wpa && a->ds_present==b->ds_present && a->beacon_seen==b->beacon_seen && a->probe_response_seen==b->probe_response_seen &&
   a->ssid_conflict==b->ssid_conflict && a->security_conflict==b->security_conflict && equal(a->bssid,b->bssid,6) && equal(a->ssid,b->ssid,a->ssid_size);
 }
 if(!identical){scrub(&verified,sizeof verified);return MT76_DISC5_INVALID;}
 unsigned index=0;
 for(unsigned i=0;i<verified.count;i++)if(!verified.row[i].hidden && verified.row[i].ssid_size==size && equal(verified.row[i].ssid,name,size)){out->matches++;index=i;}
 if(!out->matches){scrub(&verified,sizeof verified);return MT76_DISC5_UNAVAILABLE;}
 if(out->matches!=1){scrub(&verified,sizeof verified);return MT76_DISC5_AMBIGUOUS;}
 const struct mt76_discovery_row row=verified.row[index];scrub(&verified,sizeof verified);
 if(row.ssid_conflict || row.security_conflict)return MT76_DISC5_CONFLICT;
 if(!row.privacy || !row.rsn || row.wpa)return MT76_DISC5_SECURITY;
 unsigned found=0;
 for(unsigned i=0;i<w->completions;i++){
  if(!io->owned(io->context,w))return MT76_DISC5_OWNERSHIP;
  const struct mt76_rx_window_item *item=&w->item[i];if(!item->beacon)continue;
  struct mt76_rx_beacon5 b={0};struct mt76_rx_view v={0};
  if(mt76_rx_beacon5(w->plan.buffer[i],item->actual,161,20,&b)!=MT76_RX_OK || mt76_rx_usb_view(w->plan.buffer[i],item->actual,&v)!=MT76_RX_OK){scrub(&b,sizeof b);return MT76_DISC5_INVALID;}
  bool match=equal(b.beacon.bssid,row.bssid,6);scrub(&b,sizeof b);if(!match)continue;
  const uint8_t *rsn=NULL;unsigned bytes=0;
  for(size_t pos=12;pos<v.body_size;){
   if(v.body_size-pos<2)return MT76_DISC5_INVALID;
   unsigned id=v.body[pos],n=v.body[pos+1];if(n>v.body_size-pos-2)return MT76_DISC5_INVALID;
   if(id==48){if(rsn)return MT76_DISC5_INVALID;rsn=v.body+pos;bytes=n+2;}
   pos+=n+2;
  }
  struct mt76_rsn_profile security={0};
  if(!rsn || mt76_rsn_psk_ccmp(rsn,bytes,&security)!=MT76_RSN_OK)return MT76_DISC5_SECURITY;
  if(found && (bytes!=out->rsn_size || !equal(rsn,out->rsn,bytes)))return MT76_DISC5_CONFLICT;
  if(!found){out->rsn_size=bytes;for(unsigned n=0;n<bytes;n++)out->rsn[n]=rsn[n];out->security=security;}
  found++;
  if(!io->owned(io->context,w))return MT76_DISC5_OWNERSHIP;
 }
 if(found!=row.observations)return MT76_DISC5_INVALID;
 out->row=row;out->generation=generation;out->selected=true;return MT76_DISC5_OK;
}
enum mt76_discovery5_status mt76_discovery5_choose(struct mt76_discovery5_choice *out,
 const struct mt76_discovery *s,const struct mt76_rx_window *w,const struct mt76_discovery_io *io,
 uint64_t generation,const uint8_t *name,unsigned size){
 if(!out)return MT76_DISC5_INVALID;
 if((s && overlap(out,sizeof *out,s,sizeof *s)) || (w && overlap(out,sizeof *out,w,sizeof *w)) ||
 (io && overlap(out,sizeof *out,io,sizeof *io)) || (name && overlap(out,sizeof *out,name,size)))return MT76_DISC5_INVALID;
 /* Never clear a retained input page by treating it as output storage. */
 if(w)for(unsigned i=0;i<8;i++)if(w->plan.buffer[i] && overlap(out,sizeof *out,w->plan.buffer[i],4096))return MT76_DISC5_INVALID;
 if(out->attempted)return MT76_DISC5_INVALID;
 *out=(struct mt76_discovery5_choice){.attempted=true};
 enum mt76_discovery5_status status=choose(out,s,w,io,generation,name,size);
 if(status!=MT76_DISC5_OK){unsigned matches=out->matches;scrub(out,sizeof *out);out->attempted=true;out->matches=matches;}
 return status;
}

enum mt76_discovery5_status mt76_discovery5_history(const struct mt76_discovery5_choice *chosen,
 const struct mt76_discovery *cache,const struct mt76_rx_window *w,const struct mt76_discovery_io *io){
 if(!chosen || !chosen->attempted || !chosen->selected || !chosen->generation || chosen->matches!=1 ||
    !chosen->row.ssid_size || chosen->row.ssid_size>32 || chosen->row.channel!=161 ||
    chosen->rsn_size<20 || chosen->rsn_size>257 || !cache || !w || !io || !io->owned)
  return MT76_DISC5_INVALID;
 /* choose() rebuilds the immutable history cache and checks every retained
  * parser receipt. A past visible target would have ended the survey already. */
 struct mt76_discovery5_choice prior={0};
 enum mt76_discovery5_status status=mt76_discovery5_choose(&prior,cache,w,io,
  chosen->generation,chosen->row.ssid,chosen->row.ssid_size);
 scrub(&prior,sizeof prior);
 if(status!=MT76_DISC5_UNAVAILABLE)return status==MT76_DISC5_OK?MT76_DISC5_CONFLICT:status;
 for(unsigned i=0;i<cache->count;i++){
  const struct mt76_discovery_row *r=&cache->row[i];
  if(!equal(r->bssid,chosen->row.bssid,6))continue;
  if(r->ssid_conflict || r->security_conflict || !r->hidden || !r->privacy || !r->rsn || r->wpa)
   return MT76_DISC5_CONFLICT;
 }
 for(unsigned i=0;i<w->completions;i++){
  if(!io->owned(io->context,w))return MT76_DISC5_OWNERSHIP;
  const struct mt76_rx_window_item *item=&w->item[i];if(!item->beacon)continue;
  struct mt76_rx_beacon5 b={0};struct mt76_rx_view v={0};
  if(mt76_rx_beacon5(w->plan.buffer[i],item->actual,161,20,&b)!=MT76_RX_OK ||
     mt76_rx_usb_view(w->plan.buffer[i],item->actual,&v)!=MT76_RX_OK)return MT76_DISC5_INVALID;
  bool match=equal(b.beacon.bssid,chosen->row.bssid,6);scrub(&b,sizeof b);if(!match)continue;
  const uint8_t *rsn=NULL;unsigned bytes=0;
  for(size_t pos=12;pos<v.body_size;){
   if(v.body_size-pos<2)return MT76_DISC5_INVALID;
   unsigned id=v.body[pos],n=v.body[pos+1];if(n>v.body_size-pos-2)return MT76_DISC5_INVALID;
   if(id==48){if(rsn)return MT76_DISC5_INVALID;rsn=v.body+pos;bytes=n+2;}
   pos+=n+2;
  }
  if(!rsn || bytes!=chosen->rsn_size || !equal(rsn,chosen->rsn,bytes))return MT76_DISC5_CONFLICT;
  if(!io->owned(io->context,w))return MT76_DISC5_OWNERSHIP;
 }
 return io->owned(io->context,w)?MT76_DISC5_OK:MT76_DISC5_OWNERSHIP;
}
