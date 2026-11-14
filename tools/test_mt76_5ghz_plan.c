#include <srvros/mt76_channel_plan.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    struct mt76_channel_step out[21];assert(mt76_channel_5ghz_plan(161,out));
    for(unsigned i=0;i<21;i++){
        assert(out[i].reg==fgetc(f) && out[i].mask==fgetc(f) && out[i].bits==fgetc(f));
        assert(!(out[i].bits&~out[i].mask));
        for(unsigned v=0;v<256;v++){
            unsigned after=(v&~out[i].mask)|out[i].bits;
            assert((after&~out[i].mask)==(v&~out[i].mask));
        }
    }
    assert(fgetc(f)==EOF);fclose(f);
    for(unsigned ch=0;ch<=256;ch++)if(ch!=161){
        memset(out,255,sizeof out);assert(!mt76_channel_5ghz_plan(ch,out));
        for(unsigned i=0;i<sizeof out;i++)assert(!((unsigned char *)out)[i]);
    }
    assert(!mt76_channel_5ghz_plan(161,NULL) && !mt76_channel_5ghz_plan(UINT32_MAX,out));
    assert(!mt76_channel_plan(161,out)); /* Legacy US1-11 planner stays separate. */
    puts("MT76 5GHz frequency plan: PASS pinned channel161 fixture, masked fields, zeroed/refused other channels;offline only,no native caller");
}
