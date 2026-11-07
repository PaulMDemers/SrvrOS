#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/wpa2_keywrap.c"
static void zeros(const uint8_t *p,size_t n){for(size_t i=0;i<n;i++)assert(!p[i]);}
int main(void){
 uint8_t key[16],rk[176],block[]={0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
 uint8_t plain[16];for(unsigned i=0;i<16;i++){key[i]=(uint8_t)i;plain[i]=(uint8_t)(17*i);}
 expand(key,rk);decrypt(rk,block);assert(!memcmp(block,plain,16));
 for(unsigned i=0;i<256;i++)assert(invsub(sub((uint8_t)i))==i);
 const uint8_t wrapped[]={0x1f,0xa6,0x8b,0x0a,0x81,0x12,0xb4,0x47,0xae,0xf3,0x4b,0xd8,0xfb,0x5a,0x7b,0x82,0x9d,0x3e,0x86,0x23,0x71,0xd2,0xcf,0xe5};
 uint8_t out[1016];assert(wpa2_key_unwrap(key,wrapped,24,out,sizeof out)==16);assert(!memcmp(out,plain,16));zeros(out+16,1000);
 for(unsigned i=0;i<24;i++)for(unsigned bit=0;bit<8;bit++){
  uint8_t copy[24];memcpy(copy,wrapped,24);copy[i]^=1u<<bit;memset(out,0xa5,sizeof out);assert(!wpa2_key_unwrap(key,copy,24,out,sizeof out));zeros(out,sizeof out);
 }
 for(unsigned n=0;n<24;n++){uint8_t *p=malloc(n?n:1);memcpy(p,wrapped,n);memset(out,0xa5,sizeof out);assert(!wpa2_key_unwrap(key,p,n,out,sizeof out));zeros(out,sizeof out);free(p);}
 for(unsigned n=1;n<16;n++){memset(out,0xa5,sizeof out);assert(!wpa2_key_unwrap(key,wrapped,24,out,n));zeros(out,n);assert(out[n]==0xa5);}
 assert(!wpa2_key_unwrap(NULL,wrapped,24,out,sizeof out));zeros(out,sizeof out);assert(!wpa2_key_unwrap(key,NULL,24,out,sizeof out));zeros(out,sizeof out);
 assert(!wpa2_key_unwrap(key,wrapped,24,NULL,16));
 uint8_t arena[128],saved[128];
 for(unsigned pos=1;pos<56;pos++){
  memset(arena,0xa5,128);memcpy(arena+32,wrapped,24);memcpy(saved,arena,128);assert(!wpa2_key_unwrap(key,arena+32,24,arena+pos,32));assert(!memcmp(arena,saved,128));
 }
 for(unsigned pos=1;pos<48;pos++){
  memset(arena,0xa5,128);memcpy(arena+32,key,16);memcpy(saved,arena,128);assert(!wpa2_key_unwrap(arena+32,wrapped,24,arena+pos,32));assert(!memcmp(arena,saved,128));
 }
 memset(out,0xa5,sizeof out);assert(!wpa2_key_unwrap(key,wrapped,24,out,0));assert(out[0]==0xa5);assert(!wpa2_key_unwrap(key,wrapped,24,out,1017));assert(out[0]==0xa5);
 puts("wpa2-keywrap: PASS FIPS197 AES128,RFC3394 integrity,every ciphertext bit,truncation,capacity/alias/null refusal and scrub;no installation authority");return 0;
}
