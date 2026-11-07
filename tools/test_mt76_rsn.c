#include <srvros/mt76_rsn.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint8_t good[]={48,20,1,0,0,15,172,4,1,0,0,15,172,4,1,0,0,15,172,2,12,0};
static enum mt76_rsn_status parse(const uint8_t *p,size_t n){
 struct mt76_rsn_profile r;memset(&r,0xa5,sizeof r);enum mt76_rsn_status s=mt76_rsn_psk_ccmp(p,n,&r);
 if(s!=MT76_RSN_OK)for(size_t i=0;i<sizeof r;i++)assert(!((uint8_t *)&r)[i]);
 return s;
}
int main(void){
 struct mt76_rsn_profile r;assert(mt76_rsn_psk_ccmp(good,sizeof good,&r)==MT76_RSN_OK && r.capabilities==12 && !r.pmf_capable);
 for(size_t n=0;n<sizeof good;n++){uint8_t *p=malloc(n?n:1);assert(p);memcpy(p,good,n);assert(parse(p,n)==MT76_RSN_INVALID);free(p);}
 uint8_t p[258];
 for(size_t n=2;n<22;n++){memcpy(p,good,n);p[1]=(uint8_t)(n-2);assert(parse(p,n)==(n==20?MT76_RSN_OK:MT76_RSN_INVALID));}
 memcpy(p+1,good,sizeof good);assert(parse(p+1,sizeof good)==MT76_RSN_OK);
 for(unsigned i=0;i<20;i++)for(unsigned bit=0;bit<8;bit++){
  memcpy(p,good,sizeof good);p[i]^=(uint8_t)(1u<<bit);assert(parse(p,sizeof good)!=MT76_RSN_OK);
 }
 for(unsigned cap=0;cap<=65535;cap++){
  memcpy(p,good,sizeof good);p[20]=(uint8_t)cap;p[21]=(uint8_t)(cap>>8);
  assert(parse(p,sizeof good)==((cap&~0xbdu)?MT76_RSN_UNSUPPORTED:MT76_RSN_OK));
 }
 memcpy(p,good,sizeof good);p[1]=18;assert(parse(p,20)==MT76_RSN_OK); /* Optional capabilities absent. */
 memcpy(p,good,sizeof good);p[1]=26;p[20]=128;p[22]=p[23]=0;p[24]=0;p[25]=15;p[26]=172;p[27]=6;
 assert(parse(p,28)==MT76_RSN_OK);
 p[20]=0;assert(parse(p,28)==MT76_RSN_UNSUPPORTED);p[20]=128;p[27]=4;assert(parse(p,28)==MT76_RSN_UNSUPPORTED);
 memcpy(p,good,sizeof good);p[1]=22;p[22]=p[23]=0;assert(parse(p,24)==MT76_RSN_OK);
 p[22]=1;assert(parse(p,24)==MT76_RSN_INVALID);
 p[1]=38;memset(p+24,0,16);assert(parse(p,40)==MT76_RSN_UNSUPPORTED); /* Cached PMKID not admitted. */
 for(unsigned n=0;n<22;n++){memset(p,0xa5,sizeof p);assert(!mt76_rsn_station_ie(p,n));for(unsigned i=0;i<sizeof p;i++)assert(p[i]==0xa5);}
 assert(mt76_rsn_station_ie(p,sizeof p)==22 && parse(p,22)==MT76_RSN_OK);
 assert(!mt76_rsn_station_ie(NULL,22));assert(parse(NULL,0)==MT76_RSN_INVALID);
 assert(mt76_rsn_psk_ccmp(good,sizeof good,NULL)==MT76_RSN_INVALID);
 memcpy(p,good,sizeof good);assert(mt76_rsn_psk_ccmp(p,sizeof good,(void *)p)==MT76_RSN_INVALID && !memcmp(p,good,sizeof good));
 uint32_t seed=161;
 for(unsigned i=0;i<100000;i++){
  seed=seed*1664525u+1013904223u;size_t n=seed%258;uint8_t *v=malloc(n?n:1);assert(v);
  for(size_t k=0;k<n;k++){seed=seed*1664525u+1013904223u;v[k]=(uint8_t)(seed>>24);}
  (void)parse(v,n);free(v);
 }
 puts("mt76-rsn: PASS explicit PSK/CCMP suites,all truncation/field mutations,65536 capabilities,PMF-required/PMKID/unknown refusal,unaligned/alias and100k hostile inputs;no authentication authority");return 0;
}
