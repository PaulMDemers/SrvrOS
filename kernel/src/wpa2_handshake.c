#include <srvros/wpa2_handshake.h>
#include <srvros/wpa2_crypto.h>
#include <srvros/wpa2_keywrap.h>
#include <srvros/mt76_rsn.h>
static void scrub(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static bool overlaps(const void *a,size_t an,const void *b,size_t bn){
 if(!an || !bn)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;
}
static bool equal(const uint8_t *a,const uint8_t *b,size_t n){unsigned diff=0;for(size_t i=0;i<n;i++)diff|=a[i]^b[i];return !diff;}
static bool zero(const void *p,size_t n){const uint8_t *q=p;unsigned any=0;for(size_t i=0;i<n;i++)any|=q[i];return !any;}
static bool unicast(const uint8_t *p){return !(p[0]&1) && !zero(p,6);}
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static uint64_t be64(const uint8_t *p){uint64_t v=0;for(unsigned i=0;i<8;i++)v=(v<<8)|p[i];return v;}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(v>>(56-8*i));}
static void secrets(struct wpa2_handshake *s){
 scrub(s->pmk,sizeof s->pmk);scrub(s->snonce,sizeof s->snonce);scrub(s->anonce,sizeof s->anonce);
 scrub(s->ptk,sizeof s->ptk);scrub(s->gtk,sizeof s->gtk);scrub(s->scratch,sizeof s->scratch);
}
static bool same(const struct wpa2_handshake *s,uint64_t g){return s && g && s->generation==g && s->phase!=WPA2_HS_EMPTY && s->phase!=WPA2_HS_CLOSED;}
static bool fail(struct wpa2_handshake *s){secrets(s);s->phase=WPA2_HS_FAILED;return false;}
bool wpa2_hs_start(struct wpa2_handshake *s,const struct wpa2_hs_peer *p,const uint8_t pmk[32],const uint8_t sn[32],uint64_t g){
 if(!s || !p || !pmk || !sn || !g || overlaps(s,sizeof *s,p,sizeof *p) || overlaps(s,sizeof *s,pmk,32) || overlaps(s,sizeof *s,sn,32) || !zero(s,sizeof *s))return false;
 struct mt76_rsn_profile profile={0};uint8_t nonff=0;for(unsigned i=0;i<32;i++)nonff|=sn[i]^255;
 if(!unicast(p->aa) || !unicast(p->spa) || equal(p->aa,p->spa,6) || zero(sn,32) || !nonff ||
 p->rsne_size>sizeof p->rsne || mt76_rsn_psk_ccmp(p->rsne,p->rsne_size,&profile)!=MT76_RSN_OK)return false;
 s->peer=*p;s->generation=g;s->phase=WPA2_HS_M1;
 for(unsigned i=0;i<32;i++){s->pmk[i]=pmk[i];s->snonce[i]=sn[i];}return true;
}
static bool envelope(const uint8_t *p,size_t n){return p && n>=99 && n<=1123 && (p[0]==1 || p[0]==2) && p[1]==3 && be16(p+2)==n-4 && p[4]==2 && be16(p+7)==16 && be16(p+97)==n-99;}
static bool keydata(struct wpa2_handshake *s,size_t n){
 bool rsn=false,gtk=false;
 for(size_t pos=0;pos<n;){
  const uint8_t *p=s->scratch+pos;size_t left=n-pos;
  /* RSN padding starts with dd and the remaining bytes are zero. At most
   * seven pad bytes for this RSNE+GTK profile. Never parse beyond the bound. */
  if(p[0]==221 && (left==1 || p[1]==0))return rsn && gtk && left<=7 && zero(p+1,left-1);
  if(left<2 || p[1]>left-2)return false;
  size_t size=2+p[1];
  if(p[0]==48){
   if(rsn || size!=s->peer.rsne_size || !equal(p,s->peer.rsne,size))return false;
   rsn=true;
  }else if(p[0]==221){
   static const uint8_t gtk_type[]={0,15,172,1};
   if(gtk || size!=24 || !equal(p+2,gtk_type,4) || (p[6]&~7u) || p[7])return false;
   s->group_id=p[6]&3;for(unsigned i=0;i<16;i++)s->gtk[i]=p[8+i];gtk=true;
  }else return false;
  pos+=size;
 }
 return rsn && gtk;
}
bool wpa2_hs_receive(struct wpa2_handshake *s,uint64_t g,const uint8_t src[6],const uint8_t dst[6],const uint8_t *p,size_t n){
 if(!same(s,g) || (src && overlaps(s,sizeof *s,src,6)) || (dst && overlaps(s,sizeof *s,dst,6)) || (p && overlaps(s,sizeof *s,p,n)))return false;
 if(!src || !dst || !equal(src,s->peer.aa,6) || !equal(dst,s->peer.spa,6) || !envelope(p,n))return fail(s);
 if(s->phase==WPA2_HS_M1){
  static const uint8_t pmkid_type[]={221,20,0,15,172,4};
  if(be16(p+5)!=0x008a || be64(p+9)==UINT64_MAX || !zero(p+49,48) ||
   (n!=99 && (n!=121 || !equal(p+99,pmkid_type,sizeof pmkid_type))))return fail(s);
  if(!wpa2_ccmp_ptk(s->pmk,s->peer.aa,s->peer.spa,p+17,s->snonce,s->ptk))return fail(s);
  for(unsigned i=0;i<32;i++)s->anonce[i]=p[17+i];
  scrub(s->pmk,sizeof s->pmk);s->version=p[0];s->replay=be64(p+9);s->phase=WPA2_HS_M2_READY;return true;
 }
 if(s->phase!=WPA2_HS_M3 || p[0]!=s->version || be16(p+5)!=0x13ca || be64(p+9)<=s->replay ||
 !equal(p+17,s->anonce,32) || !zero(p+49,16) || !zero(p+71,10) || !wpa2_eapol_mic_verify(s->ptk,p,n))return fail(s);
 size_t plain=wpa2_key_unwrap(s->ptk+16,p+99,n-99,s->scratch,sizeof s->scratch);
 bool ok=plain && keydata(s,plain);scrub(s->scratch,sizeof s->scratch);if(!ok)return fail(s);
 for(unsigned i=0;i<6;i++)s->group_rsc[i]=p[65+i];
 s->replay=be64(p+9);s->phase=WPA2_HS_KEYS_READY;return true;
}
size_t wpa2_hs_emit(struct wpa2_handshake *s,uint64_t g,uint8_t *out,size_t cap){
 if(!same(s,g) || !out || !cap || cap>121 || overlaps(s,sizeof *s,out,cap))return 0;
 scrub(out,cap);bool m2=s->phase==WPA2_HS_M2_READY;size_t n=m2?121:99;
 if((!m2 && s->phase!=WPA2_HS_M4_READY) || cap<n || s->emissions!=(m2?0u:1u)){fail(s);return 0;}
 out[0]=s->version;out[1]=3;put16(out+2,(unsigned)n-4);out[4]=2;put16(out+5,m2?0x010a:0x030a);put64(out+9,s->replay);
 /* RSN replies use zero Key Length. Only M2 carries SNonce and station RSNE. */
 if(m2){
  for(unsigned i=0;i<32;i++)out[17+i]=s->snonce[i];
  put16(out+97,22);if(mt76_rsn_station_ie(out+99,22)!=22){scrub(out,cap);fail(s);return 0;}
 }
 uint8_t mic[16]={0};bool ok=wpa2_eapol_mic(s->ptk,out,n,mic);
 if(ok)for(unsigned i=0;i<16;i++)out[81+i]=mic[i];
 scrub(mic,sizeof mic);if(!ok){scrub(out,cap);fail(s);return 0;}
 s->emissions++;s->phase=m2?WPA2_HS_M2_PENDING:WPA2_HS_M4_PENDING;return n;
}
bool wpa2_hs_tx_result(struct wpa2_handshake *s,uint64_t g,enum wpa2_hs_result r){
 if(!same(s,g))return false;
 bool m2=s->phase==WPA2_HS_M2_PENDING;
 if((!m2 && s->phase!=WPA2_HS_M4_PENDING) || r!=WPA2_HS_CONFIRMED){if(r!=WPA2_HS_REFUSED)s->unsafe=true;return fail(s);}
 s->phase=m2?WPA2_HS_M3:WPA2_HS_COMPLETE;
 if(!m2)secrets(s);
 return true;
}
bool wpa2_hs_take_keys(struct wpa2_handshake *s,uint64_t g,struct wpa2_hs_keys *out){
 if(!same(s,g) || !out || overlaps(s,sizeof *s,out,sizeof *out))return false;
 scrub(out,sizeof *out);
 if(s->phase!=WPA2_HS_KEYS_READY || s->install_handoffs || s->keys_may_live)return fail(s);
 for(unsigned i=0;i<16;i++){out->pairwise[i]=s->ptk[32+i];out->group[i]=s->gtk[i];}
 for(unsigned i=0;i<6;i++)out->group_rsc[i]=s->group_rsc[i];
 out->group_id=s->group_id;out->generation=g;s->install_handoffs++;s->keys_may_live=true;s->phase=WPA2_HS_INSTALL_PENDING;return true;
}
bool wpa2_hs_claim_keys(struct wpa2_handshake *s,uint64_t g,uintptr_t owner){
 if(!same(s,g) || !owner || s->phase!=WPA2_HS_INSTALL_PENDING || s->install_handoffs!=1 || !s->keys_may_live || s->key_owner)return false;
 s->key_owner=owner;return true;
}
bool wpa2_hs_install_result(struct wpa2_handshake *s,uint64_t g,enum wpa2_hs_result r){
 if(!same(s,g))return false;
 if(s->phase!=WPA2_HS_INSTALL_PENDING || s->install_handoffs!=1 || !s->keys_may_live || !s->key_owner || r!=WPA2_HS_CONFIRMED){if(r!=WPA2_HS_REFUSED)s->unsafe=true;return fail(s);}
 scrub(s->ptk+16,32);scrub(s->gtk,sizeof s->gtk);scrub(s->anonce,sizeof s->anonce);scrub(s->snonce,sizeof s->snonce);
 s->phase=WPA2_HS_M4_READY;return true;
}
bool wpa2_hs_abort(struct wpa2_handshake *s,uint64_t g){
 if(!same(s,g) || s->phase==WPA2_HS_FAILED)return false;
 fail(s);return true;
}
bool wpa2_hs_retire(struct wpa2_handshake *s,uint64_t g,bool removed){
 if(!same(s,g))return false;
 secrets(s);if(s->keys_may_live && !removed){s->phase=WPA2_HS_FAILED;return false;}
 s->keys_may_live=false;s->phase=WPA2_HS_CLOSED;return true;
}
