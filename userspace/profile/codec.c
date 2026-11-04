/* Bounded SHA-256 (FIPS 180-4) and canonical RFC 4648 base64. No platform crypto
 * dependency; callers limit decoded content to 1 MiB and JSON to 12 MiB. */
#include "codec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static uint32_t ror(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
static void block(uint32_t h[8],const unsigned char p[64]){
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64],a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
    for(unsigned i=0;i<16;i++)w[i]=((uint32_t)p[4*i]<<24)|((uint32_t)p[4*i+1]<<16)|((uint32_t)p[4*i+2]<<8)|p[4*i+3];
    for(unsigned i=16;i<64;i++){uint32_t x=w[i-15],y=w[i-2];w[i]=w[i-16]+(ror(x,7)^ror(x,18)^(x>>3))+w[i-7]+(ror(y,17)^ror(y,19)^(y>>10));}
    for(unsigned i=0;i<64;i++){uint32_t t=z+(ror(e,6)^ror(e,11)^ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];uint32_t u=(ror(a,2)^ror(a,13)^ror(a,22))+((a&b)^(a&c)^(b&c));z=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
void profile_sha256(const void *data,size_t size,char hex[65]){
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const unsigned char *p=data;size_t n=size;unsigned char tail[128]={0};
    while(n>=64){block(h,p);p+=64;n-=64;}memcpy(tail,p,n);tail[n]=0x80;
    size_t padded=n<56?64:128;uint64_t bits=(uint64_t)size*8;
    for(unsigned i=0;i<8;i++)tail[padded-1-i]=(unsigned char)(bits>>(i*8));
    block(h,tail);if(padded==128)block(h,tail+64);
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<32;i++){unsigned v=(h[i/4]>>(24-8*(i%4)))&255;hex[2*i]=digits[v>>4];hex[2*i+1]=digits[v&15];}hex[64]=0;
}
static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *profile_base64_encode(const unsigned char *data,size_t size){
    size_t len=4*((size+2)/3);char *s=malloc(len+1);if(!s)return NULL;
    size_t j=0;for(size_t i=0;i<size;i+=3){unsigned a=data[i],b=i+1<size?data[i+1]:0,c=i+2<size?data[i+2]:0;
        s[j++]=alphabet[a>>2];s[j++]=alphabet[((a&3)<<4)|(b>>4)];s[j++]=i+1<size?alphabet[((b&15)<<2)|(c>>6)]:'=';s[j++]=i+2<size?alphabet[c&63]:'=';}s[j]=0;return s;
}
unsigned char *profile_base64_decode(const char *text,size_t expected){
    size_t n=strlen(text);if(n!=4*((expected+2)/3))return NULL;
    unsigned char *out=malloc(expected+1);if(!out)return NULL;size_t j=0;
    for(size_t i=0;i<n;i+=4){unsigned v=0;for(unsigned k=0;k<4;k++){const char *p=strchr(alphabet,text[i+k]);if(text[i+k]=='='){if(i+4!=n || k<2){free(out);return NULL;}v<<=6;}else if(!p){free(out);return NULL;}else v=(v<<6)|(unsigned)(p-alphabet);}
        for(unsigned k=0;k<3&&j<expected;k++)out[j++]=(unsigned char)(v>>(16-8*k));}
    char *canonical=profile_base64_encode(out,expected);int okay=canonical&&!strcmp(canonical,text);free(canonical);if(!okay){free(out);return NULL;}return out;
}
