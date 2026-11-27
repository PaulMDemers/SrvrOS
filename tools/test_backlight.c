#include <assert.h>
#include <stdio.h>
#include <srvros/backlight.h>
static uint32_t regs[4];static unsigned reads,writes;static int broken;
static uint32_t read_reg(void *ctx,unsigned reg) { assert(ctx==regs && reg<4);reads++;return regs[reg]; }
static void write_duty(void *ctx,uint32_t value) { assert(ctx==regs);writes++;if(!broken) regs[2]=value; }
static void write_cpu_duty(void *ctx,uint32_t value) { assert(ctx==regs);writes++;if(!broken) regs[3]=value; }
static enum backlight_profile profile;
static unsigned expected_period=2777;
static void ready(void) {
    regs[0]=0;regs[1]=0xc0000000u;regs[2]=(expected_period<<16)|388;broken=0;
    unsigned before=writes;assert(backlight_bind_profile((struct backlight_io){regs,read_reg,write_duty},profile));assert(writes==before);
}
static void exercise(void) {
    ready();
    assert(backlight_percent(0)==(int)((388*100+expected_period/2)/expected_period));
    for(unsigned v=5;v<=100;v++) {
        assert(backlight_percent(v)==(int)v);
        assert(regs[0]==0 && regs[1]==0xc0000000u && (regs[2]>>16)==expected_period);
        assert((regs[2]&65535)>0 && (regs[2]&65535)<=expected_period);
    }
    unsigned r=reads,w=writes;
    assert(backlight_percent(1)==-1 && backlight_percent(101)==-1 && backlight_percent(~0u)==-1);
    assert(reads==r && writes==w);
    assert(backlight_percent(100)==100 && writes==w); /* Same duty is not rewritten. */
    for(unsigned reg=0;reg<3;reg++) {
        ready();regs[reg]^=reg==2?1u<<16:1;w=writes;
        assert(backlight_percent(50)==-1 && writes==w);r=reads;
        assert(backlight_percent(14)==-1 && reads==r && writes==w);
    }
    ready();broken=1;w=writes;assert(backlight_percent(90)==-1 && writes==w+1);
    assert(backlight_percent(14)==-1 && writes==w+1); /* No speculative recovery write. */
    regs[2]=expected_period<<16;assert(!backlight_bind_profile((struct backlight_io){regs,read_reg,write_duty},profile));
    assert(!backlight_bind((struct backlight_io){0}));
}
static void exercise_pro81(void) {
    regs[0]=regs[1]=0x80000000u;
    regs[2]=1808u<<16;
    regs[3]=0x690;
    broken=0;
    struct backlight_io io={regs,read_reg,write_cpu_duty};
    unsigned before=writes;
    assert(backlight_bind_profile(io,BACKLIGHT_PRO8_1) && writes==before);
    assert(backlight_percent(0)==93 && writes==before);
    assert(backlight_percent(35)==35 && regs[3]==633);
    assert(backlight_percent(75)==75 && regs[3]==1356);
    struct backlight_trial result=backlight_pro81_finish();
    assert(result.low && result.high && result.restored && result.writes==2);
    assert(result.initial_duty==0x690 && result.final_duty==1356);
    assert(regs[3]==0x690 && writes==before+3 && backlight_percent(35)==-1);
    assert(regs[0]==0x80000000u && regs[1]==0x80000000u && regs[2]==1808u<<16);
    assert(backlight_bind_profile(io,BACKLIGHT_PRO8_1));
    regs[1]|=1u<<30; /* A route change prevents further writes, including restoration. */
    before=writes;
    assert(!backlight_pro81_finish().restored && writes==before);
    assert(!backlight_bind_profile(io,BACKLIGHT_PRO8_1));
    regs[1]=0x80000000u;
    assert(backlight_bind_profile(io,BACKLIGHT_PRO8_1));
    broken=1;
    assert(backlight_percent(35)==-1 && !backlight_pro81_finish().restored && writes==before+1);
}
int main(void) {
    assert(backlight_percent(0)==-1 && !reads && !writes);
    ready();assert(backlight_bind((struct backlight_io){regs,read_reg,write_duty}));
    assert(!backlight_bind_profile((struct backlight_io){regs,read_reg,write_duty},BACKLIGHT_PRO11_2));
    exercise();profile=BACKLIGHT_AIR6_1;exercise();
    ready();regs[2]=(2777u<<16)|1228;unsigned before=writes;assert(backlight_percent(0)==44 && writes==before);
    regs[2]=(1953u<<16)|1228;assert(!backlight_bind_profile((struct backlight_io){regs,read_reg,write_duty},BACKLIGHT_AIR6_1));
    assert(writes==before);
    profile=BACKLIGHT_PRO11_2;expected_period=1953;exercise();
    ready();unsigned w=writes;
    assert(!backlight_bind((struct backlight_io){regs,read_reg,write_duty}));
    assert(writes==w && backlight_percent(50)==-1);
    assert(!backlight_bind_profile((struct backlight_io){regs,read_reg,write_duty},99));
    exercise_pro81();
    puts("backlight: PASS discovery without writes, 5-100% bounds, mode/period preservation, unchanged-duty suppression, drift/readback quarantine");
}
