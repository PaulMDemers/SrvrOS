#include <srvros/mt76_discovery5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t pages[8][4096] __attribute__((aligned(4096)));
static size_t bytes[8];
static struct mt76_rx_window window;
static struct mt76_discovery cache;
static struct mt76_discovery5_choice choice;
static unsigned posts,calls,fail_at;
static bool stopped;
static const uint8_t name[]="Demers5";
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void put32(uint8_t *p,unsigned n){put16(p,n);put16(p+2,n>>16);}
static void frame(unsigned i,unsigned id,const uint8_t *ssid,unsigned n,unsigned sec){
 uint8_t *p=pages[i];memset(p,0,4096);uint8_t *wi=p+4,*h=p+36,*b=p+60;
 put16(h,i&1?0x50:0x80);memset(h+4,255,6);h[10]=h[16]=2;h[15]=h[21]=(uint8_t)id;put16(h+22,16*i);
 put16(b+8,100);put16(b+10,1|(sec?16:0));unsigned pos=12;
 b[pos++]=0;b[pos++]=(uint8_t)n;if(n)memcpy(b+pos,ssid,n);pos+=n;
 b[pos++]=1;b[pos++]=1;b[pos++]=0x8c;
 if(sec){pos+=(unsigned)mt76_rsn_station_ie(b+pos,4096-pos-60);if(sec==2)b[pos-2]=64;}
 unsigned mpdu=24+pos,dma=(32+mpdu+3)&~3u;put16(p,dma);put32(wi+4,mpdu<<16);bytes[i]=dma+4;
}
static bool owned(void *c,bool cleanup){(void)c;return !cleanup || stopped;}
static bool post(void *c,unsigned i,uint64_t trb,uint64_t phys){(void)c;assert(i==posts++ && trb==0x1000+16*i && phys==0x2000+4096*i);return true;}
static bool poll(void *c,struct mt76_rx_window_event *e,bool *present){(void)c;*present=true;*e=(struct mt76_rx_window_event){.trb=0x1000+16*(posts-1),.slot=1,.dci=9,.completion=13,.remaining=4096-(unsigned)bytes[posts-1]};return true;}
static uint64_t now(void *c){(void)c;static uint64_t t;t+=1000;return t;}
static bool retained(void *c,const struct mt76_rx_window *w){(void)c;assert(w==&window && stopped);return ++calls!=fail_at;}
static struct mt76_discovery_io io={.owned=retained};
static void finish(void){
 memset(&window,0,sizeof window);posts=calls=fail_at=0;stopped=false;cache=(struct mt76_discovery){0};choice=(struct mt76_discovery5_choice){0};
 struct mt76_rx_window_plan p={.ring=0x1000,.slot=1,.dci=9,.country={'U','S'},.channel=161};
 for(unsigned i=0;i<8;i++){p.physical[i]=0x2000+4096*i;p.buffer[i]=pages[i];}
 struct mt76_rx_window_io f={.owned=owned,.post=post,.poll=poll,.now=now};assert(mt76_rx5_window_poll(&window,&p,&f));
 stopped=true;const struct mt76_rx_window_stop stop={.dequeue=0x1000+16*posts+1,.slot=1,.dci=9,.state=3};assert(mt76_rx5_window_finish(&window,&f,&stop));
 assert(mt76_discovery5_collect(&cache,&window,&io));calls=0;
}
static void setup(void){for(unsigned i=0;i<8;i++)frame(i,1,name,7,1);finish();}
static enum mt76_discovery5_status select(void){return mt76_discovery5_choose(&choice,&cache,&window,&io,1,name,7);}
static void refuses(enum mt76_discovery5_status status){assert(select()==status);struct mt76_discovery5_choice empty={.attempted=true,.matches=choice.matches};assert(!memcmp(&empty,&choice,sizeof choice));}
int main(void){
 setup();assert(cache.count==1 && cache.parsed==8 && cache.row[0].channel==161 && cache.row[0].observations==8);assert(select()==MT76_DISC5_OK && choice.selected && choice.generation==1 && choice.rsn_size==22 && !memcmp(choice.row.ssid,name,7));
 unsigned ownership_calls=calls;struct mt76_discovery5_choice saved=choice;assert(select()==MT76_DISC5_INVALID && !memcmp(&saved,&choice,sizeof saved));
 memset(pages,0,sizeof pages);assert(!memcmp(choice.row.ssid,name,7) && choice.rsn[0]==48);
 for(unsigned n=1;n<=ownership_calls;n++){setup();fail_at=n;refuses(MT76_DISC5_OWNERSHIP);}
 setup();for(unsigned i=0;i<8;i++)frame(i,i<4?1:2,name,7,1);finish();refuses(MT76_DISC5_AMBIGUOUS);
 setup();for(unsigned i=0;i<8;i++)frame(i,1,(const uint8_t *)"other",5,1);finish();refuses(MT76_DISC5_UNAVAILABLE);
 setup();for(unsigned i=0;i<8;i++)frame(i,1,name,7,0);finish();refuses(MT76_DISC5_SECURITY);
 setup();for(unsigned i=0;i<8;i++)frame(i,1,name,7,2);finish();refuses(MT76_DISC5_SECURITY);
 setup();frame(7,1,(const uint8_t *)"conflict",8,1);finish();refuses(MT76_DISC5_CONFLICT);
 setup();frame(7,1,name,7,0);finish();refuses(MT76_DISC5_CONFLICT);
 /* Conflicting valid replay-counter capabilities on the same BSSID. */
 setup();pages[7][60+12+2+7+3+20]=12;finish();refuses(MT76_DISC5_CONFLICT);
 setup();cache.row[0].observations++;refuses(MT76_DISC5_INVALID);
 setup();cache.empty++;refuses(MT76_DISC5_INVALID);
 setup();window.item[0].actual--;refuses(MT76_DISC5_OWNERSHIP);
 setup();window.plan.channel=1;refuses(MT76_DISC5_INVALID);
 setup();window.stopped=false;refuses(MT76_DISC5_INVALID);
 setup();assert(mt76_discovery5_choose(&choice,&cache,&window,&io,0,name,7)==MT76_DISC5_INVALID);
 setup();assert(mt76_discovery5_choose(&choice,&cache,&window,&io,1,name,0)==MT76_DISC5_INVALID);
 setup();struct mt76_discovery legacy={0};assert(!mt76_discovery_collect(&legacy,&window,&io));
 setup();uint8_t copy[4096];memcpy(copy,pages[0],4096);assert(!mt76_discovery5_collect((void *)pages[0],&window,&io) && !memcmp(copy,pages[0],4096));
 assert(mt76_discovery5_choose((void *)pages[0],&cache,&window,&io,1,name,7)==MT76_DISC5_INVALID && !memcmp(copy,pages[0],4096));
 struct mt76_discovery saved_cache=cache;assert(mt76_discovery5_choose((void *)&cache,&cache,&window,&io,1,name,7)==MT76_DISC5_INVALID && !memcmp(&saved_cache,&cache,sizeof cache));
 /* A fresh selection may only follow windows with no earlier visible target.
  * Hidden observations of its BSSID must agree on the full security IE. */
 setup();assert(select()==MT76_DISC5_OK);struct mt76_discovery5_choice selected=choice;
 for(unsigned i=0;i<8;i++){frame(i,1,(const uint8_t *)"",0,1);}finish();
 assert(mt76_discovery5_history(&selected,&cache,&window,&io)==MT76_DISC5_OK);
 unsigned history_calls=calls;
 for(unsigned n=1;n<=history_calls;n++){
  for(unsigned i=0;i<8;i++){frame(i,1,(const uint8_t *)"",0,1);}finish();fail_at=n;
  assert(mt76_discovery5_history(&selected,&cache,&window,&io)==MT76_DISC5_OWNERSHIP);
 }
 for(unsigned variant=0;variant<5;variant++){
  for(unsigned i=0;i<8;i++)frame(i,1,variant==1?name:variant==2?(const uint8_t *)"other":(const uint8_t *)"",variant==1?7:variant==2?5:0,variant==0?0:1);
  if(variant==3)pages[7][60+12+2+3+20]=12;
  finish();if(variant==4)cache.row[0].observations++;
  assert(mt76_discovery5_history(&selected,&cache,&window,&io)!=MT76_DISC5_OK);
 }
 assert(mt76_discovery5_history(NULL,&cache,&window,&io)==MT76_DISC5_INVALID);
 puts("mt76-discovery5: PASS fixed161 copied aggregation,explicit unique target/RSN selection,all ownership cancellation points,ambiguity/conflict/security/tamper/generation refusal,legacy isolation and retained-page alias protection;no authentication authority");return 0;
}
