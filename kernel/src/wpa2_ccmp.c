#include <srvros/wpa2_ccmp.h>
#include <srvros/wpa2_aes128.h>
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){if(!an || !bn)return false;uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned d=0;for(size_t i=0;i<n;i++)d|=a[i]^b[i];return !d;}
static bool zero(const void *p,size_t n){const uint8_t *v=p;unsigned d=0;for(size_t i=0;i<n;i++)d|=v[i];return !d;}
static bool unicast(const uint8_t *p){return !(p[0]&1) && !zero(p,6);}
static unsigned le16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static bool binding(const struct wpa2_ccmp *s,const struct wpa2_handshake *h){
 return h->generation && s->generation==h->generation && s->peer.rsne_size==h->peer.rsne_size && h->peer.rsne_size<=64 &&
 equal(s->peer.aa,h->peer.aa,6) && equal(s->peer.spa,h->peer.spa,6) && equal(s->peer.rsne,h->peer.rsne,h->peer.rsne_size);
}
bool wpa2_ccmp_install(struct wpa2_ccmp *s,struct wpa2_handshake *h,const struct wpa2_hs_keys *k){
 if(!s || !h || !k || overlap(s,sizeof *s,h,sizeof *h) || overlap(s,sizeof *s,k,sizeof *k) || overlap(h,sizeof *h,k,sizeof *k) || !zero(s,sizeof *s))return false;
 if(h->phase!=WPA2_HS_INSTALL_PENDING || !h->generation || k->generation!=h->generation || h->install_handoffs!=1 || !h->keys_may_live || h->unsafe || h->peer.rsne_size>64 ||
 !unicast(h->peer.aa) || !unicast(h->peer.spa) || equal(h->peer.aa,h->peer.spa,6) || k->group_id>3 || k->group_id!=h->group_id ||
 !equal(k->pairwise,h->ptk+32,16) || !equal(k->group,h->gtk,16) || !equal(k->group_rsc,h->group_rsc,6))return false;
 if(!wpa2_hs_claim_keys(h,h->generation,(uintptr_t)s))return false;
 s->generation=k->generation;s->peer=h->peer;s->group_id=k->group_id;
 for(unsigned i=0;i<16;i++){s->tk[i]=k->pairwise[i];s->gtk[i]=k->group[i];}
 uint64_t rsc=0;for(unsigned i=0;i<6;i++)rsc|=(uint64_t)k->group_rsc[i]<<(8*i);
 for(unsigned i=0;i<17;i++)s->rx_group[i]=rsc;
 s->phase=WPA2_CCMP_INSTALLED;return true;
}
bool wpa2_ccmp_activate(struct wpa2_ccmp *s,const struct wpa2_handshake *h){
 if(!s || !h || overlap(s,sizeof *s,h,sizeof *h) || s->phase!=WPA2_CCMP_INSTALLED || h->key_owner!=(uintptr_t)s || !binding(s,h) || h->phase!=WPA2_HS_COMPLETE || h->install_handoffs!=1 || h->emissions!=2 || !h->keys_may_live || h->unsafe)return false;
 s->phase=WPA2_CCMP_ACTIVE;return true;
}
static void nonce(uint8_t out[13],const uint8_t *h,unsigned tid,uint64_t pn){
 out[0]=(uint8_t)tid;for(unsigned i=0;i<6;i++){out[1+i]=h[10+i];out[7+i]=(uint8_t)(pn>>(40-8*i));}
}
static void ctr(const uint8_t rk[176],const uint8_t nn[13],const uint8_t *in,size_t n,uint8_t *out){
 uint8_t b[16];unsigned counter=1;
 for(size_t pos=0;pos<n;pos+=16,counter++){
  b[0]=1;for(unsigned i=0;i<13;i++)b[i+1]=nn[i];b[14]=(uint8_t)(counter>>8);b[15]=(uint8_t)counter;wpa2_aes128_encrypt(rk,b);
  size_t take=n-pos;if(take>16)take=16;for(size_t i=0;i<take;i++)out[pos+i]=in[pos+i]^b[i];
 }scrub(b,sizeof b);
}
static void mac_block(const uint8_t rk[176],uint8_t mac[16],const uint8_t block[16]){for(unsigned i=0;i<16;i++)mac[i]^=block[i];wpa2_aes128_encrypt(rk,mac);}
static void tag(const uint8_t rk[176],const uint8_t nn[13],const uint8_t *h,bool qos,const uint8_t *plain,size_t n,uint8_t out[8]){
 uint8_t mac[16]={0x59},aad[32]={0},block[16]={0};
 for(unsigned i=0;i<13;i++)mac[i+1]=nn[i];
 mac[14]=(uint8_t)(n>>8);mac[15]=(uint8_t)n;wpa2_aes128_encrypt(rk,mac);
 aad[1]=(uint8_t)(qos?24:22);aad[2]=h[0]&0x8f;aad[3]=(h[1]&0xc7)|0x40;
 for(unsigned i=0;i<18;i++)aad[4+i]=h[4+i];
 aad[22]=h[22]&15;if(qos)aad[24]=h[24]&15;
 mac_block(rk,mac,aad);mac_block(rk,mac,aad+16);
 for(size_t pos=0;pos<n;pos+=16){
  scrub(block,16);size_t take=n-pos;if(take>16)take=16;for(size_t i=0;i<take;i++)block[i]=plain[pos+i];mac_block(rk,mac,block);
 }
 block[0]=1;for(unsigned i=0;i<13;i++)block[i+1]=nn[i];block[14]=block[15]=0;wpa2_aes128_encrypt(rk,block);
 for(unsigned i=0;i<8;i++)out[i]=mac[i]^block[i];
 scrub(mac,sizeof mac);scrub(aad,sizeof aad);scrub(block,sizeof block);
}
size_t wpa2_ccmp_seal(struct wpa2_ccmp *s,uint64_t g,const uint8_t h[24],const uint8_t *plain,size_t n,uint8_t *out,size_t cap){
 if(!s || !g || g!=s->generation || !out || !cap || cap>4096 || overlap(s,sizeof *s,out,cap) || (h && (overlap(s,sizeof *s,h,24) || overlap(out,cap,h,24))) || (plain && (overlap(s,sizeof *s,plain,n) || overlap(out,cap,plain,n))))return 0;
 scrub(out,cap);
 if(s->phase!=WPA2_CCMP_ACTIVE || !h || !plain || !n || n>WPA2_CCMP_DATA_MAX || cap<24+8+n+8 || le16(h)!=0x0108 || (le16(h+22)&15) ||
 !equal(h+4,s->peer.aa,6) || !equal(h+10,s->peer.spa,6) || zero(h+16,6) || s->tx_pn>=WPA2_CCMP_PN_MAX)return 0;
 uint64_t pn=++s->tx_pn;for(unsigned i=0;i<24;i++)out[i]=h[i];out[1]|=0x40;
 uint8_t *ccmp=out+24;ccmp[0]=(uint8_t)pn;ccmp[1]=(uint8_t)(pn>>8);ccmp[3]=0x20;for(unsigned i=2;i<6;i++)ccmp[2+i]=(uint8_t)(pn>>(8*i));
 uint8_t rk[176],nn[13],mic[8];wpa2_aes128_expand(s->tk,rk);nonce(nn,out,0,pn);tag(rk,nn,out,false,plain,n,mic);ctr(rk,nn,plain,n,out+32);
 for(unsigned i=0;i<8;i++)out[32+n+i]=mic[i];
 scrub(rk,sizeof rk);scrub(nn,sizeof nn);scrub(mic,sizeof mic);return 40+n;
}
static size_t open_packet(struct wpa2_ccmp *s,uint64_t g,const uint8_t *p,size_t size,uint8_t *out,size_t cap,bool publish){
 if(!s || !g || g!=s->generation || !cap || cap>WPA2_CCMP_DATA_MAX || (publish && (!out || overlap(s,sizeof *s,out,cap))) || (p && (overlap(s,sizeof *s,p,size) || (publish && overlap(out,cap,p,size)))))return 0;
 if(publish)scrub(out,cap);
 if(s->phase!=WPA2_CCMP_ACTIVE || !p || size<24+8+1+8 || size>26+8+WPA2_CCMP_DATA_MAX+8)return 0;
 unsigned fc=le16(p),base=fc&~0x3800u;bool qos=base==0x4288;if(base!=0x4208 && !qos)return 0;
 unsigned header=qos?26:24;if(size<header+17 || (le16(p+22)&15) || !equal(p+10,s->peer.aa,6) || !unicast(p+16))return 0;
 bool group=!!(p[4]&1);if(!group && !equal(p+4,s->peer.spa,6))return 0;
 unsigned qc=qos?le16(p+24):0;if(qc&~0x17u)return 0;
 unsigned tid=qos?(qc&7):0,index=qos?tid:16;const uint8_t *c=p+header;
 if(c[2] || (c[3]&0x3f)!=0x20 || (c[3]>>6)!=(group?s->group_id:0))return 0;
 uint64_t pn=c[0]|(uint64_t)c[1]<<8;for(unsigned i=2;i<6;i++)pn|=(uint64_t)c[2+i]<<(8*i);
 uint64_t *last=group?&s->rx_group[index]:&s->rx_pairwise[index];size_t n=size-header-16;
 if(!pn || pn<=*last || n>WPA2_CCMP_DATA_MAX || n>cap)return 0;
 uint8_t rk[176],nn[13],mic[8];wpa2_aes128_expand(group?s->gtk:s->tk,rk);nonce(nn,p,tid,pn);ctr(rk,nn,c+8,n,s->scratch);tag(rk,nn,p,qos,s->scratch,n,mic);
 bool ok=equal(mic,c+8+n,8);if(ok && publish){*last=pn;for(size_t i=0;i<n;i++)out[i]=s->scratch[i];}
 scrub(s->scratch,sizeof s->scratch);scrub(rk,sizeof rk);scrub(nn,sizeof nn);scrub(mic,sizeof mic);return ok?n:0;
}
size_t wpa2_ccmp_open(struct wpa2_ccmp *s,uint64_t g,const uint8_t *p,size_t n,uint8_t *out,size_t cap){return open_packet(s,g,p,n,out,cap,true);}
bool wpa2_ccmp_verify(struct wpa2_ccmp *s,uint64_t g,const uint8_t *p,size_t n){
 if(!s || !zero(s->scratch,sizeof s->scratch))return false;
 return open_packet(s,g,p,n,0,WPA2_CCMP_DATA_MAX,false)!=0;
}
bool wpa2_ccmp_retire(struct wpa2_ccmp *s,uint64_t g){
 if(!s || !g || s->generation!=g || (s->phase!=WPA2_CCMP_INSTALLED && s->phase!=WPA2_CCMP_ACTIVE))return false;
 scrub(s->tk,sizeof s->tk);scrub(s->gtk,sizeof s->gtk);scrub(s->scratch,sizeof s->scratch);s->phase=WPA2_CCMP_CLOSED;return true;
}
bool wpa2_ccmp_erased(const struct wpa2_ccmp *s,uint64_t g){return s && g && s->generation==g && s->phase==WPA2_CCMP_CLOSED && zero(s->tk,16) && zero(s->gtk,16) && zero(s->scratch,sizeof s->scratch);}
