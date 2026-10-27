#include <srvros/mt76_scan_batch.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct mt76_scan_batch_visit visits[3];
static uint8_t pages[3][8][4096] __attribute__((aligned(4096)));
static unsigned calls,deny,cases,posts,checks,stops;
static uint64_t clock_us;
static bool quiet;
static unsigned zero_slot=8;
static unsigned active;
static void put16(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void packet(unsigned visit,unsigned index,unsigned id,unsigned ch,unsigned name,bool probe,unsigned security){
    uint8_t *p=pages[visit][index];memset(p,0,4096);
    put16(p,80);put16(p+8,0);put16(p+10,45);put16(p+36,probe?0x50:0x80);
    memset(p+40,255,6);p[46]=p[52]=2;p[51]=p[57]=(uint8_t)id;
    put16(p+68,100);put16(p+70,1|(security?16:0));
    const uint8_t ies[]={0,1,(uint8_t)name,1,1,0x82,3,1,(uint8_t)ch};memcpy(p+72,ies,sizeof ies);
}
static bool window_owned(void *c,bool clean){assert(c==&active);checks++;return !clean || stops==1;}
static bool post(void *c,unsigned i,uint64_t trb,uint64_t phys){
    assert(c==&active && i==posts++ && trb==visits[active].window.plan.ring+16*i && phys==visits[active].window.plan.physical[i]);return true;
}
static bool poll(void *c,struct mt76_rx_window_event *e,bool *present){
    assert(c==&active);*present=!quiet;*e=(struct mt76_rx_window_event){.slot=1,.dci=9,.trb=visits[active].window.plan.ring+16*(posts-1),.completion=13,.remaining=posts==zero_slot+1?4096:4096-84};return true;
}
static bool stop(void *c,struct mt76_rx_window_stop *s){assert(c==&active && ++stops==1);*s=(struct mt76_rx_window_stop){.slot=1,.dci=9,.state=3,.dequeue=visits[active].window.plan.ring+16*posts+1};return true;}
static uint64_t now(void *c){assert(c==&active);clock_us+=quiet?10000:100;return clock_us;}
static bool owned(void *c,const struct mt76_scan_batch_visit *v,unsigned count){assert(c==visits && v==visits && count==3);return ++calls!=deny;}
static void refresh(unsigned i){
    struct mt76_rx_window_plan plan=visits[i].window.plan;
    visits[i].window=(struct mt76_rx_window){0};active=i;posts=checks=stops=0;clock_us=0;
    const struct mt76_rx_window_io io={&active,window_owned,post,poll,stop,now};
    assert(mt76_rx_window_run(&visits[i].window,&plan,&io));
}
static void setup(bool no_events){
    memset(visits,0,sizeof visits);calls=deny=0;quiet=no_events;
    for(unsigned i=0;i<3;i++){
        struct mt76_scan_batch_visit *v=&visits[i];unsigned channel=mt76_scan_channel(i);
        v->tune=(struct mt76_channel_tune){.attempted=true,.complete=true,.target_complete=true,.cleanup_attempted=true,.cleanup_complete=true,.vco_attempted=true,.cleanup_vco_attempted=true,.target_vco_clear=true,.cleanup_vco_clear=true,.observe_attempted=true,.observe_complete=true,.channel=channel,.steps=21,.target_polls=3,.cleanup_polls=3,.target_delay_us=100,.cleanup_delay_us=100,.saved_mask=8191,.touched_mask=8191,.restored_mask=8191};
        for(unsigned j=0;j<13;j++)v->tune.saved[j]=(uint8_t)mt76_channel_snapshot_initial(j);
        struct mt76_rx_window_plan plan={.ring=0x1000+0x10000*i,.slot=1,.dci=9,.budget_us=10000000,.country={'U','S'},.channel=channel};
        for(unsigned j=0;j<8;j++){plan.physical[j]=plan.ring+4096*(j+1);plan.buffer[j]=pages[i][j];packet(i,j,i*8+j+1,channel,'a'+i,false,0);}
        active=i;posts=checks=stops=0;clock_us=0;
        const struct mt76_rx_window_io io={&active,window_owned,post,poll,stop,now};
        assert(mt76_rx_window_run(&v->window,&plan,&io));
        assert(v->window.beacons==(quiet?0u:8u));
    }
}
static bool collect(struct mt76_scan_batch *s){cases++;const struct mt76_scan_batch_io io={visits,owned};return mt76_scan_batch_collect(s,visits,3,&io);}
static void refuse(void){struct mt76_scan_batch s={0},expected={.attempted=true,.failed=true};assert(!collect(&s) && !memcmp(&s,&expected,sizeof s));}
int main(void){
    setup(false);struct mt76_scan_batch s={0};assert(collect(&s) && s.complete && s.inputs==24 && s.parsed==24 && s.count==24 && !s.pending);
    unsigned boundaries=calls;
    for(unsigned i=0;i<24;i++)assert(s.row[i].advertisement.bssid[5]==i+1 && s.row[i].advertisement.channel==mt76_scan_channel(i/8));
    struct mt76_scan_batch saved=s;assert(!collect(&s) && !memcmp(&s,&saved,sizeof s));
    for(unsigned k=1;k<=boundaries;k++){setup(false);deny=k;refuse();assert(calls==k);}
    setup(true);s=(struct mt76_scan_batch){0};assert(collect(&s) && !s.count && s.pending==3 && !s.inputs && !s.parsed);
    /* Reparse all transfers, preserving private cross-channel/name/security
     * conflicts while retaining the first advertised identity and name. */
    setup(false);
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++){
        packet(i,j,9,mt76_scan_channel(i),'a'+i,i==2,i==1);
        visits[i].window.item[j].frame_control=i==2?0x50:0x80;visits[i].window.item[j].probe_response=i==2;visits[i].window.item[j].privacy=i==1;
    }
    s=(struct mt76_scan_batch){0};assert(collect(&s) && s.count==1 && s.parsed==24);
    assert(s.row[0].channels==1057 && s.row[0].channel_conflict && s.row[0].advertisement.observations==24 && s.row[0].advertisement.ssid_conflict && s.row[0].advertisement.security_conflict && s.row[0].advertisement.ssid[0]=='a' && s.row[0].advertisement.beacon_seen && s.row[0].advertisement.probe_response_seen);
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)for(unsigned kind=0;kind<4;kind++){
        setup(false);
        if(kind==0)zero_slot=j;
        if(kind==1)pages[i][j][0]=0;
        if(kind==2)put16(pages[i][j]+36,0x88);
        if(kind==3)pages[i][j][80]=2;
        refresh(i);zero_slot=8;
        s=(struct mt76_scan_batch){0};assert(collect(&s) && s.parsed==23 && s.count==23);
        assert(s.empty==(kind==0) && s.unrelated==(kind==2) && s.rejected==(kind==1 || kind==3));
    }
    /* Hidden names cannot erase revealed names, and reveal does not erase
     * earlier security/name conflicts. All24 observations still deduplicate. */
    for(unsigned reveal=0;reveal<3;reveal++){
        setup(false);
        for(unsigned i=0;i<3;i++){
            for(unsigned j=0;j<8;j++)packet(i,j,9,mt76_scan_channel(i),i==reveal?'x':0,false,0);
            refresh(i);
        }
        s=(struct mt76_scan_batch){0};assert(collect(&s) && s.count==1 && !s.row[0].advertisement.hidden && s.row[0].advertisement.ssid[0]=='x' && s.row[0].advertisement.observations==24 && !s.row[0].advertisement.ssid_conflict);
    }
    for(unsigned i=0;i<3;i++)for(unsigned k=0;k<31;k++){
        setup(false);struct mt76_scan_batch_visit *v=&visits[i];struct mt76_rx_window *w=&v->window;
        switch(k){
        case 0:v->tune.cleanup_complete=false;break;case 1:v->tune.channel=2;break;case 2:v->tune.saved[5]^=1;break;
        case 3:w->plan.country[0]='C';break;case 4:w->plan.channel=2;break;case 5:w->plan.country[0]=w->plan.country[1]=w->plan.channel=0;break;
        case 6:w->plan.budget_us=1000000;break;case 7:w->elapsed_us=10100001;break;case 8:w->attempts=0;break;case 9:w->attempts=9;break;
        case 10:w->completions=9;break;case 11:w->polls=0;break;case 12:w->polls=20000001;break;
        case 13:w->stopped=false;break;case 14:w->stop_attempted=false;break;case 15:w->complete=false;break;case 16:w->failed=true;break;
        case 17:w->polled=false;break;case 18:w->poll_ok=false;break;case 19:w->finish_attempted=false;break;case 20:w->attempted=false;break;
        case 21:w->plan.ring++;break;case 22:w->plan.slot=2;break;case 23:w->plan.dci=8;break;
        case 24:w->item[4].actual++;break;case 25:w->item[4].frame_control++;break;case 26:w->beacons--;break;
        case 27:w->item[4].completion=26;break;case 28:w->item[4].completed=false;break;
        case 29:pages[i][4][80]=(uint8_t)(mt76_scan_channel(i)==1?6:1);break;
        case 30:w->plan.channel=(uint8_t)(mt76_scan_channel(i)==1?6:1);break;
        }refuse();
    }
    for(unsigned i=1;i<3;i++)for(unsigned j=0;j<8;j++)for(unsigned k=0;k<5;k++){
        setup(false);struct mt76_rx_window_plan *p=&visits[i].window.plan,*q=&visits[0].window.plan;
        switch(k){case 0:p->ring=q->ring;break;case 1:p->physical[j]=q->ring;break;case 2:p->ring=q->physical[j];break;case 3:p->physical[j]=q->physical[7-j];break;case 4:p->buffer[j]=q->buffer[7-j];break;}refuse();assert(!calls);
    }
    for(unsigned i=0;i<3;i++)for(unsigned k=0;k<7;k++){
        setup(true);struct mt76_rx_window *w=&visits[i].window;
        switch(k){case 0:w->elapsed_us=9999999;break;case 1:w->item[7].submitted=true;break;case 2:w->item[7].privacy=true;break;case 3:w->item[7].view_status=0;break;case 4:w->item[0].actual=1;break;case 5:w->item[0].inspected=true;break;case 6:w->item[0].completion=26;break;}refuse();
    }
    /* Quiet windows cannot read any data page, even when all retained virtual
     * identities are aligned but deliberately inaccessible poison pointers. */
    setup(true);for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)visits[i].window.plan.buffer[j]=(const uint8_t *)(uintptr_t)(0x100000+4096*(i*8+j));
    s=(struct mt76_scan_batch){0};assert(collect(&s) && !s.inputs && !s.count && s.pending==3);
    setup(false);for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)visits[i].window.plan.buffer[j]=(const uint8_t *)(uintptr_t)(0x100000+4096*(i*8+j));
    deny=1;refuse();assert(calls==1);
    setup(false);const struct mt76_scan_batch_io io={visits,owned},empty={0};
    for(unsigned k=0;k<6;k++){s=(struct mt76_scan_batch){0};assert(!mt76_scan_batch_collect(k==0?NULL:&s,k==1?NULL:visits,k==2?2:3,k==3?NULL:k==4?&empty:&io) || k==5);}
    printf("mt76-scan-batch: PASS %u collections;24 distinct retained pages,three exact tunes,channel-aware8-IN windows,all ownership/alias/ledger boundaries,quiet/conflict/private publication;no native adapter\n",cases);
}
