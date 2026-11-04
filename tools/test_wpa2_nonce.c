#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/wpa2_nonce.c"
struct fake {struct nonce_state *state;unsigned calls,failures,word_index,damage,fail_slot;bool available,reenter;uint64_t values[4];};
static bool available(void *p){return ((struct fake *)p)->available;}
static bool word(void *p,uint64_t *v){
 struct fake *f=p;f->calls++;
 if(f->reenter){uint8_t b[32];struct nonce_source io={f,sizeof *f,available,word};assert(!nonce_fill(f->state,&io,b));for(unsigned i=0;i<32;i++)assert(!b[i]);}
 if(f->damage==1)f->state->generation++;
 if(f->damage==2)f->state->last[0]^=1;
 if(f->damage==3)f->state->have^=1;
 if(f->fail_slot && f->word_index%4==f->fail_slot-1){*v=0xdead;return false;}
 if(f->failures){f->failures--;*v=0xdead;return false;}
 *v=f->values[f->word_index++%4];return true;
}
static struct fake init(struct nonce_state *s){*s=(struct nonce_state){0};return (struct fake){.state=s,.available=true,.values={0x102030405060708,0x213243546576879,0x32435465768798a,0x435465768798a9b}};}
static bool run(struct fake *f,uint8_t *out){const struct nonce_source io={f,sizeof *f,available,word};return nonce_fill(f->state,&io,out);}
static void refuses(struct fake *f){uint8_t b[32];memset(b,0xa5,sizeof b);assert(!run(f,b));for(unsigned i=0;i<32;i++)assert(!b[i]);assert(f->state->failed);}
int main(void){
 struct nonce_state s;struct fake f=init(&s);uint8_t a[32],b[32];
 assert(run(&f,a) && f.calls==4 && s.generation==1 && s.have && !s.busy);
 for(unsigned i=0;i<4;i++)for(unsigned k=0;k<8;k++)assert(a[8*i+k]==(uint8_t)(f.values[i]>>(8*k)));
 for(unsigned retry=0;retry<10;retry++){f=init(&s);f.failures=retry;assert(run(&f,a) && f.calls==4+retry);}
 f=init(&s);f.failures=10;refuses(&f);assert(f.calls==10);unsigned old=f.calls;refuses(&f);assert(f.calls==old);
 for(unsigned slot=1;slot<=4;slot++){f=init(&s);f.fail_slot=slot;refuses(&f);assert(f.calls==slot-1+10 && !s.generation && !s.have);}
 f=init(&s);assert(run(&f,a));uint64_t prior[4];memcpy(prior,s.last,sizeof prior);f.word_index=0;f.fail_slot=3;for(unsigned i=0;i<4;i++)f.values[i]^=0x5555555555555555ull;refuses(&f);assert(s.generation==1 && !memcmp(prior,s.last,sizeof prior));
 f=init(&s);f.available=false;refuses(&f);assert(!f.calls);
 for(unsigned i=0;i<4;i++)for(unsigned ones=0;ones<2;ones++){f=init(&s);f.values[i]=ones?UINT64_MAX:0;refuses(&f);assert(f.calls==i+1);}
 for(unsigned i=1;i<4;i++)for(unsigned j=0;j<i;j++){f=init(&s);f.values[i]=f.values[j];refuses(&f);assert(f.calls==i+1);}
 f=init(&s);assert(run(&f,a));refuses(&f);assert(s.generation==1); /* Same entire block. */
 f=init(&s);assert(run(&f,a));for(unsigned i=0;i<4;i++)f.values[i]^=0x5555555555555555ull;assert(run(&f,b) && memcmp(a,b,32) && s.generation==2);
 f=init(&s);f.reenter=true;assert(run(&f,a) && !s.failed && !s.busy);
 for(unsigned n=1;n<4;n++){f=init(&s);f.damage=n;refuses(&f);}
 f=init(&s);s.generation=UINT64_MAX;s.have=1;refuses(&f);assert(!f.calls);
 f=init(&s);s.have=1;refuses(&f);assert(!f.calls);
 f=init(&s);s.busy=1;memset(a,0xa5,32);assert(!run(&f,a) && !s.failed && s.busy && !f.calls);for(unsigned i=0;i<32;i++)assert(!a[i]);
 f=init(&s);struct nonce_state saved=s;assert(!run(&f,(void *)&s) && !memcmp(&s,&saved,sizeof s));struct fake saved_f=f;assert(!run(&f,(void *)&f) && !memcmp(&f,&saved_f,sizeof f));
 const struct nonce_source io={&f,sizeof f,available,word};struct nonce_source saved_io=io;assert(!nonce_fill(&s,&io,(void *)&io) && !memcmp(&io,&saved_io,sizeof io));
 assert(!nonce_fill(NULL,&io,a) && !nonce_fill(&s,NULL,a) && !nonce_fill(&s,&io,NULL));
 if(nonce_supported(NULL)){assert(wpa2_nonce_generate(a));assert(wpa2_nonce_generate(b) && memcmp(a,b,32));scrub(a,32);scrub(b,32);puts("nonce-native: CPUID/RDRAND+CF success and distinct blocks;values not logged;not SrvrOS native acceptance");}
 else {assert(!wpa2_nonce_generate(a));for(unsigned i=0;i<32;i++)assert(!a[i]);}
 puts("wpa2-nonce: PASS ten-attempt bound,unavailable/zero/ones/word/block repetition,sticky failures,generation exhaustion,alias/reentry/state mutation;no deterministic fallback or key/association authority");return 0;
}
