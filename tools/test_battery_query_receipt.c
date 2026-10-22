#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_query_receipt.h"
int main(void) {
    for(unsigned code=0;code<256;code++)assert(battery_query_model_admits(code)==(code==32));
    struct battery_query_receipt r={.sequence=1,.generation=2,.code=255,.refused=true,
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
    puts("battery-query-receipt: PASS exact VM admission, 512 hazards, completed callback and whole-batch effects");
}
