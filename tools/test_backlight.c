#include <assert.h>
#include <stdio.h>
#include <srvros/backlight.h>
static uint32_t regs[3];static unsigned reads,writes;static int broken;
static uint32_t read_reg(void *ctx,unsigned reg) { assert(ctx==regs && reg<3);reads++;return regs[reg]; }
static void write_duty(void *ctx,uint32_t value) { assert(ctx==regs);writes++;if(!broken) regs[2]=value; }
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
    puts("backlight: PASS discovery without writes, 5-100% bounds, mode/period preservation, unchanged-duty suppression, drift/readback quarantine");
}
