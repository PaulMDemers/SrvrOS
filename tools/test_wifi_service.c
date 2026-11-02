#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/wifi_service.c"
static bool live[5]={true,true,true,true,true};
static unsigned owned_calls,deny,cases;
static bool present(void *p){return *(bool *)p;}
static bool owned(void *p,const struct mt76_scan_batch *b){assert(p && b);return ++owned_calls!=deny;}
static struct mt76_scan_batch batch(void){
    struct mt76_scan_batch b={.attempted=true,.complete=true,.visits=3,.inputs=1,.parsed=1,.count=1};
    b.row[0]=(struct mt76_scan_row){.channels=32,.advertisement={.bssid={2,1,2,3,4,5},.ssid={'a',0,'b'},.ssid_size=3,.channel=6,.interval=100,.observations=1,.beacon_seen=true,.privacy=true,.rsn=true}};return b;
}
static uint64_t publish(unsigned n,struct mt76_scan_batch *b){
    struct wifi_service_provider p={&live[n],"USB Wi-Fi",present};struct mt76_selection_io io={&live[n],owned};owned_calls=0;cases++;return wifi_service_publish(&p,b,&io);
}
static struct srv_wifi_select request(uint64_t id,uint64_t gen){return (struct srv_wifi_select){.version=1,.size=sizeof(struct srv_wifi_select),.owner=id,.generation=gen};}
int main(void){
    struct srv_wifi_snapshot out;struct mt76_scan_batch b=batch();
    _Static_assert(sizeof(struct srv_wifi_row)==148,"stable Wi-Fi row ABI");
    _Static_assert(sizeof out==3640,"stable Wi-Fi snapshot ABI");
    _Static_assert(sizeof(struct srv_wifi_select)==40,"stable choice ABI");
    assert(wifi_service_snapshot(0,&out)==0 && !out.owner && !out.count && out.selected==SRV_WIFI_NONE);
    assert(!wifi_service_trial_ready(0,&out,true)); /* No provider isn't an empty scan. */
    struct srv_wifi_snapshot gate={.version=1,.size=sizeof gate,.owner=1,.generation=1,
        .adapter_count=1,.flags=SRV_WIFI_READY,.selected=SRV_WIFI_NONE};
    assert(wifi_service_trial_ready(0,&gate,true) && !wifi_service_trial_ready(0,&gate,false));
    assert(!wifi_service_trial_ready(1,&gate,true) && !wifi_service_trial_ready(0,NULL,true));
    for(unsigned k=0;k<9;k++){
        struct srv_wifi_snapshot bad=gate;
        switch(k){case 0:bad.owner=0;break;case 1:bad.generation=0;break;case 2:bad.adapter_count=0;break;
        case 3:bad.adapter_count=5;break;case 4:bad.flags=0;break;case 5:bad.count=25;break;
        case 6:bad.selected=0;break;case 7:bad.version=2;break;case 8:bad.size--;break;}
        assert(!wifi_service_trial_ready(0,&bad,true));
    }
    gate.count=1;gate.row[0].flags=SRV_WIFI_ROW_HIDDEN;
    assert(wifi_service_trial_ready(0,&gate,true) && !wifi_service_trial_ready(0,&gate,false));
    gate.row[0].flags=SRV_WIFI_ROW_SELECTABLE;
    assert(wifi_service_trial_ready(0,&gate,true) && wifi_service_trial_ready(0,&gate,false));
    uint64_t ids[5];for(unsigned i=0;i<4;i++){ids[i]=publish(i,&b);assert(ids[i]);}
    assert(!publish(4,&b) && !publish(0,&b));
    for(unsigned i=0;i<4;i++){
        assert(wifi_service_snapshot(ids[i],&out)==0 && out.owner==ids[i] && out.adapter_count==4 && out.count==1 && out.generation==1);
        assert(out.next_owner==(i==3?0:ids[i+1]));assert(!strcmp(out.row[0].name,"a\\x00b"));
        struct srv_wifi_select q=request(ids[i],1);assert(wifi_service_select(&q)==0);
        assert(wifi_service_snapshot(ids[i],&out)==0 && out.selected==0);
        q.generation=0;assert(wifi_service_select(&q)==SRV_WIFI_STALE);
        q=request(ids[i],1);q.index=1;assert(wifi_service_select(&q)==SRV_WIFI_INVALID);
    }
    /* A disconnected owner cannot resurrect even if the same context is live
     * again with generation1 and the same row index. */
    live[0]=false;assert(wifi_service_snapshot(ids[0],&out)==SRV_WIFI_STALE && !out.owner && !out.count);
    live[0]=true;assert(wifi_service_snapshot(ids[0],&out)==SRV_WIFI_STALE);
    uint64_t again=publish(0,&b);assert(again>ids[3]);struct srv_wifi_select old=request(ids[0],1);assert(wifi_service_select(&old)==SRV_WIFI_STALE);
    const struct mt76_selection_io io={&live[0],owned};
    for(unsigned n=1;n<=2;n++){
        deny=n;owned_calls=0;assert(!wifi_service_refresh(again,&b,&io));
        assert(wifi_service_snapshot(again,&out)==0 && !out.count && !(out.flags&SRV_WIFI_READY) && out.selected==SRV_WIFI_NONE);
        struct srv_wifi_select q=request(again,out.generation);assert(wifi_service_select(&q)==SRV_WIFI_UNAVAILABLE);
    }
    deny=0;owned_calls=0;assert(wifi_service_refresh(again,&b,&io));
    assert(wifi_service_snapshot(again,&out)==0 && out.generation==4 && out.count==1);
    b.row[0].advertisement.hidden=true;b.row[0].advertisement.ssid_size=0;assert(wifi_service_refresh(again,&b,&io));
    struct srv_wifi_select q=request(again,5);assert(wifi_service_select(&q)==SRV_WIFI_HIDDEN);
    b=batch();b.row[0].advertisement.security_conflict=true;assert(wifi_service_refresh(again,&b,&io));q=request(again,6);assert(wifi_service_select(&q)==SRV_WIFI_CONFLICT);
    for(unsigned k=0;k<5;k++){
        q=request(again,6);switch(k){case 0:q.version=2;break;case 1:q.size--;break;case 2:q.reserved=1;break;case 3:q.owner=0;break;case 4:q.index=24;break;}
        assert(wifi_service_select(&q)==SRV_WIFI_INVALID);cases++;
    }
    assert(wifi_service_select(NULL)==SRV_WIFI_INVALID && wifi_service_snapshot(0,NULL)==SRV_WIFI_INVALID);
    assert(!wifi_service_refresh(again,&b,NULL));
    for(unsigned i=0;i<4;i++)wifi_service_remove(ids[i]);
    wifi_service_remove(again);
    assert(wifi_service_snapshot(0,&out)==0 && !out.owner && !out.adapter_count);
    struct wifi_service_counts stats;wifi_service_counts(&stats);
    assert(stats.published==5 && stats.selected==4 && stats.removed==5 && stats.stale>=5 && stats.refused>=5 && stats.reads);
    struct mt76_scan_batch empty={.attempted=true,.complete=true,.visits=3,.pending=3};
    uint64_t empty_owner=publish(0,&empty);assert(empty_owner);
    assert(wifi_service_snapshot(empty_owner,&out)==SRV_WIFI_OK && !out.count);
    assert(wifi_service_trial_ready(0,&out,true) && !wifi_service_trial_ready(0,&out,false));
    q=request(empty_owner,out.generation);assert(wifi_service_select(&q)==SRV_WIFI_INVALID);
    live[0]=false;assert(wifi_service_snapshot(empty_owner,&out)==SRV_WIFI_STALE);
    assert(!wifi_service_trial_ready(SRV_WIFI_STALE,&out,true));live[0]=true;
    assert(wifi_service_snapshot(empty_owner,&out)==SRV_WIFI_STALE);
    counts.selected=UINT64_MAX;increment(&counts.selected);assert(counts.selected==UINT64_MAX);
    serial=UINT64_MAX;assert(!publish(0,&b));
    puts("wifi-service: PASS four adapters,exact owner/generation selection,disconnected identity retirement,refresh proof failures,hidden/conflict/ABI refusals and owner exhaustion");
}
