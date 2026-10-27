#include <srvros/mt76_channel_tune.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct event {unsigned kind,reg,value;bool clean;};
struct fake {
    unsigned calls,owners,fail,bad,deny,cancel,loss,drift;
    unsigned reads[2],pulses[2],clear_after[2];
    bool ambiguous;
    unsigned observe_calls,observe_fault,observe_exit;bool observe_safe;
    uint8_t rf[128];struct event trace[1024];
};
static unsigned cases;
static const unsigned regs[13]={37,36,35,34,33,32,31,30,29,28,26,27,24};
static const uint8_t initial[13]={2,63,40,221,226,66,66,2,40,48,0,0,51};
static void init(struct fake *f) {
    memset(f,0,sizeof *f);f->observe_safe=true;f->rf[4]=0x30;f->clear_after[0]=f->clear_after[1]=3;
    for(unsigned i=0;i<13;i++)f->rf[regs[i]]=initial[i];
}
static bool owner(void *p,bool clean) {
    struct fake *f=p;f->owners++;
    return f->observe_safe && f->owners!=f->deny && !(f->loss && f->calls>=f->loss) &&
        (clean || !f->cancel || f->calls<f->cancel);
}
static bool record(struct fake *f,unsigned kind,unsigned reg,unsigned v,bool clean) {
    assert(f->calls<1024);f->trace[f->calls++]=(struct event){kind,reg,v,clean};
    if(f->drift==f->calls && !clean)f->rf[37]^=1;
    return f->calls!=f->fail;
}
static bool guard(void *p,uint32_t a,uint32_t *v,bool clean) {
    struct fake *f=p;assert(a==0x1004 || a==0x2000 || a==0x208 || a==0x1200);
    *v=a==0x2000?0xf000f200:a==0x208?0x40000050:0;
    bool ok=record(f,0,a,*v,clean);if(f->bad==f->calls)*v^=1;return ok;
}
static bool read_rf(void *p,uint8_t a,uint8_t *v,bool clean) {
    struct fake *f=p;bool known=a==4;for(unsigned i=0;i<13;i++)known|=a==regs[i];assert(known);
    if(a==4 && f->rf[a]&0x80) {
        unsigned n=clean?1:0;f->reads[n]++;
        if(f->clear_after[n] && f->reads[n]>=f->clear_after[n])f->rf[a]&=0x7f;
    }
    *v=f->rf[a];bool ok=record(f,1,a,*v,clean);if(f->bad==f->calls)*v^=1;return ok;
}
static bool write_rf(void *p,uint8_t a,uint8_t v,bool clean) {
    struct fake *f=p;bool known=a==4;for(unsigned i=0;i<13;i++)known|=a==regs[i];assert(known);
    if(a==4)assert(v==0xb0 && !(f->rf[a]&0x80));
    bool ok=record(f,2,a,v,clean);
    if(ok || f->ambiguous) {
        f->rf[a]=v;
        if(a==4){unsigned n=clean?1:0;f->reads[n]=0;f->pulses[n]++;}
    }
    return ok;
}
static bool delay(void *p,unsigned us,bool clean) {
    struct fake *f=p;assert(us==10 || us==90 || us==900 || us==9000 || us==90000);
    return record(f,3,0,us,clean);
}
static struct mt76_channel_tune_io io(struct fake *f) {
    return (struct mt76_channel_tune_io){f,owner,guard,read_rf,write_rf,delay};
}
static bool run(struct fake *f,struct mt76_channel_tune *s,unsigned ch) {
    const struct mt76_channel_tune_io ops=io(f);const uint8_t us[2]={'U','S'};
    cases++;return mt76_channel_tune_run(s,us,ch,20,&ops);
}
static void restored(struct fake *f) {
    for(unsigned i=0;i<13;i++)assert(f->rf[regs[i]]==initial[i]);
    assert(f->rf[4]==0x30);
}
static bool observe(void *p,unsigned channel) {
    struct fake *f=p;assert(channel==6 && !f->observe_calls++);
    assert(f->rf[4]==0x30);unsigned writes=0;
    for(unsigned i=0;i<f->calls;i++)writes+=f->trace[i].kind==2;
    assert(writes==22);f->observe_exit=f->calls;
    if(f->observe_fault==1)return false;
    if(f->observe_fault==2){f->observe_safe=false;return false;}
    if(f->observe_fault==3){f->rf[37]^=1;return true;}
    if(f->observe_fault==4){f->cancel=f->calls;return true;}
    if(f->observe_fault==5){f->rf[4]=0xb0;f->clear_after[0]=f->clear_after[1]=0;return true;}
    return true;
}
static bool run_observed(struct fake *f,struct mt76_channel_tune *s) {
    struct mt76_channel_tune_io ops=io(f);const uint8_t us[2]={'U','S'};
    struct mt76_channel_tune_observer o={f,observe};cases++;
    return mt76_channel_tune_observe_run(s,us,6,20,&ops,&o);
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);
    struct mt76_channel_step fixture[11][21];assert(fread(fixture,1,sizeof fixture,file)==sizeof fixture && fgetc(file)==EOF);fclose(file);
    struct fake good={0};struct mt76_channel_tune reference={0};
    for(unsigned ch=1;ch<=11;ch++) {
        struct fake f;init(&f);struct mt76_channel_tune s={0};assert(run(&f,&s,ch));restored(&f);
        assert(s.target_complete && s.complete && !s.failed && s.cleanup_complete);
        assert(s.target_vco_clear && s.cleanup_vco_clear && s.vco_attempted && s.cleanup_vco_attempted);
        assert(s.saved_mask==8191 && s.touched_mask==8191 && s.restored_mask==8191 && s.steps==21);
        assert(s.target_polls==3 && s.cleanup_polls==3 && s.target_delay_us==100 && s.cleanup_delay_us==100);
        assert(f.pulses[0]==1 && f.pulses[1]==1);
        uint8_t expected[128]={0};for(unsigned i=0;i<13;i++)expected[regs[i]]=initial[i];
        unsigned k=0,back=13,pulses=0,snapshot=0;
        for(unsigned j=0;j<f.calls;j++) {
            struct event e=f.trace[j];
            if(e.kind==1 && !e.clean && e.reg!=4 && !k) {
                /* The first thirteen RF-frequency reads precede any write. */
                if(snapshot<13){assert(e.reg==regs[snapshot] && e.value==initial[snapshot]);snapshot++;}
            }
            if(e.kind!=2)continue;
            if(e.reg==4) {
                assert(e.value==0xb0);pulses++;
                if(!e.clean)assert(k==21 && snapshot==13 && back==13);
                else assert(back==0);
            } else if(!e.clean) {
                assert(snapshot==13 && k<21 && !pulses);
                struct mt76_channel_step step=fixture[ch-1][k++];assert(e.reg==step.reg);
                expected[e.reg]=(uint8_t)((expected[e.reg]&~step.mask)|step.bits);
                assert(e.value==expected[e.reg]);
            } else {
                assert(back && pulses==1);back--;assert(e.reg==regs[back] && e.value==initial[back]);
            }
        }
        assert(k==21 && !back && pulses==2);
        if(ch==11){good=f;reference=s;}
        unsigned before=f.calls;assert(!run(&f,&s,ch) && f.calls==before);
    }
    /* Every successful-path I/O failure, wrong read value, owner refusal,
     * forward cancellation and complete transport loss. Ambiguous writes count
     * as touched, and restoration never converts the failed transaction to pass. */
    for(unsigned n=1;n<=good.calls;n++)for(unsigned mode=0;mode<7;mode++) {
        struct fake f;init(&f);struct mt76_channel_tune s={0};
        if(mode<2){f.fail=n;f.ambiguous=mode==1;}
        if(mode==2) {
            if(good.trace[n-1].kind>1)continue;
            f.bad=n;
        }
        if(mode==3)f.cancel=n;
        if(mode==4)f.loss=n;
        /* Arbitrary mutation after the final read is outside exclusive ownership.
         * Inject register drift during forward work; cleanup readback corruption
         * and lost cleanup ownership are covered independently. */
        if(mode==5){if(good.trace[n-1].clean)continue;f.drift=n;}
        if(mode==6)f.deny=n;
        bool ok=run(&f,&s,11);
        if(mode<3 || mode==4)assert(!ok && !s.complete && s.failed);
        if(mode==3 && n<good.calls)assert(!ok && !s.complete);
        if(s.cleanup_complete && s.touched_mask)restored(&f);
        if(ok)assert(s.target_complete && s.cleanup_complete && !s.failed);
        assert(s.target_polls<=6 && s.cleanup_polls<=6 && s.target_delay_us<=100000 && s.cleanup_delay_us<=100000);
        assert(!(s.touched_mask&~s.saved_mask) && !(s.restored_mask&~s.touched_mask));
    }
    for(unsigned n=1;n<=good.owners;n++) {
        struct fake f;init(&f);f.deny=n;struct mt76_channel_tune s={0};
        assert(!run(&f,&s,11) && s.failed && !s.complete);
        if(s.cleanup_complete && s.touched_mask)restored(&f);
    }
    for(unsigned phase=0;phase<2;phase++)for(unsigned clear=0;clear<=8;clear++) {
        struct fake f;init(&f);f.clear_after[phase]=clear;struct mt76_channel_tune s={0};bool ok=run(&f,&s,6);
        assert(ok==(clear>=1 && clear<=6));
        if(ok)restored(&f);
        else assert(!s.complete && s.failed && !s.cleanup_complete);
        assert(s.target_polls<=6 && s.cleanup_polls<=6 && s.target_delay_us<=100000 && s.cleanup_delay_us<=100000);
        if(!phase && !clear)assert(s.restored_mask==0 && !s.cleanup_vco_attempted);
    }
    /* Profile/refusal never performs any I/O. */
    for(unsigned ch=0;ch<=256;ch++)if(!ch || ch>11) {
        struct fake f;init(&f);struct mt76_channel_tune s={0};assert(!run(&f,&s,ch) && !f.calls && s.status==MT76_CHANNEL_REFUSED);
    }
    for(unsigned which=0;which<10;which++) {
        struct fake f;init(&f);struct mt76_channel_tune s={0};struct mt76_channel_tune_io ops=io(&f);uint8_t us[2]={'U','S'};
        if(which==0)ops.owned=NULL;
        if(which==1)ops.read_guard=NULL;
        if(which==2)ops.read_rf=NULL;
        if(which==3)ops.write_rf=NULL;
        if(which==4)ops.delay=NULL;
        if(which==5)us[0]='C';
        cases++;assert(!mt76_channel_tune_run(which==6?NULL:&s,which==7?NULL:us,which==9?UINT32_MAX:1,which==8?40:20,which==9?NULL:&ops) && !f.calls);
    }
    for(unsigned i=0;i<14;i++)for(unsigned bit=0;bit<8;bit++) {
        struct fake f;init(&f);f.rf[i<13?regs[i]:4]^=(uint8_t)(1u<<bit);struct mt76_channel_tune s={0};
        assert(!run(&f,&s,11) && !s.touched_mask && !s.vco_attempted);
    }
    struct fake observed;init(&observed);struct mt76_channel_tune os={0};
    assert(run_observed(&observed,&os) && os.observe_attempted && os.observe_complete);restored(&observed);
    assert(observed.calls==good.calls+22 && observed.observe_calls==1);
    for(unsigned fault=1;fault<=5;fault++) {
        struct fake f;init(&f);f.observe_fault=fault;struct mt76_channel_tune s={0};
        assert(!run_observed(&f,&s) && s.failed && !s.complete && !s.observe_complete && f.observe_calls==1);
        if(fault==2 || fault==5)assert(!s.cleanup_complete && !s.restored_mask);
        else {assert(s.cleanup_complete);restored(&f);}
    }
    for(unsigned n=1;n<=observed.calls;n++)for(unsigned mode=0;mode<5;mode++) {
        struct fake f;init(&f);struct mt76_channel_tune s={0};
        if(mode<2){f.fail=n;f.ambiguous=mode==1;}
        if(mode==2){if(observed.trace[n-1].kind>1)continue;f.bad=n;}
        if(mode==3)f.loss=n;
        if(mode==4)f.deny=n;
        assert(!run_observed(&f,&s) && !s.complete && s.failed && f.observe_calls<=1);
        if(s.cleanup_complete && s.touched_mask)restored(&f);
        if(f.observe_calls)assert(s.target_complete);
    }
    for(unsigned n=1;n<=observed.owners;n++) {
        struct fake f;init(&f);f.deny=n;struct mt76_channel_tune s={0};
        assert(!run_observed(&f,&s) && s.failed && !s.complete && f.observe_calls<=1);
        if(s.cleanup_complete && s.touched_mask)restored(&f);
    }
    struct fake refused;init(&refused);struct mt76_channel_tune rs={0};struct mt76_channel_tune_io ops=io(&refused);uint8_t us[2]={'U','S'};
    struct mt76_channel_tune_observer empty={0};
    assert(!mt76_channel_tune_observe_run(&rs,us,6,20,&ops,NULL) && !refused.calls && !rs.attempted);
    assert(!mt76_channel_tune_observe_run(&rs,us,6,20,&ops,&empty) && !refused.calls && !rs.attempted);
    printf("PASS observer single-shot/boundary failure and independent quiescence/restore; %u calls/%u owners\n",observed.calls,observed.owners);
    printf("PASS %u tune transactions;11 reference frequency plans,all %u I/O and %u ownership boundaries;bounded VCO and independent channel1 cleanup\n",cases,good.calls,good.owners);
    assert(reference.steps==21);return 0;
}
