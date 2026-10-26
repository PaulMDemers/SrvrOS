#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned op,auth,fail,deny,bad,mutate;uint32_t second;struct mt76_agc_state *s;};
static struct mt76_agc_state states[8];static bool save;
static bool auth(void *p){struct fake *f=p;++f->auth;if(f->mutate==f->auth)f->s->reads++;return f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->op++;assert(a==mt76_agc_state_address(op));if(save)states[op]=*f->s;
 unsigned n=op<3?op:op>=5?op-5:3;*v=n==0?0:n==1?0xf000f200:n==2?0x40000050:op==3?0x16344ef0:f->second;
 if(f->bad==op+1)*v=UINT32_MAX;
 return f->fail!=op+1;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_agc_state *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,0};return mt76_agc_state_capture(s,&io);}
static void incomplete(const struct mt76_agc_state *s){assert(!s->complete && !s->init_done && !s->low_gain && !s->initial[0] && !s->initial[1] && !s->current[0] && !s->current[1] && !mt76_agc_state_valid(s));}
int main(void){struct fake f={.second=0x12345678};struct mt76_agc_state s={0};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_agc_state_valid(&s) && s.reads==8 && f.op==8 && f.auth==9 && s.initial[0]==78 && s.initial[1]==86 && s.current[1]==86 && s.low_gain==-1);save=false;assert(run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned op=0;op<8;op++){
  f=(struct fake){.fail=op+1};s=(struct mt76_agc_state){0};assert(run(&f,&s)==MT76_TIMING_IO && f.op==op+1);incomplete(&s);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  f=(struct fake){.bad=op+1};s=(struct mt76_agc_state){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.op==op+1);incomplete(&s);
  s=states[op];assert(mt76_agc_state_admit(&s,op));s.reads++;assert(!mt76_agc_state_admit(&s,op));s=states[op];s.init_done=true;assert(!mt76_agc_state_admit(&s,op));s=states[op];s.low_gain=-1;assert(!mt76_agc_state_admit(&s,op));s=states[op];s.current[0]=1;assert(!mt76_agc_state_admit(&s,op));
 }
 for(unsigned n=1;n<=9;n++){f=(struct fake){.deny=n};s=(struct mt76_agc_state){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && f.op==n-1);incomplete(&s);f=(struct fake){.mutate=n};s=(struct mt76_agc_state){0};assert(run(&f,&s)==MT76_TIMING_STATE && f.op==n-1);incomplete(&s);}
 uint32_t seed=71;
 for(unsigned i=0;i<65536;i++){seed=seed*1664525u+1013904223u;f=(struct fake){.second=seed};s=(struct mt76_agc_state){0};assert(run(&f,&s)==MT76_TIMING_OK && mt76_agc_state_valid(&s) && s.initial[1]==((seed>>8)&127));s.current[1]^=1;assert(!mt76_agc_state_valid(&s));s.current[1]^=1;s.guards[1][2]^=1;assert(!mt76_agc_state_valid(&s));}
 struct mt76_post_vco pv={.attempted=true,.complete=true,.operations=63,.reads=54,.exchanges=9,.acks=1,.observed=8,.waits=6,.delay_us=1000000,.first_clear=1,.received=16};
 for(unsigned q=0;q<9;q++){pv.words[q][0]=pv.words[q][3]=0x4a000008|(mt76_post_vco_sequence(q)<<16);pv.words[q][1]=0x80000004;pv.words[q][2]=q==1 || q==2?176:48;}
 for(unsigned q=0;q<18;q++){pv.guards[q][1]=0xf000f200;pv.guards[q][2]=0x40000050;}
 assert(mt76_post_vco_captured_profile_valid(&pv));
 for(unsigned q=0;q<18;q++)for(unsigned j=0;j<3;j++){pv.guards[q][j]^=1;assert(!mt76_post_vco_captured_profile_valid(&pv));pv.guards[q][j]^=1;}
 for(unsigned q=0;q<9;q++)for(unsigned j=0;j<4;j++){pv.words[q][j]^=1;assert(!mt76_post_vco_captured_profile_valid(&pv));pv.words[q][j]^=1;}
 pv.words[2][2]=48;assert(!mt76_post_vco_captured_profile_valid(&pv));pv.words[2][2]=176;pv.complete=false;assert(!mt76_post_vco_captured_profile_valid(&pv));
 assert(mt76_agc_state_address(8)==UINT32_MAX);
 puts("AGC state: PASS8 fixed reads,all transfer/auth/guard/target boundaries,65536 second-register decodes,no partial initialization or replay");
}
