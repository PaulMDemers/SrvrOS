#include <srvros/mt76_scan.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t pages[3][4096] __attribute__((aligned(4096)));
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void put32(uint8_t *p,unsigned n){put16(p,n);put16(p+2,n>>16);}
static size_t frame(unsigned i,unsigned id,const uint8_t *name,unsigned n,
                    bool probe,unsigned channel,unsigned security) {
    uint8_t *p=pages[i];memset(p,0,4096);
    uint8_t *wi=p+4,*h=p+36,*b=p+60;
    put16(h,probe?0x50:0x80);memset(h+4,255,6);
    h[10]=h[16]=2;h[15]=h[21]=(uint8_t)id;put16(h+22,16*i);
    put16(b+8,100);put16(b+10,1|(security?16:0));
    unsigned pos=12;b[pos++]=0;b[pos++]=(uint8_t)n;
    if(n)memcpy(b+pos,name,n);
    pos+=n;b[pos++]=1;b[pos++]=1;b[pos++]=0x82;
    if(channel){b[pos++]=3;b[pos++]=1;b[pos++]=(uint8_t)channel;}
    if(security&2){b[pos++]=48;b[pos++]=2;b[pos++]=1;b[pos++]=0;}
    if(security&4){b[pos++]=221;b[pos++]=4;b[pos++]=0;b[pos++]=0x50;b[pos++]=0xf2;b[pos++]=1;}
    unsigned mpdu=24+pos,dma=(32+mpdu+3)&~3u;
    put16(p,dma);put32(wi+4,mpdu<<16);return dma+4;
}

static struct mt76_scan_input input[3];
static unsigned calls,deny,cases;
static bool owned(void *p,const struct mt76_scan_input *v,unsigned n){assert(p==input && v==input && n==3);return ++calls!=deny;}
static void setup(void){
 calls=deny=0;memset(input,0,sizeof input);
 for(unsigned i=0;i<3;i++){
  struct mt76_scan_input *v=&input[i];v->channel=mt76_scan_channel(i);v->ring=0x1000+0x2000*i;v->physical=v->ring+4096;v->buffer=pages[i];
  v->complete=v->stopped=v->quiescent=v->retained=v->during_window=true;v->events=1;v->completion=13;v->elapsed_us=1000;
  v->actual=frame(i,i+1,(const uint8_t *)"network",7,false,v->channel,2);v->remaining=4096-v->actual;
  v->tune=(struct mt76_channel_tune){.attempted=true,.complete=true,.target_complete=true,.cleanup_attempted=true,.cleanup_complete=true,.vco_attempted=true,.cleanup_vco_attempted=true,.target_vco_clear=true,.cleanup_vco_clear=true,.observe_attempted=true,.observe_complete=true,.channel=v->channel,.steps=21,.target_polls=3,.cleanup_polls=3,.target_delay_us=100,.cleanup_delay_us=100,.saved_mask=8191,.touched_mask=8191,.restored_mask=8191};
  for(unsigned j=0;j<13;j++)v->tune.saved[j]=(uint8_t)mt76_channel_snapshot_initial(j);
 }
}
static bool collect(struct mt76_scan *s){const uint8_t us[2]={'U','S'};const struct mt76_scan_io f={input,owned};cases++;return mt76_scan_collect(s,us,input,3,&f);}
static void refuse(void){struct mt76_scan s={0},expected={.attempted=true,.failed=true};assert(!collect(&s) && !memcmp(&s,&expected,sizeof s));}
int main(void){
 setup();struct mt76_scan s={0};assert(collect(&s) && s.complete && !s.failed && s.inputs==3 && s.parsed==3 && s.count==3);unsigned boundaries=calls;
 for(unsigned i=0;i<3;i++)assert(s.row[i].advertisement.channel==mt76_scan_channel(i) && s.row[i].advertisement.bssid[5]==i+1 && s.row[i].channels==(1u<<(mt76_scan_channel(i)-1)) && !s.row[i].channel_conflict);
 struct mt76_scan saved=s;assert(!collect(&s) && !memcmp(&s,&saved,sizeof s));
 for(unsigned n=1;n<=boundaries;n++){setup();deny=n;refuse();}
 /* A single BSSID may be observed on different channels. Preserve conflicts,
  * reveal a hidden SSID once and retain the original advertised security. */
 setup();for(unsigned i=0;i<3;i++){input[i].actual=frame(i,9,(const uint8_t *)(i==2?"second":"first"),i==2?6:i?5:0,i==2,input[i].channel,i?0:2);input[i].remaining=4096-input[i].actual;}
 s=(struct mt76_scan){0};assert(collect(&s) && s.count==1 && s.row[0].channel_conflict && s.row[0].channels==1057 && s.row[0].advertisement.observations==3 && s.row[0].advertisement.ssid_conflict && s.row[0].advertisement.security_conflict && !s.row[0].advertisement.hidden && s.row[0].advertisement.ssid_size==5 && !memcmp(s.row[0].advertisement.ssid,"first",5) && s.row[0].advertisement.beacon_seen && s.row[0].advertisement.probe_response_seen);
 setup();for(unsigned i=0;i<3;i++){input[i].events=1;input[i].completion=26;input[i].remaining=4096;input[i].actual=0;input[i].during_window=false;input[i].elapsed_us=10000000;}
 s=(struct mt76_scan){0};assert(collect(&s) && !s.count && s.unobserved==3);
 setup();input[0].actual=0;input[0].remaining=4096;put16(pages[1]+36,0x40);pages[2][0]=0;
 s=(struct mt76_scan){0};assert(collect(&s) && !s.count && s.empty==1 && s.unrelated==1 && s.rejected==1);
 for(unsigned i=0;i<3;i++)for(unsigned k=0;k<40;k++){
  setup();struct mt76_scan_input *v=&input[i];struct mt76_channel_tune *t=&v->tune;
  switch(k){
   case 0:v->channel=2;break;case 1:v->complete=false;break;case 2:v->stopped=false;break;case 3:v->quiescent=false;break;case 4:v->retained=false;break;
   case 5:v->ring=0;break;case 6:v->physical=0;break;case 7:v->ring++;break;case 8:v->physical++;break;case 9:v->ring=v->physical;break;case 10:v->buffer=NULL;break;
   case 11:t->attempted=false;break;case 12:t->complete=false;break;case 13:t->failed=true;break;case 14:t->status=MT76_CHANNEL_STATE;break;case 15:t->channel=2;break;
   case 16:t->target_complete=false;break;case 17:t->observe_attempted=false;break;case 18:t->observe_complete=false;break;case 19:t->cleanup_attempted=false;break;case 20:t->cleanup_complete=false;break;
   case 21:t->vco_attempted=false;break;case 22:t->cleanup_vco_attempted=false;break;case 23:t->target_vco_clear=false;break;case 24:t->cleanup_vco_clear=false;break;
   case 25:t->saved_mask=0;break;case 26:t->touched_mask=0;break;case 27:t->restored_mask=0;break;case 28:t->steps=20;break;case 29:t->target_polls=0;break;case 30:t->cleanup_polls=7;break;
   case 31:t->target_delay_us=101;break;case 32:t->cleanup_delay_us=101;break;case 33:t->saved[0]^=1;break;case 34:v->events=2;break;case 35:v->completion=27;break;case 36:v->remaining=4097;break;case 37:v->actual++;break;case 38:v->elapsed_us=10100001;break;case 39:v->ring=UINT64_MAX-4095;v->physical=v->ring;break;
  }refuse();assert(!calls);
 }
 for(unsigned i=1;i<3;i++)for(unsigned k=0;k<5;k++){setup();if(k==0)input[i].ring=input[0].ring;if(k==1)input[i].physical=input[0].ring;if(k==2)input[i].ring=input[0].physical;if(k==3)input[i].physical=input[0].physical;if(k==4)input[i].buffer=input[0].buffer;refuse();}
 for(unsigned i=0;i<3;i++)for(unsigned n=0;n<13;n++){setup();input[i].tune.saved[n]^=1;refuse();assert(!calls);}
 setup();uint8_t ca[2]={'C','A'};struct mt76_scan_io cf={input,owned};s=(struct mt76_scan){0};assert(!mt76_scan_collect(&s,ca,input,3,&cf) && !calls);
 for(unsigned ch=1;ch<=11;ch++)for(unsigned i=0;i<3;i++){
  setup();input[i].actual=frame(i,1,(const uint8_t *)"a",1,false,ch,0);input[i].remaining=4096-input[i].actual;s=(struct mt76_scan){0};assert(collect(&s));assert(s.parsed==(ch==input[i].channel?3u:2u));
 }
 setup();struct mt76_scan_io f={input,owned};uint8_t us[2]={'U','S'};
 for(unsigned k=0;k<7;k++){s=(struct mt76_scan){0};struct mt76_scan_io empty={0};assert(!mt76_scan_collect(k==0?NULL:&s,k==1?NULL:us,k==2?NULL:input,k==3?0:3,k==4?NULL:k==5?&empty:&f) || k==6);}
 assert(!mt76_scan_channel(3) && !mt76_scan_channel(UINT32_MAX));printf("mt76 scan: PASS %u collections;three explicit channels,all ownership/ledger/alias boundaries,private dedup/conflicts,quiet/malformed/mismatched input and all-or-nothing publication\n",cases);
 return 0;
}
