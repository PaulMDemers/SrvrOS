#include <srvros/wpa2_keywrap.h>
#include <stdbool.h>
#include <srvros/wpa2_aes128.h>
static void erase(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static bool overlap(const void *a,size_t an,const void *b,size_t bn){
 if(!an || !bn)return false;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;
}
size_t wpa2_key_unwrap(const uint8_t kek[16],const uint8_t *cipher,size_t size,uint8_t *out,size_t cap){
 if(!out || !cap || cap>1016 || (kek && overlap(out,cap,kek,16)) || (cipher && overlap(out,cap,cipher,size)))return 0;
 erase(out,cap);
 if(!kek || !cipher || size<24 || size>1024 || (size&7) || cap<size-8)return 0;
 uint8_t rk[176],a[8],block[16];wpa2_aes128_expand(kek,rk);
 for(unsigned i=0;i<8;i++)a[i]=cipher[i];
 for(size_t i=8;i<size;i++)out[i-8]=cipher[i];
 unsigned n=(unsigned)(size/8-1);
 for(unsigned j=6;j-->0;)for(unsigned i=n;i>0;i--){
  uint64_t counter=(uint64_t)n*j+i;
  for(unsigned k=0;k<8;k++){block[k]=a[k]^(uint8_t)(counter>>(56-8*k));block[8+k]=out[8*(i-1)+k];}
  wpa2_aes128_decrypt(rk,block);for(unsigned k=0;k<8;k++){a[k]=block[k];out[8*(i-1)+k]=block[8+k];}
 }
 unsigned diff=0;for(unsigned i=0;i<8;i++)diff|=a[i]^0xa6;
 erase(rk,sizeof rk);erase(a,sizeof a);erase(block,sizeof block);
 if(diff){erase(out,cap);return 0;}return size-8;
}
