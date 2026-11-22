/* Reuse the independently checked retained-window fixture and selection suite. */
#define main discovery5_suite
#include "test_mt76_discovery5.c"
#undef main
#include <srvros/mt76_peer5.h>
static struct mt76_peer5 bound;
static const uint8_t station[6]={2,9,8,7,6,5};
static void rate_frame(unsigned i,const uint8_t *r,unsigned n){
 frame(i,1,name,7,1);assert(n && n<=16);
 uint8_t *p=pages[i];unsigned pos=81,mpdu=(p[10]|(unsigned)p[11]<<8),end=36+mpdu;
 assert(p[pos]==1 && p[pos+1]==1 && p[pos+2]==0x8c);
 memmove(p+pos+2+n,p+pos+3,end-pos-3);p[pos+1]=n;memcpy(p+pos+2,r,n);
 mpdu+=n-1;unsigned dma=(32+mpdu+3)&~3u;put16(p,dma);put32(p+8,mpdu<<16);bytes[i]=dma+4;
}
static void prepare(void){
 static const uint8_t r[]={0x8c,18,0x98,36,0xb0,72,96,108};
 for(unsigned i=0;i<8;i++)rate_frame(i,r,sizeof r);
 finish();assert(select()==MT76_DISC5_OK);bound=(struct mt76_peer5){0};calls=0;
}
static enum mt76_peer5_status bind_peer(void){return mt76_peer5_bind(&bound,station,&choice,&cache,&window,&io);}
static void refuses_peer(enum mt76_peer5_status expected){assert(bind_peer()==expected);struct mt76_peer5 empty={.attempted=true};assert(!memcmp(&empty,&bound,sizeof empty));}
int main(void){
 assert(!discovery5_suite());prepare();assert(bind_peer()==MT76_PEER5_OK && bound.complete && bound.generation==1 && bound.observations==8 && bound.rates==255 && bound.peer.basic_rates==21);
 assert(!memcmp(bound.peer.station,station,6) && !memcmp(bound.peer.bssid,choice.row.bssid,6) && bound.peer.rsn_size==22);
 unsigned checks=calls;struct mt76_peer5 saved=bound;assert(bind_peer()==MT76_PEER5_INVALID && !memcmp(&saved,&bound,sizeof saved));
 uint8_t packet[124];for(unsigned i=0;i<3;i++)assert(mt76_mgmt5_usb_request(&bound.peer,i,123,false,packet,sizeof packet));
 memset(pages,0,sizeof pages);assert(!memcmp(&saved,&bound,sizeof saved));
 for(unsigned i=1;i<=checks;i++){prepare();fail_at=i;refuses_peer(MT76_PEER5_OWNERSHIP);}
 const uint8_t variants[][3]={{0x8c,0x8c,18},{0x82,0x8c,18},{12,18,24}};
 for(unsigned i=0;i<3;i++){prepare();rate_frame(7,variants[i],3);finish();assert(select()==MT76_DISC5_OK);bound=(struct mt76_peer5){0};refuses_peer(MT76_PEER5_RATES);}
 prepare();const uint8_t fewer[]={0x8c,18,24};rate_frame(7,fewer,sizeof fewer);finish();assert(select()==MT76_DISC5_OK);bound=(struct mt76_peer5){0};refuses_peer(MT76_PEER5_CONFLICT);
 prepare();choice.rsn[20]^=4;refuses_peer(MT76_PEER5_INVALID);
 prepare();choice.row.bssid[5]^=2;refuses_peer(MT76_PEER5_INVALID);
 prepare();uint8_t multicast[6]={3,1,2,3,4,5};assert(mt76_peer5_bind(&bound,multicast,&choice,&cache,&window,&io)==MT76_PEER5_INVALID);
 prepare();assert(mt76_peer5_bind(&bound,choice.row.bssid,&choice,&cache,&window,&io)==MT76_PEER5_INVALID);
 prepare();assert(mt76_peer5_bind(&bound,(uint8_t[6]){0},&choice,&cache,&window,&io)==MT76_PEER5_INVALID);
 prepare();uint8_t before[4096];memcpy(before,pages[0],4096);assert(mt76_peer5_bind((void *)pages[0],station,&choice,&cache,&window,&io)==MT76_PEER5_INVALID && !memcmp(before,pages[0],4096));
 prepare();struct mt76_discovery5_choice copy=choice;assert(mt76_peer5_bind((void *)&choice,station,&choice,&cache,&window,&io)==MT76_PEER5_INVALID && !memcmp(&copy,&choice,sizeof choice));
 puts("mt76-peer5: PASS rebuilt private target and station/rate binding,all ownership boundaries,copy/replay,conflicting/unsupported/duplicate/missing-basic rates,security/BSSID tampering,unicast and retained-input alias refusals;codec readiness only,no native TX authority");return 0;
}
