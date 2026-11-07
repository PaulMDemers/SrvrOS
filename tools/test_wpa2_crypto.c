#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/wpa2_crypto.c"
static void zero(const void *p,size_t n){const uint8_t *v=p;for(size_t i=0;i<n;i++)assert(!v[i]);}
static void hex(const uint8_t *p,const char *s,size_t n){
 static const char h[]="0123456789abcdef";assert(strlen(s)==2*n);
 for(size_t i=0;i<n;i++){assert(h[p[i]>>4]==s[2*i]);assert(h[p[i]&15]==s[2*i+1]);}
}
static void hash(const uint8_t *p,size_t n,const char *want){
 uint8_t out[20];struct sha1 s;sha1_init(&s);sha1_add(&s,p,n);sha1_end(&s,out);hex(out,want,20);zero(&s,sizeof s);
 for(size_t chunk=1;chunk<66;chunk++){
  sha1_init(&s);for(size_t i=0;i<n;){size_t z=n-i;if(z>chunk)z=chunk;sha1_add(&s,p+i,z);i+=z;}
  sha1_end(&s,out);hex(out,want,20);zero(&s,sizeof s);
 }
}
static void mac(const uint8_t *k,size_t kn,const uint8_t *p,size_t n,const char *want){
 uint8_t out[20];struct hmac h;hmac_init(&h,k,kn);sha1_add(&h.inner,p,n);hmac_end(&h,out);hex(out,want,20);zero(&h,sizeof h);
}
static void frame(uint8_t *p,size_t n){memset(p,0,n);p[0]=2;p[1]=3;p[2]=(uint8_t)((n-4)>>8);p[3]=(uint8_t)(n-4);p[4]=2;p[5]=1;p[6]=10;p[7]=0;p[8]=16;p[97]=(uint8_t)((n-99)>>8);p[98]=(uint8_t)(n-99);}
int main(void){
 hash((const uint8_t *)"",0,"da39a3ee5e6b4b0d3255bfef95601890afd80709");
 hash((const uint8_t *)"abc",3,"a9993e364706816aba3e25717850c26c9cd0d89d");
 const char *longmsg="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
 hash((const uint8_t *)longmsg,strlen(longmsg),"84983e441c3bd26ebaae4aa1f95129e5e54670f1");
 uint8_t million[1000];memset(million,'a',sizeof million);struct sha1 s;sha1_init(&s);for(unsigned i=0;i<1000;i++)sha1_add(&s,million,sizeof million);uint8_t digest[20];sha1_end(&s,digest);hex(digest,"34aa973cd4c4daa4f61eeb2bdbad27316534016f",20);zero(&s,sizeof s);
 uint8_t k[80],data[50];memset(k,0x0b,20);mac(k,20,(const uint8_t *)"Hi There",8,"b617318655057264e28bc0b6fb378c8ef146be00");
 mac((const uint8_t *)"Jefe",4,(const uint8_t *)"what do ya want for nothing?",28,"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
 memset(k,0xaa,20);memset(data,0xdd,50);mac(k,20,data,50,"125d7342b9ac11cd91a39af48aa17b4f63f175d3");
 for(unsigned i=0;i<25;i++)k[i]=(uint8_t)(i+1);
 memset(data,0xcd,50);mac(k,25,data,50,"4c9007f4026250c6bc8414f9bf50c86c2d7235da");
 memset(k,0x0c,20);mac(k,20,(const uint8_t *)"Test With Truncation",20,"4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
 memset(k,0xaa,80);const char *msg="Test Using Larger Than Block-Size Key - Hash Key First";mac(k,80,(const uint8_t *)msg,strlen(msg),"aa4ae5e15272d00e95705637ce8a3b55ed402112");
 msg="Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data";mac(k,80,(const uint8_t *)msg,strlen(msg),"e8e99d0f45237d786d6bbaa7965c7808bbff1a91");
 uint8_t out[48],pass[64],ssid[33];memset(pass,'x',sizeof pass);memset(ssid,'s',sizeof ssid);
 assert(wpa2_psk_pmk((const uint8_t *)"password",8,(const uint8_t *)"IEEE",4,out));hex(out,"f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e",32);
 assert(wpa2_psk_pmk((const uint8_t *)"ThisIsAPassword",15,(const uint8_t *)"ThisIsASSID",11,out));hex(out,"0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af",32);
 for(unsigned n=0;n<=64;n++)if(n<8 || n>63){memset(out,0xa5,32);assert(!wpa2_psk_pmk(pass,n,ssid,1,out));zero(out,32);}
 for(unsigned n=0;n<=33;n++)if(!n || n>32){memset(out,0xa5,32);assert(!wpa2_psk_pmk(pass,8,ssid,n,out));zero(out,32);}
 for(unsigned i=0;i<8;i++)for(unsigned c=0;c<256;c++)if(c<32 || c>126){pass[i]=(uint8_t)c;memset(out,0xa5,32);assert(!wpa2_psk_pmk(pass,8,ssid,1,out));zero(out,32);pass[i]='x';}
 assert(!wpa2_psk_pmk(NULL,8,ssid,1,out));zero(out,32);assert(!wpa2_psk_pmk(pass,8,NULL,1,out));zero(out,32);assert(!wpa2_psk_pmk(pass,8,ssid,1,NULL));
 uint8_t arena[256],saved[256];memset(arena,'x',256);memcpy(saved,arena,256);
 for(unsigned i=1;i<40;i++){assert(!wpa2_psk_pmk(arena+32,8,ssid,1,arena+i));assert(!memcmp(arena,saved,256));}
 for(unsigned i=1;i<40;i++){assert(!wpa2_psk_pmk(pass,8,arena+32,8,arena+i));assert(!memcmp(arena,saved,256));}
 uint8_t pmk[32],aa[6]={2,1,2,3,4,5},spa[6]={2,6,7,8,9,10},an[32],sn[32],ptk[48];
 for(unsigned i=0;i<32;i++){pmk[i]=(uint8_t)i;an[i]=(uint8_t)(i+1);sn[i]=(uint8_t)(i+40);}
 assert(wpa2_ccmp_ptk(pmk,aa,spa,an,sn,ptk));assert(wpa2_ccmp_ptk(pmk,spa,aa,sn,an,out));assert(!memcmp(ptk,out,48));
 assert(!wpa2_ccmp_ptk(NULL,aa,spa,an,sn,out));zero(out,48);
 assert(!wpa2_ccmp_ptk(pmk,NULL,spa,an,sn,out));zero(out,48);
 assert(!wpa2_ccmp_ptk(pmk,aa,NULL,an,sn,out));zero(out,48);
 assert(!wpa2_ccmp_ptk(pmk,aa,spa,NULL,sn,out));zero(out,48);
 assert(!wpa2_ccmp_ptk(pmk,aa,spa,an,NULL,out));zero(out,48);
 assert(!wpa2_ccmp_ptk(pmk,aa,spa,an,sn,NULL));
 for(unsigned first=1;first<256;first+=2){aa[0]=(uint8_t)first;assert(!wpa2_ccmp_ptk(pmk,aa,spa,an,sn,out));zero(out,48);}aa[0]=2;
 assert(!wpa2_ccmp_ptk(pmk,aa,aa,an,sn,out));zero(out,48);assert(!wpa2_ccmp_ptk(pmk,aa,spa,an,an,out));zero(out,48);
 for(unsigned fill=0;fill<2;fill++){memset(sn,fill?255:0,32);assert(!wpa2_ccmp_ptk(pmk,aa,spa,an,sn,out));zero(out,48);assert(!wpa2_ccmp_ptk(pmk,aa,spa,sn,an,out));zero(out,48);}
 memset(sn,40,32);uint8_t *inputs[]={pmk,aa,spa,an,sn};size_t sizes[]={32,6,6,32,32};
 for(unsigned j=0;j<5;j++)for(unsigned shift=1;shift<48+sizes[j];shift++){
  memset(arena,0xa5,256);memcpy(arena+64,inputs[j],sizes[j]);memcpy(saved,arena,256);
  const uint8_t *p[5]={pmk,aa,spa,an,sn};p[j]=arena+64;
  assert(!wpa2_ccmp_ptk(p[0],p[1],p[2],p[3],p[4],arena+64+shift-48));assert(!memcmp(arena,saved,256));
 }
 uint8_t packet[1124],original[1124],mic[16];frame(packet,1123);memcpy(original,packet,1123);
 assert(wpa2_eapol_mic(ptk,packet,1123,mic));assert(!memcmp(packet,original,1123));memcpy(packet+81,mic,16);assert(wpa2_eapol_mic_verify(ptk,packet,1123));
 for(unsigned i=0;i<1123;i++){packet[i]^=1;assert(!wpa2_eapol_mic_verify(ptk,packet,1123));packet[i]^=1;}
 for(unsigned n=0;n<1123;n++){uint8_t *small=malloc(n?n:1);assert(small);memcpy(small,packet,n);memset(mic,0xa5,16);assert(!wpa2_eapol_mic(ptk,small,n,mic));zero(mic,16);free(small);}
 frame(packet,1124);assert(!wpa2_eapol_mic(ptk,packet,1124,mic));zero(mic,16);
 frame(packet,99);for(unsigned i=0;i<99;i++){memcpy(original,packet,99);assert(!wpa2_eapol_mic(ptk,packet,99,packet+i));assert(!memcmp(original,packet,99));}
 memset(arena,0xa5,sizeof arena);memcpy(saved,arena,sizeof arena);assert(!wpa2_eapol_mic(arena+16,packet,99,arena+1));assert(!memcmp(saved,arena,sizeof arena));
 assert(!wpa2_eapol_mic(NULL,packet,99,mic));zero(mic,16);assert(!wpa2_eapol_mic(ptk,NULL,99,mic));zero(mic,16);assert(!wpa2_eapol_mic(ptk,packet,99,NULL));
 assert(!wpa2_eapol_mic_verify(ptk,NULL,99));assert(!wpa2_eapol_mic_verify(NULL,packet,99));
 for(unsigned version=0;version<256;version++){packet[0]=(uint8_t)version;bool ok=version==1 || version==2;assert(wpa2_eapol_mic(ptk,packet,99,mic)==ok);if(!ok)zero(mic,16);}packet[0]=2;
 for(unsigned v=0;v<256;v++){packet[4]=(uint8_t)v;assert(wpa2_eapol_mic(ptk,packet,99,mic)==(v==2));}packet[4]=2;
 for(unsigned v=0;v<8;v++){packet[6]=(uint8_t)(8|v);assert(wpa2_eapol_mic(ptk,packet,99,mic)==(v==2));}packet[6]=10;packet[5]=0;assert(!wpa2_eapol_mic(ptk,packet,99,mic));zero(mic,16);
 puts("wpa2-crypto: PASS SHA1/RFC2202,PMK vectors,PTK ordering,all aliases/refusals,exact-size truncated EAPOL allocations,full-frame MIC mutation and scrub;no native key or controlled-port authority");return 0;
}
