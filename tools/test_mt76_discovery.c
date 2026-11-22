#include <srvros/mt76_discovery.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t pages[8][4096] __attribute__((aligned(4096)));
static size_t bytes[8];
static struct mt76_rx_window window;
static unsigned tests, posted, fail_at, calls;
static bool quiet, stopped;
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
    put16(p,dma);put32(wi+4,mpdu<<16);bytes[i]=dma+4;return bytes[i];
}
static bool owned(void *c,bool cleanup){(void)c;return !cleanup || stopped;}
static bool post(void *c,unsigned i,uint64_t trb,uint64_t phys){
    (void)c;assert(i==posted++ && trb==0x1000+16*i && phys==0x2000+4096*i);return true;
}
static bool poll(void *c,struct mt76_rx_window_event *e,bool *present){
    (void)c;*present=!quiet;
    *e=(struct mt76_rx_window_event){.trb=0x1000+16*(posted-1),.slot=1,.dci=9,
        .completion=13,.remaining=4096-(unsigned)bytes[posted-1]};return true;
}
static bool stop(void *c,struct mt76_rx_window_stop *s){
    (void)c;stopped=true;*s=(struct mt76_rx_window_stop){.dequeue=0x1000+16*posted+1,.slot=1,.dci=9,.state=3};return true;
}
static uint64_t now(void *c){(void)c;static uint64_t t;t+=1000;return t;}
static bool retained(void *c,const struct mt76_rx_window *w){
    (void)c;assert(w==&window && stopped);return ++calls!=fail_at;
}
static void run_window(void){
    memset(&window,0,sizeof window);posted=calls=fail_at=0;stopped=false;
    struct mt76_rx_window_plan p={.ring=0x1000,.slot=1,.dci=9};
    for(unsigned i=0;i<8;i++){p.physical[i]=0x2000+4096*i;p.buffer[i]=pages[i];}
    struct mt76_rx_window_io io={.owned=owned,.post=post,.poll=poll,.stop=stop,.now=now};
    assert(mt76_rx_window_run(&window,&p,&io));
}
static void setup(void){
    quiet=false;
    for(unsigned i=0;i<8;i++)frame(i,i+1,(const uint8_t *)"network",7,false,1,2);
    run_window();
}
static bool collect(struct mt76_discovery *s){
    struct mt76_discovery_io io={.owned=retained};tests++;
    return mt76_discovery_collect(s,&window,&io);
}
static void no_rows(const struct mt76_discovery *s){
    assert(s->attempted && s->failed && !s->complete);
    struct mt76_discovery expected={.attempted=true,.failed=true};
    assert(!memcmp(s,&expected,sizeof expected));
}
static void refuses(void){struct mt76_discovery s={0};assert(!collect(&s));no_rows(&s);}
int main(void){
    struct mt76_discovery s={0};setup();assert(collect(&s));
    assert(s.complete && !s.failed && s.inputs==8 && s.parsed==8 && s.count==8);
    for(unsigned i=0;i<8;i++)assert(s.row[i].bssid[5]==i+1 && s.row[i].channel==1 &&
        s.row[i].observations==1 && s.row[i].ssid_size==7 && s.row[i].rsn &&
        s.row[i].privacy && !s.row[i].wpa && !s.row[i].security_conflict);
    struct mt76_discovery saved=s;assert(!collect(&s));assert(!memcmp(&saved,&s,sizeof s));
    /* Distinct BSSIDs with same SSID stay separate; same BSSID merges even
     * with a different transmitter, sequence or beacon/probe-response subtype. */
    setup();for(unsigned i=0;i<8;i++)frame(i,9,(const uint8_t *)"one",3,i&1,1,2);
    pages[1][51]=99;run_window();s=(struct mt76_discovery){0};assert(collect(&s));
    assert(s.count==1 && s.row[0].observations==8 && s.row[0].beacon_seen && s.row[0].probe_response_seen);
    memset(pages,0,sizeof pages);assert(!memcmp(s.row[0].ssid,"one",3));
    /* Hidden (empty or all-zero) then revealed; embedded NUL/high bytes are
     * copied with length, never C-string interpreted. Later hidden cannot erase. */
    uint8_t binary[]={ 'a',0,0xff,'b' },zeros[32]={0};
    for(unsigned k=0;k<3;k++){
        for(unsigned i=0;i<8;i++)frame(i,9,i==2?binary:zeros,i==2?4:(k==0?0:32),i&1,k==2?0:1,2);
        run_window();s=(struct mt76_discovery){0};assert(collect(&s));
        assert(s.count==1 && !s.row[0].hidden && s.row[0].ssid_size==4 && !memcmp(s.row[0].ssid,binary,4));
        assert(!s.row[0].ssid_conflict && s.row[0].ds_present==(k!=2));
    }
    for(unsigned i=0;i<8;i++)frame(i,9,zeros,32,false,1,0);
    run_window();s=(struct mt76_discovery){0};assert(collect(&s));assert(s.row[0].hidden && !s.row[0].ssid_size);
    for(unsigned i=0;i<8;i++)frame(i,9,(const uint8_t *)(i?"second":"first"),i?6:5,false,1,i?0:6);
    run_window();s=(struct mt76_discovery){0};assert(collect(&s));
    assert(s.count==1 && s.row[0].ssid_conflict && s.row[0].security_conflict && s.row[0].rsn && s.row[0].wpa && s.row[0].privacy);
    assert(s.row[0].ssid_size==5 && !memcmp(s.row[0].ssid,"first",5));
    /* Neither unrelated probe requests, mismatched channels nor malformed
     * entries become access points. Empty normal completions are separate. */
    setup();for(unsigned i=0;i<8;i++){
        frame(i,i+1,binary,4,false,i==1?2:1,0);
        if(i==0)put16(pages[i]+36,0x40);
        if(i==2)pages[i][0]=0;
        if(i==3)bytes[i]=0;
    }
    run_window();s=(struct mt76_discovery){0};assert(collect(&s));
    assert(s.inputs==8 && s.empty==1 && s.unrelated==1 && s.rejected==2 && s.parsed==4 && s.count==4);
    setup();quiet=true;run_window();s=(struct mt76_discovery){0};assert(collect(&s));assert(s.inputs==s.count && s.count==0);
    /* Refuse every lifecycle boundary and invalid completion ledger before
     * reading DMA, including cancellation masquerading as received bytes. */
    for(unsigned k=0;k<22;k++){
        setup();switch(k){
        case 0:window.attempted=false;break;case 1:window.polled=false;break;
        case 2:window.poll_ok=false;break;case 3:window.finish_attempted=false;break;
        case 4:window.complete=false;break;case 5:window.failed=true;break;
        case 6:window.stop_attempted=false;break;case 7:window.stopped=false;break;
        case 8:window.attempts=0;break;case 9:window.attempts=9;break;
        case 10:window.completions=9;break;case 11:window.completions=5;break;
        case 12:window.beacons=9;break;case 13:window.item[3].submitted=false;break;
        case 14:window.item[3].completed=false;break;case 15:window.item[3].completion=26;break;
        case 16:window.item[3].remaining=4097;break;case 17:window.item[3].actual++;break;
        case 18:window.item[3].inspected=false;break;case 19:window.plan.buffer[3]=0;break;
        case 20:window.item[3].completion=27;break;case 21:window.item[3].completion=28;break;
        }refuses();assert(calls==0);
    }
    for(unsigned k=0;k<12;k++){
        setup();switch(k){case 0:window.item[5].view_status=255;break;
        case 1:window.item[5].consumed++;break;case 2:window.item[5].mpdu++;break;
        case 3:window.item[5].frame_control^=0x10;break;case 4:window.item[5].beacon_status=1;break;
        case 5:window.item[5].beacon=false;break;case 6:window.item[5].interval++;break;
        case 7:window.item[5].ssid_size++;break;case 8:window.item[5].privacy=false;break;
        case 9:window.item[5].rsn=false;break;case 10:window.item[5].wpa=true;break;
        case 11:window.item[5].ds_present=false;break;
        }refuses();
    }
    /* Failure on each initial/per-packet/final ownership check must erase all
     * previously staged rows; even seven valid frames do not publish partially. */
    for(unsigned k=1;k<=18;k++){setup();fail_at=k;refuses();assert(calls==k);}
    setup();window.item[5].probe_response=true;refuses();
    setup();pages[5][36]=0x40;refuses();
    setup();window.beacons--;refuses();
    setup();s=(struct mt76_discovery){0};assert(!mt76_discovery_collect(&s,&window,0));no_rows(&s);
    setup();s=(struct mt76_discovery){0};struct mt76_discovery_io io={0};assert(!mt76_discovery_collect(&s,&window,&io));no_rows(&s);
    assert(!mt76_discovery_collect(0,&window,&io));
    s=(struct mt76_discovery){0};assert(!mt76_discovery_collect(&s,0,&io));no_rows(&s);
    /* Every beacon truncation through exact allocations: no partial AP row
     * from that input; the seven intact entries remain independent. */
    setup();size_t full=bytes[0];
    for(size_t n=0;n<full;n++){
        setup();bytes[0]=n;run_window();
        uint8_t *p=malloc(n?n:1);assert(p);memcpy(p,pages[0],n);window.plan.buffer[0]=p;
        s=(struct mt76_discovery){0};assert(collect(&s));
        assert(s.count==7 && s.parsed==7 && s.rejected==(n>0) && s.empty==(n==0));free(p);
    }
    /* Pending and zero-byte entries must not smuggle stale parser metadata. */
    for(unsigned k=0;k<10;k++){
        setup();quiet=true;run_window();
        switch(k){case 0:window.item[0].completion=26;break;
        case 1:window.item[0].remaining=4096;break;case 2:window.item[0].actual=1;break;
        case 3:window.item[0].inspected=true;break;case 4:window.item[0].view_status=0;break;
        case 5:window.item[0].beacon_status=0;break;case 6:window.item[0].consumed=100;break;
        case 7:window.item[0].mpdu=24;break;case 8:window.item[0].frame_control=0x80;break;
        case 9:window.item[0].beacon=true;break;}
        refuses();assert(calls==0);
    }
    setup();for(unsigned i=0;i<8;i++)bytes[i]=0;run_window();
    window.item[7].frame_control=0x80;refuses();
    /* Exact-sized hostile buffers are decoded by window_finish first; collect
     * must preserve their rejection classification without inventing rows. */
    uint32_t seed=42;
    for(unsigned n=0;n<10000;n++){
        setup();for(unsigned i=0;i<8;i++){
            seed=seed*1664525u+1013904223u;bytes[i]=seed%512;
            for(unsigned j=0;j<bytes[i];j++){seed=seed*1664525u+1013904223u;pages[i][j]=seed>>24;}
        }
        run_window();uint8_t *exact[8];
        for(unsigned i=0;i<8;i++){
            exact[i]=malloc(bytes[i]?bytes[i]:1);assert(exact[i]);memcpy(exact[i],pages[i],bytes[i]);window.plan.buffer[i]=exact[i];
        }
        s=(struct mt76_discovery){0};assert(collect(&s));
        assert(s.inputs==s.empty+s.parsed+s.unrelated+s.rejected && s.count<=s.parsed && s.count<=8);
        for(unsigned i=0;i<8;i++)free(exact[i]);
    }
    printf("MT76 discovery: PASS %u collections; real window/stop pipeline,8 rows,dedup,binary/hidden/conflicting SSIDs,advertised security,atomic ownership failure,10000 hostile batches\n",tests);
}
