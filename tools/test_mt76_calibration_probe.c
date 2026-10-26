#include <srvros/mt76_calibration.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fixture {
    struct mt76_calibration_probe state;
    unsigned calls,auths,fail,cancel,restore_bad;
    uint32_t alc,ibi,ibi_after_zero;
    bool failed,cancelled,readonly_ibi;
};
static bool owned(void *p){struct fixture *f=p;if(++f->auths==f->cancel)f->cancelled=true;return !f->cancelled;}
static bool event(struct fixture *f,bool cleanup){if(!cleanup)assert(!f->failed && !f->cancelled);if(++f->calls==f->fail){f->failed=true;return false;}return true;}
static bool read_word(void *p,uint32_t a,uint32_t *v){struct fixture *f=p;bool ok=event(f,false);
    switch(a){case 0x1004:*v=12;break;case 0x2000:*v=0xf000f200;break;case 0x208:*v=0x40000050;break;case 0x13b0:*v=f->alc;break;case 0x2124:*v=f->ibi;break;default:assert(0);}return ok;}
static bool write_word(void *p,uint32_t a,uint32_t v){struct fixture *f=p;bool ok=event(f,false);
    if(a==0x13b0){assert(v==0);f->alc=v;}else{assert(a==0x2124 && v==0xffffff7e);if(!f->readonly_ibi)f->ibi=v;}return ok;}
static bool delay(void *p,unsigned us){struct fixture *f=p;assert(us==500 && !f->alc);bool ok=event(f,false);f->ibi=f->ibi_after_zero;return ok;}
static bool restore(void *p,uint32_t a,uint32_t saved,uint32_t *observed){struct fixture *f=p;bool ok=event(f,true);
    if(a==0x13b0){assert(saved==0x2f2f1b1b);f->alc=saved;*observed=f->alc;}
    else{assert(a==0x2124 && saved==f->ibi_after_zero);if(!f->readonly_ibi)f->ibi=saved;*observed=f->ibi;}
    if(a==f->restore_bad){*observed^=1;}
    return ok;}
static struct fixture fresh(void){return (struct fixture){.alc=0x2f2f1b1b,.ibi=0xffffffff,.ibi_after_zero=0xffffffff};}
static enum mt76_calibration_status run(struct fixture *f){const struct mt76_calibration_io io={f,owned,read_word,write_word,NULL,delay,restore};return mt76_calibration_probe_run(&f->state,&io);}
static void no_replay(struct fixture *f){unsigned n=f->calls;struct mt76_calibration_probe s=f->state;assert(run(f)==MT76_CAL_REFUSED && f->calls==n && !memcmp(&s,&f->state,sizeof(s)));}
int main(void){
    struct fixture base=fresh();assert(run(&base)==MT76_CAL_OK && base.state.complete);
    assert(base.calls==15 && base.auths==14 && base.state.reads==10 && base.state.writes==2 && base.state.waits==1);
    assert(base.state.saved_mask==3 && base.state.modified_mask==3 && base.state.restore_attempted==3 && base.state.raw_restore_mask==3);
    assert(base.state.ibi_write_matched && base.state.ibi_changed && base.state.observed_ibi==0xffffff7e);
    assert(base.alc==0x2f2f1b1b && base.ibi==0xffffffff);no_replay(&base);
    for(unsigned n=1;n<=15;n++){struct fixture f=fresh();f.fail=n;assert(run(&f)!=MT76_CAL_OK && !f.state.complete);
        assert(f.state.restore_attempted==f.state.modified_mask);assert(f.alc==0x2f2f1b1b);
        if(f.state.saved_mask&2){assert(f.ibi==f.state.saved[1]);}
        no_replay(&f);}
    for(unsigned n=1;n<=14;n++){struct fixture f=fresh();f.cancel=n;assert(run(&f)==MT76_CAL_REFUSED && !f.state.complete);
        assert(f.alc==0x2f2f1b1b && f.state.restore_attempted==f.state.modified_mask);no_replay(&f);}
    for(unsigned n=0;n<2;n++){struct fixture f=fresh();f.restore_bad=n?0x2124:0x13b0;
        assert(run(&f)==MT76_CAL_RESTORE && !f.state.complete && f.state.restore_attempted==3);no_replay(&f);}
    struct fixture f=fresh();f.readonly_ibi=true;assert(run(&f)==MT76_CAL_OK && f.state.complete && !f.state.ibi_write_matched && !f.state.ibi_changed);
    assert(f.state.observed_ibi==0xffffffff && f.state.raw_restore_mask==3);no_replay(&f);
    f=fresh();f.ibi_after_zero=0x12345678;assert(run(&f)==MT76_CAL_OK && f.state.saved[1]==0x12345678 && f.ibi==0x12345678);no_replay(&f);
    f=fresh();f.alc=0xffffffff;assert(run(&f)!=MT76_CAL_OK && !f.state.modified_mask);no_replay(&f);
    f=fresh();f.alc=0;assert(run(&f)==MT76_CAL_REFUSED && !f.state.modified_mask);no_replay(&f);
    assert(mt76_calibration_probe_run(NULL,NULL)==MT76_CAL_REFUSED);
    puts("calibration probe: PASS reference order,fresh post-delay IBI save,write-effect versus readonly observation,every failure/cancellation,reverse raw restoration,no commands or replay; synthetic only");
}
