#include <srvros/mt76_peer5.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool equal(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return false;return true;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool unicast(const uint8_t *p){unsigned any=0;for(unsigned i=0;i<6;i++)any|=p[i];return any && !(p[0]&1);}
static bool rate_mask(const uint8_t *p,size_t n,uint8_t *supported,uint8_t *basic){
 static const uint8_t ofdm[]={12,18,24,36,48,72,96,108};
 if(!n)return false;
 for(size_t i=0;i<n;i++){
  unsigned j;for(j=0;j<8;j++)if((p[i]&127)==ofdm[j])break;
  if(j==8){if(p[i]&128)return false;continue;}
  if(*supported&(1u<<j))return false;
  *supported|=1u<<j;if(p[i]&128)*basic|=1u<<j;
 }
 return true;
}
static bool rates(const struct mt76_rx_view *v,uint8_t *supported,uint8_t *basic){
 bool base=false,extended=false;
 if(v->body_size<12)return false;
 for(size_t pos=12;pos<v->body_size;){
  if(v->body_size-pos<2)return false;
  unsigned id=v->body[pos],n=v->body[pos+1];if(n>v->body_size-pos-2)return false;
  if(id==1){if(base || n>8 || !rate_mask(v->body+pos+2,n,supported,basic))return false;base=true;}
  if(id==50){if(extended || !rate_mask(v->body+pos+2,n,supported,basic))return false;extended=true;}
  pos+=n+2;
 }
 return base && (*basic&1);
}
static enum mt76_peer5_status bind(struct mt76_peer5 *out,const uint8_t *station,
 const struct mt76_discovery5_choice *choice,const struct mt76_discovery *cache,
 const struct mt76_rx_window *window,const struct mt76_discovery_io *io){
 if(!station || !choice || !cache || !window || !io || !io->owned ||
 !choice->attempted || !choice->selected || choice->matches!=1 || !choice->generation || !unicast(station))return MT76_PEER5_INVALID;
 struct mt76_discovery5_choice verified={0};
 enum mt76_discovery5_status status=mt76_discovery5_choose(&verified,cache,window,io,choice->generation,choice->row.ssid,choice->row.ssid_size);
 if(status!=MT76_DISC5_OK)return status==MT76_DISC5_OWNERSHIP?MT76_PEER5_OWNERSHIP:MT76_PEER5_INVALID;
 if(verified.rsn_size!=choice->rsn_size || !equal(verified.rsn,choice->rsn,verified.rsn_size) ||
 !equal(verified.row.bssid,choice->row.bssid,6) || choice->row.channel!=161 ||
 equal(station,verified.row.bssid,6))return MT76_PEER5_INVALID;
 uint8_t supported=0,basic=0;unsigned found=0;
 for(unsigned i=0;i<window->completions;i++){
  if(!io->owned(io->context,window))return MT76_PEER5_OWNERSHIP;
  if(!window->item[i].beacon)continue;
  struct mt76_rx_beacon5 b={0};struct mt76_rx_view view={0};
  if(mt76_rx_beacon5(window->plan.buffer[i],window->item[i].actual,161,20,&b)!=MT76_RX_OK ||
     mt76_rx_usb_view(window->plan.buffer[i],window->item[i].actual,&view)!=MT76_RX_OK)return MT76_PEER5_INVALID;
  if(!equal(b.beacon.bssid,verified.row.bssid,6))continue;
  uint8_t s=0,k=0;if(!rates(&view,&s,&k))return MT76_PEER5_RATES;
  if(found && (s!=supported || k!=basic))return MT76_PEER5_CONFLICT;
  supported=s;basic=k;found++;
  if(!io->owned(io->context,window))return MT76_PEER5_OWNERSHIP;
 }
 if(!found || found!=verified.row.observations)return MT76_PEER5_INVALID;
 if(!io->owned(io->context,window))return MT76_PEER5_OWNERSHIP;
 out->generation=verified.generation;out->observations=found;out->rates=supported;
 struct mt76_mgmt5_peer *p=&out->peer;
 for(unsigned i=0;i<6;i++){p->station[i]=station[i];p->bssid[i]=verified.row.bssid[i];}
 p->ssid_size=verified.row.ssid_size;for(unsigned i=0;i<p->ssid_size;i++)p->ssid[i]=verified.row.ssid[i];
 p->channel=161;p->basic_rates=basic;p->rsn_size=verified.rsn_size;for(unsigned i=0;i<p->rsn_size;i++)p->rsn[i]=verified.rsn[i];
 out->complete=true;return MT76_PEER5_OK;
}
enum mt76_peer5_status mt76_peer5_bind(struct mt76_peer5 *out,const uint8_t station[6],
 const struct mt76_discovery5_choice *choice,const struct mt76_discovery *cache,
 const struct mt76_rx_window *window,const struct mt76_discovery_io *io){
 if(!out || (station && overlap(out,sizeof *out,station,6)) || (choice && overlap(out,sizeof *out,choice,sizeof *choice)) ||
 (cache && overlap(out,sizeof *out,cache,sizeof *cache)) || (window && overlap(out,sizeof *out,window,sizeof *window)) ||
 (io && overlap(out,sizeof *out,io,sizeof *io)))return MT76_PEER5_INVALID;
 if(window)for(unsigned i=0;i<8;i++)if(window->plan.buffer[i] && overlap(out,sizeof *out,window->plan.buffer[i],4096))return MT76_PEER5_INVALID;
 if(out->attempted)return MT76_PEER5_INVALID;
 *out=(struct mt76_peer5){.attempted=true};
 enum mt76_peer5_status status=bind(out,station,choice,cache,window,io);
 if(status!=MT76_PEER5_OK){scrub(out,sizeof *out);out->attempted=true;}
 return status;
}
