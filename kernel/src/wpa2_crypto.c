#include <srvros/wpa2_crypto.h>
/* Fixed bounded WPA2 SHA1 use only;not a general-purpose security hash API. */
static void erase(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
static bool overlaps(const void *a,size_t an,const void *b,size_t bn){
 if(!an || !bn)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;
}
static uint32_t rotl(uint32_t x,unsigned n){return (x<<n)|(x>>(32-n));}
static uint32_t be32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static unsigned be16(const uint8_t *p){return (unsigned)p[0]<<8|p[1];}
static void put32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(24-8*i));}
struct sha1 {uint32_t h[5];uint64_t bytes;uint8_t block[64];size_t used;};
static void sha1_init(struct sha1 *s){
 *s=(struct sha1){.h={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u,0xc3d2e1f0u}};
}
static void sha1_block(struct sha1 *s,const uint8_t *p){
 uint32_t w[16];for(unsigned i=0;i<16;i++)w[i]=be32(p+4*i);
 uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4];
 for(unsigned i=0;i<80;i++){
  if(i>=16)w[i&15]=rotl(w[(i-3)&15]^w[(i-8)&15]^w[(i-14)&15]^w[i&15],1);
  uint32_t f,k;
  if(i<20){f=(b&c)|(~b&d);k=0x5a827999u;}
  else if(i<40){f=b^c^d;k=0x6ed9eba1u;}
  else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8f1bbcdcu;}
  else {f=b^c^d;k=0xca62c1d6u;}
  uint32_t t=rotl(a,5)+f+e+k+w[i&15];e=d;d=c;c=rotl(b,30);b=a;a=t;
 }
 s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;
 erase(w,sizeof w);
}
static void sha1_add(struct sha1 *s,const uint8_t *p,size_t n){
 s->bytes+=n;
 while(n){size_t take=64-s->used;if(take>n)take=n;
  for(size_t i=0;i<take;i++)s->block[s->used+i]=p[i];
  p+=take;n-=take;s->used+=take;
  if(s->used==64){sha1_block(s,s->block);s->used=0;}
 }
}
static void sha1_end(struct sha1 *s,uint8_t out[20]){
 uint64_t bits=s->bytes*8;s->block[s->used++]=128;
 if(s->used>56){while(s->used<64)s->block[s->used++]=0;sha1_block(s,s->block);s->used=0;}
 while(s->used<56)s->block[s->used++]=0;
 for(unsigned i=0;i<8;i++)s->block[56+i]=(uint8_t)(bits>>(56-8*i));
 sha1_block(s,s->block);for(unsigned i=0;i<5;i++)put32(out+4*i,s->h[i]);
 erase(s,sizeof *s);
}
struct hmac {struct sha1 inner,outer;};
static void hmac_init(struct hmac *h,const uint8_t *key,size_t n){
 uint8_t pad[64]={0},digest[20]={0};
 if(n>64){struct sha1 s;sha1_init(&s);sha1_add(&s,key,n);sha1_end(&s,digest);key=digest;n=20;}
 for(size_t i=0;i<n;i++)pad[i]=key[i];
 for(unsigned i=0;i<64;i++)pad[i]^=0x36;
 sha1_init(&h->inner);sha1_add(&h->inner,pad,64);
 for(unsigned i=0;i<64;i++)pad[i]^=0x36^0x5c;
 sha1_init(&h->outer);sha1_add(&h->outer,pad,64);
 erase(pad,sizeof pad);erase(digest,sizeof digest);
}
static void hmac_end(struct hmac *h,uint8_t out[20]){
 uint8_t digest[20];sha1_end(&h->inner,digest);sha1_add(&h->outer,digest,20);sha1_end(&h->outer,out);
 erase(digest,sizeof digest);erase(h,sizeof *h);
}
bool wpa2_psk_pmk(const uint8_t *pass,size_t pn,const uint8_t *ssid,size_t sn,uint8_t out[32]){
 if(!out || (pass && overlaps(out,32,pass,pn)) || (ssid && overlaps(out,32,ssid,sn)))return false;
 erase(out,32);
 if(!pass || !ssid || pn<8 || pn>63 || !sn || sn>32)return false;
 for(size_t i=0;i<pn;i++)if(pass[i]<32 || pass[i]>126)return false;
 struct hmac base,h;hmac_init(&base,pass,pn);
 uint8_t u[20]={0},sum[20]={0},count[4]={0};
 for(unsigned block=1;block<=2;block++){
  put32(count,block);h=base;sha1_add(&h.inner,ssid,sn);sha1_add(&h.inner,count,4);hmac_end(&h,u);
  for(unsigned j=0;j<20;j++)sum[j]=u[j];
  for(unsigned round=1;round<4096;round++){
   h=base;sha1_add(&h.inner,u,20);hmac_end(&h,u);for(unsigned j=0;j<20;j++)sum[j]^=u[j];
  }
  unsigned take=block==1?20:12;for(unsigned j=0;j<take;j++)out[(block-1)*20+j]=sum[j];
 }
 erase(&base,sizeof base);erase(&h,sizeof h);erase(u,sizeof u);erase(sum,sizeof sum);erase(count,sizeof count);return true;
}
static int lexical(const uint8_t *a,const uint8_t *b,size_t n){
 for(size_t i=0;i<n;i++){if(a[i]<b[i])return -1;if(a[i]>b[i])return 1;}return 0;
}
static bool unicast(const uint8_t *a){unsigned any=0;for(unsigned i=0;i<6;i++)any|=a[i];return any && !(a[0]&1);}
static bool nonce_shape(const uint8_t *n){unsigned any=0,nonff=0;for(unsigned i=0;i<32;i++){any|=n[i];nonff|=n[i]^255;}return any && nonff;}
bool wpa2_ccmp_ptk(const uint8_t pmk[32],const uint8_t aa[6],const uint8_t spa[6],
 const uint8_t an[32],const uint8_t sn[32],uint8_t out[48]){
 if(!out || (pmk && overlaps(out,48,pmk,32)) || (aa && overlaps(out,48,aa,6)) ||
 (spa && overlaps(out,48,spa,6)) || (an && overlaps(out,48,an,32)) || (sn && overlaps(out,48,sn,32)))return false;
 erase(out,48);
 if(!pmk || !aa || !spa || !an || !sn || !unicast(aa) || !unicast(spa) || !lexical(aa,spa,6) ||
 !nonce_shape(an) || !nonce_shape(sn) || !lexical(an,sn,32))return false;
 static const uint8_t label[]="Pairwise key expansion";
 const uint8_t *a=aa,*b=spa,*c=an,*d=sn;
 if(lexical(a,b,6)>0){a=spa;b=aa;}if(lexical(c,d,32)>0){c=sn;d=an;}
 struct hmac base,h;hmac_init(&base,pmk,32);uint8_t digest[20]={0};
 for(uint8_t counter=0;counter<3;counter++){
  h=base;sha1_add(&h.inner,label,sizeof label);sha1_add(&h.inner,a,6);sha1_add(&h.inner,b,6);
  sha1_add(&h.inner,c,32);sha1_add(&h.inner,d,32);sha1_add(&h.inner,&counter,1);hmac_end(&h,digest);
  unsigned take=counter==2?8:20;for(unsigned j=0;j<take;j++)out[20*counter+j]=digest[j];
 }
 erase(&base,sizeof base);erase(&h,sizeof h);erase(digest,sizeof digest);return true;
}
static bool eapol_shape(const uint8_t *p,size_t n){
 return p && n>=99 && n<=1123 && (p[0]==1 || p[0]==2) && p[1]==3 &&
 be16(p+2)==n-4 && p[4]==2 && (be16(p+5)&7)==2 && (be16(p+5)&0x100) && be16(p+97)==n-99;
}
bool wpa2_eapol_mic(const uint8_t kck[16],const uint8_t *p,size_t n,uint8_t out[16]){
 if(!out || (kck && overlaps(out,16,kck,16)) || (p && overlaps(out,16,p,n)))return false;
 erase(out,16);if(!kck || !eapol_shape(p,n))return false;
 struct hmac h;uint8_t digest[20],zero[16]={0};hmac_init(&h,kck,16);
 sha1_add(&h.inner,p,81);sha1_add(&h.inner,zero,16);sha1_add(&h.inner,p+97,n-97);hmac_end(&h,digest);
 for(unsigned i=0;i<16;i++)out[i]=digest[i];
 erase(digest,sizeof digest);return true;
}
bool wpa2_eapol_mic_verify(const uint8_t kck[16],const uint8_t *p,size_t n){
 uint8_t digest[16]={0};if(!wpa2_eapol_mic(kck,p,n,digest))return false;
 unsigned different=0;for(unsigned i=0;i<16;i++)different|=digest[i]^p[81+i];erase(digest,sizeof digest);return different==0;
}
