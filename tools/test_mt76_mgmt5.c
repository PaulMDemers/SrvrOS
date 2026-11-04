#include <srvros/mt76_mgmt5.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint8_t ap[6]={2,0,0,0,0,17},sta[6]={2,0,0,0,0,34};
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static struct mt76_mgmt5_peer peer(void){
 struct mt76_mgmt5_peer p={.ssid_size=6,.channel=161,.basic_rates=0x15};
 memcpy(p.station,sta,6);memcpy(p.bssid,ap,6);memcpy(p.ssid,"target",6);
 p.rsn_size=(uint16_t)mt76_rsn_station_ie(p.rsn,sizeof p.rsn);p.rsn[20]=12;return p;
}
static size_t reply(uint8_t *p,bool assoc){
 memset(p,0,128);put16(p,assoc?0x10:0xb0);memcpy(p+4,sta,6);memcpy(p+10,ap,6);memcpy(p+16,ap,6);put16(p+22,0xfff0);
 if(!assoc){put16(p+26,2);return 30;}
 put16(p+24,0x411);put16(p+28,0xc001);p[30]=1;p[31]=8;
 const uint8_t rates[]={0x8c,18,0x98,36,0xb0,72,96,108};memcpy(p+32,rates,8);return 40;
}
static enum mt76_mgmt5_status parse(const struct mt76_mgmt5_peer *p,enum mt76_mgmt5_kind kind,const uint8_t *b,size_t n){
 struct mt76_mgmt5_response out;memset(&out,0xa5,sizeof out);
 enum mt76_mgmt5_status s=mt76_mgmt5_response(p,kind,b,n,&out);
 if(s==MT76_MGMT5_INVALID)for(unsigned i=0;i<sizeof out;i++)assert(!((uint8_t *)&out)[i]);
 if(s==MT76_MGMT5_REJECTED)assert(out.status && !out.aid);
 if(s==MT76_MGMT5_OK)assert(!out.status && (kind==MT76_MGMT5_AUTH?!out.aid:(out.aid>=1 && out.aid<=2007)));
 return s;
}
int main(void){
 struct mt76_mgmt5_peer p=peer(),saved=p;uint8_t b[128],r[128],copy[128];
 static const uint8_t expected[30]={0xb0,0,60,0,2,0,0,0,0,17,2,0,0,0,0,34,2,0,0,0,0,17,0x30,0x12,0,0,1,0,0,0};
 assert(mt76_mgmt5_request(&p,MT76_MGMT5_AUTH,0x123,false,b,sizeof b)==30 && !memcmp(b,expected,30));
 assert(!memcmp(&p,&saved,sizeof p));
 for(unsigned kind=0;kind<3;kind++){
  size_t n=mt76_mgmt5_request(&p,kind,4095,true,b,sizeof b);assert(n==(kind==0?30:kind==1?68:26));
  assert(b[1]==8 && b[22]==0xf0 && b[23]==0xff && !memcmp(b+4,ap,6) && !memcmp(b+10,sta,6) && !memcmp(b+16,ap,6));
  for(size_t cap=0;cap<n;cap++){memset(r,0xa5,sizeof r);assert(!mt76_mgmt5_request(&p,kind,0,false,r,cap));for(unsigned i=0;i<sizeof r;i++)assert(r[i]==0xa5);}
  if(kind==1){assert(b[24]==0x11 && b[25]==4 && b[26]==1 && !b[27] && !memcmp(b+30,"target",6));struct mt76_rsn_profile sec;assert(mt76_rsn_psk_ccmp(b+n-22,22,&sec)==MT76_RSN_OK && !sec.capabilities);}
  if(kind==2)assert(b[24]==3 && !b[25]);
 }
 for(unsigned n=1;n<=32;n++){p=peer();p.ssid_size=(uint8_t)n;memset(p.ssid,0xff,32);assert(mt76_mgmt5_request(&p,MT76_MGMT5_ASSOC,0,false,b,sizeof b)==62+n);}
 p=peer();
 for(unsigned kind=0;kind<3;kind++){
  size_t mpdu=mt76_mgmt5_request(&p,kind,0x123,false,r,sizeof r),wire=mt76_mgmt5_usb_request(&p,kind,0x123,false,b,sizeof b);
  assert(wire==(kind==0?60:kind==1?96:56) && wire==(((20+mpdu+3)&~(size_t)3)+8));
  assert(b[0]==wire-8 && !b[1] && b[2]==8 && b[3]==5);
  const uint8_t txwi[20]={0,0,0,0x20,1,255,0,0,0,0,0,0,0,0,0,0,0,0,0,128};
  uint8_t want[20];memcpy(want,txwi,20);want[6]=(uint8_t)mpdu;
  assert(!memcmp(b+4,want,20) && !memcmp(b+24,r,mpdu));
  for(size_t i=24+mpdu;i<wire;i++)assert(!b[i]);
  for(size_t cap=0;cap<wire;cap++){memset(r,0xa5,sizeof r);assert(!mt76_mgmt5_usb_request(&p,kind,0,false,r,cap));for(unsigned i=0;i<sizeof r;i++)assert(r[i]==0xa5);}
 }
 saved=p;assert(!mt76_mgmt5_usb_request(&p,0,0,false,(void *)&p,sizeof p) && !memcmp(&p,&saved,sizeof p));
 for(unsigned kind=0;kind<2;kind++){
  size_t n=reply(b,kind);assert(parse(&p,kind,b,n)==MT76_MGMT5_OK);
  for(size_t k=0;k<n;k++){uint8_t *q=malloc(k?k:1);assert(q);memcpy(q,b,k);assert(parse(&p,kind,q,k)==MT76_MGMT5_INVALID);free(q);}
  for(unsigned bit=0;bit<16;bit++){memcpy(r,b,n);r[bit/8]^=(uint8_t)(1u<<(bit%8));assert(parse(&p,kind,r,n)==(bit==11?MT76_MGMT5_OK:MT76_MGMT5_INVALID));}
  for(unsigned i=4;i<22;i++)for(unsigned bit=0;bit<8;bit++){memcpy(r,b,n);r[i]^=(uint8_t)(1u<<bit);assert(parse(&p,kind,r,n)==MT76_MGMT5_INVALID);}
  for(unsigned frag=1;frag<16;frag++){memcpy(r,b,n);r[22]|=(uint8_t)frag;assert(parse(&p,kind,r,n)==MT76_MGMT5_INVALID);}
  memcpy(r,b,n);put16(r+(kind?26:28),17);assert(parse(&p,kind,r,30)==MT76_MGMT5_REJECTED);
 }
 reply(b,false);b[24]=1;assert(parse(&p,MT76_MGMT5_AUTH,b,30)==MT76_MGMT5_INVALID);reply(b,false);b[26]=1;assert(parse(&p,MT76_MGMT5_AUTH,b,30)==MT76_MGMT5_INVALID);
 const unsigned aids[]={0,1,0xc000,0xc7d8,0xffff};
 for(unsigned i=0;i<sizeof aids/sizeof aids[0];i++){reply(b,true);put16(b+28,aids[i]);assert(parse(&p,MT76_MGMT5_ASSOC,b,40)==MT76_MGMT5_INVALID);}
 reply(b,true);put16(b+28,0xc7d7);assert(parse(&p,MT76_MGMT5_ASSOC,b,40)==MT76_MGMT5_OK);
 for(unsigned bit=0;bit<3;bit++){reply(b,true);if(bit==0)b[24]&=~1u;if(bit==1)b[24]&=~16u;if(bit==2)b[24]|=2;assert(parse(&p,MT76_MGMT5_ASSOC,b,40)==MT76_MGMT5_INVALID);}
 reply(b,true);b[40]=50;b[41]=1;b[42]=12;assert(parse(&p,MT76_MGMT5_ASSOC,b,43)==MT76_MGMT5_INVALID);
 reply(b,true);b[40]=50;b[41]=1;b[42]=0x82;assert(parse(&p,MT76_MGMT5_ASSOC,b,43)==MT76_MGMT5_INVALID);
 b[42]=2;assert(parse(&p,MT76_MGMT5_ASSOC,b,43)==MT76_MGMT5_OK);
 reply(b,true);size_t rsn=mt76_rsn_station_ie(b+40,88);assert(rsn==22 && parse(&p,MT76_MGMT5_ASSOC,b,62)==MT76_MGMT5_OK);b[60]=64;assert(parse(&p,MT76_MGMT5_ASSOC,b,62)==MT76_MGMT5_INVALID);
 for(unsigned fault=0;fault<7;fault++){
  p=peer();if(fault==0)p.channel=1;if(fault==1)p.station[0]=1;if(fault==2)memcpy(p.station,p.bssid,6);if(fault==3)p.ssid_size=33;if(fault==4)p.basic_rates=0;if(fault==5)p.rsn_size=258;if(fault==6)p.rsn[19]=6;
  memset(b,0xa5,sizeof b);assert(!mt76_mgmt5_request(&p,MT76_MGMT5_ASSOC,0,false,b,sizeof b));for(unsigned i=0;i<sizeof b;i++)assert(b[i]==0xa5);
  reply(r,false);assert(parse(&p,MT76_MGMT5_AUTH,r,30)==MT76_MGMT5_INVALID);
 }
 p=peer();assert(!mt76_mgmt5_request(&p,MT76_MGMT5_AUTH,4096,false,b,sizeof b));assert(!mt76_mgmt5_request(&p,3,0,false,b,sizeof b));assert(!mt76_mgmt5_request(&p,0,0,false,NULL,0));
 saved=p;assert(!mt76_mgmt5_request(&p,0,0,false,(void *)&p,sizeof p) && !memcmp(&p,&saved,sizeof p));
 reply(b,false);memcpy(copy,b,sizeof b);assert(mt76_mgmt5_response(&p,0,b,30,(void *)b)==MT76_MGMT5_INVALID && !memcmp(copy,b,sizeof b));
 assert(mt76_mgmt5_response(&p,0,b,30,(void *)&p)==MT76_MGMT5_INVALID && !memcmp(&p,&saved,sizeof p));
 uint32_t seed=0x161;
 for(unsigned k=0;k<100000;k++){seed=seed*1664525u+1013904223u;size_t n=seed%257;uint8_t *q=malloc(n?n:1);assert(q);for(size_t i=0;i<n;i++){seed=seed*1664525u+1013904223u;q[i]=(uint8_t)(seed>>24);}parse(&p,k&1,q,n);free(q);}
 puts("mt76-mgmt5: PASS explicit auth/association/leave MPDUs and USB/TXWI framing,SSID bounds/canonical RSNE,short-capacity/alias refusal,exact peer/subtype/transaction/fragment/status/AID/rate/IE validation,all header/address mutations and100k hostile inputs;no TX/live association/WPA2 authority");return 0;
}
