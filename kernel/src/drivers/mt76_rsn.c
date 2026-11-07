#include <srvros/mt76_rsn.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static unsigned le16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static bool suite(const uint8_t *p,unsigned type){return !p[0] && p[1]==15 && p[2]==172 && p[3]==type;}
enum mt76_rsn_status mt76_rsn_psk_ccmp(const uint8_t *p,size_t n,struct mt76_rsn_profile *out){
 if(!out)return MT76_RSN_INVALID;
 if(p && overlap(p,n,out,sizeof *out))return MT76_RSN_INVALID;
 scrub(out,sizeof *out);
 if(!p || n<20 || n>257 || p[0]!=48 || p[1]!=n-2 || le16(p+2)!=1)return MT76_RSN_INVALID;
 if(!suite(p+4,4))return MT76_RSN_UNSUPPORTED;
 size_t pos=8;unsigned count=le16(p+pos);pos+=2;
 if(!count || count>(n-pos)/4)return MT76_RSN_INVALID;
 bool ccmp=false;
 for(unsigned i=0;i<count;i++){if(suite(p+pos,4))ccmp=true;else return MT76_RSN_UNSUPPORTED;pos+=4;}
 if(!ccmp || n-pos<2)return MT76_RSN_INVALID;
 count=le16(p+pos);pos+=2;
 if(!count || count>(n-pos)/4)return MT76_RSN_INVALID;
 bool psk=false;
 for(unsigned i=0;i<count;i++){if(suite(p+pos,2))psk=true;else return MT76_RSN_UNSUPPORTED;pos+=4;}
 if(!psk)return MT76_RSN_UNSUPPORTED;
 unsigned cap=0;
 if(pos<n){if(n-pos<2)return MT76_RSN_INVALID;cap=le16(p+pos);pos+=2;}
 /* Replay-counter advertisements are accepted; no-pairwise/PMF-required or
  * unimplemented capabilities must not silently turn into another mode. */
 if(cap&~0x00bdu)return MT76_RSN_UNSUPPORTED;
 if(pos<n){if(n-pos<2)return MT76_RSN_INVALID;count=le16(p+pos);pos+=2;
  if(count>(n-pos)/16)return MT76_RSN_INVALID;
  if(count)return MT76_RSN_UNSUPPORTED;
 }
 if(pos<n){if(n-pos!=4)return MT76_RSN_INVALID;
  if(!(cap&128) || !suite(p+pos,6))return MT76_RSN_UNSUPPORTED;
  pos+=4;
 }
 if(pos!=n)return MT76_RSN_INVALID;
 out->capabilities=(uint16_t)cap;out->pmf_capable=!!(cap&128);
 return MT76_RSN_OK;
}
size_t mt76_rsn_station_ie(uint8_t *p,size_t cap){
 static const uint8_t ie[22]={48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,0,0};
 if(!p || cap<sizeof ie)return 0;
 for(unsigned i=0;i<sizeof ie;i++)p[i]=ie[i];
 return sizeof ie;
}
