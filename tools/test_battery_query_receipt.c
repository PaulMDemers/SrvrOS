#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_query_receipt.h"
int main(void) {
    for(unsigned code=0;code<256;code++)assert((battery_query_classify(BATTERY_QUERY_MODEL,code)==BATTERY_QUERY_NOTIFY)==(code==32));
    struct battery_query_receipt r={.sequence=1,.generation=2,.code=255,.policy=BATTERY_QUERY_MODEL,.refused=true,
        .transfer_done=true,.completed_before=9};
    assert(battery_query_returned(&r,10,true));
    assert(!battery_query_returned(NULL,10,true));
    /* Independent uncertainty must always refuse, even alongside other hazards. */
    for(unsigned mask=0;mask<512;mask++) {
        struct battery_query_receipt v=r;
        if(mask&1)v.sequence=0;
        if(mask&2)v.generation=0;
        if(mask&4)v.refused=false;
        if(mask&8)v.transfer_done=false;
        if(mask&16)v.in_aml=true;
        if(mask&32)v.aml_calls=1;
        if(mask&64)v.effects=1;
        assert(battery_query_returned(&v,mask&128?9:10,!(mask&256))==!mask);
    }
    for(unsigned code=0;code<256;code++) {
        r.code=code;assert(battery_query_returned(&r,10,true)==(code && code!=32));
    }
    r.code=255;r.aml_calls=r.aml_returns=1;
    assert(battery_query_returned(&r,10,true));
    r.effects=1;assert(!battery_query_returned(&r,10,true));
    r.effects=0;r.completed_before=UINT64_MAX;assert(!battery_query_returned(&r,0,true));
    r.completed_before=9;
    for(unsigned policy=0;policy<256;policy++)for(unsigned code=0;code<256;code++) {
        r.policy=policy;r.code=code;
        bool denied=(code && (policy==BATTERY_QUERY_MODEL ? code!=32 :
            policy==BATTERY_QUERY_AIR72 && code!=0x20 && code!=0x21 && code!=0x40 &&
            code!=0x5a && code!=0xce && code!=0xcf && code!=0xd0)) ||
            (policy==BATTERY_QUERY_AIR61 && code && code!=0x20 && code!=0x21 && code!=0x40 && code!=0x5a);
        assert(battery_query_returned(&r,10,true)==denied);
    }
    for(unsigned code=0;code<256;code++) {
        bool rejected=battery_query_rejected(battery_query_classify(BATTERY_QUERY_AIR61,code));
        assert(rejected==(code && code!=0x20 && code!=0x21 && code!=0x40 && code!=0x5a));
    }
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0x10)==BATTERY_QUERY_OS_BRANCH);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0x80)==BATTERY_QUERY_DEVICE_POLICY);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0x20)==BATTERY_QUERY_SHARED);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0x21)==BATTERY_QUERY_SHARED);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0xcf)==BATTERY_QUERY_NOTIFY);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0xce)==BATTERY_QUERY_EMPTY);
    assert(battery_query_classify(BATTERY_QUERY_AIR72,0xd0)==BATTERY_QUERY_EMPTY);
    r.policy=BATTERY_QUERY_AIR72;r.generation=0;r.code=0x10;
    assert(battery_query_returned(&r,10,true));
    r.policy=BATTERY_QUERY_AIR61;
    assert(battery_query_returned(&r,10,true));
    for(unsigned mask=0;mask<256;mask++) {
        struct battery_query_receipt v=r;
        if(mask&1)v.sequence=0;
        if(mask&2)v.refused=false;
        if(mask&4)v.transfer_done=false;
        if(mask&8)v.in_aml=true;
        if(mask&16)v.aml_calls++;
        if(mask&32)v.effects=1;
        assert(battery_query_returned(&v,mask&64?9:10,!(mask&128))==!mask);
    }
    r.code=0;assert(!battery_query_returned(&r,10,true));
    puts("battery-query-receipt: PASS exact VM admission, 512 hazards, 65536 policy/code receipts, completed callback and whole-batch effects");
}
