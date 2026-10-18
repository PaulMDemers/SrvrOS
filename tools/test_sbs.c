#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/sbs_sample.h>
struct battery {
    uint8_t memory[256],mode,address,output;
    unsigned step,busy,transactions,mode_reads;
    bool obf,stall,change_mode;
    uint16_t words[32];
};
static uint8_t status(void *v) { struct battery *b=v;return b->stall?2:b->obf?1:0; }
static uint8_t data(void *v) { struct battery *b=v;assert(b->obf);b->obf=false;return b->output; }
static void command(void *v,uint8_t n) { struct battery *b=v;assert(n==0x80||n==0x81);b->mode=n;b->step=0; }
static void value(void *v,uint8_t n) {
    struct battery *b=v;
    if(!b->step++) {
        b->address=n;
        if(b->mode==0x80) {
            if(n==0x20&&b->busy&&!--b->busy) b->memory[0x20]=0;
            b->output=b->memory[n];b->obf=true;
        }
        return;
    }
    assert(b->mode==0x81);
    /* Only EC-SMBus command-bank request writes: never battery write-word. */
    assert(b->address==0x20||b->address==0x22||b->address==0x23);
    b->memory[b->address]=n;
    if(b->address!=0x20) return;
    assert(n==9 && b->memory[0x22]==0x16); /* ReadWord, battery address 0x0b. */
    unsigned cmd=b->memory[0x23];assert(cmd<32);
    uint16_t word=b->words[cmd];
    if(cmd==3 && b->change_mode && ++b->mode_reads==2) word^=0x8000;
    b->memory[0x24]=word;b->memory[0x25]=word>>8;b->busy=2;b->transactions++;
}
static void setup(struct sbs_sampler *s,struct ec_transport *e,struct battery *b) {
    *s=(struct sbs_sampler){0};*b=(struct battery){0};
    b->words[0x1a]=0x31;b->words[9]=12000;b->words[10]=(uint16_t)-250;
    b->words[13]=86;b->words[15]=4300;b->words[16]=5000;b->words[24]=6000;
    b->words[8]=2981;b->words[18]=120;b->words[19]=65535;b->words[22]=0xc0;
    ec_transport_init(e,(struct ec_io){b,status,data,command,value});
}
static int run(struct sbs_sampler *s,uint64_t *now) {
    int r=0;for(unsigned i=0;i<30000&&!r;i++) { *now+=100;r=sbs_sample_poll(s,*now); }return r;
}
int main(void) {
    struct sbs_sampler s;struct ec_transport e;struct battery b;uint64_t now=100;
    setup(&s,&e,&b);assert(!sbs_sample_fresh(&s,now,1000000));
    assert(sbs_sample_begin(&s,&e,0x20,now));assert(!sbs_sample_begin(&s,&e,0x20,now));
    assert(run(&s,&now)==1 && b.transactions==SBS_WORD_COUNT && s.sample.generation==1);
    struct sbs_snapshot *p=&s.sample;
    assert(p->voltage_uv==12000000 && p->current_ua==-250000 && p->temperature_mc==24950);
    assert(p->relative_soc==86 && p->remaining_capacity==4300000 && p->full_capacity==5000000 && p->design_capacity==6000000);
    assert(p->capacity_unit==SBS_CAPACITY_UAH && (p->valid&SBS_VALID_TIME_EMPTY) && !(p->valid&SBS_VALID_TIME_FULL));
    assert(sbs_sample_fresh(&s,now,0) && !sbs_sample_fresh(&s,now+1,0) && !sbs_sample_fresh(&s,now-1,100));
    /* Full is a reported flag; it does not change an independently reported 86%. */
    b.words[22]=0xa0;b.words[10]=0;
    assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==1);
    assert(p->relative_soc==86 && p->current_ua==0 && p->status==0xa0 && p->generation==2);
    b.words[3]=0x8000;b.words[0x1a]=0x3231;b.words[10]=0x8000;
    assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==1);
    assert(p->capacity_unit==SBS_CAPACITY_UWH && p->current_ua==-32768000000ll && p->voltage_uv==1200000000ull);
    assert(p->remaining_capacity==4300000000000ull);
    /* No clamp or made-up percentage for invalid fields/scales. */
    b.words[13]=101;b.words[0x1a]=0x4431;b.words[18]=65535;
    assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==1);
    assert(p->relative_soc==101 && !(p->valid&(SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CURRENT|SBS_VALID_CAPACITY|SBS_VALID_TIME_EMPTY)));
    b.words[22]=0x40;assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==1);assert(!p->valid);
    b.words[22]=0x81;assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==1);assert(!p->valid);
    struct sbs_snapshot last=s.sample;
    b.change_mode=true;assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==-1);
    assert(s.error==SBS_CHANGED_MODE&&!s.current&&!e.poisoned&&!memcmp(&last,&s.sample,sizeof(last)));
    b.change_mode=false;b.stall=true;assert(sbs_sample_begin(&s,&e,0x20,now) && run(&s,&now)==-1);
    assert(s.error==SBS_TRANSPORT&&e.poisoned&&!s.current&&!memcmp(&last,&s.sample,sizeof(last)));
    assert(!sbs_sample_begin(&s,&e,0x20,now));
    setup(&s,&e,&b);assert(sbs_sample_begin(&s,&e,0x20,now));assert(!sbs_sample_poll(&s,now));sbs_sample_cancel(&s);
    assert(s.error==SBS_CANCELED&&e.poisoned&&!s.current);
    setup(&s,&e,&b);assert(sbs_sample_begin(&s,&e,0x20,now));assert(!sbs_sample_poll(&s,now));assert(sbs_sample_poll(&s,now-1)==-1&&s.error==SBS_CLOCK&&e.poisoned);
    setup(&s,&e,&b);assert(sbs_sample_begin(&s,&e,0x20,now));assert(!sbs_sample_poll(&s,now));assert(sbs_sample_poll(&s,now+20000000)==-1&&s.error==SBS_DEADLINE&&e.poisoned);
    setup(&s,&e,&b);assert(sbs_sample_begin(&s,&e,0x20,now));s.polls=100000;assert(sbs_sample_poll(&s,now)==-1&&s.error==SBS_POLL_LIMIT);
    puts("sbs: PASS bounded read-only EC/SMBus pipeline, signed/scaled units, capacity modes, independent full/SOC, invalid fields, sample age, last-good retention, mode races, timeout/cancel/clock quarantine");
}
