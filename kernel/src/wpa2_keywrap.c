#include <srvros/wpa2_keywrap.h>
#include <stdbool.h>
static void erase(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){
 if(!an || !bn)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;
}
/* GF(2^8),x^8+x^4+x^3+x+1. Masks avoid secret-dependent table indices and
 * source-level branches. Control-flow and memory addresses depend on rounds. */
static uint8_t mul(uint8_t a,uint8_t b){
 uint8_t c=0;
 for(unsigned i=0;i<8;i++){c^=a&(uint8_t)(0u-(b&1u));a=(uint8_t)((a<<1)^(0x1bu&(0u-(a>>7))));b>>=1;}
 return c;
}
static uint8_t rotate(uint8_t v,unsigned n){return (uint8_t)((v<<n)|(v>>(8-n)));}
static uint8_t inverse(uint8_t x){
 uint8_t p=mul(x,x),v=p;
 for(unsigned i=0;i<6;i++){p=mul(p,p);v=mul(v,p);}return v;
}
static uint8_t sub(uint8_t x){uint8_t y=inverse(x);return y^rotate(y,1)^rotate(y,2)^rotate(y,3)^rotate(y,4)^0x63;}
static uint8_t invsub(uint8_t x){return inverse((uint8_t)(rotate(x,1)^rotate(x,3)^rotate(x,6)^5));}
static void expand(const uint8_t key[16],uint8_t rounds[176]){
 for(unsigned i=0;i<16;i++)rounds[i]=key[i];
 uint8_t t[4],rc=1;
 for(unsigned pos=16;pos<176;pos+=4){
  for(unsigned j=0;j<4;j++)t[j]=rounds[pos-4+j];
  if(!(pos&15)){uint8_t first=t[0];t[0]=sub(t[1])^rc;t[1]=sub(t[2]);t[2]=sub(t[3]);t[3]=sub(first);rc=mul(rc,2);}
  for(unsigned j=0;j<4;j++)rounds[pos+j]=rounds[pos-16+j]^t[j];
 }
 erase(t,sizeof t);
}
static void decrypt(const uint8_t rk[176],uint8_t s[16]){
 uint8_t t[16];for(unsigned i=0;i<16;i++)s[i]^=rk[160+i];
 for(unsigned round=9;;round--){
  for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)t[4*c+r]=invsub(s[4*((c+4-r)&3)+r]);
  for(unsigned i=0;i<16;i++)s[i]=t[i]^rk[16*round+i];
  if(!round)break;
  for(unsigned c=0;c<4;c++){
   const uint8_t *p=s+4*c;uint8_t *q=t+4*c;
   q[0]=mul(p[0],14)^mul(p[1],11)^mul(p[2],13)^mul(p[3],9);
   q[1]=mul(p[0],9)^mul(p[1],14)^mul(p[2],11)^mul(p[3],13);
   q[2]=mul(p[0],13)^mul(p[1],9)^mul(p[2],14)^mul(p[3],11);
   q[3]=mul(p[0],11)^mul(p[1],13)^mul(p[2],9)^mul(p[3],14);
  }
  for(unsigned i=0;i<16;i++)s[i]=t[i];
 }
 erase(t,sizeof t);
}
size_t wpa2_key_unwrap(const uint8_t kek[16],const uint8_t *cipher,size_t size,uint8_t *out,size_t cap){
 if(!out || !cap || cap>1016 || (kek && overlap(out,cap,kek,16)) || (cipher && overlap(out,cap,cipher,size)))return 0;
 erase(out,cap);
 if(!kek || !cipher || size<24 || size>1024 || (size&7) || cap<size-8)return 0;
 uint8_t rk[176],a[8],block[16];expand(kek,rk);
 for(unsigned i=0;i<8;i++)a[i]=cipher[i];
 for(size_t i=8;i<size;i++)out[i-8]=cipher[i];
 unsigned n=(unsigned)(size/8-1);
 for(unsigned j=6;j-->0;)for(unsigned i=n;i>0;i--){
  uint64_t counter=(uint64_t)n*j+i;
  for(unsigned k=0;k<8;k++){block[k]=a[k]^(uint8_t)(counter>>(56-8*k));block[8+k]=out[8*(i-1)+k];}
  decrypt(rk,block);for(unsigned k=0;k<8;k++){a[k]=block[k];out[8*(i-1)+k]=block[8+k];}
 }
 unsigned diff=0;for(unsigned i=0;i<8;i++)diff|=a[i]^0xa6;
 erase(rk,sizeof rk);erase(a,sizeof a);erase(block,sizeof block);
 if(diff){erase(out,cap);return 0;}return size-8;
}
