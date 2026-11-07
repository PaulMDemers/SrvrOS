#include <srvros/mt76_channel_plan.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct event {unsigned type,address,value;bool cleanup;};
struct fake {
    unsigned calls,owners,fail,deny,bad,cancel,loss;
    bool ambiguous;
    uint8_t regs[128];struct event trace[2700];
};
static unsigned cases;
static const uint8_t unique[13]={37,36,35,34,33,32,31,30,29,28,26,27,24};
static const uint8_t initial[13]={2,63,40,221,226,66,66,2,40,48,0,0,51};
static void init(struct fake *f){memset(f,0,sizeof *f);for(unsigned i=0;i<13;i++)f->regs[unique[i]]=initial[i];}
static bool owned(void *p,bool cleanup){struct fake *f=p;return ++f->owners!=f->deny && !(f->loss && f->calls>=f->loss) && (cleanup || !f->cancel || f->calls<f->cancel);}
static bool record(struct fake *f,unsigned type,unsigned a,unsigned v,bool cleanup){
    assert(f->calls<2700);f->trace[f->calls++]=(struct event){type,a,v,cleanup};return f->calls!=f->fail;
}
static bool guard(void *p,uint32_t a,uint32_t *v,bool cleanup){
    struct fake *f=p;assert(a==0x1004 || a==0x2000 || a==0x208 || a==0x1200);
    *v=a==0x2000?0xf000f200:a==0x208?0x40000050:0;
    bool ok=record(f,0,a,*v,cleanup);if(f->bad==f->calls)*v^=1;return ok;
}
static bool rf_read(void *p,uint8_t a,uint8_t *v,bool cleanup){
    struct fake *f=p;bool valid=false;for(unsigned i=0;i<13;i++)valid|=a==unique[i];assert(valid);
    *v=f->regs[a];bool ok=record(f,1,a,*v,cleanup);if(f->bad==f->calls)*v^=1;return ok;
}
static bool rf_write(void *p,uint8_t a,uint8_t v,bool cleanup){
    struct fake *f=p;bool valid=false;for(unsigned i=0;i<13;i++)valid|=a==unique[i];assert(valid);
    bool ok=record(f,2,a,v,cleanup);if(ok || f->ambiguous)f->regs[a]=v;return ok;
}
static struct mt76_channel_preview_plan plan(void){
    struct mt76_channel_preview_plan p={.country={'U','S'},.bandwidth=20,.count=11};
    for(unsigned i=0;i<11;i++)p.channel[i]=(uint8_t)(i+1);
    return p;
}
static bool run(struct fake *f,struct mt76_channel_preview *s,const struct mt76_channel_preview_plan *p){
    const struct mt76_channel_preview_io io={f,owned,guard,rf_read,rf_write};cases++;return mt76_channel_preview_run(s,p,&io);
}
static void restored(const struct fake *f){for(unsigned i=0;i<13;i++)assert(f->regs[unique[i]]==initial[i]);}
int main(int argc,char **argv){
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);unsigned masked=0;
    for(unsigned ch=1;ch<=11;ch++) {
        struct mt76_channel_step steps[21];assert(mt76_channel_plan(ch,steps));
        for(unsigned i=0;i<21;i++) {
            assert(steps[i].reg==fgetc(file) && steps[i].mask==fgetc(file) && steps[i].bits==fgetc(file));
            assert(!(steps[i].bits&~steps[i].mask));
            for(unsigned v=0;v<256;v++) {
                unsigned target=(v&~steps[i].mask)|steps[i].bits;
                assert((target&~steps[i].mask)==(v&~steps[i].mask));
                assert((target&steps[i].mask)==steps[i].bits);masked++;
            }
        }
    }
    assert(fgetc(file)==EOF);fclose(file);
    for(unsigned ch=0;ch<=256;ch++)if(ch<1 || ch>11){
        struct mt76_channel_step steps[21];memset(steps,255,sizeof steps);assert(!mt76_channel_plan(ch,steps));
        for(unsigned i=0;i<sizeof steps;i++)assert(!((uint8_t *)steps)[i]);
    }
    struct mt76_channel_step steps[21];assert(!mt76_channel_plan(UINT32_MAX,steps) && !mt76_channel_plan(1,NULL));
    for(unsigned i=0;i<13;i++)assert(mt76_channel_snapshot_reg(i)==unique[i] && mt76_channel_snapshot_initial(i)==initial[i]);
    assert(mt76_channel_snapshot_reg(13)==UINT32_MAX && mt76_channel_snapshot_initial(13)==UINT32_MAX);
    struct fake good;init(&good);struct mt76_channel_preview s={0};struct mt76_channel_preview_plan p=plan();
    assert(run(&good,&s,&p) && s.complete && !s.failed && s.cleanup_complete && s.status==MT76_CHANNEL_OK);
    assert(s.channels_completed==11 && s.steps_completed==231 && s.saved_mask==8191 && s.touched_mask==8191 && s.restored_mask==8191);
    assert(s.guard_reads==1860 && s.rf_reads==488 && s.rf_writes==244 && s.cleanup_reads==13 && s.cleanup_writes==13);
    assert(good.calls==2592 && good.owners==2593);restored(&good);
    for(unsigned n=0;n<13;n++)assert(good.trace[4+n].type==1 && good.trace[4+n].address==unique[n]);
    /* Independent fixture drives each expected intermediate value, including
     * repeated masked writes to one register and cross-channel changes. */
    uint8_t state[128]={0};for(unsigned i=0;i<13;i++)state[unique[i]]=initial[i];
    for(unsigned c=0;c<11;c++) {
        assert(mt76_channel_plan(c+1,steps));
        for(unsigned k=0;k<21;k++) {
            unsigned a=steps[k].reg,before=state[a],after=(before&~steps[k].mask)|steps[k].bits;
            assert(s.before[c][k]==before && s.after[c][k]==after);state[a]=(uint8_t)after;
        }
    }
    struct mt76_channel_preview saved=s;unsigned count=good.calls;assert(!run(&good,&s,&p) && good.calls==count && !memcmp(&s,&saved,sizeof s));
    for(unsigned failure=1;failure<=2592;failure++) {
        struct fake f;init(&f);f.fail=failure;f.ambiguous=true;s=(struct mt76_channel_preview){0};
        assert(!run(&f,&s,&p) && !s.complete && s.failed && s.status==MT76_CHANNEL_IO);
        assert(f.calls<2700);
        if(failure>=2559 && failure<=2562) {
            assert(!s.cleanup_complete && !s.restored_mask && !s.cleanup_reads && !s.cleanup_writes);
        } else restored(&f);
        if(good.trace[failure-1].type==2 && !good.trace[failure-1].cleanup) {
            assert(f.trace[failure].type==1 && f.trace[failure].cleanup && f.trace[failure].address==good.trace[failure-1].address);
        }
        if(good.trace[failure-1].type==0 && good.trace[failure-1].cleanup)assert(!s.cleanup_complete);
        if(failure<18)assert(!s.rf_writes);
        if(good.trace[failure-1].type!=0)continue;
        init(&f);f.bad=failure;s=(struct mt76_channel_preview){0};assert(!run(&f,&s,&p) && s.failed && s.status==MT76_CHANNEL_STATE);
    }
    /* A false write result is also tested with no publication. Other saved
     * fields must still be restored; an unverified field is never accepted. */
    for(unsigned n=0;n<good.calls;n++)if(good.trace[n].type==2) {
        struct fake f;init(&f);f.fail=n+1;s=(struct mt76_channel_preview){0};
        assert(!run(&f,&s,&p) && s.failed && !s.complete && s.status==MT76_CHANNEL_IO);
        if(!good.trace[n].cleanup)restored(&f);
        else {
            assert(!s.cleanup_complete);
            for(unsigned i=0;i<13;i++)if(unique[i]!=good.trace[n].address)assert(f.regs[unique[i]]==initial[i]);
        }
    }
    for(unsigned denial=1;denial<=2593;denial++) {
        struct fake f;init(&f);f.deny=denial;s=(struct mt76_channel_preview){0};assert(!run(&f,&s,&p) && s.failed && !s.complete && s.status==MT76_CHANNEL_OWNER);
    }
    /* Permanent forward cancellation still allows independent cleanup; loss
     * of all transport ownership stops I/O and cannot claim restoration. */
    for(unsigned at=1;at<=2558;at++) {
        struct fake f;init(&f);f.cancel=at;s=(struct mt76_channel_preview){0};
        assert(run(&f,&s,&p)==(at==2558));
        if(at<2558)assert(s.failed && s.status==MT76_CHANNEL_OWNER);
        restored(&f);
        if(s.touched_mask)assert(s.cleanup_complete && s.restored_mask==s.touched_mask);
        init(&f);f.loss=at;s=(struct mt76_channel_preview){0};assert(!run(&f,&s,&p) && f.calls==at && !s.complete && s.failed);
        if(s.touched_mask)assert(!s.cleanup_complete);
    }
    /* Readback mismatches at each normal RF read, including initial snapshot,
     * per-step old/new words and restored originals. */
    for(unsigned n=0;n<good.calls;n++)if(good.trace[n].type==1) {
        struct fake f;init(&f);f.bad=n+1;s=(struct mt76_channel_preview){0};assert(!run(&f,&s,&p) && s.failed && !s.complete);restored(&f);
    }
    for(unsigned channel=1;channel<=11;channel++) {
        struct fake f;init(&f);p=plan();p.count=1;memset(p.channel,0,sizeof p.channel);p.channel[0]=(uint8_t)channel;s=(struct mt76_channel_preview){0};
        assert(run(&f,&s,&p) && s.channels_completed==1 && s.steps_completed==21);restored(&f);
    }
    for(unsigned variant=0;variant<12;variant++) {
        struct fake f;init(&f);p=plan();
        switch(variant){case 0:p.country[0]='u';break;case 1:p.country[1]='K';break;
        case 2:p.bandwidth=40;break;case 3:p.bandwidth=80;break;case 4:p.count=0;break;
        case 5:p.count=12;break;case 6:p.channel[0]=0;break;case 7:p.channel[0]=12;break;
        case 8:p.channel[0]=14;break;case 9:p.channel[0]=36;break;case 10:p.channel[0]=2;break;
        case 11:p.count=1;break;}
        s=(struct mt76_channel_preview){0};assert(!run(&f,&s,&p) && s.failed && s.status==MT76_CHANNEL_REFUSED && !f.calls && !f.owners);
    }
    struct fake f;init(&f);p=plan();struct mt76_channel_preview_io io={&f,owned,guard,rf_read,rf_write};
    assert(!mt76_channel_preview_run(NULL,&p,&io));
    for(unsigned variant=0;variant<6;variant++) {
        struct mt76_channel_preview_io bad=io;s=(struct mt76_channel_preview){0};
        if(variant==2)bad.owned=NULL;
        if(variant==3)bad.read_guard=NULL;
        if(variant==4)bad.read_rf=NULL;
        if(variant==5)bad.write_rf=NULL;
        assert(!mt76_channel_preview_run(&s,variant==0?NULL:&p,variant==1?NULL:&bad));assert(s.failed && !f.calls && !f.owners);
    }
    printf("MT76 US channel preview: PASS %u transactions,%u masked words,11 independent frequency plans,all callback/ownership/readback failures,reverse cleanup,ambiguous writes,no replay; no native caller\n",cases,masked);
}
