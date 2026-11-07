#include <srvros/mt76_selection.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,deny,tests;
static bool owned(void *c,const struct mt76_scan_batch *b){assert(c==b && b);return ++calls!=deny;}
static struct mt76_scan_batch batch(unsigned count){
    struct mt76_scan_batch b={.attempted=true,.complete=true,.visits=3,.inputs=count,.parsed=count,.count=count};
    for(unsigned i=0;i<count;i++){
        b.row[i]=(struct mt76_scan_row){.channels=1,.advertisement={.bssid={2,0,0,0,0,(uint8_t)(i+1)},.ssid={'a',0,'b'},.ssid_size=3,.channel=1,.interval=100,.observations=1,.privacy=true,.rsn=true,.ds_present=true,.beacon_seen=true}};
    }
    return b;
}
static bool publish(struct mt76_selection *s,struct mt76_scan_batch *b){
    calls=0;const struct mt76_selection_io io={b,owned};tests++;return mt76_selection_publish(s,b,&io);
}
static void empty(const void *p,size_t n){const uint8_t *v=p;for(size_t i=0;i<n;i++)assert(!v[i]);}
static void refused(struct mt76_scan_batch *b){
    struct mt76_selection s={0};struct mt76_scan_batch good=batch(1);deny=0;assert(publish(&s,&good));
    assert(!publish(&s,b) && s.generation==2 && !s.complete && !s.count);empty(s.row,sizeof s.row);
    struct mt76_selection_choice choice;memset(&choice,0xa5,sizeof choice);
    assert(mt76_selection_choose(&s,1,0,&choice)==MT76_SELECTION_STALE);empty(&choice,sizeof choice);
}
static void lifecycle(void){
    struct mt76_selection s={0};struct mt76_scan_batch b=batch(24);struct mt76_selection_choice out;
    assert(publish(&s,&b) && s.generation==1 && s.complete && s.count==24);
    for(unsigned i=0;i<24;i++){
        assert(mt76_selection_choose(&s,1,i,&out)==MT76_SELECTION_OK);
        assert(out.index==i && out.generation==1 && out.country[0]=='U' && out.country[1]=='S' && out.bandwidth==20);
        assert(out.row.advertisement.bssid[5]==i+1 && out.row.advertisement.ssid_size==3);
        assert(!memcmp(out.row.advertisement.ssid,"a\0b",3));
    }
    b.row[0].advertisement.ssid[0]='z';assert(s.row[0].advertisement.ssid[0]=='a');
    assert(publish(&s,&b) && s.generation==2);
    assert(mt76_selection_choose(&s,1,0,&out)==MT76_SELECTION_STALE);empty(&out,sizeof out);
    assert(mt76_selection_choose(&s,2,24,&out)==MT76_SELECTION_INVALID);empty(&out,sizeof out);
    assert(mt76_selection_choose(&s,0,0,&out)==MT76_SELECTION_STALE);
    b=batch(0);assert(publish(&s,&b) && s.generation==3 && s.complete && !s.count);empty(s.row,sizeof s.row);
    assert(mt76_selection_choose(&s,3,0,&out)==MT76_SELECTION_INVALID);
    assert(mt76_selection_invalidate(&s) && s.generation==4 && !s.complete);
    assert(mt76_selection_choose(&s,4,0,&out)==MT76_SELECTION_UNAVAILABLE);
    s.generation=UINT64_MAX-1;b=batch(1);assert(publish(&s,&b) && s.generation==UINT64_MAX);
    assert(mt76_selection_choose(&s,UINT64_MAX,0,&out)==MT76_SELECTION_OK);
    assert(!publish(&s,&b) && s.generation==UINT64_MAX && !s.complete);empty(s.row,sizeof s.row);
    assert(!publish(&s,&b) && s.generation==UINT64_MAX);
    assert(!mt76_selection_invalidate(NULL));assert(!mt76_selection_publish(NULL,&b,NULL));
    assert(mt76_selection_choose(NULL,1,0,&out)==MT76_SELECTION_INVALID);empty(&out,sizeof out);
    assert(mt76_selection_choose(&s,1,0,NULL)==MT76_SELECTION_INVALID);
}
static void metadata(void){
    struct mt76_selection s={0};struct mt76_selection_choice out;
    for(unsigned ch=0;ch<3;ch++)for(unsigned flags=0;flags<64;flags++){
        struct mt76_scan_batch b=batch(1);struct mt76_scan_row *v=&b.row[0];
        unsigned channel=ch==0?1:ch==1?6:11;v->advertisement.channel=channel;v->channels=1u<<(channel-1);
        v->advertisement.hidden=flags&1;v->advertisement.ssid_size=(flags&1)?0:3;
        v->advertisement.ssid_conflict=flags&2;v->advertisement.security_conflict=flags&4;
        if(flags&8){v->channels=0x421;v->channel_conflict=true;}
        v->advertisement.privacy=flags&16;v->advertisement.rsn=flags&32;v->advertisement.wpa=flags&16;
        assert(publish(&s,&b));
        enum mt76_selection_status expected=(flags&14)?MT76_SELECTION_CONFLICT:(flags&1)?MT76_SELECTION_HIDDEN:MT76_SELECTION_OK;
        assert(mt76_selection_choose(&s,s.generation,0,&out)==expected);
        if(expected!=MT76_SELECTION_OK)empty(&out,sizeof out);
        else assert(out.row.advertisement.channel==channel && out.row.advertisement.rsn==((flags&32)!=0));
    }
    for(unsigned n=1;n<=32;n++){
        struct mt76_scan_batch b=batch(1);b.row[0].advertisement.ssid_size=n;
        memset(b.row[0].advertisement.ssid,0xff,32);assert(publish(&s,&b));
        assert(mt76_selection_choose(&s,s.generation,0,&out)==MT76_SELECTION_OK);
        for(unsigned i=n;i<32;i++)assert(!out.row.advertisement.ssid[i] && !s.row[0].advertisement.ssid[i]);
    }
}
static void malformed(void){
    struct mt76_scan_batch b;
#define BAD(statement) do{b=batch(1);statement;refused(&b);}while(0)
    BAD(b.attempted=false);BAD(b.complete=false);BAD(b.failed=true);BAD(b.visits=2);
    BAD(b.inputs=25);BAD(b.pending=4);BAD(b.pending=UINT32_MAX);BAD(b.inputs=24;b.pending=1);
    BAD(b.parsed=25);BAD(b.empty=25);BAD(b.unrelated=25);BAD(b.rejected=25);BAD(b.count=25);
    BAD(b.parsed=0);BAD(b.count=0);BAD(b.inputs=0);BAD(b.empty=1);
    BAD(b.row[0].advertisement.observations=0);BAD(b.row[0].advertisement.observations=25);
    BAD(b.row[0].advertisement.observations=2);BAD(b.row[0].advertisement.ssid_size=33);
    BAD(b.row[0].advertisement.hidden=true);BAD(b.row[0].advertisement.interval=0);
    BAD(b.row[0].advertisement.beacon_seen=false);BAD(b.row[0].advertisement.channel=0);
    BAD(b.row[0].advertisement.channel=12);BAD(b.row[0].channels=0);BAD(b.row[0].channels=2);
    BAD(b.row[0].channels=0x421);BAD(b.row[0].channel_conflict=true);
    BAD(memset(b.row[0].advertisement.bssid,0,6));BAD(b.row[0].advertisement.bssid[0]=1);
    b=batch(2);b.row[1].advertisement.bssid[5]=1;refused(&b);
#undef BAD
    for(unsigned n=0;n<32;n++){
        b=batch(1);b.inputs=UINT32_MAX-n;refused(&b);
        b=batch(1);b.count=UINT32_MAX-n;refused(&b);
        b=batch(1);b.row[0].advertisement.observations=UINT32_MAX-n;refused(&b);
    }
    b=batch(0);b.inputs=3;b.pending=3;b.empty=1;b.unrelated=1;b.rejected=1;
    struct mt76_selection s={0};assert(publish(&s,&b));
}
static void ownership(void){
    struct mt76_scan_batch b=batch(1);struct mt76_selection s={0};
    for(unsigned n=1;n<=2;n++){deny=n;assert(!publish(&s,&b));assert(!s.complete && !s.count);empty(s.row,sizeof s.row);}
    deny=0;assert(publish(&s,&b) && calls==2);
    assert(!mt76_selection_publish(&s,&b,NULL));empty(s.row,sizeof s.row);
    const struct mt76_selection_io io={&b,owned};assert(!mt76_selection_publish(&s,NULL,&io));
    assert(publish(&s,&b));struct mt76_selection saved=s;
    assert(mt76_selection_choose(&s,s.generation,0,(void *)&s)==MT76_SELECTION_INVALID);
    assert(!memcmp(&s,&saved,sizeof s));
    assert(!mt76_selection_publish(&s,(void *)&s,&io));empty(s.row,sizeof s.row);
    assert(publish(&s,&b));assert(!mt76_selection_publish(&s,&b,(void *)&s));empty(s.row,sizeof s.row);
}
static void names(void){
    char out[130];uint8_t name[32];
    assert(mt76_selection_name(NULL,0,out,sizeof out) && !*out);
    assert(!mt76_selection_name(NULL,1,out,sizeof out) && !*out);
    assert(!mt76_selection_name(name,33,out,sizeof out) && !*out);
    for(unsigned x=0;x<256;x++){
        name[0]=x;bool literal=x>=32 && x<=126 && x!='\\';unsigned need=literal?2:5;
        for(unsigned cap=1;cap<=6;cap++){
            memset(out,0x55,sizeof out);tests++;
            bool ok=mt76_selection_name(name,1,out,cap);assert(ok==(cap>=need));
            if(ok){assert(strlen(out)==need-1);if(literal)assert((uint8_t)out[0]==x);else{char expect[5];snprintf(expect,sizeof expect,"\\x%02x",x);assert(!strcmp(out,expect));}}
            else assert(!*out);
            assert(out[cap]==0x55);
        }
    }
    memset(name,0,32);assert(mt76_selection_name(name,32,out,129) && strlen(out)==128);
    assert(!mt76_selection_name(name,32,out,128) && !*out);
    assert(!mt76_selection_name(name,1,NULL,1));assert(!mt76_selection_name(name,1,out,0));
    out[0]='x';assert(!mt76_selection_name((void *)out,1,out,sizeof out) && out[0]=='x');
}
int main(void){lifecycle();metadata();malformed();ownership();names();printf("PASS %u discovery selection lifecycle/metadata/ownership/name cases\n",tests);}
