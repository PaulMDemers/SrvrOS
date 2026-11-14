#include <srvros/mt76_mgmt5.h>
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){for(size_t i=0;i<n;i++)if(a[i]!=b[i])return false;return true;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static unsigned get16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static void put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static bool unicast(const uint8_t *p){unsigned any=0;for(unsigned i=0;i<6;i++)any|=p[i];return any && !(p[0]&1);}
static const uint8_t ofdm[]={12,18,24,36,48,72,96,108};
static bool peer_valid(const struct mt76_mgmt5_peer *p){
 if(!p || p->channel!=161 || !unicast(p->station) || !unicast(p->bssid) || equal(p->station,p->bssid,6) ||
 !p->ssid_size || p->ssid_size>32 || !(p->basic_rates&1) || p->rsn_size>sizeof p->rsn)return false;
 struct mt76_rsn_profile security={0};return mt76_rsn_psk_ccmp(p->rsn,p->rsn_size,&security)==MT76_RSN_OK;
}
size_t mt76_mgmt5_request(const struct mt76_mgmt5_peer *p,enum mt76_mgmt5_kind kind,
 unsigned sequence,bool retry,uint8_t *out,size_t capacity){
 if(!out || !p || overlap(out,capacity,p,sizeof *p) || !peer_valid(p) || sequence>4095 ||
 (kind!=MT76_MGMT5_AUTH && kind!=MT76_MGMT5_ASSOC && kind!=MT76_MGMT5_LEAVE))return 0;
 uint8_t bytes[94]={0};unsigned n=24;
 put16(bytes,(kind==MT76_MGMT5_AUTH?0xb0:kind==MT76_MGMT5_ASSOC?0:0xc0)|(retry?0x800:0));
 /* Six-Mbps OFDM ACK:16us SIFS+20us PLCP+6*4us symbols for14-byte PSDU. */
 put16(bytes+2,60);put16(bytes+22,sequence<<4);
 for(unsigned i=0;i<6;i++){bytes[4+i]=p->bssid[i];bytes[10+i]=p->station[i];bytes[16+i]=p->bssid[i];}
 if(kind==MT76_MGMT5_AUTH){put16(bytes+n,0);put16(bytes+n+2,1);put16(bytes+n+4,0);n+=6;}
 else if(kind==MT76_MGMT5_LEAVE){put16(bytes+n,3);n+=2;}
 else {
  put16(bytes+n,0x411);put16(bytes+n+2,1);n+=4;
  bytes[n++]=0;bytes[n++]=p->ssid_size;for(unsigned i=0;i<p->ssid_size;i++)bytes[n++]=p->ssid[i];
  bytes[n++]=1;bytes[n++]=8;for(unsigned i=0;i<8;i++)bytes[n++]=ofdm[i]|((p->basic_rates&(1u<<i))?128:0);
  n+=(unsigned)mt76_rsn_station_ie(bytes+n,sizeof bytes-n);
 }
 if(capacity<n){scrub(bytes,sizeof bytes);return 0;}
 for(unsigned i=0;i<n;i++)out[i]=bytes[i];
 scrub(bytes,sizeof bytes);return n;
}
static bool rates(const uint8_t *p,unsigned n,unsigned *seen){
 if(!n)return false;
 for(unsigned i=0;i<n;i++){
  unsigned j;for(j=0;j<8;j++)if((p[i]&127)==ofdm[j])break;
  if(j==8){if(p[i]&128)return false;continue;}
  if(*seen&(1u<<j))return false;
  *seen|=1u<<j;
 }
 return true;
}
enum mt76_mgmt5_status mt76_mgmt5_response(const struct mt76_mgmt5_peer *p,
 enum mt76_mgmt5_kind kind,const uint8_t *mpdu,size_t size,struct mt76_mgmt5_response *out){
 if(!out)return MT76_MGMT5_INVALID;
 if((p && overlap(out,sizeof *out,p,sizeof *p)) || (mpdu && overlap(out,sizeof *out,mpdu,size)))return MT76_MGMT5_INVALID;
 scrub(out,sizeof *out);
 if(!peer_valid(p) || !mpdu || size<30 || size>4096 || (kind!=MT76_MGMT5_AUTH && kind!=MT76_MGMT5_ASSOC))return MT76_MGMT5_INVALID;
 unsigned fc=get16(mpdu),want=kind==MT76_MGMT5_AUTH?0xb0:0x10;
 if((fc&~0x800u)!=want || (get16(mpdu+22)&15) || !equal(mpdu+4,p->station,6) || !equal(mpdu+10,p->bssid,6) || !equal(mpdu+16,p->bssid,6))return MT76_MGMT5_INVALID;
 if(kind==MT76_MGMT5_AUTH){
  if(get16(mpdu+24)!=0 || get16(mpdu+26)!=2)return MT76_MGMT5_INVALID;
  /* Linux v7.0 ieee80211_rx_mgmt_auth uses the30-byte fixed minimum.
   * Optional complete IEs do not change open-auth or security authority.
   * Retain a stricter whole-stream bound;never accept an orphan/truncated IE. */
  for(size_t pos=30;pos<size;){
   if(size-pos<2 || mpdu[pos+1]>size-pos-2)return MT76_MGMT5_INVALID;
   pos+=2+mpdu[pos+1];
  }
  out->status=(uint16_t)get16(mpdu+28);return out->status?MT76_MGMT5_REJECTED:MT76_MGMT5_OK;
 }
 unsigned cap=get16(mpdu+24),status=get16(mpdu+26),aid=get16(mpdu+28),seen=0;bool base=false,extra=false,rsn=false;
 for(size_t pos=30;pos<size;){
  if(size-pos<2)return MT76_MGMT5_INVALID;
  unsigned id=mpdu[pos],n=mpdu[pos+1];if(n>size-pos-2)return MT76_MGMT5_INVALID;
  const uint8_t *data=mpdu+pos+2;
  if(id==1){if(base || n>8 || !rates(data,n,&seen))return MT76_MGMT5_INVALID;base=true;}
  if(id==50){if(extra || !rates(data,n,&seen))return MT76_MGMT5_INVALID;extra=true;}
  if(id==48){struct mt76_rsn_profile sec={0};if(rsn || mt76_rsn_psk_ccmp(mpdu+pos,n+2,&sec)!=MT76_RSN_OK)return MT76_MGMT5_INVALID;rsn=true;}
  pos+=n+2;
 }
 if(status){out->status=(uint16_t)status;return MT76_MGMT5_REJECTED;}
 if(!(cap&1) || !(cap&16) || (cap&2) || !base || !(seen&1) || (aid&0xc000)!=0xc000 || !(aid&0x3fff) || (aid&0x3fff)>2007)return MT76_MGMT5_INVALID;
 out->aid=(uint16_t)(aid&0x3fff);return MT76_MGMT5_OK;
}

size_t mt76_mgmt5_usb_request(const struct mt76_mgmt5_peer *p,enum mt76_mgmt5_kind kind,
 unsigned sequence,bool retry,uint8_t *out,size_t capacity){
 if(!out || !p || overlap(out,capacity,p,sizeof *p))return 0;
 uint8_t mpdu[94]={0},packet[124]={0};
 size_t n=mt76_mgmt5_request(p,kind,sequence,retry,mpdu,sizeof mpdu);
 if(!n)return 0;
 size_t payload=(20+n+3)&~(size_t)3,total=payload+8;
 if(capacity<total){scrub(mpdu,sizeof mpdu);return 0;}
 /* Linux mt76x02u_skb_dma_info/write_txwi:WLAN0,EDCA2,802.11,WIV;
  * length includes20-byte TXWI and four-byte rounding;final four bytes zero.
  * OFDM PHY1/index0/BW0,ACK_REQ,WCIDff and rate-only no-skb status80. */
 uint32_t info=(uint32_t)payload|(1u<<19)|(1u<<24)|(2u<<25);
 for(unsigned i=0;i<4;i++)packet[i]=(uint8_t)(info>>(8*i));
 put16(packet+6,0x2000);packet[8]=1;packet[9]=255;put16(packet+10,(unsigned)n);packet[23]=128;
 for(size_t i=0;i<n;i++)packet[24+i]=mpdu[i];
 for(size_t i=0;i<total;i++)out[i]=packet[i];
 scrub(mpdu,sizeof mpdu);scrub(packet,sizeof packet);return total;
}
